#include "llvm/Transforms/ACRIiL/CFGFunction.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/MemoryBuiltins.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/ACRIiL/ACRIiLAllocaManager.h"
#include "llvm/Transforms/ACRIiL/ACRIiLPointerAlias.h"
#include "llvm/Transforms/ACRIiL/CFGModule.h"
#include "llvm/Transforms/ACRIiL/CFGNode.h"
#include "llvm/Transforms/ACRIiL/CFGUse.h"
#include "llvm/Transforms/ACRIiL/CFGUtils.h"

#include <map>
#include <set>
#include <vector>

using namespace llvm;

CFGFunction::CFGFunction(Function &f, CFGModule &m, TargetLibraryInfo &TLI,
                         AliasAnalysis *AA)
    : function(f), am(*this), module(m) {
  pointerAnalysis(TLI, AA);
  setUpCFG();
  doLiveAnalysis();
  setUpLiveSetsAndMappings();
}

CFGFunction::~CFGFunction() {
  for (CFGNode *node : nodes) {
    delete node;
  }

  for (std::map<Value *, PointerAliasInfo *>::iterator it =
           pointerInformation.begin();
       it != pointerInformation.end(); it++) {
    delete it->second;
  }
}

void CFGFunction::addNode(BasicBlock &b, bool isPhiNode) {
  nodes.push_back(new CFGNode(b, isPhiNode, *this));
}

// TODO this needs to be improved, should the data structure be e vector? Not
// really
CFGNode *CFGFunction::findNodeByBasicBlock(BasicBlock &b) {
  for (CFGNode *node : nodes) {
    if (&node->getLLVMBasicBlock() == &b)
      return node;
  }
  return NULL;
}

