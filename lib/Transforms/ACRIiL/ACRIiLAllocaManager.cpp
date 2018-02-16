#include "llvm/Transforms/ACRIiL/ACRIiLAllocaManager.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/ACRIiL/CFGFunction.h"

#include <map>
#include <set>
#include <sstream>

using namespace llvm;

ACRIiLAllocaManager::ACRIiLAllocaManager(CFGFunction &f) : function(f) {}

AllocaInst *ACRIiLAllocaManager::getAlloca(Type *type) {
  AllocaInst *ai;
  if (unusedAllocas[type].size() > 0) {
    ai = *unusedAllocas[type].begin();
    unusedAllocas[type].erase(ai);
  } else {
    std::stringstream ss;
    ss << "alloca_mngr." << type->getScalarType()->getTypeID() << ".";
    ai = new AllocaInst(type, 0, nullptr, ss.str(),
                        &function.getLLVMFunction().getEntryBlock().front());
    allocas[type].insert(ai);
  }
  return ai;
}

void ACRIiLAllocaManager::releaseAlloca(AllocaInst *ai) {
  if (allocas[ai->getAllocatedType()].count(ai)) {
    unusedAllocas[ai->getAllocatedType()].insert(ai);
  }
}
