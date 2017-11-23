#include "llvm/Transforms/ACIiL/CFGUtils.h"
#include "llvm/Transforms/ACIiL/CFGOperand.h"

#include <set>

namespace llvm
{
bool CFGAddToSet(std::set<CFGOperand> &copyTo, CFGOperand &op)
{
  return copyTo.insert(op).second;
}

bool CFGCopyAllOperands(std::set<CFGOperand> &copyTo, std::set<CFGOperand> &copyFrom)
{
  bool changed = false;
  for(CFGOperand op : copyFrom)
    changed |= CFGAddToSet(copyTo, op);
  return changed;

}

void CFGClearAndCopyAllOperands(std::set<CFGOperand> &copyTo, std::set<CFGOperand> &copyFrom)
{
  copyTo.clear();
  CFGCopyAllOperands(copyTo, copyFrom);
}

}