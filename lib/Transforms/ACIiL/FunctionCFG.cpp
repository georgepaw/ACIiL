#include "llvm/Transforms/ACIiL/FunctionCFG.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"

#include <set>

using namespace llvm;

CFGOperand::CFGOperand(Value * v)
{
  value = v;
  fromPHI = false;
  sourcePHIBlock = NULL;
}

CFGOperand::CFGOperand(Value * v, BasicBlock * b)
{
  value = v;
  fromPHI = true;
  sourcePHIBlock = b;
}

Value * CFGOperand::getValue()
{
  return value;
}
bool CFGOperand::isFromPHI()
{
  return fromPHI;
}
BasicBlock * CFGOperand::getSourcePHIBlock()
{
  return sourcePHIBlock;
}

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

BasicBlock &CFGEdge::getFrom()
{
  return from;
}

BasicBlock &CFGEdge::getTo()
{
  return to;
}

FunctionCFG::FunctionCFG(Function & f) : function(f)
{
  setUpCFG();
}

void FunctionCFG::setUpCFG()
{
  //first need to prep basic blocks
  //if there are any phi instructions in the basicblock then
  //split the block
  // if(function.getName() == "main") function.viewCFG();
  std::vector<Instruction*> splitLocations;
  for(BasicBlock &B : function)
  {
    //According to spec
    //"There must be no non-phi instructions between the start of a basic block
    // and the PHI instructions: i.e. PHI instructions must be first in a basic
    // block."
    //So to split the block find the last phi instruction and split the block.


    //to find the last phi instruction
    //start at the first instruction, it it's not phi then this block does not
    //need to be splitted
    if(!isa<PHINode>(B.front())) continue;
    bool foundSplit = false;
    for(BasicBlock::iterator start = B.begin()++, end = B.end();
        start != end && !foundSplit; start++)
    {
      if(!isa<PHINode>(start))
      {
        splitLocations.push_back(&*start);
        foundSplit = true;
      }
    }
  }
  // Once all the locations to split on have been found then perform the splits
  for(Instruction *I : splitLocations)
  {
    BasicBlock * B = I->getParent();
    B->splitBasicBlock(I, B->getName() + ".no_phis");
  }

  for(BasicBlock &B : function)
  {
    nodes.push_back(CFGNode(B));
    for(BasicBlock *to : successors(&B))
    {
      edges.push_back(CFGEdge(B, *to));
    }
  }
  // if(function.getName() == "main") function.viewCFG();
}

void FunctionCFG::dump()
{
  errs() << "\nCFG for function " << function.getName() << "\n";
  errs() << "There are " << nodes.size() << " nodes:\n";
  for(CFGNode node : nodes)
  {
    errs() << "*" << node.getBlock().getName() << "\n";
    errs() << node.getBlock() << "\n";
    errs() << "def: \n";
    for(Value * v : node.getDef())
    {
      errs() << *v << "\n";
    }
    errs() << "use: \n";
    for(CFGOperand v : node.getUse())
    {
      errs() << *v.getValue() << "\n";
    }
    errs() << "\n";
  }

  errs() << "There are " << edges.size() << " edges:\n";
  for(CFGEdge edge : edges)
  {
    errs() << "* from " << edge.getFrom().getName() << " to " << edge.getTo().getName() << "\n";
  }
}

Function &FunctionCFG::getFunction()
{
  return function;
}
