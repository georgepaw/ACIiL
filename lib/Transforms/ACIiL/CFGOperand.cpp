#include "llvm/Transforms/ACIiL/CFGOperand.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

CFGOperand::CFGOperand(Value *v)
    : value(v), fromPHI(false), sourcePHIBlock(nullptr), numElements(1) {}

CFGOperand::CFGOperand(Value *v, BasicBlock *b)
    : value(v), fromPHI(true), sourcePHIBlock(b), numElements(1) {}

Value *CFGOperand::getValue() { return value; }
bool CFGOperand::isFromPHI() const { return fromPHI; }
BasicBlock *CFGOperand::getSourcePHIBlock() { return sourcePHIBlock; }

void CFGOperand::dump() {
  errs() << "Value: " << *value;
  errs() << " type: " << *value->getType();
  errs() << " number of elements: " << numElements;
  if (fromPHI)
    errs() << " from " << sourcePHIBlock->getName();
  errs() << "\n";
}