
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
  CFGNode(BasicBlock &b);
  BasicBlock &getBlock();
  std::set<Value*> &getDef();
  std::set<CFGOperand> &getUse();
private:
  BasicBlock &block;
  std::set<CFGOperand> use;
  std::set<Value*> def;
};
}

#endif
