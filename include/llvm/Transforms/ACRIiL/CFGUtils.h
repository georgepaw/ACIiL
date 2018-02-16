
#ifndef LLVM_TRANSFORMS_ACRIIL_CFGUTILS_H
#define LLVM_TRANSFORMS_ACRIIL_CFGUTILS_H

#include "llvm/Transforms/ACRIiL/CFGUse.h"

#include <map>
#include <set>

namespace llvm {
class CFGUtils {
public:
  // inserts the value into the set, returns true if the value was not in the
  // set before
  static bool CFGAddToSet(std::set<CFGUse> &copyTo, CFGUse op);

  // returns true if copyTo set has changed
  static bool CFGCopyAllOperands(std::set<CFGUse> &copyTo,
                                 std::set<CFGUse> &copyFrom);

  // does what CFGCopyAllOperands, but clears copyTo first
  static void CFGClearAndCopyAllOperands(std::set<CFGUse> &copyTo,
                                         std::set<CFGUse> &copyFrom);
};
} // namespace llvm
#endif