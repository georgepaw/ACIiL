#include "llvm/Transforms/ACIiL/ModuleCFG.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Transforms/ACIiL/FunctionCFG.h"

#include <map>

using namespace llvm;

ModuleCFG::ModuleCFG(Module &m, Function &ef) : module(m), entryFunction(ef)
{
  setUpCFGs();
}

void ModuleCFG::setUpCFGs()
{
  for(Function &F : module)
  {
    function_cfgs.insert(std::pair<StringRef, FunctionCFG>(F.getName(), FunctionCFG(F)));
  }
}

void ModuleCFG::dump()
{
  errs() << "\nCFG for Module " << module.getName() << "\n";
  errs() << "There are " << function_cfgs.size() << " functions:\n";
  for(std::pair<StringRef, FunctionCFG> pair : function_cfgs)
  {
    errs() << "* Function: " << pair.first;
    pair.second.dump();
  }
  errs() << "\n";
}

Module &ModuleCFG::getModule()
{
  return module;
}