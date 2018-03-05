#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/MemoryBuiltins.h"
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
#include "llvm/Transforms/ACRIiL.h"
#include "llvm/Transforms/ACRIiL/ACRIiLAllocaManager.h"
#include "llvm/Transforms/ACRIiL/ACRIiLUtils.h"
#include "llvm/Transforms/ACRIiL/CFGModule.h"
#include "llvm/Transforms/IPO/Internalize.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include <iterator>
#include <set>

#define SHOW_CFG 0

using namespace llvm;

namespace {
struct ACRIiLPass : public ModulePass {
  static char ID;
  ACRIiLPass() : ModulePass(ID) {}

  struct CheckpointRestartBlockHelper {
    CFGNode &node;
    CFGNode &checkpointNode;
    CFGNode &restartNode;
    int64_t checkpointLabel;
    std::map<Value *, Value *> checkpointedToRestoreMap;
    std::map<Value *, uint64_t> checkpointedToIndexMap;
    uint64_t nextCheckpointLabel = 0;
    CheckpointRestartBlockHelper(CFGNode &node, CFGNode &checkpointNode,
                                 CFGNode &restartNode, int64_t checkpointLabel)
        : node(node), checkpointNode(checkpointNode), restartNode(restartNode),
          checkpointLabel(checkpointLabel) {}

    void addToCheckpointMap(Value *from, Value *to) {
      checkpointedToRestoreMap[from] = to;
      checkpointedToIndexMap[from] = nextCheckpointLabel++;
    }

    void addConstantToCheckpointMap(Value *constant) {
      checkpointedToRestoreMap[constant] = constant;
    }
  };

  Function *acriilCheckpointSetup;
  Function *acriilCheckpointStart;
  Function *acriilCheckpointPointer;
  Function *acriilCheckpointAlias;
  Function *acriilCheckpointFinish;
  Function *acriilRestartGetLabel;
  Function *acriilRestartReadPointerFromCheckpoint;
  Function *acriilRestartReadAliasFromCheckpoint;
  Function *acriilRestartFinish;

  // commonly used types
  IntegerType *i64Type;
  IntegerType *i8Type;
  Type *i8PType;

  virtual bool runOnModule(Module &M) override {
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
    std::unique_ptr<Module> ACRIiLStateModule =
        parseIRFile("ACRIiLState.bc", error, M.getContext());
    if (!ACRIiLStateModule) {
      errs() << "could not load the ACRIiLState bitcode, nothing to be done\n";
      return false;
    }

    // set up the types
    i64Type = IntegerType::getInt64Ty(M.getContext());
    i8Type = IntegerType::getInt8Ty(M.getContext());
    i8PType = PointerType::get(i8Type, /*address space*/ 0);

#if SHOW_CFG == 1
    mainFunction->viewCFG();
#endif

    // create a CFG module with live analysis and split phi nodes correctly
    CFGModule cfgModule(M, *mainFunction, this);

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
    ApplicableFlags = Linker::Flags::OverrideFromSrc;
    if (Linker::linkModules(M, std::move(ACRIiLStateModule), ApplicableFlags)) {
      errs() << "Failed linking in the ACRIiLState code.\n";
      return false;
    }

    // load the checkpointing functions
    acriilCheckpointSetup = M.getFunction("__acriilCheckpointSetup");
    acriilCheckpointStart = M.getFunction("__acriilCheckpointStart");
    acriilCheckpointPointer = M.getFunction("__acriilCheckpointPointer");
    acriilCheckpointAlias = M.getFunction("__acriilCheckpointAlias");
    acriilCheckpointFinish = M.getFunction("__acriilCheckpointFinish");
    if (!acriilCheckpointSetup || !acriilCheckpointStart ||
        !acriilCheckpointPointer || !acriilCheckpointFinish ||
        !acriilCheckpointAlias) {
      errs() << "could not load the checkpointing functions, checkpointing "
                "will not be added\n";
      return false;
    }
    // load the restart functions
    acriilRestartGetLabel = M.getFunction("__acriilRestartGetLabel");
    acriilRestartReadPointerFromCheckpoint =
        M.getFunction("__acriilRestartReadPointerFromCheckpoint");
    acriilRestartReadAliasFromCheckpoint =
        M.getFunction("__acriilRestartReadAliasFromCheckpoint");
    acriilRestartFinish = M.getFunction("__acriilRestartFinish");
    if (!acriilRestartGetLabel || !acriilRestartReadPointerFromCheckpoint ||
        !acriilRestartReadAliasFromCheckpoint || !acriilRestartFinish) {
      errs() << "could not load the restarting functions, checkpointing will "
                "not be added\n";
      return false;
    }
    bool changed = addCheckpointsToFunction(cfgModule.getEntryFunction());
    return changed;
  }

