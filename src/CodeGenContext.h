#pragma once

#include "AST.h"

#include "llvm/Analysis/CGSCCPassManager.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/IR/DIBuilder.h"
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

  // Currently-in-scope local variables, keyed by name. Each one is a
  // pointer to a stack slot (an 'alloca') rather than a Value directly --
  // reading a variable is a load from this slot, writing one (var/in's
  // initializer aside, and the '=' operator) is a store to it. The
  // Mem2Reg pass in the optimization pipeline below is what turns these
  // back into ordinary SSA registers when a slot is never really needed
  // (i.e. whenever the "variable" is never reassigned).
  std::map<std::string, llvm::AllocaInst *> &getNamedValues() {
    return NamedValues;
  }

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

  // Runs real interprocedural inlining over the *entire* current module in
  // one pass (plan.md 4.5). Unlike getFPM(), which runs per-function right
  // as each function is generated, this only makes sense once every
  // function that might get inlined into another has already been
  // generated -- so callers must run it after the whole file has been
  // parsed (see compile_main.cpp), not interleaved with codegen the way
  // main.cpp's JIT REPL works.
  void runModuleInlining();

  // Precedence of every binary operator the parser currently knows about
  // (builtins plus any user-defined 'binary<op>' functions compiled so
  // far). Lives here rather than in Parser because FunctionAST::codegen()
  // is what installs a new operator the moment its definition compiles --
  // and it needs to take effect before the *next* top-level statement is
  // parsed. Survives module resets, like FunctionProtos.
  std::map<char, int> &getBinopPrecedence() { return BinopPrecedence; }

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

  // Debug info (DWARF) support -- disabled by default. Call
  // enableDebugInfo() once, right after construction, to turn it on for
  // this CodeGenContext's module. With it off, emitLocation() is a no-op
  // and FunctionAST::codegen() skips creating any debug metadata. Not
  // wired into main.cpp's JIT REPL: resetModule() runs once per
  // definition there, which would tear down the DIBuilder (it's tied to
  // the LLVMContext being replaced) and the single compile unit along
  // with it -- kcc's one-module-per-file static compile has no such
  // problem, so that's where this gets used.
  void enableDebugInfo(const std::string &Filename,
                        const std::string &Directory);
  bool hasDebugInfo() const { return DBuilder != nullptr; }
  void finalizeDebugInfo();

  llvm::DIBuilder &getDIBuilder() { return *DBuilder; }
  llvm::DICompileUnit *getCompileUnit() { return TheCU; }
  llvm::DIType *getDoubleDIType();
  std::vector<llvm::DIScope *> &getLexicalBlocks() { return LexicalBlocks; }

  // Points the IR builder's "current debug location" at AST's source
  // position, or clears it for AST == nullptr (used for a function's
  // prologue, so a debugger steps past it when breaking on the
  // function). No-op when debug info isn't enabled.
  void emitLocation(ExprAST *AST);

private:
  std::string ModuleName;

  std::unique_ptr<llvm::LLVMContext> TheContext;
  std::unique_ptr<llvm::Module> TheModule;
  std::unique_ptr<llvm::IRBuilder<>> Builder;
  std::map<std::string, llvm::AllocaInst *> NamedValues;
  std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;
  std::map<char, int> BinopPrecedence;

  // Per-function optimization pipeline (InstCombine, Reassociate, GVN,
  // SimplifyCFG) plus the analysis managers/registration it needs.
  std::unique_ptr<llvm::FunctionPassManager> TheFPM;
  std::unique_ptr<llvm::LoopAnalysisManager> TheLAM;
  std::unique_ptr<llvm::FunctionAnalysisManager> TheFAM;
  std::unique_ptr<llvm::CGSCCAnalysisManager> TheCGAM;
  std::unique_ptr<llvm::ModuleAnalysisManager> TheMAM;
  std::unique_ptr<llvm::PassInstrumentationCallbacks> ThePIC;
  std::unique_ptr<llvm::StandardInstrumentations> TheSI;

  std::unique_ptr<llvm::DIBuilder> DBuilder;
  llvm::DICompileUnit *TheCU = nullptr;
  llvm::DIType *DblDIType = nullptr;
  std::vector<llvm::DIScope *> LexicalBlocks;
};
