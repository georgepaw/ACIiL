
#ifndef LLVM_TRANSFORMS_ACRIIL_ACRIILUTILS_H
#define LLVM_TRANSFORMS_ACRIIL_ACRIILUTILS_H

#include "llvm/IR/Value.h"

namespace llvm {
class ACRIiLUtils {
public:
  ACRIiLUtils() = delete;
  static bool isCheckpointableType(Value *v);
};
} // namespace llvm
#endif