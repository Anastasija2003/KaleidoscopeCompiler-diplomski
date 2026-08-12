#include "Parser.h"

#include <cstdio>

Parser::Parser(Lexer &Lex) : Lex(Lex) {
  // Lowest precedence first; '*' binds tightest.
  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40;
}

int Parser::getNextToken() { return CurTok = Lex.getTok(); }

int Parser::getTokPrecedence() const {
  if (CurTok < 0 || CurTok > 255)
    return -1;

  auto It = BinopPrecedence.find(static_cast<char>(CurTok));
  if (It == BinopPrecedence.end())
    return -1;
  return It->second;
}

std::unique_ptr<ExprAST> Parser::logError(const char *Str) const {
  fprintf(stderr, "Error: %s\n", Str);
  return nullptr;
}

std::unique_ptr<PrototypeAST> Parser::logErrorP(const char *Str) const {
  logError(Str);
  return nullptr;
}

// numberexpr ::= number
std::unique_ptr<ExprAST> Parser::parseNumberExpr() {
  auto Result = std::make_unique<NumberExprAST>(Lex.getNumVal());
  getNextToken(); // consume the number
  return Result;
}

// parenexpr ::= '(' expression ')'
std::unique_ptr<ExprAST> Parser::parseParenExpr() {
  getNextToken(); // eat (
  auto V = parseExpression();
  if (!V)
    return nullptr;

  if (CurTok != ')')
    return logError("expected ')'");
  getNextToken(); // eat )
  return V;
}

// identifierexpr
//   ::= identifier
//   ::= identifier '(' expression* ')'
std::unique_ptr<ExprAST> Parser::parseIdentifierExpr() {
  std::string IdName = Lex.getIdentifier();

  getNextToken(); // eat identifier

  if (CurTok != '(') // Simple variable ref.
    return std::make_unique<VariableExprAST>(IdName);

  // Call.
  getNextToken(); // eat (
  std::vector<std::unique_ptr<ExprAST>> Args;
  if (CurTok != ')') {
    while (true) {
      if (auto Arg = parseExpression())
        Args.push_back(std::move(Arg));
      else
        return nullptr;

      if (CurTok == ')')
        break;

      if (CurTok != ',')
        return logError("Expected ')' or ',' in argument list");
      getNextToken();
    }
  }

  getNextToken(); // eat )
  return std::make_unique<CallExprAST>(IdName, std::move(Args));
}

// primary ::= identifierexpr | numberexpr | parenexpr
std::unique_ptr<ExprAST> Parser::parsePrimary() {
  switch (CurTok) {
  default:
    return logError("unknown token when expecting an expression");
  case tok_identifier:
    return parseIdentifierExpr();
  case tok_number:
    return parseNumberExpr();
  case '(':
    return parseParenExpr();
  }
}

// binoprhs ::= ('+' primary)*
std::unique_ptr<ExprAST> Parser::parseBinOpRHS(int ExprPrec,
                                                std::unique_ptr<ExprAST> LHS) {
  while (true) {
    int TokPrec = getTokPrecedence();

    // If this binop binds at least as tightly as the current one, consume
    // it; otherwise we're done.
    if (TokPrec < ExprPrec)
      return LHS;

    int BinOp = CurTok;
    getNextToken(); // eat binop

    auto RHS = parsePrimary();
    if (!RHS)
      return nullptr;

    // If BinOp binds less tightly with RHS than the operator after RHS,
    // let the pending operator take RHS as its LHS.
    int NextPrec = getTokPrecedence();
    if (TokPrec < NextPrec) {
      RHS = parseBinOpRHS(TokPrec + 1, std::move(RHS));
      if (!RHS)
        return nullptr;
    }

    LHS = std::make_unique<BinaryExprAST>(static_cast<char>(BinOp),
                                           std::move(LHS), std::move(RHS));
  }
}

// expression ::= primary binoprhs
std::unique_ptr<ExprAST> Parser::parseExpression() {
  auto LHS = parsePrimary();
  if (!LHS)
    return nullptr;

  return parseBinOpRHS(0, std::move(LHS));
}

// prototype ::= id '(' id* ')'
std::unique_ptr<PrototypeAST> Parser::parsePrototype() {
  if (CurTok != tok_identifier)
    return logErrorP("Expected function name in prototype");

  std::string FnName = Lex.getIdentifier();
  getNextToken();

  if (CurTok != '(')
    return logErrorP("Expected '(' in prototype");

  std::vector<std::string> ArgNames;
  while (getNextToken() == tok_identifier)
    ArgNames.push_back(Lex.getIdentifier());
  if (CurTok != ')')
    return logErrorP("Expected ')' in prototype");

  getNextToken(); // eat )
  return std::make_unique<PrototypeAST>(FnName, std::move(ArgNames));
}

// definition ::= 'def' prototype expression
std::unique_ptr<FunctionAST> Parser::parseDefinition() {
  getNextToken(); // eat def
  auto Proto = parsePrototype();
  if (!Proto)
    return nullptr;

  if (auto E = parseExpression())
    return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
  return nullptr;
}

// toplevelexpr ::= expression
std::unique_ptr<FunctionAST> Parser::parseTopLevelExpr() {
  if (auto E = parseExpression()) {
    // Wrap it in an anonymous no-argument function.
    auto Proto =
        std::make_unique<PrototypeAST>("__anon_expr", std::vector<std::string>());
    return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
  }
  return nullptr;
}

// external ::= 'extern' prototype
std::unique_ptr<PrototypeAST> Parser::parseExtern() {
  getNextToken(); // eat extern
  return parsePrototype();
}

void Parser::handleDefinition() {
  if (parseDefinition()) {
    fprintf(stderr, "Parsed a function definition.\n");
  } else {
    getNextToken(); // skip token for error recovery
  }
}

void Parser::handleExtern() {
  if (parseExtern()) {
    fprintf(stderr, "Parsed an extern\n");
  } else {
    getNextToken();
  }
}

void Parser::handleTopLevelExpression() {
  if (parseTopLevelExpr()) {
    fprintf(stderr, "Parsed a top-level expr\n");
  } else {
    getNextToken();
  }
}

// top ::= definition | external | expression | ';'
void Parser::run() {
  fprintf(stderr, "ready> ");
  getNextToken(); // prime the first token

  while (true) {
    fprintf(stderr, "ready> ");
    switch (CurTok) {
    case tok_eof:
      return;
    case ';': // ignore top-level semicolons
      getNextToken();
      break;
    case tok_def:
      handleDefinition();
      break;
    case tok_extern:
      handleExtern();
      break;
    default:
      handleTopLevelExpression();
      break;
    }
  }
}
