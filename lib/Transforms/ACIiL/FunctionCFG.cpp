#include "llvm/Transforms/ACIiL/FunctionCFG.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

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
  for(BasicBlock &B : function)
  {
    nodes.push_back(B);
    for(BasicBlock *to : successors(&B))
    {
      edges.push_back(CFGEdge(B, *to));
    }
  }
}

void FunctionCFG::dump()
{
  errs() << "\nCFG for function " << function.getName() << "\n";
  errs() << "There are " << nodes.size() << " nodes:\n";
  for(CFGNode node : nodes)
  {
    errs() << "*" << node.getBlock().getName() << "\n";
  }

  errs() << "There are " << edges.size() << " edges:\n";
  for(CFGEdge edge : edges)
  {
    errs() << "* from " << edge.getFrom().getName() << " to " << edge.getTo().getName() << "\n";
  }
  errs() << "\n";
}