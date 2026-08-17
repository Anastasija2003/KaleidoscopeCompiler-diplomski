#pragma once

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

#include <map>
#include <memory>
#include <string>

// Owns the LLVM state shared across a codegen() walk of the AST: the
// context, the module IR is emitted into, the IR builder, and the table
// of currently-in-scope named values (e.g. a function's arguments).
// Each AST node's codegen(CodeGenContext &) reads/writes this instead of
// reaching into globals.
class CodeGenContext {
public:
  explicit CodeGenContext(const std::string &ModuleName);

  llvm::LLVMContext &getContext() { return *TheContext; }
  llvm::Module &getModule() { return *TheModule; }
  llvm::IRBuilder<> &getBuilder() { return *Builder; }
  std::map<std::string, llvm::Value *> &getNamedValues() { return NamedValues; }

private:
  std::unique_ptr<llvm::LLVMContext> TheContext;
  std::unique_ptr<llvm::Module> TheModule;
  std::unique_ptr<llvm::IRBuilder<>> Builder;
  std::map<std::string, llvm::Value *> NamedValues;
};
