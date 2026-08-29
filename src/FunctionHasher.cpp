#include "FunctionHasher.h"

#include "AST.h"

#include "picosha2.h"

std::string hashFunction(const FunctionAST &Fn) {
  return picosha2::hash256_hex_string(Fn.canonicalize());
}
