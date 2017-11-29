#include "llvm/Transforms/ACIiL/CFGFunction.h"
#include "llvm/Transforms/ACIiL/CFGModule.h"
#include "llvm/Transforms/ACIiL/ACIiLAllocaManager.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/ACIiL/CFGOperand.h"
#include "llvm/Transforms/ACIiL/CFGNode.h"
#include "llvm/Transforms/ACIiL/CFGUtils.h"

#include <vector>
#include <set>
#include <map>

using namespace llvm;

CFGFunction::CFGFunction(Function & f, CFGModule &m) : function(f), am(*this), module(m)
{
  setUpCFG();
}

CFGFunction::~CFGFunction()
{
  for(CFGNode * node : nodes)
  {
    delete node;
  }
}

void CFGFunction::addNode(BasicBlock &b, bool isPhiNode)
{
  nodes.push_back(new CFGNode(b, isPhiNode, *this));
}

//TODO this needs to be improved, should the data structure be e vector? Not really
CFGNode* CFGFunction::findNodeByBasicBlock(BasicBlock &b)
{
  for(CFGNode * node : nodes)
  {
    if(&node->getLLVMBasicBlock() == &b) return node;
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
    addNode(B, isPhiNode);
  }

  //get all the edges
  for(BasicBlock &B : function)
  {
    CFGNode *thisNode = findNodeByBasicBlock(B);
    for(BasicBlock *to : successors(&B))
    {
      CFGNode *toNode = findNodeByBasicBlock(*to);
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
    bool changed = false;
    for(CFGNode * cfgNode : nodes)
    {
      //in[n] = (use[n]) union (out[n]-def[n])
      //first insert the use[n]
      changed = CFGCopyAllOperands(cfgNode->getIn(), cfgNode->getUse());
      //then insert the (out[n]-def[n])
      std::vector<CFGOperand> outDefDiff;
      for(CFGOperand op_out : cfgNode->getOut())
      {
        //if element from out is not in def
        if(cfgNode->getDef().find(op_out) == cfgNode->getDef().end())
        {
          //add it and update the changed flag accordingly
          changed |= CFGAddToSet(cfgNode->getIn(), op_out);
        }
      }

      //out[n] = union of in[s] for all successors s of n
      for(CFGNode * s : cfgNode->getSuccessors())
      {
        for(CFGOperand op : s->getIn())
        {
          if((op.isFromPHI() && &cfgNode->getLLVMBasicBlock() == op.getSourcePHIBlock()) //if this operand is used by a phi instruction and is form this block
             || !op.isFromPHI()) // or it's not used by phi
          {
            //not sure if this is needed
            //but it removes the fact that the variable is from the phi node when it is propagating
            CFGOperand op_clear = CFGOperand(op.getValue());
            changed |= CFGAddToSet(cfgNode->getOut(), op_clear);
          }
        }
      }
      //check if they have changed
      converged &= !changed;
    }
  } while(!converged);

  //set up mappings for variables in nodes
  for(CFGNode * cfgNode : nodes)
    for(CFGOperand op : cfgNode->getIn())
      cfgNode->addLiveMapping(op, op);
  for(CFGNode * cfgNode : nodes)
    for(CFGOperand op : cfgNode->getDef())
      cfgNode->addLiveMapping(op, op);
}

void CFGFunction::dump()
{
  errs() << "\nCFG for function " << function.getName() << "\n";
  errs() << "There are " << nodes.size() << " nodes:\n";
  for(CFGNode * node : nodes)
  {
    // if(node.getLLVMBasicBlock().getName() != "entry") continue;
    node->dump();
  }
}

Function &CFGFunction::getLLVMFunction()
{
  return function;
}

std::vector<CFGNode*> &CFGFunction::getNodes()
{
  return nodes;
}

CFGNode& CFGFunction::addCheckpointNode(BasicBlock &b, CFGNode &nodeForCR)
{
  //add the node
  addNode(b, false);
  //copy the in set
  CFGCopyAllOperands(nodes.back()->getIn(), nodeForCR.getIn());
  //set up the mapping
  for(CFGOperand op : nodeForCR.getIn())
    nodes.back()->addLiveMapping(op, op);
  return *nodes.back();
}

CFGNode& CFGFunction::addRestartNode(BasicBlock &b, CFGNode &nodeForCR)
{
  //add the node
  addNode(b, false);
  //nothing is live before the restart block so don't copy the in set
  //set up the mapping
  for(CFGOperand op : nodeForCR.getIn())
    nodes.back()->addLiveMapping(op, op);
  return *nodes.back();
}

CFGNode& CFGFunction::addNoCREntryNode(BasicBlock &b)
{
  //add the node
  addNode(b, false);
  //set up the mapping for all variables that are *defined*
  for(CFGOperand op : nodes.back()->getDef())
    nodes.back()->addLiveMapping(op, op);
  return *nodes.back();
}

ACIiLAllocaManager& CFGFunction::getAllocManager()
{
  return am;
}

CFGModule& CFGFunction::getParentModule()
{
  return module;
}
