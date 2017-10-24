
#ifndef LLVM_TRANSFORMS_ACIIL_FUNCTIONCFG_H
#define LLVM_TRANSFORMS_ACIIL_FUNCTIONCFG_H

#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Value.h"

#include <vector>
#include <set>

namespace llvm
{
class CFGOperand
{
public:
  CFGOperand(Value * v);
  CFGOperand(Value * v, BasicBlock * b);
  Value * getValue();
  bool isFromPHI();
  BasicBlock * getSourcePHIBlock();
  bool operator<(const CFGOperand& other) const
  {
    return value < other.value;
  }
private:
  Value * value;
  bool fromPHI = false;
  BasicBlock * sourcePHIBlock;
};

class CFGNode
{
public:
  CFGNode(BasicBlock &b);
  BasicBlock &getBlock();
  std::set<Value*> &getDef();
  std::set<CFGOperand> &getUse();
private:
  BasicBlock &block;
  std::set<CFGOperand> use;
  std::set<Value*> def;
};

class CFGEdge
{
public:
  CFGEdge(BasicBlock &f, BasicBlock &t) : from(f), to(t) {};
  BasicBlock &getFrom();
  BasicBlock &getTo();
private:
  BasicBlock &from;
  BasicBlock &to;
};

class FunctionCFG
{
public:
  FunctionCFG(Function &f);
  void dump();
  Function &getFunction();
private:
  void setUpCFG();
  Function &function;
  std::vector<CFGNode> nodes;
  std::vector<CFGEdge> edges;
};

}//namespace
#endif