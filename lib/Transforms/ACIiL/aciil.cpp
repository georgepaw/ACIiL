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

using namespace llvm;

namespace {
  struct ACIiLPass : public ModulePass {
    static char ID;
    ACIiLPass() : ModulePass(ID) {}

    virtual bool runOnModule(Module &M) {
      errs() << "In module called: " << M.getName() << "!\n";

      bool modified = false;

      for(Function &F : M)
      {
        errs() << "Function : " << F.getName() << " found!\n";
        if(F.getName() == "fmad")
        // if(F.getName() == "foo" || F.getName() == "bar")
        {
          errs() << F.getName() << "\n";
          for(BasicBlock &B : F)
          {
            for(Instruction &I : B)
            {
              if(I.getOpcode() == Instruction::FAdd)
              {
                BinaryOperator* op = dyn_cast<BinaryOperator>(&I);
                IRBuilder<> builder(op);
                Value* lhs = op->getOperand(0);
                Value* rhs = op->getOperand(1);
                Value* mul = builder.CreateFSub(lhs, rhs);
                
                I.replaceAllUsesWith(mul);

                // for (auto& U : op->uses()) {
                  // U->dump();
                  // User* user = U.getUser();  // A User is anything with operands.
                  // user->replaceUsesOfWith(U, mul);

                  // user->setOperand(U.getOperandNo(), mul);
                // }
                modified = true;
                I.eraseFromParent();
              }
            }
          }
          F.dump();
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
