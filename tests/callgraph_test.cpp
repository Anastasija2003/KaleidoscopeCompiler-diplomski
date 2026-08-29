#include <catch2/catch_test_macros.hpp>

#include "AST.h"
#include "CallGraph.h"

#include <algorithm>
#include <memory>
#include <string>
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

std::vector<std::unique_ptr<ExprAST>> args1(std::unique_ptr<ExprAST> A) {
  std::vector<std::unique_ptr<ExprAST>> V;
  V.push_back(std::move(A));
  return V;
}

std::unique_ptr<ExprAST> call(std::string Callee,
                               std::vector<std::unique_ptr<ExprAST>> Args) {
  return std::make_unique<CallExprAST>(Loc(), std::move(Callee), std::move(Args));
}

std::unique_ptr<FunctionAST> fn(std::string Name, std::vector<std::string> Args,
                                 std::unique_ptr<ExprAST> Body) {
  auto Proto =
      std::make_unique<PrototypeAST>(Loc(), std::move(Name), std::move(Args));
  return std::make_unique<FunctionAST>(std::move(Proto), std::move(Body));
}

} // namespace

TEST_CASE("leaf function has no callees", "[callgraph]") {
  auto Fn = fn("leaf", {"x"}, bin('+', var("x"), num(1.0)));
  REQUIRE(Fn->collectCallees().empty());
}

TEST_CASE("self-recursive function calls itself", "[callgraph]") {
  auto Fn = fn("fib", {"n"}, call("fib", args1(var("n"))));
  auto Callees = Fn->collectCallees();
  REQUIRE(Callees == std::vector<std::string>{"fib"});
}

TEST_CASE("buildCallGraph captures mutual recursion", "[callgraph]") {
  auto A = fn("a", {"n"}, call("b", args1(var("n"))));
  auto B = fn("b", {"n"}, call("a", args1(var("n"))));

  std::vector<const FunctionAST *> Fns = {A.get(), B.get()};
  CallGraph Graph = buildCallGraph(Fns);

  REQUIRE(Graph["a"] == std::vector<std::string>{"b"});
  REQUIRE(Graph["b"] == std::vector<std::string>{"a"});
}

TEST_CASE("calls nested inside call arguments are found too", "[callgraph]") {
  auto Fn = fn("outer", {"x"}, call("f", args1(call("g", args1(var("x"))))));
  auto Callees = Fn->collectCallees();

  REQUIRE(Callees.size() == 2);
  REQUIRE(std::find(Callees.begin(), Callees.end(), "f") != Callees.end());
  REQUIRE(std::find(Callees.begin(), Callees.end(), "g") != Callees.end());
}

TEST_CASE("buildCallGraph deduplicates repeated calls to the same callee",
          "[callgraph]") {
  auto Fn = fn("twice", {"x"},
               bin('+', call("f", args1(var("x"))), call("f", args1(var("x")))));

  std::vector<const FunctionAST *> Fns = {Fn.get()};
  CallGraph Graph = buildCallGraph(Fns);

  REQUIRE(Graph["twice"] == std::vector<std::string>{"f"});
}
