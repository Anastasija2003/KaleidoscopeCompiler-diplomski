#pragma once

#include "AST.h"
#include "CodeGenContext.h"
#include "KaleidoscopeJIT.h"
#include "Lexer.h"

#include "llvm/Support/Error.h"

#include <memory>

class Parser {
public:
  // JIT is optional: with one, this drives the usual JIT REPL (each
  // definition/expression is handed to the JIT and the module is reset
  // right after). Without one (nullptr), every definition accumulates
  // into CG's single module instead -- what a static, whole-file compile
  // to object code needs.
  Parser(Lexer &Lex, CodeGenContext &CG,
         llvm::orc::KaleidoscopeJIT *JIT = nullptr);

  void run();

private:
  int getNextToken();
  int getTokPrecedence() const;

  std::unique_ptr<ExprAST> logError(const char *Str) const;
  std::unique_ptr<PrototypeAST> logErrorP(const char *Str) const;

  std::unique_ptr<ExprAST> parseNumberExpr();
  std::unique_ptr<ExprAST> parseParenExpr();
  std::unique_ptr<ExprAST> parseIdentifierExpr();
  std::unique_ptr<ExprAST> parseIfExpr();
  std::unique_ptr<ExprAST> parseForExpr();
  std::unique_ptr<ExprAST> parseVarExpr();
  std::unique_ptr<ExprAST> parsePrimary();
  std::unique_ptr<ExprAST> parseUnary();

  std::unique_ptr<ExprAST> parseBinOpRHS(int ExprPrec,
                                          std::unique_ptr<ExprAST> LHS);
  std::unique_ptr<ExprAST> parseExpression();

  std::unique_ptr<PrototypeAST> parsePrototype();
  std::unique_ptr<FunctionAST> parseDefinition();
  std::unique_ptr<FunctionAST> parseTopLevelExpr();
  std::unique_ptr<PrototypeAST> parseExtern();

  void handleDefinition();
  void handleExtern();
  void handleTopLevelExpression();

  Lexer &Lex;
  CodeGenContext &CG;
  llvm::orc::KaleidoscopeJIT *JIT;
  llvm::ExitOnError ExitOnErr;
  int CurTok = 0;
};
