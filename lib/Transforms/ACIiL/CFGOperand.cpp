#include "llvm/Transforms/ACIiL/CFGOperand.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

CFGOperand::CFGOperand(Value * v)
{
  value = v;
  fromPHI = false;
  sourcePHIBlock = NULL;
}

CFGOperand::CFGOperand(Value * v, BasicBlock * b)
{
  value = v;
  fromPHI = true;
  sourcePHIBlock = b;
}

Value * CFGOperand::getValue()
{
  return value;
}
bool CFGOperand::isFromPHI()
{
  return fromPHI;
}
BasicBlock * CFGOperand::getSourcePHIBlock()
{
  return sourcePHIBlock;
}

void CFGOperand::dump()
{
  errs() << "Value: " << *value;
  errs() << " type: " << *value->getType();
  if(fromPHI) errs() << " from " << sourcePHIBlock->getName();
  errs() << "\n";
}