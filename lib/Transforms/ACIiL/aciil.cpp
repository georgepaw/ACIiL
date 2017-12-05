#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Pass.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/ACIiL.h"
#include "llvm/Transforms/ACIiL/ACIiLAllocaManager.h"
#include "llvm/Transforms/ACIiL/CFGModule.h"
#include "llvm/Transforms/IPO/Internalize.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include <iterator>
#include <set>

using namespace llvm;

namespace {
struct ACIiLPass : public ModulePass {
  static char ID;
  ACIiLPass() : ModulePass(ID) {}

  struct CheckpointRestartBlocksInfo {
    CFGNode &node;
    BasicBlock &checkpointBlock;
    BasicBlock &restartBlock;
    int64_t checkpointLabel;
    CheckpointRestartBlocksInfo(CFGNode &n, BasicBlock &cBB, BasicBlock &rBB,
                                int64_t lN)
        : node(n), checkpointBlock(cBB), restartBlock(rBB),
          checkpointLabel(lN) {}
  };

  Function *aciilCheckpointSetup;
  Function *aciilCheckpointStart;
  Function *aciilCheckpointPointer;
  Function *aciilCheckpointFinish;
  Function *aciilRestartGetLabel;
  Function *aciilRestartReadFromCheckpoint;

  // commonly used types
  IntegerType *i64Type;
  IntegerType *i8Type;
  Type *i8PType;

  virtual bool runOnModule(Module &M) {
    errs() << "In module called: " << M.getName() << "!\n";
    Function *mainFunction = M.getFunction("main");
    if (!mainFunction)
      return false;

    // load the bitcodes for checkpointing and restarting exists
    SMDiagnostic error;
    std::unique_ptr<Module> checkpointModule =
        parseIRFile("checkpoint.bc", error, M.getContext());
    if (!checkpointModule) {
      errs() << "could not load the checkpoint bitcode, nothing to be done\n";
      return false;
    }
    std::unique_ptr<Module> restartModule =
        parseIRFile("restart.bc", error, M.getContext());
    if (!restartModule) {
      errs() << "could not load the restart bitcode, nothing to be done\n";
      return false;
    }

    // create a CFG module with live analysis and split phi nodes correctly
    CFGModule cfgModule(M, *mainFunction);

    // link in the bitcode with functions for checkpointing
    unsigned ApplicableFlags = Linker::Flags::OverrideFromSrc;
    if (Linker::linkModules(M, std::move(checkpointModule), ApplicableFlags)) {
      errs() << "Failed linking in the checkpoint code.\n";
      return false;
    }
    ApplicableFlags = Linker::Flags::OverrideFromSrc;
    if (Linker::linkModules(M, std::move(restartModule), ApplicableFlags)) {
      errs() << "Failed linking in the restart code.\n";
      return false;
    }

    // load the checkpointing functions
    aciilCheckpointSetup = M.getFunction("__aciil_checkpoint_setup");
    aciilCheckpointStart = M.getFunction("__aciil_checkpoint_start");
    aciilCheckpointPointer = M.getFunction("__aciil_checkpoint_pointer");
    aciilCheckpointFinish = M.getFunction("__aciil_checkpoint_finish");
    if (!aciilCheckpointSetup || !aciilCheckpointStart ||
        !aciilCheckpointPointer || !aciilCheckpointFinish) {
      errs() << "could not load the checkpointing functions, checkpointing "
                "will not be added\n";
      return false;
    }
    // load the restart functions
    aciilRestartGetLabel = M.getFunction("__aciil_restart_get_label");
    aciilRestartReadFromCheckpoint =
        M.getFunction("__aciil_restart_read_from_checkpoint");
    if (!aciilRestartGetLabel || !aciilRestartReadFromCheckpoint) {
      errs() << "could not load the restarting functions, checkpointing will "
                "not be added\n";
      return false;
    }

    // set up before inserting checkpoints
    i64Type = IntegerType::getInt64Ty(M.getContext());
    i8Type = IntegerType::getInt8Ty(M.getContext());
    i8PType = PointerType::get(i8Type, /*address space*/ 0);

    // get the main cfg
    CFGFunction &cfgMain = cfgModule.getEntryFunction();
    bool changed = addCheckpointsToFunction(cfgMain);
    return changed;
  }