void CFGFunction::pointerAnalysis(TargetLibraryInfo &TLI, AliasAnalysis *AA) {

  std::set<Value *> unknownSizeAliasPointers;
  std::set<PHINode *> unknownSizePHINodeAliasPointers;
  std::set<PHINode *> phis;

  // first identify all allocations and PHINodes and set up their sizes and
  // aliases
  for (BasicBlock &B : function) {
    for (Instruction &I : B) {
      if (!I.getType()->isPtrOrPtrVectorTy())
        continue;

      if (AllocaInst *ai = dyn_cast<AllocaInst>(&I)) {
        pointerInformation[&I] = new AllocationPointerAliasInfo(
            ConstantInt::get(
                Type::getInt64Ty(getParentLLVMModule().getContext()),
                getParentLLVMModule().getDataLayout().getTypeSizeInBits(
                    ai->getAllocatedType()),
                false),
            ConstantInt::get(
                Type::getInt64Ty(getParentLLVMModule().getContext()),
                cast<ConstantInt>(ai->getArraySize())
                    ->getValue()
                    .getZExtValue(),
                false),
            ai);
      } else if (isAllocationFn(&I, &TLI)) {
        // TODO this needs to be fixed once dealing with dynamic memory
        // this function only works on malloc like functions it seems?
        CallInst *malloc = extractMallocCall(&I, &TLI);
        pointerInformation[&I] = new AllocationPointerAliasInfo(
            ConstantInt::get(
                Type::getInt64Ty(getParentLLVMModule().getContext()),
                getParentLLVMModule().getDataLayout().getTypeSizeInBits(
                    getMallocAllocatedType(malloc, &TLI)),
                false),
            getMallocArraySize(malloc, getParentLLVMModule().getDataLayout(),
                               &TLI),
            malloc);
      } else if (PHINode *phi = dyn_cast<PHINode>(&I)) {
        // TODO assuming that the sizeInBits and numElements are 64bit integers
        PHINode *phiTypeSizeInBits = PHINode::Create(
            Type::getInt64Ty(getParentLLVMModule().getContext()),
            phi->getNumIncomingValues(), phi->getName() + ".typeSizeInBits",
            &I);
        PHINode *phiNumElements = PHINode::Create(
            Type::getInt64Ty(getParentLLVMModule().getContext()),
            phi->getNumIncomingValues(), phi->getName() + ".numElements", &I);
        // add provisional aliasing for this PHINode
        // at first only alias to incoming values
        std::set<Value *> aliasSet;
        for (unsigned phiIdx = 0; phiIdx < phi->getNumIncomingValues();
             phiIdx++) {
          aliasSet.insert(phi->getIncomingValue(phiIdx));
        }
        pointerInformation[phi] = new PHINodePointerAliasInfo(
            phiTypeSizeInBits, phiNumElements, aliasSet);
        phis.insert(phi);
        unknownSizePHINodeAliasPointers.insert(phi);
      } else {
        unknownSizeAliasPointers.insert(&I);
      }
    }
  }

  // Set up aliasing of non PHINodes
  while (unknownSizeAliasPointers.size()) {
    errs() << "unknownSizeAliasPointers " << unknownSizeAliasPointers.size()
           << "\n";
    std::set<Value *> pointersToRemove;
    for (Value *v : unknownSizeAliasPointers) {
      if (!isa<Instruction>(v)) {
        errs() << "Live value does not come from an instruction, not supported "
                  "at the moment\n";
        exit(1);
      }
      Instruction *I = cast<Instruction>(v);
      switch (I->getOpcode()) {
      case Instruction::GetElementPtr: {
        GetElementPtrInst *gep = cast<GetElementPtrInst>(I);
        Value *pointer = gep->getPointerOperand();
        std::map<Value *, PointerAliasInfo *>::iterator it =
            pointerInformation.find(pointer);
        if (it != pointerInformation.end()) {
          pointerInformation[I] =
              new PointerAliasInfo(it->second->getTypeSizeInBits(),
                                   it->second->getNumElements(), pointer);
          pointersToRemove.insert(I);
        }

        break;
      }
      case Instruction::BitCast: {
        BitCastInst *bc = cast<BitCastInst>(I);
        Value *pointer = bc->getOperand(0);
        std::map<Value *, PointerAliasInfo *>::iterator it =
            pointerInformation.find(pointer);
        if (it != pointerInformation.end()) {
          pointerInformation[I] =
              new PointerAliasInfo(it->second->getTypeSizeInBits(),
                                   it->second->getNumElements(), pointer);
          pointersToRemove.insert(I);
        }
        break;
      }
      default: {
        errs() << "****** ACRIiL ********\n";
        errs() << "Pointer " << *I << " is not supported!\n";
        exit(-1);
      }
      }
    }
    for (Value *v : pointersToRemove) {
      unknownSizeAliasPointers.erase(v);
    }
  }

  // Find pointer type size and num elements for PHINodes
  while (unknownSizePHINodeAliasPointers.size()) {
    errs() << "unknownSizePHINodeAliasPointers "
           << unknownSizePHINodeAliasPointers.size() << "\n";
    std::set<PHINode *> pointersToRemove;
    for (PHINode *phi : unknownSizePHINodeAliasPointers) {
      PHINodePointerAliasInfo *phiPAI =
          (PHINodePointerAliasInfo *)pointerInformation[phi];
      PHINode *phiTypeSizeInBits = cast<PHINode>(phiPAI->getTypeSizeInBits());
      PHINode *phiNumElements = cast<PHINode>(phiPAI->getNumElements());
      bool allPhisAreKnown = true;
      for (unsigned phiIdx = 0; phiIdx < phi->getNumIncomingValues();
           phiIdx++) {
        Value *v = phi->getIncomingValue(phiIdx);
        BasicBlock *b = phi->getIncomingBlock(phiIdx);
        if (phiTypeSizeInBits->getBasicBlockIndex(b) == -1 ||
            phiNumElements->getBasicBlockIndex(b) == -1) {
          std::map<Value *, PointerAliasInfo *>::iterator it =
              pointerInformation.find(v);
          if (it != pointerInformation.end()) {
            phiTypeSizeInBits->addIncoming(it->second->getTypeSizeInBits(), b);
            phiNumElements->addIncoming(it->second->getNumElements(), b);
          } else {
            allPhisAreKnown = false;
          }
        }
      }

      if (allPhisAreKnown) {
        pointersToRemove.insert(phi);
      }
    }
    for (PHINode *phi : pointersToRemove) {
      unknownSizePHINodeAliasPointers.erase(phi);
    }
  }

  // Set up alias set for PHINodes
  bool changed;
  do {
    changed = false;
    for (PHINode *phi : phis) {
      std::vector<Value *> valuesToAdd;
      std::vector<Value *> valuesToRemove;
      PHINodePointerAliasInfo *phiPAI =
          (PHINodePointerAliasInfo *)pointerInformation[phi];
      for (Value *alias : phiPAI->getAliasSet()) {
        if (PHINode *aliasedPhi = dyn_cast<PHINode>(alias)) {
          for (Value *v : pointerInformation[aliasedPhi]->getAliasSet())
            if (AA->alias(MemoryLocation(phi), MemoryLocation(v)) !=
                AliasResult::NoAlias)
              valuesToAdd.push_back(v);
          valuesToRemove.push_back(alias);
        }
      }
      for (Value *v : valuesToAdd) {
        bool added = phiPAI->addAlias(v);
        if (added) {
          // errs() << "Added " << *v << " to alias " << *phi << "\n";
        }
        changed |= added;
      }
      for (Value *v : valuesToRemove) {
        bool removed = phiPAI->removeAlias(v);
        if (removed) {
          // errs() << "Removed " << *v << " to alias " << *phi << "\n";
        }
        changed |= removed;
      }
    }
  } while (changed);

  // for (PHINode *phi : phis) {
  //   pointerInformation[phi]->dump();
  // }
}

