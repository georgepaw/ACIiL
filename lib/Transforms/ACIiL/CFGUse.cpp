#include "llvm/Transforms/ACIiL/CFGUse.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

CFGUse::CFGUse(Value *v)
    : value(v), useType(CFGUseType::ANY), sourcePHIBlock(nullptr) {}

CFGUse::CFGUse(Value *v, BasicBlock *b)
    : value(v), useType(b == nullptr ? CFGUseType::ANY : CFGUseType::PHIOnly),
      sourcePHIBlock(b) {}

CFGUse::CFGUse(Value *v, CFGUseType uT, BasicBlock *b)
    : value(v), useType(uT), sourcePHIBlock(b) {}

CFGUseType CFGUse::getUseType() const { return useType; }
Value *CFGUse::getValue() const { return value; }
BasicBlock *CFGUse::getSourcePHIBlock() const { return sourcePHIBlock; }

void CFGUse::dump() {
  errs() << "Value: " << *value;
  errs() << " type: " << *value->getType();
  if (useType == CFGUseType::PHIOnly)
    errs() << " from " << sourcePHIBlock->getName();
  errs() << "\n";
}
