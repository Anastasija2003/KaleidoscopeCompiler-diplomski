#include <catch2/catch_test_macros.hpp>

#include "AST.h"
#include "FunctionHasher.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

SourceLocation Loc() { return SourceLocation{1, 0}; }

std::unique_ptr<ExprAST> num(double V) {
  return std::make_unique<NumberExprAST>(Loc(), V);
}

std::unique_ptr<ExprAST> var(std::string Name) {
  return std::make_unique<VariableExprAST>(Loc(), std::move(Name));
}

std::unique_ptr<ExprAST> bin(char Op, std::unique_ptr<ExprAST> L,
                              std::unique_ptr<ExprAST> R) {
  return std::make_unique<BinaryExprAST>(Loc(), Op, std::move(L), std::move(R));
}

std::unique_ptr<FunctionAST> fn(std::string Name, std::vector<std::string> Args,
                                 std::unique_ptr<ExprAST> Body) {
  auto Proto =
      std::make_unique<PrototypeAST>(Loc(), std::move(Name), std::move(Args));
  return std::make_unique<FunctionAST>(std::move(Proto), std::move(Body));
}

} // namespace

TEST_CASE("canonicalize is stable across identical ASTs", "[canonicalize]") {
  auto Fn1 = fn("foo", {"x", "y"}, bin('+', var("x"), var("y")));
  auto Fn2 = fn("foo", {"x", "y"}, bin('+', var("x"), var("y")));

  REQUIRE(Fn1->canonicalize() == Fn2->canonicalize());
}

TEST_CASE("canonicalize is alpha-equivalent under parameter renaming",
          "[canonicalize]") {
  auto Fn1 = fn("foo", {"x", "y"}, bin('+', var("x"), var("y")));
  auto Fn2 = fn("foo", {"a", "b"}, bin('+', var("a"), var("b")));

  REQUIRE(Fn1->canonicalize() == Fn2->canonicalize());
}

TEST_CASE("canonicalize distinguishes structurally different bodies",
          "[canonicalize]") {
  auto Plus = fn("foo", {"x", "y"}, bin('+', var("x"), var("y")));
  auto Minus = fn("foo", {"x", "y"}, bin('-', var("x"), var("y")));

  REQUIRE(Plus->canonicalize() != Minus->canonicalize());
}

TEST_CASE("canonicalize distinguishes different precedence for an operator "
          "with the same body",
          "[canonicalize]") {
  SourceLocation L = Loc();
  std::vector<std::string> Args = {"a", "b"};

  auto Proto1 = std::make_unique<PrototypeAST>(L, "binary>", Args, true, 10);
  auto Fn1 = std::make_unique<FunctionAST>(std::move(Proto1), var("a"));

  auto Proto2 = std::make_unique<PrototypeAST>(L, "binary>", Args, true, 20);
  auto Fn2 = std::make_unique<FunctionAST>(std::move(Proto2), var("a"));

  REQUIRE(Fn1->canonicalize() != Fn2->canonicalize());
}

TEST_CASE("canonicalize correctly restores shadowed bindings after nested var",
          "[canonicalize]") {
  using VarBinding = std::pair<std::string, std::unique_ptr<ExprAST>>;

  auto Build = [](const std::string &OuterName, const std::string &InnerName) {
    std::vector<VarBinding> Inner;
    Inner.emplace_back(InnerName, num(2.0));
    auto InnerVar =
        std::make_unique<VarExprAST>(Loc(), std::move(Inner), var(InnerName));

    auto Sum = bin('+', std::move(InnerVar), var(OuterName));

    std::vector<VarBinding> Outer;
    Outer.emplace_back(OuterName, num(1.0));
    auto OuterVar =
        std::make_unique<VarExprAST>(Loc(), std::move(Outer), std::move(Sum));

    return fn("foo", {}, std::move(OuterVar));
  };

  auto Shadowed = Build("x", "x");
  auto NotShadowed = Build("p", "q");

  REQUIRE(Shadowed->canonicalize() == NotShadowed->canonicalize());
}

TEST_CASE("for-loop start does not see the induction variable, "
          "end/step/body do",
          "[canonicalize]") {
  auto ForLoop = std::make_unique<ForExprAST>(
      Loc(), "i", var("i"), bin('<', var("i"), num(10.0)), num(1.0), var("i"));
  auto Fn = fn("foo", {}, std::move(ForLoop));

  std::string Canon = Fn->canonicalize();

  REQUIRE(Canon.find("(var i)") != std::string::npos);
  REQUIRE(Canon.find("(var local0)") != std::string::npos);
}

TEST_CASE("hashFunction is stable and alpha-equivalence-aware", "[hash]") {
  auto Fn1 = fn("foo", {"x", "y"}, bin('+', var("x"), var("y")));
  auto Fn2 = fn("foo", {"a", "b"}, bin('+', var("a"), var("b")));

  REQUIRE(hashFunction(*Fn1) == hashFunction(*Fn2));
}

TEST_CASE("hashFunction distinguishes different bodies", "[hash]") {
  auto Plus = fn("foo", {"x", "y"}, bin('+', var("x"), var("y")));
  auto Minus = fn("foo", {"x", "y"}, bin('-', var("x"), var("y")));

  REQUIRE(hashFunction(*Plus) != hashFunction(*Minus));
}