  bool addCheckpointsToFunction(CFGFunction &cfgFunction) {
    // if no checkpoints are to be performed then just return now
    if (cfgFunction.getNodesToCheckpoint().size() == 0)
      return true;

    std::vector<CheckpointRestartBlockHelper> checkpointAndRestartBlocks;
    uint64_t nextCheckpointLabel = 0;
    // insert the checkpoint and restart blocks
    // the blocks are empty at first, just with correct branching
    for (CFGNode *cfgNode : cfgFunction.getNodesToCheckpoint()) {
      checkpointAndRestartBlocks.push_back(
          createCheckpointAndRestartBlocksForNode(cfgNode,
                                                  nextCheckpointLabel++));
    }
    // insert the restart function and add the branch instructions for
    // a restart
    insertRestartBlock(cfgFunction, checkpointAndRestartBlocks);

    // fill out the checkpoint and restart blocks
    for (CheckpointRestartBlockHelper CRBH : checkpointAndRestartBlocks) {
      fillCheckpointAndRestartBlocksForNode(CRBH);
    }

    // now need to fix dominanance
    fixDominance(cfgFunction);
#if SHOW_CFG == 1
    cfgFunction.getLLVMFunction().viewCFG();
#endif
    return true;
  }

  CheckpointRestartBlockHelper
  createCheckpointAndRestartBlocksForNode(CFGNode *node,
                                          int64_t checkpointLabel) {
    BasicBlock &B = node->getLLVMBasicBlock();
    // add a checkpoint block
    BasicBlock *checkpointBlock = BasicBlock::Create(
        node->getParentLLVMModule().getContext(), B.getName() + ".checkpoint",
        &node->getParentLLVMFunction());
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
    BasicBlock *restartBlock = BasicBlock::Create(
        node->getParentLLVMModule().getContext(),
        B.getName() + ".read_checkpoint", &node->getParentLLVMFunction());
    // add a branch instruction from the end of the checkpoint block to the
    // original block
    BranchInst::Create(&B, restartBlock);
    CFGNode &checkpointNode =
        node->getParentFunction().addCheckpointNode(*checkpointBlock, *node);
    CFGNode &restartNode =
        node->getParentFunction().addRestartNode(*restartBlock, *node);

    return CheckpointRestartBlockHelper(*node, checkpointNode, restartNode,
                                        checkpointLabel);
  }

