#pragma once

#include "AST.h"
#include "Lexer.h"

#include <map>
#include <memory>

// Parser - Recursive-descent / operator-precedence parser that turns a
// token stream from a Lexer into an AST. Owns the "CurTok" lookahead and
// the top-level "ready>" driver loop.
class Parser {
public:
  explicit Parser(Lexer &Lex);

  // Reads tokens from the lexer until EOF, parsing and reporting each
  // top-level definition/extern/expression as it goes.
  void run();

private:
  int getNextToken();
  int getTokPrecedence() const;

  std::unique_ptr<ExprAST> logError(const char *Str) const;
  std::unique_ptr<PrototypeAST> logErrorP(const char *Str) const;

  // primary ::= identifierexpr | numberexpr | parenexpr
  std::unique_ptr<ExprAST> parseNumberExpr();
  std::unique_ptr<ExprAST> parseParenExpr();
  std::unique_ptr<ExprAST> parseIdentifierExpr();
  std::unique_ptr<ExprAST> parsePrimary();

  // expression ::= primary binoprhs
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
  int CurTok = 0;
  std::map<char, int> BinopPrecedence;
};
