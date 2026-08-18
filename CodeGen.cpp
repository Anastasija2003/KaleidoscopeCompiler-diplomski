#include "AST.h"
#include "CodeGenContext.h"

#include "llvm/ADT/APFloat.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"

#include <cstdio>

using namespace llvm;

CodeGenContext::CodeGenContext(std::string ModuleName, const DataLayout &DL)
    : ModuleName(std::move(ModuleName)) {
  resetModule(DL);
}

llvm::orc::ThreadSafeModule CodeGenContext::takeModule() {
  return llvm::orc::ThreadSafeModule(std::move(TheModule),
                                      std::move(TheContext));
}

void CodeGenContext::resetModule(const DataLayout &DL) {
  TheContext = std::make_unique<LLVMContext>();
  TheModule = std::make_unique<Module>(ModuleName, *TheContext);
  TheModule->setDataLayout(DL);
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

// getFunction - Look up Name as a function in the module currently being
// built. If it isn't there (e.g. this is a fresh module started after the
// previous one was handed off to the JIT), fall back to CG's registry of
// every prototype seen so far and (re)materialize a declaration for it
// into the current module on demand.
static Function *getFunction(CodeGenContext &CG, const std::string &Name) {
  if (auto *F = CG.getModule().getFunction(Name))
    return F;

  auto &Protos = CG.getFunctionProtos();
  auto FI = Protos.find(Name);
  if (FI != Protos.end())
    return FI->second->codegen(CG);

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
  Function *CalleeF = getFunction(CG, Callee);
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

Value *IfExprAST::codegen(CodeGenContext &CG) {
  Value *CondV = Cond->codegen(CG);
  if (!CondV)
    return nullptr;

  IRBuilder<> &Builder = CG.getBuilder();
  LLVMContext &Context = CG.getContext();

  // Convert condition to a bool by comparing non-equal to 0.0.
  CondV = Builder.CreateFCmpONE(CondV, ConstantFP::get(Context, APFloat(0.0)),
                                 "ifcond");

  Function *TheFunction = Builder.GetInsertBlock()->getParent();

  // Create blocks for the then/else cases. Insert the 'then' block at the
  // end of the function; 'else' and 'merge' are inserted once we know
  // where codegen-ing the previous block left off.
  BasicBlock *ThenBB = BasicBlock::Create(Context, "then", TheFunction);
  BasicBlock *ElseBB = BasicBlock::Create(Context, "else");
  BasicBlock *MergeBB = BasicBlock::Create(Context, "ifcont");

  Builder.CreateCondBr(CondV, ThenBB, ElseBB);

  // Emit the 'then' value.
  Builder.SetInsertPoint(ThenBB);
  Value *ThenV = Then->codegen(CG);
  if (!ThenV)
    return nullptr;

  Builder.CreateBr(MergeBB);
  // Codegen of 'Then' can change the current block; update ThenBB for the
  // PHI below.
  ThenBB = Builder.GetInsertBlock();

  // Emit the 'else' block.
  TheFunction->insert(TheFunction->end(), ElseBB);
  Builder.SetInsertPoint(ElseBB);
  Value *ElseV = Else->codegen(CG);
  if (!ElseV)
    return nullptr;

  Builder.CreateBr(MergeBB);
  ElseBB = Builder.GetInsertBlock();

  // Emit the merge block.
  TheFunction->insert(TheFunction->end(), MergeBB);
  Builder.SetInsertPoint(MergeBB);
  PHINode *PN = Builder.CreatePHI(Type::getDoubleTy(Context), 2, "iftmp");
  PN->addIncoming(ThenV, ThenBB);
  PN->addIncoming(ElseV, ElseBB);
  return PN;
}

// Lowers to:
//   ...
//   start = startexpr
//   goto loop
// loop:
//   variable = phi [start, loopheader], [nextvariable, loopend]
//   ...
//   bodyexpr
//   ...
// loopend:
//   step = stepexpr
//   nextvariable = variable + step
//   endcond = endexpr
//   br endcond, loop, afterloop
// afterloop:
Value *ForExprAST::codegen(CodeGenContext &CG) {
  Value *StartVal = Start->codegen(CG);
  if (!StartVal)
    return nullptr;

  IRBuilder<> &Builder = CG.getBuilder();
  LLVMContext &Context = CG.getContext();

  Function *TheFunction = Builder.GetInsertBlock()->getParent();
  BasicBlock *PreheaderBB = Builder.GetInsertBlock();
  BasicBlock *LoopBB = BasicBlock::Create(Context, "loop", TheFunction);

  // Fall through from the current block into the loop.
  Builder.CreateBr(LoopBB);
  Builder.SetInsertPoint(LoopBB);

  PHINode *Variable =
      Builder.CreatePHI(Type::getDoubleTy(Context), 2, VarName);
  Variable->addIncoming(StartVal, PreheaderBB);

  // The loop variable shadows any outer variable of the same name for the
  // duration of the loop; save whatever it shadows so it can be restored
  // afterward.
  auto &NamedValues = CG.getNamedValues();
  Value *OldVal = NamedValues[VarName];
  NamedValues[VarName] = Variable;

  // The body's value is discarded -- only its side effects (e.g. a call)
  // matter -- but a codegen failure still aborts the loop.
  if (!Body->codegen(CG))
    return nullptr;

  Value *StepVal = nullptr;
  if (Step) {
    StepVal = Step->codegen(CG);
    if (!StepVal)
      return nullptr;
  } else {
    StepVal = ConstantFP::get(Context, APFloat(1.0));
  }

  Value *NextVar = Builder.CreateFAdd(Variable, StepVal, "nextvar");

  Value *EndCond = End->codegen(CG);
  if (!EndCond)
    return nullptr;

  EndCond = Builder.CreateFCmpONE(
      EndCond, ConstantFP::get(Context, APFloat(0.0)), "loopcond");

  BasicBlock *LoopEndBB = Builder.GetInsertBlock();
  BasicBlock *AfterBB =
      BasicBlock::Create(Context, "afterloop", TheFunction);

  Builder.CreateCondBr(EndCond, LoopBB, AfterBB);
  Builder.SetInsertPoint(AfterBB);

  Variable->addIncoming(NextVar, LoopEndBB);

  if (OldVal)
    NamedValues[VarName] = OldVal;
  else
    NamedValues.erase(VarName);

  // A for expression always evaluates to 0.0.
  return Constant::getNullValue(Type::getDoubleTy(Context));
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
  // Hand the prototype's ownership to CG's registry (it needs to outlive
  // this module -- a later module may need to redeclare or, for a 'def',
  // redefine this same function), keeping a reference for use below. This
  // is also what makes redefinition possible: getFunction() below always
  // (re)materializes the declaration into *this* fresh module, so there's
  // no stale "already has a body" Function left over to reject against.
  auto &P = *Proto;
  CG.getFunctionProtos()[Proto->getName()] = std::move(Proto);
  Function *TheFunction = getFunction(CG, P.getName());
  if (!TheFunction)
    return nullptr;

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
