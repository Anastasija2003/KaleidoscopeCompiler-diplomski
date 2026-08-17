#include "AST.h"
#include "CodeGenContext.h"

#include "llvm/ADT/APFloat.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"

#include <cstdio>

using namespace llvm;

CodeGenContext::CodeGenContext(const std::string &ModuleName) {
  TheContext = std::make_unique<LLVMContext>();
  TheModule = std::make_unique<Module>(ModuleName, *TheContext);
  Builder = std::make_unique<IRBuilder<>>(*TheContext);

  TheFPM = std::make_unique<FunctionPassManager>();
  TheLAM = std::make_unique<LoopAnalysisManager>();
  TheFAM = std::make_unique<FunctionAnalysisManager>();
  TheCGAM = std::make_unique<CGSCCAnalysisManager>();
  TheMAM = std::make_unique<ModuleAnalysisManager>();
  ThePIC = std::make_unique<PassInstrumentationCallbacks>();
  TheSI = std::make_unique<StandardInstrumentations>(*TheContext,
                                                      /*DebugLogging=*/false);
  TheSI->registerCallbacks(*ThePIC, TheMAM.get());

  // Peephole (InstCombine), then expose more of them by canonicalizing
  // expression trees (Reassociate), then remove what's now redundant
  // (GVN), then tidy up the control-flow graph (SimplifyCFG).
  TheFPM->addPass(InstCombinePass());
  TheFPM->addPass(ReassociatePass());
  TheFPM->addPass(GVNPass());
  TheFPM->addPass(SimplifyCFGPass());

  PassBuilder PB;
  PB.registerModuleAnalyses(*TheMAM);
  PB.registerFunctionAnalyses(*TheFAM);
  PB.crossRegisterProxies(*TheLAM, *TheFAM, *TheCGAM, *TheMAM);
}

// LogErrorV - Same idea as Parser's logError, but for the codegen phase,
// where the "no value" sentinel is a null Value* instead of a null AST node.
static Value *LogErrorV(const char *Str) {
  fprintf(stderr, "Error: %s\n", Str);
  return nullptr;
}

Value *NumberExprAST::codegen(CodeGenContext &CG) {
  return ConstantFP::get(CG.getContext(), APFloat(Val));
}

Value *VariableExprAST::codegen(CodeGenContext &CG) {
  Value *V = CG.getNamedValues()[Name];
  if (!V)
    return LogErrorV("Unknown variable name");
  return V;
}

Value *BinaryExprAST::codegen(CodeGenContext &CG) {
  Value *L = LHS->codegen(CG);
  Value *R = RHS->codegen(CG);
  if (!L || !R)
    return nullptr;

  IRBuilder<> &Builder = CG.getBuilder();
  switch (Op) {
  case '+':
    return Builder.CreateFAdd(L, R, "addtmp");
  case '-':
    return Builder.CreateFSub(L, R, "subtmp");
  case '*':
    return Builder.CreateFMul(L, R, "multmp");
  case '<':
    L = Builder.CreateFCmpULT(L, R, "cmptmp");
    // Convert bool 0/1 to double 0.0 or 1.0.
    return Builder.CreateUIToFP(L, Type::getDoubleTy(CG.getContext()),
                                 "booltmp");
  default:
    return LogErrorV("invalid binary operator");
  }
}

Value *CallExprAST::codegen(CodeGenContext &CG) {
  Function *CalleeF = CG.getModule().getFunction(Callee);
  if (!CalleeF)
    return LogErrorV("Unknown function referenced");

  if (CalleeF->arg_size() != Args.size())
    return LogErrorV("Incorrect # arguments passed");

  std::vector<Value *> ArgsV;
  for (auto &Arg : Args) {
    ArgsV.push_back(Arg->codegen(CG));
    if (!ArgsV.back())
      return nullptr;
  }

  return CG.getBuilder().CreateCall(CalleeF, ArgsV, "calltmp");
}

Function *PrototypeAST::codegen(CodeGenContext &CG) {
  // Every Kaleidoscope function currently takes/returns double.
  std::vector<Type *> Doubles(Args.size(), Type::getDoubleTy(CG.getContext()));
  FunctionType *FT =
      FunctionType::get(Type::getDoubleTy(CG.getContext()), Doubles, false);

  Function *F = Function::Create(FT, Function::ExternalLinkage, Name,
                                  CG.getModule());

  unsigned Idx = 0;
  for (auto &Arg : F->args())
    Arg.setName(Args[Idx++]);

  return F;
}

Function *FunctionAST::codegen(CodeGenContext &CG) {
  // Reuse an existing forward declaration (from a prior 'extern') if there
  // is one, instead of redeclaring the function.
  Function *TheFunction = CG.getModule().getFunction(Proto->getName());

  if (!TheFunction)
    TheFunction = Proto->codegen(CG);

  if (!TheFunction)
    return nullptr;

  if (!TheFunction->empty())
    return static_cast<Function *>(LogErrorV("Function cannot be redefined."));

  BasicBlock *BB = BasicBlock::Create(CG.getContext(), "entry", TheFunction);
  CG.getBuilder().SetInsertPoint(BB);

  auto &NamedValues = CG.getNamedValues();
  NamedValues.clear();
  for (auto &Arg : TheFunction->args())
    NamedValues[std::string(Arg.getName())] = &Arg;

  if (Value *RetVal = Body->codegen(CG)) {
    CG.getBuilder().CreateRet(RetVal);
    verifyFunction(*TheFunction);

    // Run the peephole optimization pipeline on the finished function.
    CG.getFPM().run(*TheFunction, CG.getFAM());

    return TheFunction;
  }

  // Error reading body: remove the function so a later redefinition attempt
  // isn't blocked by this failed one.
  TheFunction->eraseFromParent();
  return nullptr;
}
