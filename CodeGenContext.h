#pragma once

#include "AST.h"

#include "llvm/Analysis/CGSCCPassManager.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/IR/DataLayout.h"
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
// currently-in-scope named values (e.g. a function's arguments), the
// function-level optimization pipeline run on each function right after
// it's generated, and the registry of parsed-but-not-yet-(re)materialized
// prototypes (FunctionProtos) that survives across module swaps. Each AST
// node's codegen(CodeGenContext &) reads/writes this instead of reaching
// into globals.
class CodeGenContext {
public:
  CodeGenContext(std::string ModuleName, const llvm::DataLayout &DL);

  llvm::LLVMContext &getContext() { return *TheContext; }
  llvm::Module &getModule() { return *TheModule; }
  llvm::IRBuilder<> &getBuilder() { return *Builder; }
  std::map<std::string, llvm::Value *> &getNamedValues() { return NamedValues; }

  // Prototypes of every function seen so far (from a 'def' or an
  // 'extern'), keyed by name. Kept around after their defining module has
  // been handed off to the JIT so a later module can re-declare (and, for
  // 'def', redefine) the function on demand -- see getFunction() in
  // CodeGen.cpp.
  std::map<std::string, std::unique_ptr<PrototypeAST>> &getFunctionProtos() {
    return FunctionProtos;
  }

  llvm::FunctionPassManager &getFPM() { return *TheFPM; }
  llvm::FunctionAnalysisManager &getFAM() { return *TheFAM; }

  // Hands ownership of the current context+module to the caller, bundled
  // the way ORC requires (a context and the module(s) that live in it
  // travel together). Must be followed by resetModule() before this
  // CodeGenContext is used for codegen again.
  llvm::orc::ThreadSafeModule takeModule();

  // Starts a fresh context+module+optimization pipeline (the old pipeline
  // is tied to the old context and can't outlive it). FunctionProtos is
  // untouched -- it's what lets code generated into the new module call
  // functions that were defined in a previous one.
  void resetModule(const llvm::DataLayout &DL);

private:
  std::string ModuleName;

  std::unique_ptr<llvm::LLVMContext> TheContext;
  std::unique_ptr<llvm::Module> TheModule;
  std::unique_ptr<llvm::IRBuilder<>> Builder;
  std::map<std::string, llvm::Value *> NamedValues;
  std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;

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
