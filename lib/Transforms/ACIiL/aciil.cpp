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

#include <iterator> 

using namespace llvm;

namespace {
  struct ACIiLPass : public ModulePass {
    static char ID;
    ACIiLPass() : ModulePass(ID) {}

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
      Function * aciilSetupCheckpoint = M.getFunction("__aciil_setup_checkpoints");
      Function * aciilStartCheckpoint = M.getFunction("__aciil_start_checkpoint");
      Function * aciilCheckpoint = M.getFunction("__aciil_checkpoint");
      Function * aciilFinishCheckpoint = M.getFunction("__aciil_finish_checkpoint");
      if(!aciilSetupCheckpoint || !aciilStartCheckpoint
        || !aciilCheckpoint || !aciilFinishCheckpoint)
      {
        errs() << "could not load the checkpointing functions, checkpointing will not be added\n";
        return false;
      }
      //load the restart functions
      Function * aciilRestartGetLabel = M.getFunction("__aciil_restart_get_label");
      Function * aciilRestartReadFromCheckpoint = M.getFunction("__aciil_restart_read_from_checkpoint");
      if(!aciilRestartGetLabel || !aciilRestartReadFromCheckpoint)
      {
        errs() << "could not load the restarting functions, checkpointing will not be added\n";
        return false;
      }

      //get the main cfg
      CFGFunction &cfgMain = cfgModule.getEntryFunction();

      //initilise data layout
      DataLayout dataLayout(&M);
      //create commonly used types
      IntegerType *i64Type = IntegerType::getInt64Ty(M.getContext());
      IntegerType *i32Type = IntegerType::getInt32Ty(M.getContext());
      IntegerType *i8Type = IntegerType::getInt8Ty(M.getContext());
      Type *i8PType = PointerType::get(i8Type, /*address space*/0);


      int64_t nextLabel = 0;
      std::vector<std::tuple<CFGNode*, BasicBlock*, BasicBlock*, int64_t>> checkpointAndRestartBlocks;

      //insert the checkpoint and restart blocks
      //need to do this first so that dominator tree can be used
      //the blocks are empty at first, just with correct branching
      for(CFGNode &node : cfgMain.getNodes())
      {
        //don't checkpoint phiNodes
        if(node.isPhiNode()) continue;
        //don't do this for the entry block or the exit blocks
        BasicBlock &B = node.getBlock();
        unsigned numSuccessors = std::distance(succ_begin(&B), succ_end(&B));
        unsigned numPredecessors = std::distance(pred_begin(&B), pred_end(&B));
        if(numSuccessors == 0 || numPredecessors == 0) continue;
        //skip a block if it has no live variables
        if(node.getIn().size() == 0) continue;

        //add a checkpoint block
        BasicBlock * checkpointBlock = BasicBlock::Create(M.getContext(), B.getName() + ".checkpoint", &cfgMain.getFunction());
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
        BasicBlock * restartBlock = BasicBlock::Create(M.getContext(), B.getName() + ".read_checkpoint", &cfgMain.getFunction());
        //add a branch instruction from the end of the checkpoint block to the original block
        BranchInst::Create(&B, restartBlock);
        // auto tuple = std::make_tuple(&node, checkpointBlock, restartBlock, nextLabel);
        checkpointAndRestartBlocks.push_back(std::make_tuple(&node, checkpointBlock, restartBlock, nextLabel));
        //incremeant the label counter
        nextLabel++;
        //TODO at the moment only one checkpoint is allowed
        break;
      }

      //if no checkpoints were inserted then just return now
      if(checkpointAndRestartBlocks.size() == 0) return true;

      //otherwise insert the restart function and add the branch instructions for a restart
      {
        BasicBlock &entry = cfgMain.getFunction().getEntryBlock();
        BasicBlock * crEntry = BasicBlock::Create(M.getContext(), "no_cr_entry", &cfgMain.getFunction());

        for (succ_iterator I = succ_begin(&entry), E = succ_end(&entry); I != E; ++I) {
          // Loop over any phi nodes in the basic block, updating the BB field of
          // incoming values...
          BasicBlock *Successor = *I;
          for (auto &PN : Successor->phis()) {
            int Idx = PN.getBasicBlockIndex(&entry);
            while (Idx != -1) {
              PN.setIncomingBlock((unsigned)Idx, crEntry);
              Idx = PN.getBasicBlockIndex(&entry);
            }
          }
        }
        crEntry->getInstList().splice(crEntry->end(), entry.getInstList(), entry.begin(), entry.end());

        std::vector<Value*> emptyArgs;
        //insert the call that gets the label for the restart
        CallInst* ciGetLabel = CallInst::Create(aciilRestartGetLabel, emptyArgs, "", &entry);

        // //insert the checkpoint set up call
        CallInst::Create(aciilSetupCheckpoint, emptyArgs, "", &entry);

        //insert the switch with the default being carry on as if not checkpoint happened
        SwitchInst * si = SwitchInst::Create(ciGetLabel, crEntry, checkpointAndRestartBlocks.size(), &entry);
        for(std::tuple<CFGNode*, BasicBlock*, BasicBlock*, int64_t> tuple : checkpointAndRestartBlocks)
        {
          si->addCase(ConstantInt::get(M.getContext(), APInt(64, std::get<3>(tuple), true)), std::get<2>(tuple));
        }
      }

