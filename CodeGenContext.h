#pragma once

#include "llvm/Analysis/CGSCCPassManager.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/StandardInstrumentations.h"

#include <map>
#include <memory>
#include <string>

// Owns the LLVM state shared across a codegen() walk of the AST: the
// context, the module IR is emitted into, the IR builder, the table of
// currently-in-scope named values (e.g. a function's arguments), and the
// function-level optimization pipeline run on each function right after
// it's generated. Each AST node's codegen(CodeGenContext &) reads/writes
// this instead of reaching into globals.
class CodeGenContext {
public:
  explicit CodeGenContext(const std::string &ModuleName);

  llvm::LLVMContext &getContext() { return *TheContext; }
  llvm::Module &getModule() { return *TheModule; }
  llvm::IRBuilder<> &getBuilder() { return *Builder; }
  std::map<std::string, llvm::Value *> &getNamedValues() { return NamedValues; }

  llvm::FunctionPassManager &getFPM() { return *TheFPM; }
  llvm::FunctionAnalysisManager &getFAM() { return *TheFAM; }

private:
  std::unique_ptr<llvm::LLVMContext> TheContext;
  std::unique_ptr<llvm::Module> TheModule;
  std::unique_ptr<llvm::IRBuilder<>> Builder;
  std::map<std::string, llvm::Value *> NamedValues;

  // Per-function optimization pipeline (InstCombine, Reassociate, GVN,
  // SimplifyCFG) plus the analysis managers/registration it needs.
  std::unique_ptr<llvm::FunctionPassManager> TheFPM;
  std::unique_ptr<llvm::LoopAnalysisManager> TheLAM;
  std::unique_ptr<llvm::FunctionAnalysisManager> TheFAM;
  std::unique_ptr<llvm::CGSCCAnalysisManager> TheCGAM;
  std::unique_ptr<llvm::ModuleAnalysisManager> TheMAM;
  std::unique_ptr<llvm::PassInstrumentationCallbacks> ThePIC;
  std::unique_ptr<llvm::StandardInstrumentations> TheSI;
};
