#include "llvm/Transforms/ACIiL/CFGFunction.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/ACIiL/CFGOperand.h"
#include "llvm/Transforms/ACIiL/CFGNode.h"
#include "llvm/Transforms/ACIiL/CFGEdge.h"

#include <set>

using namespace llvm;

CFGFunction::CFGFunction(Function & f) : function(f)
{
  setUpCFG();
}

void CFGFunction::setUpCFG()
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

void CFGFunction::dump()
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
      v.dump();
    }
    errs() << "\n";
  }

  errs() << "There are " << edges.size() << " edges:\n";
  for(CFGEdge edge : edges)
  {
    errs() << "* from " << edge.getFrom().getName() << " to " << edge.getTo().getName() << "\n";
  }
}

Function &CFGFunction::getFunction()
{
  return function;
}
