#include "Parser.h"

#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"

#include <cstdio>

using namespace llvm::orc;

Parser::Parser(Lexer &Lex, CodeGenContext &CG, KaleidoscopeJIT *JIT)
    : Lex(Lex), CG(CG), JIT(JIT) {}

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

// numberexpr ::= number
std::unique_ptr<ExprAST> Parser::parseNumberExpr() {
  auto Result = std::make_unique<NumberExprAST>(Lex.getLoc(), Lex.getNumVal());
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
  SourceLocation LitLoc = Lex.getLoc();

  getNextToken(); // eat identifier

  if (CurTok != '(') // Simple variable ref.
    return std::make_unique<VariableExprAST>(LitLoc, IdName);

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
  return std::make_unique<CallExprAST>(LitLoc, IdName, std::move(Args));
}

// ifexpr ::= 'if' expression 'then' expression 'else' expression
std::unique_ptr<ExprAST> Parser::parseIfExpr() {
  SourceLocation IfLoc = Lex.getLoc();
  getNextToken(); // eat if

  auto Cond = parseExpression();
  if (!Cond)
    return nullptr;

  if (CurTok != tok_then)
    return logError("expected then");
  getNextToken(); // eat then

  auto Then = parseExpression();
  if (!Then)
    return nullptr;

  if (CurTok != tok_else)
    return logError("expected else");
  getNextToken(); // eat else

  auto Else = parseExpression();
  if (!Else)
    return nullptr;

  return std::make_unique<IfExprAST>(IfLoc, std::move(Cond), std::move(Then),
                                      std::move(Else));
}

// forexpr ::= 'for' identifier '=' expr ',' expr (',' expr)? 'in' expression
std::unique_ptr<ExprAST> Parser::parseForExpr() {
  SourceLocation ForLoc = Lex.getLoc();
  getNextToken(); // eat for

  if (CurTok != tok_identifier)
    return logError("expected identifier after for");

  std::string IdName = Lex.getIdentifier();
  getNextToken(); // eat identifier

  if (CurTok != '=')
    return logError("expected '=' after for");
  getNextToken(); // eat '='

  auto Start = parseExpression();
  if (!Start)
    return nullptr;
  if (CurTok != ',')
    return logError("expected ',' after for start value");
  getNextToken();

  auto End = parseExpression();
  if (!End)
    return nullptr;

  // The step value is optional.
  std::unique_ptr<ExprAST> Step;
  if (CurTok == ',') {
    getNextToken();
    Step = parseExpression();
    if (!Step)
      return nullptr;
  }

  if (CurTok != tok_in)
    return logError("expected 'in' after for");
  getNextToken(); // eat 'in'

  auto Body = parseExpression();
  if (!Body)
    return nullptr;

  return std::make_unique<ForExprAST>(ForLoc, IdName, std::move(Start),
                                       std::move(End), std::move(Step),
                                       std::move(Body));
}

// varexpr ::= 'var' identifier ('=' expression)?
//                    (',' identifier ('=' expression)?)* 'in' expression
std::unique_ptr<ExprAST> Parser::parseVarExpr() {
  SourceLocation VarLoc = Lex.getLoc();
  getNextToken(); // eat var

  std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;

  // At least one variable name is required.
  if (CurTok != tok_identifier)
    return logError("expected identifier after var");

  while (true) {
    std::string Name = Lex.getIdentifier();
    getNextToken(); // eat identifier

    // Read the optional initializer.
    std::unique_ptr<ExprAST> Init;
    if (CurTok == '=') {
      getNextToken(); // eat '='
      Init = parseExpression();
      if (!Init)
        return nullptr;
    }

    VarNames.emplace_back(std::move(Name), std::move(Init));

    if (CurTok != ',')
      break;
    getNextToken(); // eat ','

    if (CurTok != tok_identifier)
      return logError("expected identifier list after var");
  }

  if (CurTok != tok_in)
    return logError("expected 'in' keyword after 'var'");
  getNextToken(); // eat 'in'

  auto Body = parseExpression();
  if (!Body)
    return nullptr;

  return std::make_unique<VarExprAST>(VarLoc, std::move(VarNames),
                                       std::move(Body));
}

// primary
//   ::= identifierexpr | numberexpr | parenexpr | ifexpr | forexpr | varexpr
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