void CFGFunction::setUpCFG() {
  // first need to prep basic blocks
  // if there are any phi instructions in the basicblock then
  // split the block
  // if(function.getName() == "main") function.viewCFG();
  std::vector<Instruction *> splitLocations;
  for (BasicBlock &B : function) {
    // According to spec
    //"There must be no non-phi instructions between the start of a basic block
    // and the PHI instructions: i.e. PHI instructions must be first in a basic
    // block."
    // So to split the block find the last phi instruction and split the block.

    // to find the last phi instruction
    // start at the first instruction, it it's not phi then this block does not
    // need to be splitted
    if (!isa<PHINode>(B.front()))
      continue;
    bool foundSplit = false;
    for (BasicBlock::iterator start = B.begin()++, end = B.end();
         start != end && !foundSplit; start++) {
      if (!isa<PHINode>(start)) {
        splitLocations.push_back(&*start);
        foundSplit = true;
      }
    }
  }

  std::set<BasicBlock *> phiNodes;

  // Once all the locations to split on have been found then perform the splits
  for (Instruction *I : splitLocations) {
    BasicBlock *B = I->getParent();
    B->splitBasicBlock(I, B->getName() + ".no_phis");
    phiNodes.insert(B);
  }

  // Now that blocks are ready, set up the CFG graph for the function
  // get all the nodes
  for (BasicBlock &B : function) {
    bool isPhiNode = phiNodes.find(&B) != phiNodes.end();
    addNode(B, isPhiNode);
  }

  // get all the edges
  for (BasicBlock &B : function) {
    CFGNode *thisNode = findNodeByBasicBlock(B);
    for (BasicBlock *to : successors(&B)) {
      CFGNode *toNode = findNodeByBasicBlock(*to);
      thisNode->addSuccessor(toNode);
    }
  }
}

