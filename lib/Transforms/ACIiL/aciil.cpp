#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/ACIiL.h"
#include "llvm/Transforms/IPO/Internalize.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/ACIiL/CFGModule.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/CFG.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Linker/Linker.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/ACIiL/ACIiLAllocaManager.h"

#include <iterator>

using namespace llvm;

namespace {
  struct ACIiLPass : public ModulePass {
    static char ID;
    ACIiLPass() : ModulePass(ID) {}

    struct CheckpointRestartBlocksInfo {
      CFGNode * node;
      BasicBlock &checkpointBlock;
      BasicBlock &restartBlock;
      int64_t labelNumber;
      CheckpointRestartBlocksInfo(CFGNode * n, BasicBlock &cBB, BasicBlock &rBB,
        int64_t lN) : node(n), checkpointBlock(cBB), restartBlock(rBB), labelNumber(lN)
      {
      }
    };

    Function * aciilCheckpointSetup;
    Function * aciilCheckpointStart;
    Function * aciilCheckpointPointer;
    Function * aciilCheckpointFinish;
    Function * aciilRestartGetLabel;
    Function * aciilRestartReadFromCheckpoint;

    virtual bool runOnModule(Module &M) {
      errs() << "In module called: " << M.getName() << "!\n";
      Function * mainFunction = M.getFunction("main");
      if(!mainFunction) return false;

      //load the bitcodes for checkpointing and restarting exists
      SMDiagnostic error;
      std::unique_ptr<Module> checkpointModule = parseIRFile("checkpoint.bc", error, M.getContext());
      if(!checkpointModule)
      {
        errs() << "could not load the checkpoint bitcode, nothing to be done\n";
        return false;
      }
      std::unique_ptr<Module> restartModule = parseIRFile("restart.bc", error, M.getContext());
      if(!restartModule)
      {
        errs() << "could not load the restart bitcode, nothing to be done\n";
        return false;
      }

      //create a CFG module with live analysis and split phi nodes correctly
      CFGModule cfgModule(M, *mainFunction);

      //link in the bitcode with functions for checkpointing
      unsigned ApplicableFlags = Linker::Flags::OverrideFromSrc;
      if(Linker::linkModules(M, std::move(checkpointModule), ApplicableFlags))
      {
        errs() << "Failed linking in the checkpoint code.\n";
        return false;
      }
      ApplicableFlags = Linker::Flags::OverrideFromSrc;
      if(Linker::linkModules(M, std::move(restartModule), ApplicableFlags))
      {
        errs() << "Failed linking in the restart code.\n";
        return false;
      }

      //load the checkpointing functions
      aciilCheckpointSetup = M.getFunction("__aciil_checkpoint_setup");
      aciilCheckpointStart = M.getFunction("__aciil_checkpoint_start");
      aciilCheckpointPointer = M.getFunction("__aciil_checkpoint_pointer");
      aciilCheckpointFinish = M.getFunction("__aciil_checkpoint_finish");
      if(!aciilCheckpointSetup || !aciilCheckpointStart
        || !aciilCheckpointPointer || !aciilCheckpointFinish)
      {
        errs() << "could not load the checkpointing functions, checkpointing will not be added\n";
        return false;
      }
      //load the restart functions
      aciilRestartGetLabel = M.getFunction("__aciil_restart_get_label");
      aciilRestartReadFromCheckpoint = M.getFunction("__aciil_restart_read_from_checkpoint");
      if(!aciilRestartGetLabel || !aciilRestartReadFromCheckpoint)
      {
        errs() << "could not load the restarting functions, checkpointing will not be added\n";
        return false;
      }

      //get the main cfg
      CFGFunction &cfgMain = cfgModule.getEntryFunction();
      bool changed = addCheckpointsToFunction(cfgMain, cfgModule);
      return changed;
    }

