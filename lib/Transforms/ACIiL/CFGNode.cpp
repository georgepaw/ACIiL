#include "llvm/Transforms/ACIiL/CFGNode.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/ACIiL/CFGOperand.h"

#include <set>

using namespace llvm;

CFGNode::CFGNode(BasicBlock &b, bool isPhiNode) : phiNode(isPhiNode), block(b)
{
  for(BasicBlock::iterator end = --block.end(), start = --block.begin(); end != start; end--)
  {
    Instruction &inst = *end;
    //get defines
    //if it has a non void return then it defines
    if(!end->getType()->isVoidTy())
    {

      def.insert(CFGOperand(&inst));
      //since this is SSA, remove the definition from uses set
      if(use.find(CFGOperand(&inst)) != use.end()) use.erase(CFGOperand(&inst));
    }


    //get uses
    if(PHINode * phi = dyn_cast<PHINode>(&inst))
    {
      //if it's a phi instruction
      for(Use &u : phi->operands())
      {
        //at the momemnt assume if the operand is a result of instruction then it should be in use set
        if(isa<Instruction>(u)) use.insert(CFGOperand(u, phi->getIncomingBlock(u)));
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


void CFGNode::addSuccessor(CFGNode *s)
{
  successors.insert(s);
}

std::set<CFGNode*> &CFGNode::getSuccessors()
{
  return successors;
}

std::set<CFGOperand> &CFGNode::getDef()
{
  return def;
}

std::set<CFGOperand> &CFGNode::getUse()
{
  return use;
}

std::set<CFGOperand> &CFGNode::getIn()
{
  return in;
}

std::set<CFGOperand> &CFGNode::getOut()
{
  return out;
}

BasicBlock &CFGNode::getBlock()
{
  return block;
}

bool CFGNode::isPhiNode()
{
  return phiNode;
}

void CFGNode::addLiveMapping(CFGOperand from, CFGOperand to)
{
  std::map<CFGOperand, CFGOperand>::iterator it = liveVariablesMap.find(from);
  if(it == liveVariablesMap.end())
    liveVariablesMap.insert(std::make_pair(from, to));
  else
    it->second = to;
}

bool CFGNode::isLive(CFGOperand in)
{
  return liveVariablesMap.find(in) != liveVariablesMap.end();
}

CFGOperand * CFGNode::getLiveMapping(CFGOperand from)
{
  return &liveVariablesMap.find(from)->second;
}

void CFGNode::dump()
{
  errs() << "* " << getBlock().getName() << "\n";
  errs() << getBlock() << "\n";
  errs() << "def: \n";
  for(CFGOperand v : getDef())
  {
    v.dump();
  }
  errs() << "use: \n";
  for(CFGOperand v : getUse())
  {
    v.dump();
  }
  errs() << "\n";
  errs() << "in: \n";
  for(CFGOperand v : getIn())
  {
    v.dump();
  }
  errs() << "\n";
  errs() << "out: \n";
  for(CFGOperand v : getOut())
  {
    v.dump();
  }
  errs() << "\n";

  errs() << "This node has " << getSuccessors().size() << " edges to nodes:\n";
  for(CFGNode * s : getSuccessors())
  {
    errs() << "\t* " << s->getBlock().getName() << "\n";
  }
  errs() << "\n";
}

