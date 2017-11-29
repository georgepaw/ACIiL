#include "llvm/Transforms/ACIiL/CFGModule.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/ACIiL/CFGFunction.h"

#include <map>

using namespace llvm;

CFGModule::CFGModule(Module &m, Function &ef) : module(m), entryFunction(ef) {
  setUpCFGs();
}

CFGModule::~CFGModule() {
  for (std::pair<Function *, CFGFunction *> pair : functions) {
    delete pair.second;
  }
}

void CFGModule::setUpCFGs() {
  for (Function &F : module) {
    functions.insert(std::make_pair(&F, new CFGFunction(F, *this)));
  }
}

void CFGModule::dump() {
  errs() << "\nCFG for Module " << module.getName() << "\n";
  errs() << "There are " << functions.size() << " functions:\n";
  for (std::pair<Function *, CFGFunction *> pair : functions) {
    errs() << "* Function: " << pair.first->getName();
    pair.second->dump();
  }
  errs() << "\n";
}

Module &CFGModule::getLLVMModule() { return module; }

std::map<Function *, CFGFunction *> &CFGModule::getFunctions() {
  return functions;
}

CFGFunction &CFGModule::getEntryFunction() {
  return *functions.find(&entryFunction)->second;
}
