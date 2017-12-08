
#ifndef LLVM_TRANSFORMS_ACIIL_CFGFUNCTION_H
#define LLVM_TRANSFORMS_ACIIL_CFGFUNCTION_H

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/Transforms/ACIiL/ACIiLAllocaManager.h"
#include "llvm/Transforms/ACIiL/CFGNode.h"

#include <map>
#include <set>
#include <vector>

namespace llvm {
class CFGModule;
class CFGFunction {
public:
  CFGFunction(Function &f, CFGModule &m);
  ~CFGFunction();
  void dump();
  Function &getLLVMFunction();
  std::vector<CFGNode *> &getNodes();
  CFGNode &addCheckpointNode(BasicBlock &b, CFGNode &nodeForCR);
  CFGNode &addRestartNode(BasicBlock &b, CFGNode &nodeForCR);
  CFGNode &addNoCREntryNode(BasicBlock &b);
  CFGNode *findNodeByBasicBlock(BasicBlock &b);
  ACIiLAllocaManager &getAllocManager();
  CFGModule &getParentModule();
  Module &getParentLLVMModule();

private:
  void addNode(BasicBlock &b, bool isPhiNode);
  void setUpCFG();
  void doLiveAnalysis();
  Function &function;
  std::vector<CFGNode *> nodes;
  ACIiLAllocaManager am;
  CFGModule &module;
};

} // namespace llvm
#endif