  bool addCheckpointsToFunction(CFGFunction &cfgFunction) {

    cfgFunction.getLLVMFunction().viewCFG();
    std::vector<CheckpointRestartBlocksInfo> checkpointAndRestartBlocks;
    // insert the checkpoint and restart blocks
    // the blocks are empty at first, just with correct branching
    int64_t nextCheckpointLabel = 0;
    for (CFGNode *cfgNode : cfgFunction.getNodes()) {
      // don't checkpoint phiNodes
      if (cfgNode->isPhiNode())
        continue;
      // don't do this for the entry block or the exit blocks
      BasicBlock &B = cfgNode->getLLVMBasicBlock();
      unsigned numSuccessors = std::distance(succ_begin(&B), succ_end(&B));
      unsigned numPredecessors = std::distance(pred_begin(&B), pred_end(&B));
      if (numSuccessors == 0 || numPredecessors == 0)
        continue;
      // skip a block if it has no live variables
      if (cfgNode->getIn().size() == 0)
        continue;

      checkpointAndRestartBlocks.push_back(
          createCheckpointAndRestartBlocksForNode(cfgNode,
                                                  nextCheckpointLabel++));
    }

    // if no checkpoints were inserted then just return now
    if (checkpointAndRestartBlocks.size() == 0)
      return true;

    // otherwise insert the restart function and add the branch instructions for
    // a restart
    insertRestartBlock(cfgFunction, checkpointAndRestartBlocks);

    // insert the checkpoint and restart nodes into the CFG
    for (CheckpointRestartBlocksInfo crbi : checkpointAndRestartBlocks) {
      cfgFunction.addCheckpointNode(crbi.checkpointBlock, crbi.node);
      cfgFunction.addRestartNode(crbi.restartBlock, crbi.node);
    }

    // fill out the checkpoint and restart blocks
    for (CheckpointRestartBlocksInfo crbi : checkpointAndRestartBlocks) {
      fillCheckpointAndRestartBlocksForNode(crbi);
    }

    // now need to fix dominanance
    fixDominance(cfgFunction);
    return true;
  }

  CheckpointRestartBlocksInfo
  createCheckpointAndRestartBlocksForNode(CFGNode *node,
                                          int64_t checkpointLabel) {
    BasicBlock &B = node->getLLVMBasicBlock();
    // add a checkpoint block
    BasicBlock *checkpointBlock =
        BasicBlock::Create(node->getParentFunction()
                               .getParentModule()
                               .getLLVMModule()
                               .getContext(),
                           B.getName() + ".checkpoint",
                           &node->getParentFunction().getLLVMFunction());
    // make sure all predecessors of B now point at the checkpoint
    for (BasicBlock *p : predecessors(&B)) {
      for (unsigned i = 0; i < p->getTerminator()->getNumSuccessors(); i++) {
        if (p->getTerminator()->getSuccessor(i) == &B) {
          p->getTerminator()->setSuccessor(i, checkpointBlock);
        }
      }
    }
    // add a branch instruction from the end of the checkpoint block to the
    // original block
    BranchInst::Create(&B, checkpointBlock);

    // add a restart block
    BasicBlock *restartBlock =
        BasicBlock::Create(node->getParentFunction()
                               .getParentModule()
                               .getLLVMModule()
                               .getContext(),
                           B.getName() + ".read_checkpoint",
                           &node->getParentFunction().getLLVMFunction());
    // add a branch instruction from the end of the checkpoint block to the
    // original block
    BranchInst::Create(&B, restartBlock);

    return CheckpointRestartBlocksInfo(*node, *checkpointBlock, *restartBlock,
                                       checkpointLabel);
  }