void CFGFunction::doLiveAnalysis() {
  bool converged;
  do {
    converged = true;
    bool changed = false;
    for (CFGNode *cfgNode : nodes) {
      // in[n] = (use[n]) union (out[n]-def[n])
      // first insert the use[n]
      changed =
          CFGUtils::CFGCopyAllOperands(cfgNode->getIn(), cfgNode->getUse());
      // then insert the (out[n]-def[n])
      std::vector<CFGUse> outDefDiff;
      for (CFGUse use_out : cfgNode->getOut()) {
        // if element from out is not in def
        if (cfgNode->getDef().find(use_out.getValue()) ==
            cfgNode->getDef().end()) {
          // add it and update the changed flag accordingly
          changed |= CFGUtils::CFGAddToSet(cfgNode->getIn(), use_out);
        }
      }

      // out[n] = union of in[s] for all successors s of n
      for (CFGNode *s : cfgNode->getSuccessors()) {
        for (CFGUse use : s->getIn()) {
          // if this operand is used by a phi instruction and is form this block
          // or this is not a PHI use, then add it to out
          if ((use.getUseType() == CFGUseType::PHIOnly &&
               &cfgNode->getLLVMBasicBlock() == use.getSourcePHIBlock()) ||
              use.getUseType() != CFGUseType::PHIOnly) {
            // remove the fact that the variable is from the phi node
            // when it is propagating
            CFGUse use_clear = CFGUse(use.getValue());
            changed |= CFGUtils::CFGAddToSet(cfgNode->getOut(), use_clear);
          }
        }
      }
      // check if they have changed
      converged &= !changed;
    }
  } while (!converged);
}

void CFGFunction::setUpLiveSetsAndMappings() {
  // set up the live sets for nodes and mappings
  for (CFGNode *cfgNode : nodes) {
    for (CFGUse use : cfgNode->getIn()) {
      Value *v = use.getValue();
      cfgNode->getLiveValues().insert(use);
      cfgNode->addLiveMapping(v, v);
    }
    for (CFGUse use : cfgNode->getDef()) {
      Value *v = use.getValue();
      cfgNode->addLiveMapping(v, v);
    }
  }
}

void CFGFunction::dump() {
  errs() << "\nCFG for function " << function.getName() << "\n";
  errs() << "There are " << nodes.size() << " nodes:\n";
  for (CFGNode *node : nodes) {
    // if(node.getLLVMBasicBlock().getName() != "entry") continue;
    node->dump();
  }
}

Function &CFGFunction::getLLVMFunction() { return function; }

std::vector<CFGNode *> &CFGFunction::getNodes() { return nodes; }

CFGNode &CFGFunction::addCheckpointNode(BasicBlock &b, CFGNode &nodeForCR) {
  // add the node
  addNode(b, false);
  // copy the live set and set up mapping
  for (CFGUse use : nodeForCR.getLiveValues()) {
    nodes.back()->getLiveValues().insert(use);
    nodes.back()->addLiveMapping(use.getValue(), use.getValue());
  }
  return *nodes.back();
}

CFGNode &CFGFunction::addRestartNode(BasicBlock &b, CFGNode &nodeForCR) {
  // add the node
  addNode(b, false);
  // nothing is live before the restart block so don't copy the in set
  // set up the mapping
  for (CFGUse use : nodeForCR.getLiveValues())
    nodes.back()->addLiveMapping(use.getValue(), use.getValue());
  return *nodes.back();
}

CFGNode &CFGFunction::addNoCREntryNode(BasicBlock &b) {
  // add the node
  addNode(b, false);
  // set up the mapping for all variables that are *defined*
  for (Value *op : nodes.back()->getDef())
    nodes.back()->addLiveMapping(op, op);
  return *nodes.back();
}

ACRIiLAllocaManager &CFGFunction::getAllocManager() { return am; }

CFGModule &CFGFunction::getParentModule() { return module; }

Module &CFGFunction::getParentLLVMModule() { return module.getLLVMModule(); }

std::map<Value *, PointerAliasInfo *> &CFGFunction::getPointerInformation() {
  return pointerInformation;
}
