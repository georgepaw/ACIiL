
#ifndef LLVM_TRANSFORMS_ACIIL_ModuleCFG_H
#define LLVM_TRANSFORMS_ACIIL_ModuleCFG_H

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Transforms/ACIiL/FunctionCFG.h"

#include <map>

namespace llvm
{
class ModuleCFG
{
public:
  ModuleCFG(Module &m, Function &ef);
  void dump();
private:
  void setUpCFGs();
  Module &module;
  Function &entryFunction;
  std::map<StringRef, FunctionCFG> function_cfgs;
};

}//namespace
#endif