#include "llvm/Transforms/ACRIiL/CFGUtils.h"
#include "llvm/Transforms/ACRIiL/CFGUse.h"

#include <set>

namespace llvm {
bool CFGUtils::CFGAddToSet(std::set<CFGUse> &copyTo, CFGUse op) {
  return copyTo.insert(op).second;
}

bool CFGUtils::CFGCopyAllOperands(std::set<CFGUse> &copyTo,
                                  std::set<CFGUse> &copyFrom) {
  bool changed = false;
  for (CFGUse op : copyFrom)
    changed |= CFGAddToSet(copyTo, op);
  return changed;
}

void CFGUtils::CFGClearAndCopyAllOperands(std::set<CFGUse> &copyTo,
                                          std::set<CFGUse> &copyFrom) {
  copyTo.clear();
  CFGCopyAllOperands(copyTo, copyFrom);
}

} // namespace llvm