
#ifndef LLVM_TRANSFORMS_ACIIL_CFGMODULE_H
#define LLVM_TRANSFORMS_ACIIL_CFGMODULE_H

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/ACIiL/CFGFunction.h"

#include <map>

namespace llvm {
class CFGModule {
public:
  CFGModule(Module &m, Function &ef);
  ~CFGModule();
  void dump();
  Module &getLLVMModule();
  std::map<Function *, CFGFunction *> &getFunctions();
  CFGFunction &getEntryFunction();

private:
  void setUpCFGs();
  Module &module;
  Function &entryFunction;
  std::map<Function *, CFGFunction *> functions;
};

} // namespace llvm
#endif