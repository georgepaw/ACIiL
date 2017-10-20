#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/ACIiL.h"
#include "llvm/Transforms/IPO/Internalize.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Transforms/Utils/UnifyFunctionExitNodes.h"
#include "llvm/Transforms/ACIiL/ModuleCFG.h"

using namespace llvm;

namespace {
  struct ACIiLPass : public ModulePass {
    static char ID;
    ACIiLPass() : ModulePass(ID) {}

    virtual bool runOnModule(Module &M) {
      bool modified = false;
      errs() << "In module called: " << M.getName() << "!\n";
      Function * mainFunction = M.getFunction("main");


      if(!mainFunction) return false;
      ModuleCFG moduleCFG(M, *mainFunction);
      moduleCFG.dump();


      for(Function &F : M)
      {
        errs() << "Function : " << F.getName() << " found!\n";
        if (F.isDeclaration())
        {
          errs() << "This functions decleration is outside of the current transaltion unit.\n";
          // errs() << "This function is used " << F.getNumUses() << " times, by:\n";
          // for(User *U : F.users())
          // {
          //   if(isa<CallInst>(U))
          //   {
          //     Instruction * caller = dyn_cast<Instruction>(U);
          //     std::string str;
          //     raw_string_ostream rso(str);
          //     caller->print(rso);
          //     errs() << "* " << caller->getFunction()->getName() << "\n";
          //     errs() << "\t- Instruction"  << str << "\n";
          //     errs() << "\t- Users:\n";
          //     for(Value *v : caller->users())
          //     {
          //       Instruction * i = dyn_cast<Instruction>(v);
          //       std::string str;
          //       raw_string_ostream rso(str);
          //       i->print(rso);
          //       errs() << "\t\t*" << str << "\n";
          //     }
          //     errs() << "\t- Uses:\n";
          //     for(Value *v : caller->operands())
          //     {
          //       std::string str;
          //       if(isa<Function>(v))
          //       {
          //         Function * f = dyn_cast<Function>(v);
          //         str = "  ";
          //         str += f->getName();
          //       }
          //       else
          //       {
          //         raw_string_ostream rso(str);
          //         v->print(rso);
          //       }
          //       errs() << "\t\t*" << str << "\n";
          //     }
          //   }
          // }
        }
        else //(!F.isDeclaration())
        {
          uint32_t num_block = F.getBasicBlockList().size();
          errs() << "This function has " << num_block << " BasicBlocks\n";
        }
        if(F.getName() == "main")
        {
          // F.end()->dump();
        }
      }
      return modified;
    }
  };
}

char ACIiLPass::ID = 0;

static void registerACIiLPass(const PassManagerBuilder &,
                         legacy::PassManagerBase &PM) {
  PM.add(new ACIiLPass());
}

ModulePass *llvm::createACIiLLinkingPass() {
  errs() <<"Adding ACIiLPass!\n";
  return new ACIiLPass();
}

// static RegisterStandardPasses
//     register_pass_O(PassManagerBuilder::EP_OptimizerLast, registerACIiLPass);

// static RegisterStandardPasses
//     register_pass_O0(PassManagerBuilder::EP_EnabledOnOptLevel0, registerACIiLPass);
