#include "llvm/Transforms/ACRIiL/ACRIiLUtils.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

bool ACRIiLUtils::isCheckpointableType(Value *v) {
  if (!v)
    errs() << "v is null\n";
  return isa<Instruction>(v) || isa<Argument>(v);
}