#include "llvm/Transforms/ACRIiL/ACRIiLPointerAlias.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/raw_ostream.h"

#include <set>

using namespace llvm;

PointerAliasInfo::PointerAliasInfo(Value *typeSizeInBits, Value *numElements,
                                   Value *alias)
    : typeSizeInBits(typeSizeInBits), numElements(numElements) {
  aliasSet.insert(alias);
}

PointerAliasInfo::PointerAliasInfo(Value *typeSizeInBits, Value *numElements,
                                   std::set<Value *> aliasSet)
    : typeSizeInBits(typeSizeInBits), numElements(numElements) {
  for (Value *alias : aliasSet) {
    this->aliasSet.insert(alias);
  }
}

void PointerAliasInfo::dump() {
  errs() << "Pointer Alias, type size " << *getTypeSizeInBits()
         << " numElements " << *getNumElements()
         << " this pointer points to memory allocated by:\n";
  errs() << "* " << *getAliasSet().begin() << "\n";
}

std::set<Value *> &PointerAliasInfo::getAliasSet() { return aliasSet; }
Value *PointerAliasInfo::getTypeSizeInBits() { return typeSizeInBits; }
Value *PointerAliasInfo::getNumElements() { return numElements; }

AllocationPointerAliasInfo::AllocationPointerAliasInfo(Value *typeSizeInBits,
                                                       Value *numElements,
                                                       Value *alias)
    : PointerAliasInfo(typeSizeInBits, numElements, alias) {}

void AllocationPointerAliasInfo::dump() {
  errs() << "Allocation Pointer, type size " << *getTypeSizeInBits()
         << " numElements " << *getNumElements()
         << " this pointer points to memory allocated by:\n";
  errs() << "* " << *getAliasSet().begin() << "\n";
}
PHINodePointerAliasInfo::PHINodePointerAliasInfo(Value *typeSizeInBits,
                                                 Value *numElements,
                                                 std::set<Value *> aliasSet)
    : PointerAliasInfo(typeSizeInBits, numElements, aliasSet) {}

bool PHINodePointerAliasInfo::addAlias(Value *alias) {
  return getAliasSet().insert(alias).second;
}

bool PHINodePointerAliasInfo::removeAlias(Value *alias) {
  return getAliasSet().erase(alias) > 0;
}

void PHINodePointerAliasInfo::dump() {
  errs() << "PHINode Pointer, type size " << *getTypeSizeInBits()
         << " numElements " << *getNumElements()
         << " this pointer points to memory allocated by:\n";
  for (Value *alias : getAliasSet())
    errs() << "* " << *alias << "\n";
}
