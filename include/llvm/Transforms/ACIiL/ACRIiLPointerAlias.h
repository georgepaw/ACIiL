
#ifndef LLVM_TRANSFORMS_ACRIIL_ACRIILPOINTERALIAS_H
#define LLVM_TRANSFORMS_ACRIIL_ACRIILPOINTERALIAS_H

#include "llvm/IR/Value.h"

#include <set>

namespace llvm {

class PointerAliasInfo {
public:
  PointerAliasInfo() = delete;
  PointerAliasInfo(Value *typeSizeInBits, Value *numElements, Value *alias);
  virtual ~PointerAliasInfo(){};
  virtual void dump();
  std::set<Value *> &getAliasSet();
  Value *getTypeSizeInBits();
  Value *getNumElements();

protected:
  PointerAliasInfo(Value *typeSizeInBits, Value *numElements,
                   std::set<Value *> aliasSet);

private:
  std::set<Value *> aliasSet;
  Value *typeSizeInBits;
  Value *numElements;
};

class AllocationPointerAliasInfo : public PointerAliasInfo {
public:
  AllocationPointerAliasInfo() = delete;
  AllocationPointerAliasInfo(Value *typeSizeInBits, Value *numElements,
                             Value *alias);
  void dump();
};

class PHINodePointerAliasInfo : public PointerAliasInfo {
public:
  PHINodePointerAliasInfo() = delete;
  PHINodePointerAliasInfo(Value *typeSizeInBits, Value *numElements,
                          std::set<Value *> aliasSet);
  bool addAlias(Value *alias);
  bool removeAlias(Value *alias);
  void dump();
};
} // namespace llvm

#endif