
#ifndef LLVM_TRANSFORMS_ACIIL_H
#define LLVM_TRANSFORMS_ACIIL_H

#include "llvm/IR/PassManager.h"

namespace llvm {
  ModulePass *createACIiLLinkingPass();
}

#endif
