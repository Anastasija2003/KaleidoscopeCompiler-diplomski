#include "Parser.h"

#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"

#include <cstdio>

Parser::Parser(Lexer &Lex, CodeGenContext &CG, ModuleSink *Sink)
    : Lex(Lex), CG(CG), Sink(Sink) {}

int Parser::getNextToken() { return CurTok = Lex.getTok(); }

int Parser::getTokPrecedence() const {
  if (CurTok < 0 || CurTok > 255)
    return -1;

  auto &BinopPrecedence = CG.getBinopPrecedence();
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

std::unique_ptr<ExprAST> Parser::parseNumberExpr() {
  auto Result = std::make_unique<NumberExprAST>(Lex.getLoc(), Lex.getNumVal());
  getNextToken();
  return Result;
}

std::unique_ptr<ExprAST> Parser::parseParenExpr() {
  getNextToken();
  auto V = parseExpression();
  if (!V)
    return nullptr;

  if (CurTok != ')')
    return logError("expected ')'");
  getNextToken();
  return V;
}

std::unique_ptr<ExprAST> Parser::parseIdentifierExpr() {
  std::string IdName = Lex.getIdentifier();
  SourceLocation LitLoc = Lex.getLoc();

  getNextToken();

  if (CurTok != '(')
    return std::make_unique<VariableExprAST>(LitLoc, IdName);

  getNextToken();
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

  getNextToken();
  return std::make_unique<CallExprAST>(LitLoc, IdName, std::move(Args));
}

std::unique_ptr<ExprAST> Parser::parseIfExpr() {
  SourceLocation IfLoc = Lex.getLoc();
  getNextToken();

  auto Cond = parseExpression();
  if (!Cond)
    return nullptr;

  if (CurTok != tok_then)
    return logError("expected then");
  getNextToken();

  auto Then = parseExpression();
  if (!Then)
    return nullptr;

  if (CurTok != tok_else)
    return logError("expected else");
  getNextToken();

  auto Else = parseExpression();
  if (!Else)
    return nullptr;

  return std::make_unique<IfExprAST>(IfLoc, std::move(Cond), std::move(Then),
                                      std::move(Else));
}

std::unique_ptr<ExprAST> Parser::parseForExpr() {
  SourceLocation ForLoc = Lex.getLoc();
  getNextToken();

  if (CurTok != tok_identifier)
    return logError("expected identifier after for");

  std::string IdName = Lex.getIdentifier();
  getNextToken();

  if (CurTok != '=')
    return logError("expected '=' after for");
  getNextToken();

  auto Start = parseExpression();
  if (!Start)
    return nullptr;
  if (CurTok != ',')
    return logError("expected ',' after for start value");
  getNextToken();

  auto End = parseExpression();
  if (!End)
    return nullptr;

  std::unique_ptr<ExprAST> Step;
  if (CurTok == ',') {
    getNextToken();
    Step = parseExpression();
    if (!Step)
      return nullptr;
  }

  if (CurTok != tok_in)
    return logError("expected 'in' after for");
  getNextToken();

  auto Body = parseExpression();
  if (!Body)
    return nullptr;

  return std::make_unique<ForExprAST>(ForLoc, IdName, std::move(Start),
                                       std::move(End), std::move(Step),
                                       std::move(Body));
}

std::unique_ptr<ExprAST> Parser::parseVarExpr() {
  SourceLocation VarLoc = Lex.getLoc();
  getNextToken();

  std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;

  if (CurTok != tok_identifier)
    return logError("expected identifier after var");

  while (true) {
    std::string Name = Lex.getIdentifier();
    getNextToken();

    std::unique_ptr<ExprAST> Init;
    if (CurTok == '=') {
      getNextToken();
      Init = parseExpression();
      if (!Init)
        return nullptr;
    }

    VarNames.emplace_back(std::move(Name), std::move(Init));

    if (CurTok != ',')
      break;
    getNextToken();

    if (CurTok != tok_identifier)
      return logError("expected identifier list after var");
  }

  if (CurTok != tok_in)
    return logError("expected 'in' keyword after 'var'");
  getNextToken();

  auto Body = parseExpression();
  if (!Body)
    return nullptr;

  return std::make_unique<VarExprAST>(VarLoc, std::move(VarNames),
                                       std::move(Body));
}

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
  case tok_if:
    return parseIfExpr();
  case tok_for:
    return parseForExpr();
  case tok_var:
    return parseVarExpr();
  }
}

std::unique_ptr<ExprAST> Parser::parseUnary() {
  if ((CurTok < 0 || CurTok > 255) || CurTok == '(' || CurTok == ',')
    return parsePrimary();

  SourceLocation OpLoc = Lex.getLoc();
  int Opc = CurTok;
  getNextToken();
  if (auto Operand = parseUnary())
    return std::make_unique<UnaryExprAST>(OpLoc, static_cast<char>(Opc),
                                           std::move(Operand));
  return nullptr;
}

