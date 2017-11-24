#include "llvm/Transforms/ACIiL/CFGModule.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Transforms/ACIiL/CFGFunction.h"

#include <map>

using namespace llvm;

CFGModule::CFGModule(Module &m, Function &ef) : module(m), entryFunction(ef)
{
  setUpCFGs();
}

void CFGModule::setUpCFGs()
{
  for(Function &F : module)
  {
    functions.insert(std::pair<StringRef, CFGFunction>(F.getName(), CFGFunction(F)));
  }
}

void CFGModule::dump()
{
  errs() << "\nCFG for Module " << module.getName() << "\n";
  errs() << "There are " << functions.size() << " functions:\n";
  for(std::pair<StringRef, CFGFunction> pair : functions)
  {
    errs() << "* Function: " << pair.first;
    pair.second.dump();
  }
  errs() << "\n";
}

Module &CFGModule::getModule()
{
  return module;
}

std::map<StringRef, CFGFunction> &CFGModule::getFunctions()
{
  return functions;
}

CFGFunction &CFGModule::getEntryFunction()
{
  return functions.find(entryFunction.getName())->second;
}