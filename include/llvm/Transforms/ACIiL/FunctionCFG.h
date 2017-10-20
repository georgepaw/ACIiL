
#ifndef LLVM_TRANSFORMS_ACIIL_FUNCTIONCFG_H
#define LLVM_TRANSFORMS_ACIIL_FUNCTIONCFG_H

#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"

#include <vector>


namespace llvm
{
class CFGNode
{
public:
  CFGNode(BasicBlock &b) : block(b) {};
  BasicBlock &getBlock();
private:
  BasicBlock &block;

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
private:
  void setUpCFG();
  Function &function;
  std::vector<CFGNode> nodes;
  std::vector<CFGEdge> edges;
};

}//namespace
#endif