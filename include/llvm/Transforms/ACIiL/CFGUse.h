
#ifndef LLVM_TRANSFORMS_ACIIL_CFGUSE_H
#define LLVM_TRANSFORMS_ACIIL_CFGUSE_H

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Value.h"

namespace llvm {

enum class CFGUseType {
  PHIOnly, // only use is inside PHINodes
  ANY,     // uses outside of PHINodes as well
};

class CFGUse {
  Value *value;
  CFGUseType useType;
  BasicBlock *sourcePHIBlock;

public:
  CFGUse() = delete;
  CFGUse(Value *v);
  CFGUse(Value *v, BasicBlock *b);
  CFGUse(Value *v, CFGUseType uT, BasicBlock *b);

  bool operator<(const CFGUse &other) const {
    if (value != other.value)
      return value < other.value;
    return useType < other.useType;
  }

  CFGUseType getUseType() const;
  Value *getValue() const;
  BasicBlock *getSourcePHIBlock() const;
  void dump();
};
} // namespace llvm

#endif
