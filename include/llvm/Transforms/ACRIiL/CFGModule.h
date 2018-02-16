
#ifndef LLVM_TRANSFORMS_ACRIIL_CFGMODULE_H
#define LLVM_TRANSFORMS_ACRIIL_CFGMODULE_H

#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/ACRIiL/CFGFunction.h"

#include <map>

namespace llvm {
class CFGModule {
public:
  CFGModule(Module &m, Function &ef, ModulePass *mp);
  ~CFGModule();
  void dump();
  Module &getLLVMModule();
  std::map<Function *, CFGFunction *> &getFunctions();
  CFGFunction &getEntryFunction();

private:
  void setUpCFGs(ModulePass *mp);
  Module &module;
  Function &entryFunction;
  std::map<Function *, CFGFunction *> functions;
};

} // namespace llvm
#endif