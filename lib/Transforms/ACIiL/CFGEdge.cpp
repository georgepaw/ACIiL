#include "llvm/Transforms/ACIiL/CFGEdge.h"
#include "llvm/IR/BasicBlock.h"

using namespace llvm;

BasicBlock &CFGEdge::getFrom()
{
  return from;
}

BasicBlock &CFGEdge::getTo()
{
  return to;
}
