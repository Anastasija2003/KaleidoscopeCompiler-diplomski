#pragma once

#include "AST.h"

#include <memory>
#include <vector>

struct ParsedProgram {
  std::vector<std::unique_ptr<FunctionAST>> Functions;
  std::vector<std::unique_ptr<PrototypeAST>> Externs;
};
