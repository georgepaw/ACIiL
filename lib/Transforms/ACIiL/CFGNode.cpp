#include "llvm/Transforms/ACIiL/CFGNode.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/ACIiL/CFGOperand.h"

#include <set>

using namespace llvm;

CFGNode::CFGNode(BasicBlock &b) : block(b)
{
  for(BasicBlock::iterator end = --block.end(), start = --block.begin(); end != start; end--)
  {
    Instruction &inst = *end;
    //get defines
    //if it has a non void return then it defines
    if(!end->getType()->isVoidTy()) def.insert(&inst);

    //get uses
    //TODO: dyn_cast does not work here and have to do it explicitly
    if(isa<PHINode>(inst))
    {
      //if it's a phi instruction
      PHINode &phi = cast<PHINode>(inst);
      for(Use &u : phi.operands())
      {
        //at the momemnt assume if the operand is a result of instruction then it should be in use set
        if(isa<Instruction>(u)) use.insert(CFGOperand(u, phi.getIncomingBlock(u)));
      }
    }
    else
    {
      //otherwise just iterate over operands
      for(Value *v : end->operands())
      {
        //at the momemnt assume if the operand is a result of instruction then it should be in use set
        if(isa<Instruction>(v)) use.insert(CFGOperand(v));
      }
    }
  }
}

std::set<Value*> &CFGNode::getDef()
{
  return def;
}

std::set<CFGOperand> &CFGNode::getUse()
{
  return use;
}

BasicBlock &CFGNode::getBlock()
{
  return block;
}