  void insertRestartBlock(
      CFGFunction &cfgFunction,
      std::vector<CheckpointRestartBlocksInfo> checkpointAndRestartBlocks) {
    // the entry block is transformed in the following way
    // 1. all instructions from entry are copied to a new block
    // 2. all phi nodes are updated with that new block
    // 3. Function for checkpoint and restart and inserted into the entry block
    // 4. a switch statment is used to either perform a restart or run the
    // program form the beginning (the new entry block)
    BasicBlock &entry = cfgFunction.getLLVMFunction().getEntryBlock();
    BasicBlock *noCREntry = BasicBlock::Create(
        cfgFunction.getParentModule().getLLVMModule().getContext(),
        "no_cr_entry", &cfgFunction.getLLVMFunction());

    // loop over all successor blocks and replace the basic block in phi nodes
    for (BasicBlock *Successor : successors(&entry)) {
      for (PHINode &PN : Successor->phis()) {
        int Idx = PN.getBasicBlockIndex(&entry);
        while (Idx != -1) {
          PN.setIncomingBlock((unsigned)Idx, noCREntry);
          Idx = PN.getBasicBlockIndex(&entry);
        }
      }
    }
    // move the instructions over to the "default entry block"
    noCREntry->getInstList().splice(noCREntry->end(), entry.getInstList(),
                                    entry.begin(), entry.end());

    // insert the calls
    std::vector<Value *> emptyArgs;
    IRBuilder<> builder(&entry);
    // insert the call that gets the label for the restart
    CallInst *ciGetLabel = builder.CreateCall(aciilRestartGetLabel, emptyArgs);

    // insert the checkpoint set up call
    builder.CreateCall(aciilCheckpointSetup, emptyArgs);

    // insert the switch with the default being carry on as if not checkpoint
    // happened
    SwitchInst *si = builder.CreateSwitch(ciGetLabel, noCREntry,
                                          checkpointAndRestartBlocks.size());
    for (CheckpointRestartBlocksInfo crbi : checkpointAndRestartBlocks) {
      si->addCase(
          ConstantInt::get(
              cfgFunction.getParentModule().getLLVMModule().getContext(),
              APInt(64, crbi.checkpointLabel, true)),
          &crbi.restartBlock);
    }

    // add the noCREntry block into the CFG
    cfgFunction.addNoCREntryNode(*noCREntry);
  }

  void
  fillCheckpointAndRestartBlocksForNode(CheckpointRestartBlocksInfo &crbi) {
    IRBuilder<> builderCheckpointBlock(&crbi.checkpointBlock);
    IRBuilder<> builderRestartBlock(&crbi.restartBlock);
    // insert instructions before the branch
    builderCheckpointBlock.SetInsertPoint(&crbi.checkpointBlock.front());
    builderRestartBlock.SetInsertPoint(&crbi.restartBlock.front());

    std::set<CFGOperand> liveValuesAlreadyCheckpointed;
    // note that some live values don't need to be checkpointed, only reassigned
    // so can't use the size of liveValuesAlreadyCheckpointed
    uint64_t numberOfValuesCheckpointed = 0;
    // for every live variable
    errs() << "Start checkpointing\n";
    for (CFGOperand op : crbi.node.getIn()) {
      checkpointRestoreLiveValue(
          op, crbi, builderCheckpointBlock, builderRestartBlock,
          liveValuesAlreadyCheckpointed, numberOfValuesCheckpointed);
    }
    errs() << "End checkpointing\n";

    // add extra checkpoint calls
    // set up the checkpoint block
    // firstly call the start checkpoint function
    {
      std::vector<Value *> args;
      args.push_back(ConstantInt::get(i64Type, crbi.checkpointLabel, true));
      args.push_back(
          ConstantInt::get(i64Type, numberOfValuesCheckpointed, true));
      builderCheckpointBlock.SetInsertPoint(&crbi.checkpointBlock.front());
      builderCheckpointBlock.CreateCall(aciilCheckpointStart, args);
    }
    // add a checkpoint clean up call at the end
    {
      std::vector<Value *> args;
      builderCheckpointBlock.SetInsertPoint(&crbi.checkpointBlock.back());
      builderCheckpointBlock.CreateCall(aciilCheckpointFinish, args);
    }
  }