  void insertRestartBlock(
      CFGFunction &cfgFunction,
      std::vector<CheckpointRestartBlockHelper> checkpointAndRestartBlocks) {
    // the entry block is transformed in the following way
    // 1. all instructions from entry are copied to a new block
    // 2. all phi nodes are updated with that new block
    // 3. Function for checkpoint and restart and inserted into the entry block
    // 4. a switch statment is used to either perform a restart or run the
    // program form the beginning (the new entry block)
    BasicBlock &entry = cfgFunction.getLLVMFunction().getEntryBlock();
    BasicBlock *noCREntry =
        BasicBlock::Create(cfgFunction.getParentLLVMModule().getContext(),
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
    CallInst *ciGetLabel = builder.CreateCall(acriilRestartGetLabel, emptyArgs);

    // insert the checkpoint set up call
    builder.CreateCall(acriilCheckpointSetup, emptyArgs);

    // insert the switch with the default being carry on as if not checkpoint
    // happened
    SwitchInst *si = builder.CreateSwitch(ciGetLabel, noCREntry,
                                          checkpointAndRestartBlocks.size());
    for (CheckpointRestartBlockHelper CRBH : checkpointAndRestartBlocks) {
      si->addCase(
          ConstantInt::get(cfgFunction.getParentLLVMModule().getContext(),
                           APInt(64, CRBH.checkpointLabel, true)),
          &CRBH.restartNode.getLLVMBasicBlock());
    }

    // add the noCREntry block into the CFG
    cfgFunction.addNoCREntryNode(*noCREntry);
  }

  void
  fillCheckpointAndRestartBlocksForNode(CheckpointRestartBlockHelper &CRBH) {
    IRBuilder<> builderCheckpointBlock(
        &CRBH.checkpointNode.getLLVMBasicBlock());
    IRBuilder<> builderRestartBlock(&CRBH.restartNode.getLLVMBasicBlock());
    // insert instructions before the branch
    builderCheckpointBlock.SetInsertPoint(
        &CRBH.checkpointNode.getLLVMBasicBlock().front());
    builderRestartBlock.SetInsertPoint(
        &CRBH.restartNode.getLLVMBasicBlock().front());
    // for every live variable
    // errs() << "****Start checkpointing - "
    //        << CRBH.node.getLLVMBasicBlock().getName() << "\n";
    for (CFGUse live : CRBH.node.getLiveValues()) {
      checkpointRestoreLiveValue(live.getValue(), CRBH, builderCheckpointBlock,
                                 builderRestartBlock);
    }
    // errs() << "****End checkpointing\n";

    // add extra checkpoint calls
    // set up the checkpoint block
    // firstly call the start checkpoint function
    {
      std::vector<Value *> args;
      args.push_back(ConstantInt::get(i64Type, CRBH.checkpointLabel, true));
      args.push_back(ConstantInt::get(i64Type, CRBH.nextCheckpointLabel, true));
      builderCheckpointBlock.SetInsertPoint(
          &CRBH.checkpointNode.getLLVMBasicBlock().front());
      builderCheckpointBlock.CreateCall(acriilCheckpointStart, args);
    }
    // add a checkpoint clean up call at the end
    {
      std::vector<Value *> args;
      builderCheckpointBlock.SetInsertPoint(
          &CRBH.checkpointNode.getLLVMBasicBlock().back());
      builderCheckpointBlock.CreateCall(acriilCheckpointFinish, args);
      builderRestartBlock.SetInsertPoint(
          &CRBH.restartNode.getLLVMBasicBlock().back());
      builderRestartBlock.CreateCall(acriilRestartFinish, args);
    }
  }

  Value *checkpointRestoreLiveValue(Value *liveValue,
                                    CheckpointRestartBlockHelper &CRBH,
                                    IRBuilder<> &builderCheckpointBlock,
                                    IRBuilder<> &builderRestartBlock) {
    Value *restoreLiveValue = nullptr;
    // TODO at the moment assume that live values are instructions or constants
    if (ACRIiLUtils::isCheckpointableType(liveValue)) {
      if (liveValue->getType()->isPtrOrPtrVectorTy()) {
        restoreLiveValue = checkpointRestoreLiveValuePointer(
            liveValue, CRBH, builderCheckpointBlock, builderRestartBlock);

      } else {
        restoreLiveValue = checkpointRestoreLiveValueScalar(
            liveValue, CRBH, builderCheckpointBlock, builderRestartBlock);
      }
      // update the mapping correctly
      if (restoreLiveValue)
        CRBH.restartNode.addLiveMapping(liveValue, restoreLiveValue);
    } else if (isa<Constant>(liveValue)) {
      restoreLiveValue = checkpointRestoreConstant(liveValue, CRBH);
    } else {
      errs() << "Checkpointing for this value not implemented! Value "
             << *liveValue << "\n";
      exit(-1);
    }
    return restoreLiveValue;
  }

  Value *checkpointRestoreLiveValuePointer(Value *liveValue,
                                           CheckpointRestartBlockHelper &CRBH,
                                           IRBuilder<> &builderCheckpointBlock,
                                           IRBuilder<> &builderRestartBlock) {

    Value *restoreLiveValue = nullptr;

    std::map<Value *, PointerAliasInfo *>::iterator it =
        CRBH.node.getParentFunction().getPointerInformation().find(liveValue);
    if (it == CRBH.node.getParentFunction().getPointerInformation().end()) {
      errs() << "Could not find pointer info for " << *it->first
             << ". Aborting\n";
      exit(-1);
    }
    PointerAliasInfo *PAI = it->second;
    // make sure that pointer size is checkpointed before the pointer itself
    Value *restoreTypeSizeInBits =
        checkpointRestoreLiveValue(PAI->getTypeSizeInBits(), CRBH,
                                   builderCheckpointBlock, builderRestartBlock);
    Value *restoreNumElements =
        checkpointRestoreLiveValue(PAI->getNumElements(), CRBH,
                                   builderCheckpointBlock, builderRestartBlock);
    // checkpoint pointer until this base condition
    for (Value *alias : PAI->getAliasSet()) {
      if (liveValue != alias && ACRIiLUtils::isCheckpointableType(alias)) {
        checkpointRestoreLiveValue(alias, CRBH, builderCheckpointBlock,
                                   builderRestartBlock);
      }
    }

    if (CRBH.checkpointedToRestoreMap.find(liveValue) !=
        CRBH.checkpointedToRestoreMap.end()) {
      // errs() << "Already checkpointed\n";
      return CRBH.checkpointedToRestoreMap.find(liveValue)->second;
    }
    if (!isa<Instruction>(liveValue)) {
      errs() << "TODO NEED TO SUPPORT OTHER TYPES\n";
      exit(-1);
    }
    Instruction *i = cast<Instruction>(liveValue);
    TargetLibraryInfo &TLI =
        getAnalysis<TargetLibraryInfoWrapperPass>().getTLI();
    if (isAllocationFn(i, &TLI)) {
      CallInst *mallocLive = extractMallocCall(i, &TLI);
      // checkpoint
      addCheckpointPointerInstructionsToBlock(
          mallocLive, PAI->getTypeSizeInBits(), PAI->getNumElements(),
          builderCheckpointBlock);
      // restore
      // clone the malloc instruction into restore block
      CallInst *mallocRestore = cast<CallInst>(mallocLive->clone());
      // use the restored malloc size as an argument
      mallocRestore->setArgOperand(0, restoreNumElements);
      builderRestartBlock.Insert(mallocRestore,
                                 mallocLive->getName() + ".restart");
      addRestorePointerInstructionsToBlock(mallocRestore, restoreTypeSizeInBits,
                                           restoreNumElements,
                                           builderRestartBlock);
      restoreLiveValue = mallocRestore;
      CRBH.addToCheckpointMap(liveValue, mallocRestore);
    } else {
      switch (i->getOpcode()) {
      case Instruction::Alloca: {
        AllocaInst *aiLive = cast<AllocaInst>(i);
        // checkpoint
        addCheckpointPointerInstructionsToBlock(
            aiLive, PAI->getTypeSizeInBits(), PAI->getNumElements(),
            builderCheckpointBlock);
        // restore
        // clone the allocating instruction into restore block
        AllocaInst *aiRestore = cast<AllocaInst>(aiLive->clone());
        builderRestartBlock.Insert(aiRestore, aiLive->getName() + ".restart");
        addRestorePointerInstructionsToBlock(aiRestore, restoreTypeSizeInBits,
                                             restoreNumElements,
                                             builderRestartBlock);
        restoreLiveValue = aiRestore;
        CRBH.addToCheckpointMap(liveValue, aiRestore);
        break;
      }
      case Instruction::PHI: {
        PHINode *phiLive = cast<PHINode>(i);
        // checkpoint
        addCheckpointAliasInstructionsToBlock(phiLive, PAI, CRBH,
                                              builderCheckpointBlock);
        // restore
        Value *restoredBC = addRestoreAliasInstructionsToBlock(
            restoreTypeSizeInBits, restoreNumElements, builderRestartBlock);
        Value *restored = builderRestartBlock.CreateBitCast(
            restoredBC, phiLive->getType(), phiLive->getName() + ".restart");
        restoreLiveValue = restored;
        CRBH.addToCheckpointMap(liveValue, restored);
        break;
      }
      case Instruction::GetElementPtr: {
        GetElementPtrInst *gepLive = cast<GetElementPtrInst>(i);
        // checkpoint
        addCheckpointAliasInstructionsToBlock(gepLive, PAI, CRBH,
                                              builderCheckpointBlock);
        // restore
        Value *restoredBC = addRestoreAliasInstructionsToBlock(
            restoreTypeSizeInBits, restoreNumElements, builderRestartBlock);
        Value *restored = builderRestartBlock.CreateBitCast(
            restoredBC, gepLive->getType(), gepLive->getName() + ".restart");
        restoreLiveValue = restored;
        CRBH.addToCheckpointMap(liveValue, restored);
        break;
      }
      case Instruction::BitCast: {
        BitCastInst *bcLive = cast<BitCastInst>(i);
        // checkpoint
        addCheckpointAliasInstructionsToBlock(bcLive, PAI, CRBH,
                                              builderCheckpointBlock);
        // restore
        Value *restoredBC = addRestoreAliasInstructionsToBlock(
            restoreTypeSizeInBits, restoreNumElements, builderRestartBlock);
        Value *restored = builderRestartBlock.CreateBitCast(
            restoredBC, bcLive->getType(), bcLive->getName() + ".restart");
        restoreLiveValue = restored;
        CRBH.addToCheckpointMap(liveValue, restored);
        break;
      }
      default: {
        errs() << "****** POINTER BEING CHECKPOINTED AS A SCALAR!!!! ******\n";
        liveValue->dump();
        i->getType()->dump();
        PointerType *pty = cast<PointerType>(i->getType());
        pty->getElementType()->dump();
        exit(-1);
      }
      }
    }
    return restoreLiveValue;
  }

  Value *checkpointRestoreLiveValueScalar(Value *liveValue,
                                          CheckpointRestartBlockHelper &CRBH,
                                          IRBuilder<> &builderCheckpointBlock,
                                          IRBuilder<> &builderRestartBlock) {
    if (CRBH.checkpointedToRestoreMap.find(liveValue) !=
        CRBH.checkpointedToRestoreMap.end()) {
      // errs() << "Already checkpointed\n";
      return CRBH.checkpointedToRestoreMap.find(liveValue)->second;
    }

    // if the live value is not a pointer, then store it in some memory
    // First alloca an array with just one element
    AllocaInst *ai = CRBH.node.getParentFunction().getAllocManager().getAlloca(
        liveValue->getType());
    Value *typeSizeInBits = ConstantInt::get(
        i64Type,
        CRBH.node.getParentLLVMModule().getDataLayout().getTypeSizeInBits(
            liveValue->getType()),
        false);
    Value *numElements = ConstantInt::get(i64Type, 1, false);
    // checkpoint
    // store the value in that alloca
    builderCheckpointBlock.CreateStore(liveValue, ai);
    addCheckpointPointerInstructionsToBlock(ai, typeSizeInBits, numElements,
                                            builderCheckpointBlock);
    // restore
    addRestorePointerInstructionsToBlock(ai, typeSizeInBits, numElements,
                                         builderRestartBlock);
    LoadInst *li =
        builderRestartBlock.CreateLoad(ai, liveValue->getName() + ".restart");
    CRBH.addToCheckpointMap(liveValue, li);
    CRBH.node.getParentFunction().getAllocManager().releaseAlloca(ai);
    return li;
  }

  Value *checkpointRestoreConstant(Value *liveValue,
                                   CheckpointRestartBlockHelper &CRBH) {
    CRBH.addConstantToCheckpointMap(liveValue);
    return liveValue;
  }

  void addCheckpointPointerInstructionsToBlock(Value *valueToCheckpoint,
                                               Value *typeSizeInBits,
                                               Value *numElements,
                                               IRBuilder<> &builder) {
    // bitcast alloca to bytes
    Value *bc = builder.CreateBitCast(valueToCheckpoint, i8PType,
                                      valueToCheckpoint->getName() + ".i8");

    std::vector<Value *> checkpointArgs;
    checkpointArgs.push_back(typeSizeInBits);
    checkpointArgs.push_back(numElements);
    checkpointArgs.push_back(bc);
    builder.CreateCall(acriilCheckpointPointer, checkpointArgs);
  }

  void addCheckpointAliasInstructionsToBlock(Value *valueToCheckpoint,
                                             PointerAliasInfo *PAI,
                                             CheckpointRestartBlockHelper &CRBH,
                                             IRBuilder<> &builder) {
    // bitcast alloca to bytes
    Value *bc = builder.CreateBitCast(valueToCheckpoint, i8PType,
                                      valueToCheckpoint->getName() + ".i8");

    std::vector<Value *> checkpointArgs;
    checkpointArgs.push_back(
        ConstantInt::get(i64Type, PAI->getAliasSet().size(), false));
    checkpointArgs.push_back(PAI->getTypeSizeInBits());
    checkpointArgs.push_back(PAI->getNumElements());
    checkpointArgs.push_back(bc);
    for (Value *alias : PAI->getAliasSet()) {
      PointerAliasInfo *aliasPAI =
          CRBH.node.getParentFunction().getPointerInformation()[alias];
      Value *aliasBC =
          builder.CreateBitCast(alias, i8PType, alias->getName() + ".i8");
      checkpointArgs.push_back(aliasPAI->getTypeSizeInBits());
      checkpointArgs.push_back(aliasPAI->getNumElements());
      checkpointArgs.push_back(
          ConstantInt::get(i64Type, CRBH.checkpointedToIndexMap[alias], false));
      checkpointArgs.push_back(aliasBC);
    }
    builder.CreateCall(acriilCheckpointAlias, checkpointArgs);
  }

  void addRestorePointerInstructionsToBlock(Value *valueToRestore,
                                            Value *typeSizeInBits,
                                            Value *numElements,
                                            IRBuilder<> &builder) {
    // bitcast it to bytes
    Value *bc = builder.CreateBitCast(valueToRestore, i8PType,
                                      valueToRestore->getName() + ".i8");
    std::vector<Value *> restartArgs;
    restartArgs.push_back(typeSizeInBits);
    restartArgs.push_back(numElements);
    restartArgs.push_back(bc);

    builder.CreateCall(acriilRestartReadPointerFromCheckpoint, restartArgs);
  }

  Value *addRestoreAliasInstructionsToBlock(Value *typeSizeInBits,
                                            Value *numElements,
                                            IRBuilder<> &builder) {
    std::vector<Value *> restartArgs;
    restartArgs.push_back(typeSizeInBits);
    restartArgs.push_back(numElements);

    return builder.CreateCall(acriilRestartReadAliasFromCheckpoint,
                              restartArgs);
  }

  void fixDominance(CFGFunction &cfgFunction) {
    // struct used for dominance fixing
    struct PHINodeMappingToUpdate {
      PHINode &phi;    // phi to fix
      Value *value;    // mapping from
      unsigned phiIdx; // index of the phi value
      PHINodeMappingToUpdate(PHINode &p, Value *v, unsigned pI)
          : phi(p), value(v), phiIdx(pI) {}
    };

    // Step 1.
    // If    a phi node with the live variable as a value already exists then
    // only schedule it for updating  Else  in each block, for each live
    // variable create a phi with a value from each of the predecessors
    // If a node with phi with the live variable as a value already exists then
    // only schedule it for updating.
    // Else in each block, for each live variable
    //      create a phi with a value from each of the predecessors
    std::vector<PHINodeMappingToUpdate> phisToUpdate;
    for (CFGNode *node : cfgFunction.getNodes()) {
      for (CFGUse live : node->getLiveValues()) {
        Value *value = live.getValue();

        if (live.getUseType() == CFGUseType::PHIOnly) {
          for (PHINode &phi : node->getLLVMBasicBlock().phis()) {
            for (unsigned phiIdx = 0; phiIdx < phi.getNumIncomingValues();
                 phiIdx++) {
              if (phi.getIncomingValue(phiIdx) == value) {
                phisToUpdate.push_back(
                    PHINodeMappingToUpdate(phi, value, phiIdx));
              }
            }
          }
        } else {
          // otherwise if phi does not exist, need to create a new one
          // phi node for this live variable
          PHINode *phi = PHINode::Create(
              value->getType(),
              std::distance(pred_begin(&node->getLLVMBasicBlock()),
                            pred_end(&node->getLLVMBasicBlock())),
              value->getName() + "." + node->getLLVMBasicBlock().getName(),
              &node->getLLVMBasicBlock().front());
          // update the uses
          for (Instruction &inst : node->getLLVMBasicBlock()) {
            for (unsigned i = 0; i < inst.getNumOperands(); i++) {
              if (inst.getOperand(i) == value)
                inst.setOperand(i, phi);
            }
          }

          // add the values that need to be updated with the mappings
          unsigned phiIdx = 0;
          for (BasicBlock *pred : predecessors(&node->getLLVMBasicBlock())) {
            phi->addIncoming(Constant::getNullValue(value->getType()), pred);
            phisToUpdate.push_back(
                PHINodeMappingToUpdate(*phi, value, phiIdx++));
          }
          node->addLiveMapping(value, phi);
        }
      }
    }

    // Step 2.
    // Now that all phi nodes have been created with correct mappings, update
    // the mappings in the phi nodes
    for (PHINodeMappingToUpdate ptu : phisToUpdate) {
      // todo assume instruction for now only
      if (!ACRIiLUtils::isCheckpointableType(ptu.value))
        continue;
      CFGNode *pred = cfgFunction.findNodeByBasicBlock(
          *ptu.phi.getIncomingBlock(ptu.phiIdx));
      ptu.phi.setIncomingValue(ptu.phiIdx, pred->getLiveMapping(ptu.value));
    }

    // Step 3.
    // Clean up phi nodes which were created that have only one input
    bool removed = false;
    do {
      removed = false;
      for (CFGNode *node : cfgFunction.getNodes()) {
        std::vector<PHINode *> phis;
        for (PHINode &phi : node->getLLVMBasicBlock().phis()) {
          if (phi.hasConstantValue())
            phis.push_back(&phi);
        }
        for (PHINode *phi : phis) {
          phi->replaceAllUsesWith(phi->hasConstantValue());
          phi->eraseFromParent();
          removed = true;
        }
      }
    } while (removed);
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<TargetLibraryInfoWrapperPass>();
    AU.addRequired<AAResultsWrapperPass>();
    AU.addRequired<LoopInfoWrapperPass>();
  }
};
} // namespace

INITIALIZE_PASS_BEGIN(ACRIiLPass, "ACRIiL",
                      "Automatic Checkpoint/Restart Insertion Pass", false,
                      false)
INITIALIZE_PASS_DEPENDENCY(TargetLibraryInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(AAResultsWrapperPass)
INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass)
INITIALIZE_PASS_END(ACRIiLPass, "ACRIiL",
                    "Automatic Checkpoint/Restart Insertion Pass", false, false)

char ACRIiLPass::ID = 0;

// static void registerACRIiLPass(const PassManagerBuilder &,
//                          legacy::PassManagerBase &PM) {
//   PM.add(new ACRIiLPass());
// }

ModulePass *llvm::createACRIiLLinkingPass() {
  errs() << "Adding ACRIiLPass!\n";
  return new ACRIiLPass();
}

// static RegisterStandardPasses
//     register_pass_O(PassManagerBuilder::EP_OptimizerLast,
//     registerACRIiLPass);

// static RegisterStandardPasses
//     register_pass_O0(PassManagerBuilder::EP_EnabledOnOptLevel0,
//     registerACRIiLPass);
