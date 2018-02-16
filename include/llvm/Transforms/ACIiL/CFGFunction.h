
#ifndef LLVM_TRANSFORMS_ACIIL_CFGFUNCTION_H
#define LLVM_TRANSFORMS_ACIIL_CFGFUNCTION_H

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/Transforms/ACIiL/ACIiLAllocaManager.h"
#include "llvm/Transforms/ACIiL/ACRIiLPointerAlias.h"
#include "llvm/Transforms/ACIiL/CFGNode.h"

#include <map>
#include <set>
#include <vector>

namespace llvm {

class CFGModule;
class CFGFunction {
public:
  CFGFunction(Function &f, CFGModule &m, TargetLibraryInfo &TLI,
              AliasAnalysis *AA);
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
  std::map<Value *, PointerAliasInfo *> &getPointerInformation();

private:
  void addNode(BasicBlock &b, bool isPhiNode);
  void setUpCFG();
  void doLiveAnalysis();
  void pointerAnalysis(TargetLibraryInfo &TLI, AliasAnalysis *AA);
  void setUpLiveSetsAndMappings();
  Function &function;
  std::vector<CFGNode *> nodes;
  ACIiLAllocaManager am;
  CFGModule &module;
  std::map<Value *, PointerAliasInfo *> pointerInformation;
};

} // namespace llvm
#endif