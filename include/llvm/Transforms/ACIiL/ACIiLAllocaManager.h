
#ifndef LLVM_TRANSFORMS_ACIIL_ACIILALLOCAMANAGER_H
#define LLVM_TRANSFORMS_ACIIL_ACIILALLOCAMANAGER_H

#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"

#include <map>
#include <set>
#include <vector>

namespace llvm {
class CFGFunction;
class ACIiLAllocaManager {
public:
  ACIiLAllocaManager(CFGFunction &f);
  AllocaInst *getAlloca(Type *type);
  void releaseAlloca(AllocaInst *ai);

private:
  CFGFunction &function;
  std::map<Type *, std::set<AllocaInst *>> unusedAllocas;
  std::map<Type *, std::set<AllocaInst *>> allocas;
};
} // namespace llvm
#endif