std::unique_ptr<ExprAST> Parser::parseBinOpRHS(int ExprPrec,
                                                std::unique_ptr<ExprAST> LHS) {
  while (true) {
    int TokPrec = getTokPrecedence();

    if (TokPrec < ExprPrec)
      return LHS;

    int BinOp = CurTok;
    SourceLocation BinLoc = Lex.getLoc();
    getNextToken();

    auto RHS = parseUnary();
    if (!RHS)
      return nullptr;

    int NextPrec = getTokPrecedence();
    if (TokPrec < NextPrec) {
      RHS = parseBinOpRHS(TokPrec + 1, std::move(RHS));
      if (!RHS)
        return nullptr;
    }

    LHS = std::make_unique<BinaryExprAST>(BinLoc, static_cast<char>(BinOp),
                                           std::move(LHS), std::move(RHS));
  }
}

std::unique_ptr<ExprAST> Parser::parseExpression() {
  auto LHS = parseUnary();
  if (!LHS)
    return nullptr;

  return parseBinOpRHS(0, std::move(LHS));
}

std::unique_ptr<PrototypeAST> Parser::parsePrototype() {
  std::string FnName;
  SourceLocation FnLoc = Lex.getLoc();

  unsigned Kind = 0;
  unsigned BinaryPrecedence = 30;

  switch (CurTok) {
  default:
    return logErrorP("Expected function name in prototype");
  case tok_identifier:
    FnName = Lex.getIdentifier();
    Kind = 0;
    getNextToken();
    break;
  case tok_unary:
    getNextToken();
    if (CurTok < 0 || CurTok > 255)
      return logErrorP("Expected unary operator");
    FnName = "unary";
    FnName += static_cast<char>(CurTok);
    Kind = 1;
    getNextToken();
    break;
  case tok_binary:
    getNextToken();
    if (CurTok < 0 || CurTok > 255)
      return logErrorP("Expected binary operator");
    FnName = "binary";
    FnName += static_cast<char>(CurTok);
    Kind = 2;
    getNextToken();

    if (CurTok == tok_number) {
      if (Lex.getNumVal() < 1 || Lex.getNumVal() > 100)
        return logErrorP("Invalid precedence: must be 1..100");
      BinaryPrecedence = static_cast<unsigned>(Lex.getNumVal());
      getNextToken();
    }
    break;
  }

  if (CurTok != '(')
    return logErrorP("Expected '(' in prototype");

  std::vector<std::string> ArgNames;
  while (getNextToken() == tok_identifier)
    ArgNames.push_back(Lex.getIdentifier());
  if (CurTok != ')')
    return logErrorP("Expected ')' in prototype");

  getNextToken();

  if (Kind && ArgNames.size() != Kind)
    return logErrorP("Invalid number of operands for operator");

  return std::make_unique<PrototypeAST>(FnLoc, FnName, std::move(ArgNames),
                                         Kind != 0, BinaryPrecedence);
}

std::unique_ptr<FunctionAST> Parser::parseDefinition() {
  getNextToken();
  auto Proto = parsePrototype();
  if (!Proto)
    return nullptr;

  if (auto E = parseExpression())
    return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
  return nullptr;
}

std::unique_ptr<FunctionAST> Parser::parseTopLevelExpr() {
  SourceLocation FnLoc = Lex.getLoc();
  if (auto E = parseExpression()) {
    auto Proto = std::make_unique<PrototypeAST>(FnLoc, "__anon_expr",
                                                 std::vector<std::string>());
    return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
  }
  return nullptr;
}

std::unique_ptr<PrototypeAST> Parser::parseExtern() {
  getNextToken();
  return parsePrototype();
}

void Parser::handleDefinition() {
  if (auto FnAST = parseDefinition()) {
    if (auto *FnIR = FnAST->codegen(CG)) {
      fprintf(stderr, "Read function definition:");
      FnIR->print(llvm::errs());
      fprintf(stderr, "\n");

      if (Sink)
        Sink->afterDefinition(CG);
    }
  } else {
    getNextToken();
  }
}

void Parser::handleExtern() {
  if (auto ProtoAST = parseExtern()) {
    if (auto *FnIR = ProtoAST->codegen(CG)) {
      fprintf(stderr, "Read extern: ");
      FnIR->print(llvm::errs());
      fprintf(stderr, "\n");
      CG.getFunctionProtos()[ProtoAST->getName()] = std::move(ProtoAST);
    }
  } else {
    getNextToken();
  }
}

void Parser::handleTopLevelExpression() {
  if (auto FnAST = parseTopLevelExpr()) {
    if (!FnAST->codegen(CG))
      return;

    if (Sink)
      Sink->afterTopLevelExpr(CG);
  } else {
    getNextToken();
  }
}

void Parser::run() {
  fprintf(stderr, "ready> ");
  getNextToken();

  while (true) {
    fprintf(stderr, "ready> ");
    switch (CurTok) {
    case tok_eof:
      return;
    case ';':
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

ParsedProgram Parser::parseProgram() {
  ParsedProgram Result;
  getNextToken();

  while (true) {
    switch (CurTok) {
    case tok_eof:
      return Result;
    case ';':
      getNextToken();
      break;
    case tok_def:
      if (auto Fn = parseDefinition())
        Result.Functions.push_back(std::move(Fn));
      else
        getNextToken();
      break;
    case tok_extern:
      if (auto Proto = parseExtern())
        Result.Externs.push_back(std::move(Proto));
      else
        getNextToken();
      break;
    default:
      if (auto Fn = parseTopLevelExpr())
        Result.Functions.push_back(std::move(Fn));
      else
        getNextToken();
      break;
    }
  }
}
