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

class CodeGenContext {
public:
  CodeGenContext(std::string ModuleName, const llvm::DataLayout &DL);

  llvm::LLVMContext &getContext() { return *TheContext; }
  llvm::Module &getModule() { return *TheModule; }
  llvm::IRBuilder<> &getBuilder() { return *Builder; }

  std::map<std::string, llvm::AllocaInst *> &getNamedValues() {
    return NamedValues;
  }

  std::map<std::string, std::unique_ptr<PrototypeAST>> &getFunctionProtos() {
    return FunctionProtos;
  }

  llvm::FunctionPassManager &getFPM() { return *TheFPM; }
  llvm::FunctionAnalysisManager &getFAM() { return *TheFAM; }

  void runModuleInlining();

  std::map<char, int> &getBinopPrecedence() { return BinopPrecedence; }

  llvm::orc::ThreadSafeModule takeModule();

  void resetModule(const llvm::DataLayout &DL);

  void enableDebugInfo(const std::string &Filename,
                        const std::string &Directory);
  bool hasDebugInfo() const { return DBuilder != nullptr; }
  void finalizeDebugInfo();

  llvm::DIBuilder &getDIBuilder() { return *DBuilder; }
  llvm::DICompileUnit *getCompileUnit() { return TheCU; }
  llvm::DIType *getDoubleDIType();
  std::vector<llvm::DIScope *> &getLexicalBlocks() { return LexicalBlocks; }

  void emitLocation(ExprAST *AST);

private:
  std::string ModuleName;

  std::unique_ptr<llvm::LLVMContext> TheContext;
  std::unique_ptr<llvm::Module> TheModule;
  std::unique_ptr<llvm::IRBuilder<>> Builder;
  std::map<std::string, llvm::AllocaInst *> NamedValues;
  std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;
  std::map<char, int> BinopPrecedence;

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
