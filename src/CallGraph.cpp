#include "CallGraph.h"

#include "AST.h"

#include <set>

void NumberExprAST::collectCallees(std::vector<std::string> &) const {}

void VariableExprAST::collectCallees(std::vector<std::string> &) const {}

void UnaryExprAST::collectCallees(std::vector<std::string> &Callees) const {
  Operand->collectCallees(Callees);
}

void BinaryExprAST::collectCallees(std::vector<std::string> &Callees) const {
  LHS->collectCallees(Callees);
  RHS->collectCallees(Callees);
}

void CallExprAST::collectCallees(std::vector<std::string> &Callees) const {
  Callees.push_back(Callee);
  for (auto &Arg : Args)
    Arg->collectCallees(Callees);
}

void IfExprAST::collectCallees(std::vector<std::string> &Callees) const {
  Cond->collectCallees(Callees);
  Then->collectCallees(Callees);
  Else->collectCallees(Callees);
}

void ForExprAST::collectCallees(std::vector<std::string> &Callees) const {
  Start->collectCallees(Callees);
  End->collectCallees(Callees);
  if (Step)
    Step->collectCallees(Callees);
  Body->collectCallees(Callees);
}

void VarExprAST::collectCallees(std::vector<std::string> &Callees) const {
  for (auto &Binding : VarNames)
    if (Binding.second)
      Binding.second->collectCallees(Callees);
  Body->collectCallees(Callees);
}

std::vector<std::string> FunctionAST::collectCallees() const {
  std::vector<std::string> Result;
  Body->collectCallees(Result);
  return Result;
}

CallGraph buildCallGraph(const std::vector<const FunctionAST *> &Functions) {
  CallGraph Graph;
  for (const FunctionAST *Fn : Functions) {
    std::vector<std::string> Raw = Fn->collectCallees();
    std::set<std::string> Unique(Raw.begin(), Raw.end());
    Graph[Fn->getName()] = std::vector<std::string>(Unique.begin(), Unique.end());
  }
  return Graph;
}