    bool addCheckpointsToFunction(CFGFunction &cfgFunction, CFGModule &cfgModule)
    {
      std::vector<CheckpointRestartBlocksInfo> checkpointAndRestartBlocks;
      //insert the checkpoint and restart blocks
      //the blocks are empty at first, just with correct branching
      int64_t nextLabel = 0;
      for(CFGNode * cfgNode : cfgFunction.getNodes())
      {
        //don't checkpoint phiNodes
        if(cfgNode->isPhiNode()) continue;
        //don't do this for the entry block or the exit blocks
        BasicBlock &B = cfgNode->getLLVMBasicBlock();
        unsigned numSuccessors = std::distance(succ_begin(&B), succ_end(&B));
        unsigned numPredecessors = std::distance(pred_begin(&B), pred_end(&B));
        if(numSuccessors == 0 || numPredecessors == 0) continue;
        //skip a block if it has no live variables
        if(cfgNode->getIn().size() == 0) continue;

        checkpointAndRestartBlocks.push_back(createCheckpointAndRestartBlocksForNode(cfgNode, cfgFunction, cfgModule, nextLabel++));
      }

      //if no checkpoints were inserted then just return now
      if(checkpointAndRestartBlocks.size() == 0) return true;

      //otherwise insert the restart function and add the branch instructions for a restart
      insertRestartBlock(cfgFunction, cfgModule, checkpointAndRestartBlocks);

      //insert the checkpoint and restart nodes into the CFG
      for(CheckpointRestartBlocksInfo crbi : checkpointAndRestartBlocks)
      {
        cfgFunction.addCheckpointNode(crbi.checkpointBlock, *crbi.node);
        cfgFunction.addRestartNode(crbi.restartBlock, *crbi.node);
      }

      //fill out the checkpoint and restart blocks
      for(CheckpointRestartBlocksInfo crbi : checkpointAndRestartBlocks)
      {
        fillCheckpointAndRestartBlocksForNode(crbi, cfgFunction, cfgModule);
      }

      //now need to fix dominanance
      fixDominance(cfgFunction, cfgModule);
      return true;
    }

    CheckpointRestartBlocksInfo createCheckpointAndRestartBlocksForNode(CFGNode * node, CFGFunction &cfgFunction, CFGModule &cfgModule, int64_t labelNumber)
    {
      BasicBlock &B = node->getLLVMBasicBlock();
      //add a checkpoint block
      BasicBlock * checkpointBlock = BasicBlock::Create(cfgModule.getLLVMModule().getContext(), B.getName() + ".checkpoint", &cfgFunction.getLLVMFunction());
      //make sure all predecessors of B now point at the checkpoint
      for(BasicBlock * p : predecessors(&B))
      {
        for(unsigned i = 0; i < p->getTerminator()->getNumSuccessors(); i++)
        {
          if(p->getTerminator()->getSuccessor(i) == &B)
          {
            p->getTerminator()->setSuccessor(i, checkpointBlock);
          }
        }
      }
      //add a branch instruction from the end of the checkpoint block to the original block
      BranchInst::Create(&B, checkpointBlock);

      //add a restart block
      BasicBlock * restartBlock = BasicBlock::Create(cfgModule.getLLVMModule().getContext(), B.getName() + ".read_checkpoint", &cfgFunction.getLLVMFunction());
      //add a branch instruction from the end of the checkpoint block to the original block
      BranchInst::Create(&B, restartBlock);

      return CheckpointRestartBlocksInfo(node, *checkpointBlock, *restartBlock, labelNumber);
    }

