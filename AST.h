#pragma once

#include "SourceLocation.h"

#include <cassert>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// Forward-declared instead of #include-d so that AST.h (pulled in by both
// Parser.h and CodeGen.cpp) doesn't drag the LLVM headers into every
// translation unit that only wants to parse, not codegen.
namespace llvm {
class Value;
class Function;
} // namespace llvm

class CodeGenContext;

// ExprAST - Base class for all expression nodes. Every node carries the
// source position it started at, so CodeGenContext::emitLocation() can
// attach debug info to the instructions it generates (when debug info is
// enabled -- see CodeGenContext::enableDebugInfo()).
class ExprAST {
  SourceLocation Loc;

public:
  explicit ExprAST(SourceLocation Loc) : Loc(Loc) {}
  virtual ~ExprAST() = default;

  virtual llvm::Value *codegen(CodeGenContext &CG) = 0;

  int getLine() const { return Loc.Line; }
  int getCol() const { return Loc.Col; }
};

// NumberExprAST - Expression class for numeric literals like "1.0".
class NumberExprAST : public ExprAST {
  double Val;

public:
  NumberExprAST(SourceLocation Loc, double Val) : ExprAST(Loc), Val(Val) {}

  llvm::Value *codegen(CodeGenContext &CG) override;
};

// VariableExprAST - Expression class for referencing a variable, like "a".
class VariableExprAST : public ExprAST {
  std::string Name;

public:
  VariableExprAST(SourceLocation Loc, std::string Name)
      : ExprAST(Loc), Name(std::move(Name)) {}

  llvm::Value *codegen(CodeGenContext &CG) override;
  const std::string &getName() const { return Name; }
};

// UnaryExprAST - Expression class for a unary operator.
class UnaryExprAST : public ExprAST {
  char Opcode;
  std::unique_ptr<ExprAST> Operand;

public:
  UnaryExprAST(SourceLocation Loc, char Opcode, std::unique_ptr<ExprAST> Operand)
      : ExprAST(Loc), Opcode(Opcode), Operand(std::move(Operand)) {}

  llvm::Value *codegen(CodeGenContext &CG) override;
};

// BinaryExprAST - Expression class for a binary operator.
class BinaryExprAST : public ExprAST {
  char Op;
  std::unique_ptr<ExprAST> LHS, RHS;

public:
  BinaryExprAST(SourceLocation Loc, char Op, std::unique_ptr<ExprAST> LHS,
                std::unique_ptr<ExprAST> RHS)
      : ExprAST(Loc), Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}

  llvm::Value *codegen(CodeGenContext &CG) override;
};

// CallExprAST - Expression class for function calls.
class CallExprAST : public ExprAST {
  std::string Callee;
  std::vector<std::unique_ptr<ExprAST>> Args;

public:
  CallExprAST(SourceLocation Loc, std::string Callee,
              std::vector<std::unique_ptr<ExprAST>> Args)
      : ExprAST(Loc), Callee(std::move(Callee)), Args(std::move(Args)) {}

  llvm::Value *codegen(CodeGenContext &CG) override;
};

// IfExprAST - Expression class for if/then/else.
class IfExprAST : public ExprAST {
  std::unique_ptr<ExprAST> Cond, Then, Else;

public:
  IfExprAST(SourceLocation Loc, std::unique_ptr<ExprAST> Cond,
            std::unique_ptr<ExprAST> Then, std::unique_ptr<ExprAST> Else)
      : ExprAST(Loc), Cond(std::move(Cond)), Then(std::move(Then)),
        Else(std::move(Else)) {}

  llvm::Value *codegen(CodeGenContext &CG) override;
};

// ForExprAST - Expression class for a 'for var = start, end, step in body'
// loop.
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
};

// VarExprAST - Expression class for 'var a = 1, b = 2 in body', which
// introduces new mutable local variables in scope for Body.
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
};

// PrototypeAST - Represents the "prototype" for a function: its name, its
// argument names (which implicitly gives the argument count), and --
// since a user-defined operator is just a function with a special name
// ("binary<op>"/"unary<op>") -- whether it's one, and at what precedence.
// Only tracks a line (not a full SourceLocation) since that's all
// DISubprogram (see CodeGen.cpp) needs.
class PrototypeAST {
  std::string Name;
  std::vector<std::string> Args;
  bool IsOperator;
  unsigned Precedence; // Precedence if this is a binary operator.
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
};

// FunctionAST - Represents a function definition itself.
class FunctionAST {
  std::unique_ptr<PrototypeAST> Proto;
  std::unique_ptr<ExprAST> Body;

public:
  FunctionAST(std::unique_ptr<PrototypeAST> Proto,
              std::unique_ptr<ExprAST> Body)
      : Proto(std::move(Proto)), Body(std::move(Body)) {}

  llvm::Function *codegen(CodeGenContext &CG);
};
