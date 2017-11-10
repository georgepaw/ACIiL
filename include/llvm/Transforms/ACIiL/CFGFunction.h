
#ifndef LLVM_TRANSFORMS_ACIIL_CFGFUNCTION_H
#define LLVM_TRANSFORMS_ACIIL_CFGFUNCTION_H

#include "llvm/Transforms/ACIiL/CFGNode.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Value.h"

#include <vector>
#include <set>

namespace llvm
{
class CFGFunction
{
public:
  CFGFunction(Function &f);
  void dump();
  Function &getFunction();
  std::vector<CFGNode> &getNodes();
private:
  void setUpCFG();
  void doLiveAnalysis();
  CFGNode* findNode(BasicBlock &b);
  Function &function;
  std::vector<CFGNode> nodes;
};

}//namespace
#endif