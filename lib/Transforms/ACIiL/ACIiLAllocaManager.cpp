#include "llvm/Transforms/ACIiL/ACIiLAllocaManager.h"
#include "llvm/Transforms/ACIiL/CFGFunction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/raw_ostream.h"

#include <set>
#include <map>
#include <sstream>

using namespace llvm;

ACIiLAllocaManager::ACIiLAllocaManager(CFGFunction &f) : function(f)
{
}

AllocaInst * ACIiLAllocaManager::getAlloca(Type * type)
{
  AllocaInst * ai;
  if(unusedAllocas[type].size() > 0)
  {
    ai = *unusedAllocas[type].begin();
    unusedAllocas[type].erase(ai);
  }
  else
  {
    std::stringstream ss;
    ss << "alloca_mngr." << type->getScalarType()->getTypeID() << ".";
    ai = new AllocaInst(type, 0, nullptr, ss.str(), &function.getFunction().getEntryBlock().front());
    allocas[type].insert(ai);
  }
  return ai;
}

void ACIiLAllocaManager::releaseAlloca(AllocaInst * ai)
{
  if(allocas[ai->getAllocatedType()].count(ai))
  {
    unusedAllocas[ai->getAllocatedType()].insert(ai);
  }
}