  void checkpointRestoreLiveValue(
      CFGOperand op, CheckpointRestartBlocksInfo &crbi,
      IRBuilder<> &builderCheckpointBlock, IRBuilder<> &builderRestartBlock,
      std::set<CFGOperand> &liveValuesAlreadyCheckpointed,
      uint64_t &numberOfValuesCheckpointed) {
    if (!isa<Instruction>(op.getValue())) {
      errs() << "TODO can a live value not be an instruction?\n";
      return;
    }
    op.dump();

    if (liveValuesAlreadyCheckpointed.find(op) !=
        liveValuesAlreadyCheckpointed.end()) {
      errs() << "Live value already checkpointed\n";
      return;
    }

    Value *valueOutToUpdateInMapping = nullptr;

    Instruction *i = cast<Instruction>(op.getValue());

    Type *ty = i->getType();

    if (ty->isPtrOrPtrVectorTy()) {
      switch (i->getOpcode()) {
      case Instruction::GetElementPtr: {
        // if it's a getElementPtr instruction then skip
        GetElementPtrInst *gepLive = cast<GetElementPtrInst>(i);
        Value *pointer = gepLive->getPointerOperand();
        // for GEP instruction need to checkpoint/restore the actual data
        checkpointRestoreLiveValue(CFGOperand(pointer), crbi,
                                   builderCheckpointBlock, builderRestartBlock,
                                   liveValuesAlreadyCheckpointed,
                                   numberOfValuesCheckpointed);
        // checkpoint
        // don't need to do anything as we are not checkpointing an address

        // restore
        // clone the GEP instruction into restore block
        GetElementPtrInst *gepRestore =
            cast<GetElementPtrInst>(gepLive->clone());
        builderRestartBlock.Insert(gepRestore);
        // replace the pointer to the one created in the restart block
        CFGOperand *restorePointer =
            crbi.node.getParentFunction()
                .findNodeByBasicBlock(crbi.restartBlock)
                ->getLiveMapping(CFGOperand(pointer));
        unsigned opIdx = gepRestore->getPointerOperandIndex();
        gepRestore->setOperand(opIdx, restorePointer->getValue());
        valueOutToUpdateInMapping = gepRestore;

        liveValuesAlreadyCheckpointed.insert(op);
        break;
      }
      case Instruction::Alloca: {
        AllocaInst *aiLive = cast<AllocaInst>(i);
        // TODO for now assume pointer is alloca
        uint64_t numElements =
            cast<ArrayType>(aiLive->getAllocatedType())->getNumElements();
        uint64_t elementSizeBits =
            crbi.node.getParentFunction()
                .getParentModule()
                .getLLVMModule()
                .getDataLayout()
                .getTypeSizeInBits(cast<ArrayType>(aiLive->getAllocatedType())
                                       ->getElementType());
        // checkpoint
        addCheckpointInstructionsToBlock(aiLive, numElements, elementSizeBits,
                                         builderCheckpointBlock,
                                         numberOfValuesCheckpointed);
        // restore
        // clone the allocating instruction into restore block
        AllocaInst *aiRestore = cast<AllocaInst>(aiLive->clone());
        builderRestartBlock.Insert(aiRestore);
        addRestoreInstructionsToBlock(aiRestore, numElements, elementSizeBits,
                                      builderRestartBlock);
        valueOutToUpdateInMapping = aiRestore;

        liveValuesAlreadyCheckpointed.insert(op);
        break;
      }
      default: {
        errs() << "****** POINTER BEING CHECKPOINTED AS A SCALAR!!!! ******\n";
        ty->dump();
        PointerType *pty = cast<PointerType>(ty);
        pty->getElementType()->dump();
        valueOutToUpdateInMapping = checkpointRestoreLiveValueScalar(
            op, crbi, builderCheckpointBlock, builderRestartBlock,
            liveValuesAlreadyCheckpointed, numberOfValuesCheckpointed);
      }
      }
    } else {
      valueOutToUpdateInMapping = checkpointRestoreLiveValueScalar(
          op, crbi, builderCheckpointBlock, builderRestartBlock,
          liveValuesAlreadyCheckpointed, numberOfValuesCheckpointed);
    }

    // update the mapping correctly
    if (valueOutToUpdateInMapping)
      crbi.node.getParentFunction()
          .findNodeByBasicBlock(crbi.restartBlock)
          ->addLiveMapping(CFGOperand(op.getValue()),
                           CFGOperand(valueOutToUpdateInMapping));
  }

