
#ifndef LLVM_TRANSFORMS_ACRIIL_H
#define LLVM_TRANSFORMS_ACRIIL_H

#include "llvm/IR/PassManager.h"

namespace llvm {
ModulePass *createACRIiLLinkingPass();
}

#endif
