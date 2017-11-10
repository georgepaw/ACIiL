#include "llvm/Transforms/ACIiL/CFGFunction.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/ACIiL/CFGOperand.h"
#include "llvm/Transforms/ACIiL/CFGNode.h"
#include "llvm/Transforms/ACIiL/CFGUtils.h"

#include <set>
#include <map>

using namespace llvm;

CFGFunction::CFGFunction(Function & f) : function(f)
{
  setUpCFG();
}

CFGNode* CFGFunction::findNode(BasicBlock &b)
{
  for(CFGNode &node : nodes)
  {
    if(&node.getBlock() == &b) return &node;
  }
  return NULL;
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

  std::set<BasicBlock*> phiNodes;

  // Once all the locations to split on have been found then perform the splits
  for(Instruction *I : splitLocations)
  {
    BasicBlock * B = I->getParent();
    B->splitBasicBlock(I, B->getName() + ".no_phis");
    phiNodes.insert(B);
  }

  //Now that blocks are ready, set up the CFG graph for the function
  //get all the nodes
  for(BasicBlock &B : function)
  {
    bool isPhiNode = phiNodes.find(&B) != phiNodes.end();
    nodes.push_back(CFGNode(B, isPhiNode));
  }

  //get all the edges
  for(BasicBlock &B : function)
  {
    CFGNode *thisNode = findNode(B);
    for(BasicBlock *to : successors(&B))
    {
      CFGNode *toNode = findNode(*to);
      thisNode->addSuccessor(toNode);
    }
  }
  doLiveAnalysis();
}

void CFGFunction::doLiveAnalysis()
{
  bool converged;
  do
  {
    converged = true;
    for(CFGNode &cfgNode : nodes)
    {
      bool changed = false;
      errs() << "Block: " << cfgNode.getBlock().getName() << " in " << cfgNode.getIn().size() << " out " << cfgNode.getOut().size() << "\n";

      //in[n] = (use[n]) union (out[n]-def[n])
      //first insert the use[n]
      changed = CFGCopyAllOperands(cfgNode.getIn(), cfgNode.getUse());
      //then insert the (out[n]-def[n])
      std::vector<CFGOperand> outDefDiff;
      for(CFGOperand op_out : cfgNode.getOut())
      {
        //if element from out is not in def
        if(cfgNode.getDef().find(op_out) == cfgNode.getDef().end())
        {
          //add it and update the changed flag accordingly
          auto ret = cfgNode.getIn().insert(op_out);
          changed |= ret.second;
        }
      }

      //out[n] = union of in[s] for all successors s of n
      for(CFGNode * s : cfgNode.getSuccessors())
      {
        for(CFGOperand op : s->getIn())
        {
          if((op.isFromPHI() && &cfgNode.getBlock() == op.getSourcePHIBlock()) //if this operand is used by a phi instruction and is form this block
             || !op.isFromPHI()) // or it's not used by phi
          {
            //TODO not sure if this is needed
            //but it removes the fact that the variable is from the phi node when it is propagating
            CFGOperand op_clear = CFGOperand(op.getValue());
            changed |= CFGAddToSet(cfgNode.getOut(), op_clear);
          }
        }
      }

      //check if they have changed
      converged = !changed;
    }
    errs() << "Function not converged " << function.getName() << "\n";
  } while(!converged);
}

void CFGFunction::dump()
{
  errs() << "\nCFG for function " << function.getName() << "\n";
  errs() << "There are " << nodes.size() << " nodes:\n";
  for(CFGNode node : nodes)
  {
    if(node.getBlock().getName() != "entry") continue;
    errs() << "* " << node.getBlock().getName() << "\n";
    errs() << node.getBlock() << "\n";
    errs() << "def: \n";
    for(CFGOperand v : node.getDef())
    {
      v.dump();
    }
    errs() << "use: \n";
    for(CFGOperand v : node.getUse())
    {
      v.dump();
    }
    errs() << "\n";
    errs() << "in: \n";
    for(CFGOperand v : node.getIn())
    {
      v.dump();
    }
    errs() << "\n";
    errs() << "out: \n";
    for(CFGOperand v : node.getOut())
    {
      v.dump();
    }
    errs() << "\n";

    errs() << "This node has " << node.getSuccessors().size() << " edges to nodes:\n";
    for(CFGNode * s : node.getSuccessors())
    {
      errs() << "\t* " << s->getBlock().getName() << "\n";
    }
    errs() << "\n";
  }
}

Function &CFGFunction::getFunction()
{
  return function;
}

std::vector<CFGNode> &CFGFunction::getNodes()
{
  return nodes;
}
