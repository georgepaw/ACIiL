
#ifndef LLVM_TRANSFORMS_ACRIIL_CFGFUNCTION_H
#define LLVM_TRANSFORMS_ACRIIL_CFGFUNCTION_H

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/Transforms/ACRIiL/ACRIiLAllocaManager.h"
#include "llvm/Transforms/ACRIiL/ACRIiLPointerAlias.h"
#include "llvm/Transforms/ACRIiL/CFGNode.h"

#include <map>
#include <set>
#include <vector>

namespace llvm {

class CFGModule;
class CFGFunction {
public:
  CFGFunction(Function &f, CFGModule &m, TargetLibraryInfo &TLI,
              ModulePass *mp);
  ~CFGFunction();
  void dump();
  Function &getLLVMFunction();
  std::vector<CFGNode *> &getNodes();
  CFGNode &addCheckpointNode(BasicBlock &b, CFGNode &nodeForCR);
  CFGNode &addRestartNode(BasicBlock &b, CFGNode &nodeForCR);
  CFGNode &addNoCREntryNode(BasicBlock &b);
  CFGNode *findNodeByBasicBlock(BasicBlock &b);
  ACRIiLAllocaManager &getAllocManager();
  CFGModule &getParentModule();
  Module &getParentLLVMModule();
  std::map<Value *, PointerAliasInfo *> &getPointerInformation();
  std::set<CFGNode *> &getNodesToCheckpoint();

private:
  std::set<BasicBlock *> findCheckpointPoints(ModulePass *mp);
  CFGNode &addNode(BasicBlock &b, bool isPhiNode);
  void setUpCFG(std::set<BasicBlock *> checkpointBlocks);
  void doLiveAnalysis();
  void pointerAnalysis(TargetLibraryInfo &TLI, ModulePass *mp);
  void setUpLiveSetsAndMappings();
  Function &function;
  std::vector<CFGNode *> nodes;
  ACRIiLAllocaManager am;
  CFGModule &module;
  std::map<Value *, PointerAliasInfo *> pointerInformation;
  std::set<CFGNode *> nodesToCheckpoint;
};

} // namespace llvm
#endif