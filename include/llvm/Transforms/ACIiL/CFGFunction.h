
#ifndef LLVM_TRANSFORMS_ACIIL_CFGFUNCTION_H
#define LLVM_TRANSFORMS_ACIIL_CFGFUNCTION_H

#include "llvm/Transforms/ACIiL/CFGNode.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Value.h"
#include "llvm/Transforms/ACIiL/ACIiLAllocaManager.h"

#include <vector>
#include <set>
#include <map>

namespace llvm
{
class CFGFunction
{
public:
  CFGFunction(Function &f);
  ~CFGFunction();
  void dump();
  Function &getFunction();
  std::vector<CFGNode*> &getNodes();
  CFGNode& addCheckpointNode(BasicBlock &b, CFGNode &nodeForCR);
  CFGNode& addRestartNode(BasicBlock &b, CFGNode &nodeForCR);
  CFGNode& addNoCREntryNode(BasicBlock &b);
  CFGNode* findNodeByBasicBlock(BasicBlock &b);
  ACIiLAllocaManager& getAllocManager();
private:
  void addNode(BasicBlock &b, bool isPhiNode);
  void setUpCFG();
  void doLiveAnalysis();
  Function &function;
  std::vector<CFGNode*> nodes;
  ACIiLAllocaManager am;
};

}//namespace
#endif