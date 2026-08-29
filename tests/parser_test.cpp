#include <catch2/catch_test_macros.hpp>

#include "CodeGenContext.h"
#include "Lexer.h"
#include "Parser.h"

#include "llvm/IR/DataLayout.h"

#include <sstream>

TEST_CASE("parseProgram collects multiple definitions in order", "[parser]") {
  std::istringstream In("def foo(x) x+1\ndef bar(y) foo(y)*2");
  Lexer Lex(In);
  CodeGenContext CG("test", llvm::DataLayout());
  Parser P(Lex, CG);

  ParsedProgram Program = P.parseProgram();

  REQUIRE(Program.Functions.size() == 2);
  REQUIRE(Program.Externs.empty());
  REQUIRE(Program.Functions[0]->getName() == "foo");
  REQUIRE(Program.Functions[1]->getName() == "bar");
}

TEST_CASE("parseProgram separates externs from definitions", "[parser]") {
  std::istringstream In("extern sin(x)\ndef foo(x) sin(x)");
  Lexer Lex(In);
  CodeGenContext CG("test", llvm::DataLayout());
  Parser P(Lex, CG);

  ParsedProgram Program = P.parseProgram();

  REQUIRE(Program.Externs.size() == 1);
  REQUIRE(Program.Externs[0]->getName() == "sin");
  REQUIRE(Program.Functions.size() == 1);
  REQUIRE(Program.Functions[0]->getName() == "foo");
}

TEST_CASE("parseProgram wraps a top-level expression as __anon_expr",
          "[parser]") {
  std::istringstream In("1 + 2");
  Lexer Lex(In);
  CodeGenContext CG("test", llvm::DataLayout());
  Parser P(Lex, CG);

  ParsedProgram Program = P.parseProgram();

  REQUIRE(Program.Functions.size() == 1);
  REQUIRE(Program.Functions[0]->getName() == "__anon_expr");
}

TEST_CASE("parseProgram never invokes codegen", "[parser]") {
  std::istringstream In("def foo(x) x+1");
  Lexer Lex(In);
  CodeGenContext CG("test", llvm::DataLayout());
  Parser P(Lex, CG);

  P.parseProgram();

  REQUIRE(CG.getModule().getFunction("foo") == nullptr);
}
