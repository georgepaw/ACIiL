
#ifndef LLVM_TRANSFORMS_ACIIL_CFGUTILS_H
#define LLVM_TRANSFORMS_ACIIL_CFGUTILS_H

#include "llvm/Transforms/ACIiL/CFGOperand.h"

#include <map>
#include <set>

namespace llvm {
class CFGUtils {
public:
  // inserts the value into the set, returns true if the value was not in the
  // set before
  static bool CFGAddToSet(std::set<CFGOperand> &copyTo, CFGOperand op);

  // returns true if copyTo set has changed
  static bool CFGCopyAllOperands(std::set<CFGOperand> &copyTo,
                                 std::set<CFGOperand> &copyFrom);

  // does what CFGCopyAllOperands, but clears copyTo first
  static void CFGClearAndCopyAllOperands(std::set<CFGOperand> &copyTo,
                                         std::set<CFGOperand> &copyFrom);
};
} // namespace llvm
#endif