    void insertRestartBlock(CFGFunction &cfgFunction, CFGModule &cfgModule, std::vector<CheckpointRestartBlocksInfo> checkpointAndRestartBlocks)
    {
      //the entry block is transformed in the following way
      //1. all instructions from entry are copied to a new block
      //2. all phi nodes are updated with that new block
      //3. Function for checkpoint and restart and inserted into the entry block
      //4. a switch statment is used to either perform a restart or run the program form the beginning (the new entry block)
      BasicBlock &entry = cfgFunction.getLLVMFunction().getEntryBlock();
      BasicBlock * noCREntry = BasicBlock::Create(cfgModule.getLLVMModule().getContext(), "no_cr_entry", &cfgFunction.getLLVMFunction());

      //loop over all successor blocks and replace the basic block in phi nodes
      for (BasicBlock * Successor : successors(&entry)) {
        for (PHINode &PN : Successor->phis()) {
          int Idx = PN.getBasicBlockIndex(&entry);
          while (Idx != -1) {
            PN.setIncomingBlock((unsigned)Idx, noCREntry);
            Idx = PN.getBasicBlockIndex(&entry);
          }
        }
      }
      //move the instructions over to the "default entry block"
      noCREntry->getInstList().splice(noCREntry->end(), entry.getInstList(), entry.begin(), entry.end());

      //insert the calls
      std::vector<Value*> emptyArgs;
      IRBuilder<> builder(&entry);
      //insert the call that gets the label for the restart
      CallInst* ciGetLabel = builder.CreateCall(aciilRestartGetLabel, emptyArgs);

      //insert the checkpoint set up call
      builder.CreateCall(aciilCheckpointSetup, emptyArgs);

      //insert the switch with the default being carry on as if not checkpoint happened
      SwitchInst * si = builder.CreateSwitch(ciGetLabel, noCREntry, checkpointAndRestartBlocks.size());
      for(CheckpointRestartBlocksInfo crbi : checkpointAndRestartBlocks)
      {
        si->addCase(ConstantInt::get(cfgModule.getLLVMModule().getContext(), APInt(64, crbi.labelNumber, true)), &crbi.restartBlock);
      }

      //add the noCREntry block into the CFG
      cfgFunction.addNoCREntryNode(*noCREntry);
    }

    void fillCheckpointAndRestartBlocksForNode(CheckpointRestartBlocksInfo &crbi, CFGFunction &cfgFunction, CFGModule &cfgModule)
    {
      //initilise data layout
      DataLayout dataLayout(&cfgModule.getLLVMModule());
      //create commonly used types
      IntegerType *i64Type = IntegerType::getInt64Ty(cfgModule.getLLVMModule().getContext());
      IntegerType *i32Type = IntegerType::getInt32Ty(cfgModule.getLLVMModule().getContext());
      IntegerType *i8Type = IntegerType::getInt8Ty(cfgModule.getLLVMModule().getContext());
      Type *i8PType = PointerType::get(i8Type, /*address space*/0);

      IRBuilder<> builder(&crbi.checkpointBlock);
      //insert instructions before the branch
      builder.SetInsertPoint(&crbi.checkpointBlock.front());

      //set up the checkpoint block
      //firstly call the start checkpoint function
      {
        std::vector<Value*> args;
        args.push_back(ConstantInt::get(i64Type, crbi.labelNumber, true));
        args.push_back(ConstantInt::get(i64Type, crbi.node->getIn().size(), true));
        builder.CreateCall(aciilCheckpointStart, args);
      }
      //for every live variable
      for(CFGOperand op : crbi.node->getIn())
      {
        Value * v = op.getValue();
        if(v->getType()->isPtrOrPtrVectorTy()) continue;

        //First alloca an array with just one element
        AllocaInst * ai = cfgFunction.getAllocManager().getAlloca(v->getType());
        //Then store the value in that alloca
        builder.CreateStore(v, ai);
        //bitcast it to bytes
        Value * bc = builder.CreateBitCast(ai, i8PType, ai->getName() + ".i8");
        std::vector<Value*> checkpointArgs;
        uint64_t numBits = dataLayout.getTypeSizeInBits(v->getType());
        checkpointArgs.push_back(ConstantInt::get(i64Type, numBits, false));
        checkpointArgs.push_back(bc);
        builder.CreateCall(aciilCheckpointPointer, checkpointArgs);
        cfgFunction.getAllocManager().releaseAlloca(ai);
      }

      //add a checkpoint clean up call at the end
      {
        std::vector<Value*> args;
        builder.CreateCall(aciilCheckpointFinish, args);
      }

      //set up the restart block
      builder.SetInsertPoint(&crbi.restartBlock.front());
      //for every live variable
      for(CFGOperand op : crbi.node->getIn())
      {
        Value * v = op.getValue();

        //First alloca an array with just one element
        AllocaInst * ai = builder.CreateAlloca(v->getType(),
                                                llvm::ConstantInt::get(i32Type, 1, true), //array size
                                                v->getName() + ".addr");
        //bitcast it to bytes
        Value * bc = builder.CreateBitCast(ai, i8PType, ai->getName() + ".i8");
        std::vector<Value*> restartArgs;
        uint64_t numBits = dataLayout.getTypeSizeInBits(v->getType());
        restartArgs.push_back(ConstantInt::get(i64Type, numBits, false));
        restartArgs.push_back(bc);
        //call the load function with the correct address
        builder.CreateCall(aciilRestartReadFromCheckpoint, restartArgs);

        LoadInst * li = builder.CreateLoad(ai, v->getName() + ".restart");

        //add a phi instruction for this variable in the correct block
        cfgFunction.findNodeByBasicBlock(crbi.restartBlock)->addLiveMapping(CFGOperand(v), CFGOperand(li));
      }
    }