  Value *checkpointRestoreLiveValueScalar(
      CFGOperand op, CheckpointRestartBlocksInfo &crbi,
      IRBuilder<> &builderCheckpointBlock, IRBuilder<> &builderRestartBlock,
      std::set<CFGOperand> &liveValuesAlreadyCheckpointed,
      uint64_t &numberOfValuesCheckpointed) {
    // if the live value is not a pointer, then store it in some memory

    // TODO for now assume it is a scalar type

    // First alloca an array with just one element
    AllocaInst *ai = crbi.node.getParentFunction().getAllocManager().getAlloca(
        op.getValue()->getType());
    uint64_t numElements = 1;
    uint64_t elementSizeBits = crbi.node.getParentFunction()
                                   .getParentModule()
                                   .getLLVMModule()
                                   .getDataLayout()
                                   .getTypeSizeInBits(op.getValue()->getType());
    // checkpoint
    // store the value in that alloca
    builderCheckpointBlock.CreateStore(op.getValue(), ai);
    addCheckpointInstructionsToBlock(ai, numElements, elementSizeBits,
                                     builderCheckpointBlock,
                                     numberOfValuesCheckpointed);
    // restore
    addRestoreInstructionsToBlock(ai, numElements, elementSizeBits,
                                  builderRestartBlock);
    LoadInst *li = builderRestartBlock.CreateLoad(ai, op.getValue()->getName() +
                                                          ".restart");
    crbi.node.getParentFunction().getAllocManager().releaseAlloca(ai);
    liveValuesAlreadyCheckpointed.insert(op);
    return li;
  }

  void addCheckpointInstructionsToBlock(Value *valueToCheckpoint,
                                        uint64_t numElements,
                                        uint64_t elementSizeBits,
                                        IRBuilder<> &builder,
                                        uint64_t &numberOfValuesCheckpointed) {
    errs() << "Num elements " << numElements << " element size "
           << elementSizeBits << "\n";
    // bitcast alloca to bytes
    Value *bc = builder.CreateBitCast(valueToCheckpoint, i8PType,
                                      valueToCheckpoint->getName() + ".i8");

    std::vector<Value *> checkpointArgs;
    checkpointArgs.push_back(ConstantInt::get(i64Type, elementSizeBits, false));
    checkpointArgs.push_back(ConstantInt::get(i64Type, numElements, false));
    checkpointArgs.push_back(bc);
    builder.CreateCall(aciilCheckpointPointer, checkpointArgs);

    numberOfValuesCheckpointed++;
  }

  void addRestoreInstructionsToBlock(Value *valueToRestore,
                                     uint64_t numElements,
                                     uint64_t elementSizeBits,
                                     IRBuilder<> &builder) {
    // bitcast it to bytes
    Value *bc = builder.CreateBitCast(valueToRestore, i8PType,
                                      valueToRestore->getName() + ".i8");
    std::vector<Value *> restartArgs;
    restartArgs.push_back(ConstantInt::get(i64Type, elementSizeBits, false));
    restartArgs.push_back(ConstantInt::get(i64Type, numElements, false));
    restartArgs.push_back(bc);
    // call the load function with the correct address
    builder.CreateCall(aciilRestartReadFromCheckpoint, restartArgs);
  }

