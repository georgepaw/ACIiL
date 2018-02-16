
#ifndef LLVM_TRANSFORMS_ACIIL_CFGNODE_H
#define LLVM_TRANSFORMS_ACIIL_CFGNODE_H

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/Transforms/ACIiL/CFGUse.h"

#include <map>
#include <set>

namespace llvm {
class CFGFunction;
class CFGModule;
class CFGNode {
public:
  CFGNode(BasicBlock &b, bool isPhiNode, CFGFunction &f);
  BasicBlock &getLLVMBasicBlock();
  std::set<CFGUse> &getLiveValues();
  void addSuccessor(CFGNode *s);
  std::set<CFGNode *> &getSuccessors();
  bool isPhiNode();
  void addLiveMapping(Value *from, Value *to);
  Value *getLiveMapping(Value *from);
  void dump();
  CFGFunction &getParentFunction();
  Function &getParentLLVMFunction();
  CFGModule &getParentModule();
  Module &getParentLLVMModule();

private:
  std::set<CFGUse> &getIn();
  std::set<CFGUse> &getOut();
  std::set<Value *> &getDef();
  std::set<CFGUse> &getUse();
  bool phiNode;
  BasicBlock &block;
  std::set<CFGNode *> successors;
  std::set<CFGUse> use;
  std::set<Value *> def;
  std::set<CFGUse> in;
  std::set<CFGUse> out;
  std::set<CFGUse> live;
  std::map<Value *, Value *> liveValuesMap;
  CFGFunction &function;
  void addPointerUses(Value *pointer, BasicBlock *phiBlock);
  friend class CFGFunction;
};
} // namespace llvm

#endif