    void fixDominance(CFGFunction &cfgFunction, CFGModule &cfgModule)
    {
      //struct used for dominance fixing
      struct PHINodeMappingToUpdate {
        PHINode &phi; //phi to fix
        CFGOperand op; //mappinf from
        unsigned phiIdx; //index of the phi value
        PHINodeMappingToUpdate(PHINode &p, CFGOperand o, unsigned pI): phi(p), op(o.getValue()), phiIdx(pI)
        {
        }
      };

      //Step 1.
      //If    a phi node with the live variable as a value already exists then only schedule it for updating
      //Else  in each block, for each live variable create a phi with a value from each of the predecessors
      //      and update the mapping
      //      and replace the uses of that variable with phi
      std::vector<PHINodeMappingToUpdate> phisToUpdate;
      for(CFGNode * node : cfgFunction.getNodes())
      {
        for(CFGOperand op : node->getIn())
        {
          bool phiExists = false;
          //first need to check if a phi already exists, in that case only the values need to be updated
          for(PHINode &phi : node->getLLVMBasicBlock().phis())
          {
            for(unsigned phiIdx = 0; phiIdx < phi.getNumIncomingValues(); phiIdx++)
            {
              if(phi.getIncomingValue(phiIdx) == op.getValue())
              {
                phisToUpdate.push_back(PHINodeMappingToUpdate(phi, op, phiIdx));
                phiExists = true;
              }
            }
          }
          if(phiExists) continue;

          //otherwise if phi does not exist, need to create a new one
          //phi node for this live variable
          PHINode * phi = PHINode::Create(op.getValue()->getType(),
                                          std::distance(pred_begin(&node->getLLVMBasicBlock()), pred_end(&node->getLLVMBasicBlock())),
                                          op.getValue()->getName() + "." + node->getLLVMBasicBlock().getName(),
                                          &node->getLLVMBasicBlock().front());
          //update the uses
          for(Instruction &inst : node->getLLVMBasicBlock())
          {
            for(unsigned i = 0; i < inst.getNumOperands(); i++)
            {
              if(inst.getOperand(i) == op.getValue()) inst.setOperand(i, phi);
            }
          }

          //add the values that need to be updated with the mappings
          unsigned phiIdx = 0;
          for(BasicBlock * pred : predecessors(&node->getLLVMBasicBlock()))
          {
            phi->addIncoming(Constant::getNullValue(op.getValue()->getType()), pred);
            phisToUpdate.push_back(PHINodeMappingToUpdate(*phi, op, phiIdx++));
          }
          node->addLiveMapping(op, CFGOperand(phi));
        }
      }
      //Step 2.
      //Now that all phi nodes have been created with correct mappings, update the mappings in the phi nodes
      for(PHINodeMappingToUpdate ptu : phisToUpdate)
      {
        CFGNode * pred = cfgFunction.findNodeByBasicBlock(*ptu.phi.getIncomingBlock(ptu.phiIdx));
        ptu.phi.setIncomingValue(ptu.phiIdx, pred->getLiveMapping(ptu.op)->getValue());
      }

      cfgFunction.getLLVMFunction().viewCFG();
    }
  };
}

char ACIiLPass::ID = 0;

// static void registerACIiLPass(const PassManagerBuilder &,
//                          legacy::PassManagerBase &PM) {
//   PM.add(new ACIiLPass());
// }

ModulePass *llvm::createACIiLLinkingPass() {
  errs() <<"Adding ACIiLPass!\n";
  return new ACIiLPass();
}

// static RegisterStandardPasses
//     register_pass_O(PassManagerBuilder::EP_OptimizerLast, registerACIiLPass);

// static RegisterStandardPasses
//     register_pass_O0(PassManagerBuilder::EP_EnabledOnOptLevel0, registerACIiLPass);
