
#ifndef LLVM_TRANSFORMS_ACIIL_CFGFUNCTION_H
#define LLVM_TRANSFORMS_ACIIL_CFGFUNCTION_H

#include "llvm/Transforms/ACIiL/CFGNode.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Value.h"

#include <vector>
#include <set>
#include <map>

namespace llvm
{
class CFGFunction
{
public:
  CFGFunction(Function &f);
  void dump();
  Function &getFunction();
  std::vector<CFGNode> &getNodes();
  CFGNode& addCheckpointNode(BasicBlock &b, uint64_t nodeForCRIndex);
  CFGNode& addRestartNode(BasicBlock &b, uint64_t nodeForCRIndex);
  CFGNode& addNoCREntryNode(BasicBlock &b);
  CFGNode* findNodeByBasicBlock(BasicBlock &b);
private:
  void addNode(BasicBlock &b, bool isPhiNode);
  void setUpCFG();
  void doLiveAnalysis();
  Function &function;
  std::vector<CFGNode> nodes;
};

}//namespace
#endif