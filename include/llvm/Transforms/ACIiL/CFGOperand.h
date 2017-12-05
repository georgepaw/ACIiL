
#ifndef LLVM_TRANSFORMS_ACIIL_CFGOPERAND_H
#define LLVM_TRANSFORMS_ACIIL_CFGOPERAND_H

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Value.h"

namespace llvm {
class CFGOperand {
public:
  CFGOperand(Value *v);
  CFGOperand(Value *v, BasicBlock *b);
  Value *getValue();
  bool isFromPHI() const;
  BasicBlock *getSourcePHIBlock();
  bool operator<(const CFGOperand &other) const { return value < other.value; }
  void dump();

private:
  Value *value;
  bool fromPHI;
  BasicBlock *sourcePHIBlock;
  uint64_t numElements;
};
} // namespace llvm

#endif