  void fixDominance(CFGFunction &cfgFunction) {
    // struct used for dominance fixing
    struct PHINodeMappingToUpdate {
      PHINode &phi;    // phi to fix
      CFGOperand op;   // mappinf from
      unsigned phiIdx; // index of the phi value
      PHINodeMappingToUpdate(PHINode &p, CFGOperand o, unsigned pI)
          : phi(p), op(o.getValue()), phiIdx(pI) {}
    };

    // Step 1.
    // If    a phi node with the live variable as a value already exists then
    // only schedule it for updating  Else  in each block, for each live
    // variable create a phi with a value from each of the predecessors
    //      and update the mapping
    //      and replace the uses of that variable with phi
    std::vector<PHINodeMappingToUpdate> phisToUpdate;
    for (CFGNode *node : cfgFunction.getNodes()) {
      for (CFGOperand op : node->getIn()) {
        bool phiExists = false;
        // first need to check if a phi already exists, in that case only the
        // values need to be updated
        for (PHINode &phi : node->getLLVMBasicBlock().phis()) {
          for (unsigned phiIdx = 0; phiIdx < phi.getNumIncomingValues();
               phiIdx++) {
            if (phi.getIncomingValue(phiIdx) == op.getValue()) {
              phisToUpdate.push_back(PHINodeMappingToUpdate(phi, op, phiIdx));
              phiExists = true;
            }
          }
        }
        if (phiExists)
          continue;

        // otherwise if phi does not exist, need to create a new one
        // phi node for this live variable
        PHINode *phi = PHINode::Create(
            op.getValue()->getType(),
            std::distance(pred_begin(&node->getLLVMBasicBlock()),
                          pred_end(&node->getLLVMBasicBlock())),
            op.getValue()->getName() + "." +
                node->getLLVMBasicBlock().getName(),
            &node->getLLVMBasicBlock().front());
        // update the uses
        for (Instruction &inst : node->getLLVMBasicBlock()) {
          for (unsigned i = 0; i < inst.getNumOperands(); i++) {
            if (inst.getOperand(i) == op.getValue())
              inst.setOperand(i, phi);
          }
        }

        // add the values that need to be updated with the mappings
        unsigned phiIdx = 0;
        for (BasicBlock *pred : predecessors(&node->getLLVMBasicBlock())) {
          phi->addIncoming(Constant::getNullValue(op.getValue()->getType()),
                           pred);
          phisToUpdate.push_back(PHINodeMappingToUpdate(*phi, op, phiIdx++));
        }
        node->addLiveMapping(op, CFGOperand(phi));
      }
    }
    // Step 2.
    // Now that all phi nodes have been created with correct mappings, update
    // the mappings in the phi nodes
    for (PHINodeMappingToUpdate ptu : phisToUpdate) {
      CFGNode *pred = cfgFunction.findNodeByBasicBlock(
          *ptu.phi.getIncomingBlock(ptu.phiIdx));
      ptu.phi.setIncomingValue(ptu.phiIdx,
                               pred->getLiveMapping(ptu.op)->getValue());
    }

    cfgFunction.getLLVMFunction().viewCFG();
  }
};
} // namespace

char ACIiLPass::ID = 0;

// static void registerACIiLPass(const PassManagerBuilder &,
//                          legacy::PassManagerBase &PM) {
//   PM.add(new ACIiLPass());
// }

ModulePass *llvm::createACIiLLinkingPass() {
  errs() << "Adding ACIiLPass!\n";
  return new ACIiLPass();
}

// static RegisterStandardPasses
//     register_pass_O(PassManagerBuilder::EP_OptimizerLast, registerACIiLPass);

// static RegisterStandardPasses
//     register_pass_O0(PassManagerBuilder::EP_EnabledOnOptLevel0,
//     registerACIiLPass);
