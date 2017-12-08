
#ifndef LLVM_TRANSFORMS_ACIIL_CFGNODE_H
#define LLVM_TRANSFORMS_ACIIL_CFGNODE_H

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/Transforms/ACIiL/CFGOperand.h"

#include <map>
#include <set>

namespace llvm {
class CFGFunction;
class CFGModule;
class CFGNode {
public:
  CFGNode(BasicBlock &b, bool isPhiNode, CFGFunction &f);
  BasicBlock &getLLVMBasicBlock();
  std::set<Value *> &getLiveValues();
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
  std::set<CFGOperand> &getIn();
  std::set<CFGOperand> &getOut();
  std::set<CFGOperand> &getDef();
  std::set<CFGOperand> &getUse();
  bool phiNode;
  BasicBlock &block;
  std::set<CFGNode *> successors;
  std::set<CFGOperand> use;
  std::set<CFGOperand> def;
  std::set<CFGOperand> in;
  std::set<CFGOperand> out;
  std::set<Value *> live;
  std::map<Value *, Value *> liveValuesMap;
  CFGFunction &function;

  friend class CFGFunction;
};
} // namespace llvm

#endif
