
#ifndef LLVM_TRANSFORMS_ACIIL_CFGEDGE_H
#define LLVM_TRANSFORMS_ACIIL_CFGEDGE_H

#include "llvm/Transforms/ACIiL/CFGNode.h"
#include "llvm/IR/BasicBlock.h"

#include <vector>
#include <set>

namespace llvm
{
class CFGEdge
{
public:
  CFGEdge(BasicBlock &f, BasicBlock &t) : from(f), to(t) {};
  BasicBlock &getFrom();
  BasicBlock &getTo();
private:
  BasicBlock &from;
  BasicBlock &to;
};
}

#endif
