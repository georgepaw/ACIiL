
#ifndef LLVM_TRANSFORMS_ACIIL_CFGNODE_H
#define LLVM_TRANSFORMS_ACIIL_CFGNODE_H

#include "llvm/Transforms/ACIiL/CFGOperand.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Value.h"

#include <set>

namespace llvm
{
class CFGNode
{
public:
  CFGNode(BasicBlock &b, bool isPhiNode);
  BasicBlock &getBlock();
  std::set<CFGOperand> &getDef();
  std::set<CFGOperand> &getUse();
  std::set<CFGOperand> &getIn();
  std::set<CFGOperand> &getOut();
  void addSuccessor(CFGNode *s);
  std::set<CFGNode*> &getSuccessors();
  bool isPhiNode();
private:
  bool phiNode;
  BasicBlock &block;
  std::set<CFGNode*> successors;
  std::set<CFGOperand> use;
  std::set<CFGOperand> def;
  std::set<CFGOperand> in;
  std::set<CFGOperand> out;
};
}

#endif
