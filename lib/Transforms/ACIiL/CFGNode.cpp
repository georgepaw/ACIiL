#include "llvm/Transforms/ACIiL/CFGNode.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/ACIiL/CFGFunction.h"
#include "llvm/Transforms/ACIiL/CFGModule.h"
#include "llvm/Transforms/ACIiL/CFGUse.h"

#include <set>

using namespace llvm;

void CFGNode::addPointerUses(Value *pointer, BasicBlock *phiBlock) {
  std::map<Value *, PointerAliasInfo *>::iterator it =
      function.getPointerInformation().find(pointer);
  if (it == function.getPointerInformation().end())
    return;

  if (isa<Instruction>(it->second->getTypeSizeInBits()))
    use.insert(CFGUse(it->second->getTypeSizeInBits(), phiBlock));
  if (isa<Instruction>(it->second->getNumElements()))
    use.insert(CFGUse(it->second->getNumElements(), phiBlock));
  for (Value *alias : it->second->getAliasSet()) {
    if (isa<Instruction>(alias))
      use.insert(CFGUse(alias, phiBlock));
    // recurse through the pointers of pointers until the allocation
    if (pointer != alias)
      addPointerUses(alias, phiBlock);
  }
}

CFGNode::CFGNode(BasicBlock &b, bool isPhiNode, CFGFunction &f)
    : phiNode(isPhiNode), block(b), function(f) {
  for (BasicBlock::iterator end = --block.end(), start = --block.begin();
       end != start; end--) {
    Instruction &inst = *end;
    // get define
    // if it has a non void return then it defines
    if (!end->getType()->isVoidTy()) {

      def.insert(&inst);
      // since this is SSA, remove the definition from uses set
      // note this will not remove PHI uses
      std::set<CFGUse>::iterator it = use.find(CFGUse(&inst));
      if (it != use.end())
        use.erase(CFGUse(&inst));
    }

    // get uses
    if (PHINode *phi = dyn_cast<PHINode>(&inst)) {
      // if it's a phi instruction
      for (Use &u : phi->operands()) {
        // at the momemnt assume if the operand is a result of instruction then
        // it should be in use set
        if (isa<Instruction>(u)) {
          use.insert(CFGUse(u, phi->getIncomingBlock(u)));
          addPointerUses(u, phi->getIncomingBlock(u));
        }
      }
    } else {
      // otherwise just iterate over operands
      for (Value *v : end->operands()) {
        // at the momemnt assume if the operand is a result of instruction then
        // it should be in use set
        if (isa<Instruction>(v)) {
          use.insert(CFGUse(v));
          addPointerUses(v, nullptr);
        }
      }
    }
  }
}

void CFGNode::addSuccessor(CFGNode *s) { successors.insert(s); }

std::set<CFGUse> &CFGNode::getLiveValues() { return live; };

std::set<CFGNode *> &CFGNode::getSuccessors() { return successors; }

std::set<Value *> &CFGNode::getDef() { return def; }

std::set<CFGUse> &CFGNode::getUse() { return use; }

std::set<CFGUse> &CFGNode::getIn() { return in; }

std::set<CFGUse> &CFGNode::getOut() { return out; }

BasicBlock &CFGNode::getLLVMBasicBlock() { return block; }

bool CFGNode::isPhiNode() { return phiNode; }

void CFGNode::addLiveMapping(Value *from, Value *to) {
  std::map<Value *, Value *>::iterator it = liveValuesMap.find(from);
  if (it == liveValuesMap.end())
    liveValuesMap.insert(std::make_pair(from, to));
  else
    it->second = to;
}

Value *CFGNode::getLiveMapping(Value *from) {
  return liveValuesMap.find(from)->second;
}

CFGFunction &CFGNode::getParentFunction() { return function; }

Function &CFGNode::getParentLLVMFunction() {
  return function.getLLVMFunction();
}

CFGModule &CFGNode::getParentModule() { return function.getParentModule(); }

Module &CFGNode::getParentLLVMModule() {
  return function.getParentLLVMModule();
}

void CFGNode::dump() {
  errs() << "* " << getLLVMBasicBlock().getName() << "\n";
  errs() << getLLVMBasicBlock() << "\n";
  errs() << "def: \n";
  for (Value *v : getDef()) {
    v->dump();
  }
  errs() << "use: \n";
  for (CFGUse v : getUse()) {
    v.dump();
  }
  errs() << "\n";
  errs() << "in: \n";
  for (CFGUse v : getIn()) {
    v.dump();
  }
  errs() << "\n";
  errs() << "out: \n";
  for (CFGUse v : getOut()) {
    v.dump();
  }
  errs() << "\n";

  errs() << "This node has " << getSuccessors().size() << " edges to nodes:\n";
  for (CFGNode *s : getSuccessors()) {
    errs() << "\t* " << s->getLLVMBasicBlock().getName() << "\n";
  }
  errs() << "\n";
}
