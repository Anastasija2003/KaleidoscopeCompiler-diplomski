#include "AST.h"

#include <cstdint>
#include <cstring>
#include <optional>
#include <sstream>

static std::string canonNumber(double Val) {
  uint64_t Bits;
  std::memcpy(&Bits, &Val, sizeof(Bits));
  std::ostringstream OS;
  OS << std::hex << Bits;
  return "(num " + OS.str() + ")";
}

std::string NumberExprAST::canonicalize(CanonState &) const {
  return canonNumber(Val);
}

std::string VariableExprAST::canonicalize(CanonState &State) const {
  auto It = State.Env.find(Name);
  return "(var " + (It != State.Env.end() ? It->second : Name) + ")";
}

std::string UnaryExprAST::canonicalize(CanonState &State) const {
  return std::string("(unary ") + Opcode + " " + Operand->canonicalize(State) +
         ")";
}

std::string BinaryExprAST::canonicalize(CanonState &State) const {
  return std::string("(binop ") + Op + " " + LHS->canonicalize(State) + " " +
         RHS->canonicalize(State) + ")";
}

std::string CallExprAST::canonicalize(CanonState &State) const {
  std::string Result = "(call " + Callee;
  for (auto &Arg : Args)
    Result += " " + Arg->canonicalize(State);
  Result += ")";
  return Result;
}

std::string IfExprAST::canonicalize(CanonState &State) const {
  return "(if " + Cond->canonicalize(State) + " " + Then->canonicalize(State) +
         " " + Else->canonicalize(State) + ")";
}

std::string ForExprAST::canonicalize(CanonState &State) const {
  std::string StartStr = Start->canonicalize(State);

  std::string NewName = "local" + std::to_string(State.NextLocal++);
  auto OldIt = State.Env.find(VarName);
  std::optional<std::string> OldVal =
      OldIt != State.Env.end() ? std::optional<std::string>(OldIt->second)
                                : std::nullopt;
  State.Env[VarName] = NewName;

  std::string EndStr = End->canonicalize(State);
  std::string StepStr = Step ? Step->canonicalize(State) : "none";
  std::string BodyStr = Body->canonicalize(State);

  if (OldVal)
    State.Env[VarName] = *OldVal;
  else
    State.Env.erase(VarName);

  return "(for " + NewName + " " + StartStr + " " + EndStr + " " + StepStr +
         " " + BodyStr + ")";
}

std::string VarExprAST::canonicalize(CanonState &State) const {
  std::string Result = "(var";
  std::vector<std::pair<std::string, std::optional<std::string>>> OldBindings;

  for (auto &Binding : VarNames) {
    const std::string &Name = Binding.first;
    ExprAST *Init = Binding.second.get();
    std::string InitStr = Init ? Init->canonicalize(State) : canonNumber(0.0);

    std::string NewName = "local" + std::to_string(State.NextLocal++);
    Result += " (" + NewName + " " + InitStr + ")";

    auto It = State.Env.find(Name);
    OldBindings.emplace_back(
        Name, It != State.Env.end() ? std::optional<std::string>(It->second)
                                     : std::nullopt);
    State.Env[Name] = NewName;
  }

  Result += " " + Body->canonicalize(State) + ")";

  for (auto &OB : OldBindings) {
    if (OB.second)
      State.Env[OB.first] = *OB.second;
    else
      State.Env.erase(OB.first);
  }

  return Result;
}

std::string PrototypeAST::canonicalize(CanonState &State) const {
  std::ostringstream OS;
  OS << Name << " " << (IsOperator ? 1 : 0) << " " << Precedence << " "
     << Args.size();

  for (std::size_t i = 0; i < Args.size(); ++i)
    State.Env[Args[i]] = "arg" + std::to_string(i);

  return OS.str();
}

std::string FunctionAST::canonicalize() const {
  CanonState State;
  std::string ProtoStr = Proto->canonicalize(State);
  std::string BodyStr = Body->canonicalize(State);
  return "(def " + ProtoStr + " " + BodyStr + ")";
}