// unary
//   ::= primary
//   ::= '<op>' unary
std::unique_ptr<ExprAST> Parser::parseUnary() {
  // Not an operator character, or the start of a parenthesized/argument
  // expression -- must be an ordinary primary expression.
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

// binoprhs ::= ('+' unary)*
std::unique_ptr<ExprAST> Parser::parseBinOpRHS(int ExprPrec,
                                                std::unique_ptr<ExprAST> LHS) {
  while (true) {
    int TokPrec = getTokPrecedence();

    // If this binop binds at least as tightly as the current one, consume
    // it; otherwise we're done.
    if (TokPrec < ExprPrec)
      return LHS;

    int BinOp = CurTok;
    SourceLocation BinLoc = Lex.getLoc();
    getNextToken(); // eat binop

    auto RHS = parseUnary();
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

    LHS = std::make_unique<BinaryExprAST>(BinLoc, static_cast<char>(BinOp),
                                           std::move(LHS), std::move(RHS));
  }
}

// expression ::= unary binoprhs
std::unique_ptr<ExprAST> Parser::parseExpression() {
  auto LHS = parseUnary();
  if (!LHS)
    return nullptr;

  return parseBinOpRHS(0, std::move(LHS));
}

// prototype
//   ::= id '(' id* ')'
//   ::= 'binary' LETTER number? '(' id id ')'
//   ::= 'unary' LETTER '(' id ')'
std::unique_ptr<PrototypeAST> Parser::parsePrototype() {
  std::string FnName;
  SourceLocation FnLoc = Lex.getLoc();

  // 0 = ordinary function, 1 = unary operator, 2 = binary operator. Also
  // doubles as the required argument count for the operator cases, so it
  // can be checked against ArgNames.size() below.
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

    // Read the precedence, if present.
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

  getNextToken(); // eat )

  if (Kind && ArgNames.size() != Kind)
    return logErrorP("Invalid number of operands for operator");

  return std::make_unique<PrototypeAST>(FnLoc, FnName, std::move(ArgNames),
                                         Kind != 0, BinaryPrecedence);
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
  SourceLocation FnLoc = Lex.getLoc();
  if (auto E = parseExpression()) {
    // Wrap it in an anonymous no-argument function. Kept as "__anon_expr"
    // (not renamed to "main", unlike the upstream debug-info tutorial)
    // since main.cpp's JIT mode looks it up by that exact name.
    auto Proto = std::make_unique<PrototypeAST>(FnLoc, "__anon_expr",
                                                 std::vector<std::string>());
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
  if (auto FnAST = parseDefinition()) {
    if (auto *FnIR = FnAST->codegen(CG)) {
      fprintf(stderr, "Read function definition:");
      FnIR->print(llvm::errs());
      fprintf(stderr, "\n");

      if (JIT) {
        // Hand the finished module off to the JIT, then start a fresh
        // one -- this is what lets the *next* def/extern/expression
        // redeclare or even redefine names from this one (see
        // getFunction() in CodeGen.cpp).
        ExitOnErr(JIT->addModule(CG.takeModule()));
        CG.resetModule(JIT->getDataLayout());
      }
      // No JIT (static/object-file compile): leave the function where it
      // is -- it stays in CG's one running module until the whole input
      // has been read and the caller emits that module to an object file.
    }
  } else {
    getNextToken(); // skip token for error recovery
  }
}

void Parser::handleExtern() {
  if (auto ProtoAST = parseExtern()) {
    if (auto *FnIR = ProtoAST->codegen(CG)) {
      fprintf(stderr, "Read extern: ");
      FnIR->print(llvm::errs());
      fprintf(stderr, "\n");
      // Just a declaration -- nothing to hand to the JIT yet. Keep the
      // prototype around so a later call to this name can be resolved.
      CG.getFunctionProtos()[ProtoAST->getName()] = std::move(ProtoAST);
    }
  } else {
    getNextToken();
  }
}

void Parser::handleTopLevelExpression() {
  // Evaluate a top-level expression into an anonymous function.
  if (auto FnAST = parseTopLevelExpr()) {
    if (!FnAST->codegen(CG))
      return;

    if (!JIT) {
      // Static/object-file compile: nothing to execute -- the compiled
      // '__anon_expr' function just stays in the module like any other.
      return;
    }

    // JIT it, run it, and print the result -- no IR is printed for this
    // case. Track this module's JIT'd memory separately so it can be
    // freed right after we're done calling it (it's anonymous and
    // one-shot).
    auto RT = JIT->getMainJITDylib().createResourceTracker();

    ExitOnErr(JIT->addModule(CG.takeModule(), RT));
    CG.resetModule(JIT->getDataLayout());

    auto ExprSymbol = ExitOnErr(JIT->lookup("__anon_expr"));

    double (*FP)() = ExprSymbol.toPtr<double (*)()>();
    fprintf(stderr, "Evaluated to %f\n", FP());

    ExitOnErr(RT->remove());
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
