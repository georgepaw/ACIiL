
#ifndef LLVM_TRANSFORMS_ACIIL_CFGNODE_H
#define LLVM_TRANSFORMS_ACIIL_CFGNODE_H

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Value.h"
#include "llvm/Transforms/ACIiL/CFGOperand.h"

#include <map>
#include <set>

namespace llvm {
class CFGFunction;
class CFGNode {
public:
  CFGNode(BasicBlock &b, bool isPhiNode, CFGFunction &f);
  BasicBlock &getLLVMBasicBlock();
  std::set<CFGOperand> &getDef();
  std::set<CFGOperand> &getUse();
  std::set<CFGOperand> &getIn();
  std::set<CFGOperand> &getOut();
  void addSuccessor(CFGNode *s);
  std::set<CFGNode *> &getSuccessors();
  bool isPhiNode();
  void addLiveMapping(CFGOperand from, CFGOperand to);
  bool isLive(CFGOperand in);
  CFGOperand *getLiveMapping(CFGOperand from);
  void dump();
  CFGFunction &getParentFunction();

private:
  bool phiNode;
  BasicBlock &block;
  std::set<CFGNode *> successors;
  std::set<CFGOperand> use;
  std::set<CFGOperand> def;
  std::set<CFGOperand> in;
  std::set<CFGOperand> out;
  std::map<CFGOperand, CFGOperand> liveVariablesMap;
  CFGFunction &function;
};
} // namespace llvm

#endif