      //fill out the checkpoint and restart blocks
      for(std::tuple<CFGNode*, BasicBlock*, BasicBlock*, int64_t> tuple : checkpointAndRestartBlocks)
      {
        CFGNode * node;
        BasicBlock * checkpointBlock;
        BasicBlock * restartBlock;
        int64_t label;
        std::tie(node, checkpointBlock, restartBlock, label) = tuple;

        Instruction * checkpointBlockBranchInstruction = &checkpointBlock->front();
        Instruction * restartBlockBranchInstruction = &restartBlock->front();

        //set up the checkpoint block
        //firstly call the start checkpoint function
        {
          std::vector<Value*> args;
          args.push_back(ConstantInt::get(i64Type, label, true));
          CallInst::Create(aciilStartCheckpoint, args, "", checkpointBlockBranchInstruction);
        }
        //for every live variable
        for(CFGOperand op : node->getIn())
        {
          Value * v = op.getValue();

          //First alloca an array with just one element
          AllocaInst * ai = new AllocaInst(v->getType(),
                                  0, //address space
                                  llvm::ConstantInt::get(i32Type, 1, true), //array size
                                  4, //aligment
                                  v->getName() + ".addr",
                                  checkpointBlockBranchInstruction);
          //Then store the value in that alloca
          StoreInst * si = new StoreInst(v, ai, checkpointBlockBranchInstruction);
          //bitcast it to bytes
          BitCastInst * bc = new BitCastInst(ai, i8PType, ai->getName() + ".i8", checkpointBlockBranchInstruction);
          std::vector<Value*> args;
          uint64_t numBits = dataLayout.getTypeSizeInBits(v->getType());
          args.push_back(ConstantInt::get(i64Type, numBits, false));
          args.push_back(bc);
          CallInst::Create(aciilCheckpoint, args, "", checkpointBlockBranchInstruction);
        }

        //add a clean up call at the end
        {
          std::vector<Value*> args;
          CallInst::Create(aciilFinishCheckpoint, args, "", checkpointBlockBranchInstruction);
        }

        //set up the restart block
        //for every live variable
        for(CFGOperand op : node->getIn())
        {
          Value * v = op.getValue();

          //First alloca an array with just one element
          AllocaInst * ai = new AllocaInst(v->getType(),
                                  0, //address space
                                  llvm::ConstantInt::get(i32Type, 1, true), //array size
                                  4, //aligment
                                  v->getName() + ".addr",
                                  restartBlockBranchInstruction);
          //bitcast it to bytes
          BitCastInst * bc = new BitCastInst(ai, i8PType, ai->getName() + ".i8", restartBlockBranchInstruction);
          std::vector<Value*> args;
          uint64_t numBits = dataLayout.getTypeSizeInBits(v->getType());
          args.push_back(ConstantInt::get(i64Type, numBits, false));
          args.push_back(bc);
          //call the load function with the correct address
          CallInst::Create(aciilRestartReadFromCheckpoint, args, "", restartBlockBranchInstruction);
          LoadInst * li = new LoadInst(ai, v->getName() + ".restart", restartBlockBranchInstruction);

          //add a phi instruction for this variable in the correct block
          PHINode * resultOfCheckpointRestart = PHINode::Create(v->getType(), 0, v->getName() + ".cr", &node->getBlock().front());

          resultOfCheckpointRestart->addIncoming(li, restartBlock);
          resultOfCheckpointRestart->addIncoming(v, checkpointBlock);

          //fix the uses and dominators
          DominatorTree dt(cfgMain.getFunction());

          Instruction * OP = cast<Instruction>(v);
          std::vector<Use*> usesToReplace;
          for(Use &use : v->uses())
          {
            if(!dt.dominates(OP, use))
            {
              usesToReplace.push_back(&use);
            }
          }
          for(Use *use : usesToReplace)
          {
            use->set(resultOfCheckpointRestart);
          }
        }

      }

      cfgMain.getFunction().viewCFG();
      return true;
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
