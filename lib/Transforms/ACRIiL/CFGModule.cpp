#include "llvm/Transforms/ACRIiL/CFGModule.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/ACRIiL/CFGFunction.h"

#include <map>

using namespace llvm;

CFGModule::CFGModule(Module &m, Function &ef, ModulePass *mp)
    : module(m), entryFunction(ef) {
  setUpCFGs(mp);
}

CFGModule::~CFGModule() {
  for (std::pair<Function *, CFGFunction *> pair : functions) {
    delete pair.second;
  }
}

void CFGModule::setUpCFGs(ModulePass *mp) {
  TargetLibraryInfo &TLI =
      mp->getAnalysis<TargetLibraryInfoWrapperPass>().getTLI();
  for (Function &F : module) {
    AAResults *AA =
        F.isDeclaration()
            ? nullptr
            : &mp->getAnalysis<AAResultsWrapperPass>(F).getAAResults();
    functions.insert(std::make_pair(&F, new CFGFunction(F, *this, TLI, AA)));
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
