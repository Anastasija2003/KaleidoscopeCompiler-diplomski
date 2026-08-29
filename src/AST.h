#pragma once

#include "CanonState.h"
#include "SourceLocation.h"

#include <cassert>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace llvm {
class Value;
class Function;
}

class CodeGenContext;

class ExprAST {
  SourceLocation Loc;

public:
  explicit ExprAST(SourceLocation Loc) : Loc(Loc) {}
  virtual ~ExprAST() = default;

  virtual llvm::Value *codegen(CodeGenContext &CG) = 0;
  virtual std::string canonicalize(CanonState &State) const = 0;
  virtual void collectCallees(std::vector<std::string> &Callees) const = 0;

  int getLine() const { return Loc.Line; }
  int getCol() const { return Loc.Col; }
};

class NumberExprAST : public ExprAST {
  double Val;

public:
  NumberExprAST(SourceLocation Loc, double Val) : ExprAST(Loc), Val(Val) {}

  llvm::Value *codegen(CodeGenContext &CG) override;
  std::string canonicalize(CanonState &State) const override;
  void collectCallees(std::vector<std::string> &Callees) const override;
};

class VariableExprAST : public ExprAST {
  std::string Name;

public:
  VariableExprAST(SourceLocation Loc, std::string Name)
      : ExprAST(Loc), Name(std::move(Name)) {}

  llvm::Value *codegen(CodeGenContext &CG) override;
  std::string canonicalize(CanonState &State) const override;
  void collectCallees(std::vector<std::string> &Callees) const override;
  const std::string &getName() const { return Name; }
};

class UnaryExprAST : public ExprAST {
  char Opcode;
  std::unique_ptr<ExprAST> Operand;

public:
  UnaryExprAST(SourceLocation Loc, char Opcode, std::unique_ptr<ExprAST> Operand)
      : ExprAST(Loc), Opcode(Opcode), Operand(std::move(Operand)) {}

  llvm::Value *codegen(CodeGenContext &CG) override;
  std::string canonicalize(CanonState &State) const override;
  void collectCallees(std::vector<std::string> &Callees) const override;
};

class BinaryExprAST : public ExprAST {
  char Op;
  std::unique_ptr<ExprAST> LHS, RHS;

public:
  BinaryExprAST(SourceLocation Loc, char Op, std::unique_ptr<ExprAST> LHS,
                std::unique_ptr<ExprAST> RHS)
      : ExprAST(Loc), Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}

  llvm::Value *codegen(CodeGenContext &CG) override;
  std::string canonicalize(CanonState &State) const override;
  void collectCallees(std::vector<std::string> &Callees) const override;
};

class CallExprAST : public ExprAST {
  std::string Callee;
  std::vector<std::unique_ptr<ExprAST>> Args;

public:
  CallExprAST(SourceLocation Loc, std::string Callee,
              std::vector<std::unique_ptr<ExprAST>> Args)
      : ExprAST(Loc), Callee(std::move(Callee)), Args(std::move(Args)) {}

  llvm::Value *codegen(CodeGenContext &CG) override;
  std::string canonicalize(CanonState &State) const override;
  void collectCallees(std::vector<std::string> &Callees) const override;
};

class IfExprAST : public ExprAST {
  std::unique_ptr<ExprAST> Cond, Then, Else;

public:
  IfExprAST(SourceLocation Loc, std::unique_ptr<ExprAST> Cond,
            std::unique_ptr<ExprAST> Then, std::unique_ptr<ExprAST> Else)
      : ExprAST(Loc), Cond(std::move(Cond)), Then(std::move(Then)),
        Else(std::move(Else)) {}

  llvm::Value *codegen(CodeGenContext &CG) override;
  std::string canonicalize(CanonState &State) const override;
  void collectCallees(std::vector<std::string> &Callees) const override;
};

class ForExprAST : public ExprAST {
  std::string VarName;
  std::unique_ptr<ExprAST> Start, End, Step, Body;

public:
  ForExprAST(SourceLocation Loc, std::string VarName,
             std::unique_ptr<ExprAST> Start, std::unique_ptr<ExprAST> End,
             std::unique_ptr<ExprAST> Step, std::unique_ptr<ExprAST> Body)
      : ExprAST(Loc), VarName(std::move(VarName)), Start(std::move(Start)),
        End(std::move(End)), Step(std::move(Step)), Body(std::move(Body)) {}

  llvm::Value *codegen(CodeGenContext &CG) override;
  std::string canonicalize(CanonState &State) const override;
  void collectCallees(std::vector<std::string> &Callees) const override;
};

class VarExprAST : public ExprAST {
  std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;
  std::unique_ptr<ExprAST> Body;

public:
  VarExprAST(
      SourceLocation Loc,
      std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames,
      std::unique_ptr<ExprAST> Body)
      : ExprAST(Loc), VarNames(std::move(VarNames)), Body(std::move(Body)) {}

  llvm::Value *codegen(CodeGenContext &CG) override;
  std::string canonicalize(CanonState &State) const override;
  void collectCallees(std::vector<std::string> &Callees) const override;
};

class PrototypeAST {
  std::string Name;
  std::vector<std::string> Args;
  bool IsOperator;
  unsigned Precedence;
  int Line;

public:
  PrototypeAST(SourceLocation Loc, std::string Name,
               std::vector<std::string> Args, bool IsOperator = false,
               unsigned Precedence = 0)
      : Name(std::move(Name)), Args(std::move(Args)), IsOperator(IsOperator),
        Precedence(Precedence), Line(Loc.Line) {}

  const std::string &getName() const { return Name; }

  bool isUnaryOp() const { return IsOperator && Args.size() == 1; }
  bool isBinaryOp() const { return IsOperator && Args.size() == 2; }

  char getOperatorName() const {
    assert(isUnaryOp() || isBinaryOp());
    return Name[Name.size() - 1];
  }

  unsigned getBinaryPrecedence() const { return Precedence; }
  int getLine() const { return Line; }

  llvm::Function *codegen(CodeGenContext &CG);
  std::string canonicalize(CanonState &State) const;
};

class FunctionAST {
  std::unique_ptr<PrototypeAST> Proto;
  std::unique_ptr<ExprAST> Body;

public:
  FunctionAST(std::unique_ptr<PrototypeAST> Proto,
              std::unique_ptr<ExprAST> Body)
      : Proto(std::move(Proto)), Body(std::move(Body)) {}

  llvm::Function *codegen(CodeGenContext &CG);
  std::string canonicalize() const;
  std::vector<std::string> collectCallees() const;
  const std::string &getName() const { return Proto->getName(); }
};
