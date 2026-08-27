#include "AST.h"
#include "CodeGenContext.h"

#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/TargetParser/Triple.h"
#include "llvm/Transforms/IPO/Inliner.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include "llvm/Transforms/Utils/Mem2Reg.h"

#include <cstdio>

using namespace llvm;

CodeGenContext::CodeGenContext(std::string ModuleName, const DataLayout &DL)
    : ModuleName(std::move(ModuleName)) {
  BinopPrecedence['='] = 2;
  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40;

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
                                                       false);
  TheSI->registerCallbacks(*ThePIC, TheMAM.get());

  TheFPM->addPass(PromotePass());
  TheFPM->addPass(InstCombinePass());
  TheFPM->addPass(ReassociatePass());
  TheFPM->addPass(GVNPass());
  TheFPM->addPass(SimplifyCFGPass());

  PassBuilder PB;
  PB.registerModuleAnalyses(*TheMAM);
  PB.registerCGSCCAnalyses(*TheCGAM);
  PB.registerFunctionAnalyses(*TheFAM);
  PB.registerLoopAnalyses(*TheLAM);
  PB.crossRegisterProxies(*TheLAM, *TheFAM, *TheCGAM, *TheMAM);
}

void CodeGenContext::runModuleInlining() {
  ModulePassManager MPM;
  MPM.addPass(ModuleInlinerWrapperPass());
  MPM.run(*TheModule, *TheMAM);
}

void CodeGenContext::enableDebugInfo(const std::string &Filename,
                                      const std::string &Directory) {
  TheModule->addModuleFlag(Module::Warning, "Debug Info Version",
                            DEBUG_METADATA_VERSION);

  if (Triple(sys::getProcessTriple()).isOSDarwin())
    TheModule->addModuleFlag(Module::Warning, "Dwarf Version", 2);

  DBuilder = std::make_unique<DIBuilder>(*TheModule);
  TheCU = DBuilder->createCompileUnit(
      dwarf::DW_LANG_C, DBuilder->createFile(Filename, Directory),
      "Kaleidoscope Compiler",  false, "", 0);
}

DIType *CodeGenContext::getDoubleDIType() {
  if (DblDIType)
    return DblDIType;
  DblDIType = DBuilder->createBasicType("double", 64, dwarf::DW_ATE_float);
  return DblDIType;
}

void CodeGenContext::emitLocation(ExprAST *AST) {
  if (!DBuilder)
    return;

  if (!AST) {
    Builder->SetCurrentDebugLocation(DebugLoc());
    return;
  }

  DIScope *Scope =
      LexicalBlocks.empty() ? static_cast<DIScope *>(TheCU) : LexicalBlocks.back();
  Builder->SetCurrentDebugLocation(
      DILocation::get(Scope->getContext(), AST->getLine(), AST->getCol(), Scope));
}

void CodeGenContext::finalizeDebugInfo() {
  if (DBuilder)
    DBuilder->finalize();
}

static Value *LogErrorV(const char *Str) {
  fprintf(stderr, "Error: %s\n", Str);
  return nullptr;
}

static Function *getFunction(CodeGenContext &CG, const std::string &Name) {
  if (auto *F = CG.getModule().getFunction(Name))
    return F;

  auto &Protos = CG.getFunctionProtos();
  auto FI = Protos.find(Name);
  if (FI != Protos.end())
    return FI->second->codegen(CG);

  return nullptr;
}

static AllocaInst *CreateEntryBlockAlloca(CodeGenContext &CG,
                                           Function *TheFunction,
                                           StringRef VarName) {
  IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
                    TheFunction->getEntryBlock().begin());
  return TmpB.CreateAlloca(Type::getDoubleTy(CG.getContext()), nullptr,
                            VarName);
}

static DISubroutineType *CreateFunctionType(CodeGenContext &CG,
                                             unsigned NumArgs) {
  SmallVector<Metadata *, 8> EltTys;
  DIType *DblTy = CG.getDoubleDIType();

  EltTys.push_back(DblTy);
  for (unsigned i = 0; i != NumArgs; ++i)
    EltTys.push_back(DblTy);

  return CG.getDIBuilder().createSubroutineType(
      CG.getDIBuilder().getOrCreateTypeArray(EltTys));
}

Value *NumberExprAST::codegen(CodeGenContext &CG) {
  CG.emitLocation(this);
  return ConstantFP::get(CG.getContext(), APFloat(Val));
}

Value *VariableExprAST::codegen(CodeGenContext &CG) {
  AllocaInst *A = CG.getNamedValues()[Name];
  if (!A)
    return LogErrorV("Unknown variable name");

  CG.emitLocation(this);
  return CG.getBuilder().CreateLoad(A->getAllocatedType(), A, Name);
}

Value *UnaryExprAST::codegen(CodeGenContext &CG) {
  Value *OperandV = Operand->codegen(CG);
  if (!OperandV)
    return nullptr;

  Function *F = getFunction(CG, std::string("unary") + Opcode);
  if (!F)
    return LogErrorV("Unknown unary operator");

  CG.emitLocation(this);
  return CG.getBuilder().CreateCall(F, OperandV, "unop");
}

Value *BinaryExprAST::codegen(CodeGenContext &CG) {
  CG.emitLocation(this);

  if (Op == '=') {
    auto *LHSVar = dynamic_cast<VariableExprAST *>(LHS.get());
    if (!LHSVar)
      return LogErrorV("destination of '=' must be a variable");

    Value *Val = RHS->codegen(CG);
    if (!Val)
      return nullptr;

    AllocaInst *Alloca = CG.getNamedValues()[LHSVar->getName()];
    if (!Alloca)
      return LogErrorV("Unknown variable name");

    CG.getBuilder().CreateStore(Val, Alloca);
    return Val;
  }

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
    return Builder.CreateUIToFP(L, Type::getDoubleTy(CG.getContext()),
                                 "booltmp");
  default:
    break;
  }

  Function *F = getFunction(CG, std::string("binary") + Op);
  if (!F)
    return LogErrorV("binary operator not found!");

  Value *Ops[] = {L, R};
  return Builder.CreateCall(F, Ops, "binop");
}

Value *CallExprAST::codegen(CodeGenContext &CG) {
  CG.emitLocation(this);

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
  CG.emitLocation(this);

  Value *CondV = Cond->codegen(CG);
  if (!CondV)
    return nullptr;

  IRBuilder<> &Builder = CG.getBuilder();
  LLVMContext &Context = CG.getContext();

  CondV = Builder.CreateFCmpONE(CondV, ConstantFP::get(Context, APFloat(0.0)),
                                 "ifcond");

  Function *TheFunction = Builder.GetInsertBlock()->getParent();

  BasicBlock *ThenBB = BasicBlock::Create(Context, "then", TheFunction);
  BasicBlock *ElseBB = BasicBlock::Create(Context, "else");
  BasicBlock *MergeBB = BasicBlock::Create(Context, "ifcont");

  Builder.CreateCondBr(CondV, ThenBB, ElseBB);

  Builder.SetInsertPoint(ThenBB);
  Value *ThenV = Then->codegen(CG);
  if (!ThenV)
    return nullptr;

  Builder.CreateBr(MergeBB);
  ThenBB = Builder.GetInsertBlock();

  TheFunction->insert(TheFunction->end(), ElseBB);
  Builder.SetInsertPoint(ElseBB);
  Value *ElseV = Else->codegen(CG);
  if (!ElseV)
    return nullptr;

  Builder.CreateBr(MergeBB);
  ElseBB = Builder.GetInsertBlock();

  TheFunction->insert(TheFunction->end(), MergeBB);
  Builder.SetInsertPoint(MergeBB);
  PHINode *PN = Builder.CreatePHI(Type::getDoubleTy(Context), 2, "iftmp");
  PN->addIncoming(ThenV, ThenBB);
  PN->addIncoming(ElseV, ElseBB);
  return PN;
}

Value *ForExprAST::codegen(CodeGenContext &CG) {
  IRBuilder<> &Builder = CG.getBuilder();
  LLVMContext &Context = CG.getContext();
  Function *TheFunction = Builder.GetInsertBlock()->getParent();

  AllocaInst *Alloca = CreateEntryBlockAlloca(CG, TheFunction, VarName);

  CG.emitLocation(this);

  Value *StartVal = Start->codegen(CG);
  if (!StartVal)
    return nullptr;
  Builder.CreateStore(StartVal, Alloca);

  BasicBlock *LoopBB = BasicBlock::Create(Context, "loop", TheFunction);

  Builder.CreateBr(LoopBB);
  Builder.SetInsertPoint(LoopBB);

  auto &NamedValues = CG.getNamedValues();
  AllocaInst *OldVal = NamedValues[VarName];
  NamedValues[VarName] = Alloca;

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

  Value *EndCond = End->codegen(CG);
  if (!EndCond)
    return nullptr;

  Value *CurVar = Builder.CreateLoad(Alloca->getAllocatedType(), Alloca,
                                      VarName);
  Value *NextVar = Builder.CreateFAdd(CurVar, StepVal, "nextvar");
  Builder.CreateStore(NextVar, Alloca);

  EndCond = Builder.CreateFCmpONE(
      EndCond, ConstantFP::get(Context, APFloat(0.0)), "loopcond");

  BasicBlock *AfterBB =
      BasicBlock::Create(Context, "afterloop", TheFunction);

  Builder.CreateCondBr(EndCond, LoopBB, AfterBB);
  Builder.SetInsertPoint(AfterBB);

  if (OldVal)
    NamedValues[VarName] = OldVal;
  else
    NamedValues.erase(VarName);

  return Constant::getNullValue(Type::getDoubleTy(Context));
}

Value *VarExprAST::codegen(CodeGenContext &CG) {
  IRBuilder<> &Builder = CG.getBuilder();
  Function *TheFunction = Builder.GetInsertBlock()->getParent();
  auto &NamedValues = CG.getNamedValues();

  std::vector<AllocaInst *> OldBindings;

  for (auto &VarBinding : VarNames) {
    const std::string &VarName = VarBinding.first;
    ExprAST *Init = VarBinding.second.get();

    Value *InitVal;
    if (Init) {
      InitVal = Init->codegen(CG);
      if (!InitVal)
        return nullptr;
    } else {
      InitVal = ConstantFP::get(CG.getContext(), APFloat(0.0));
    }

    AllocaInst *Alloca = CreateEntryBlockAlloca(CG, TheFunction, VarName);
    Builder.CreateStore(InitVal, Alloca);

    OldBindings.push_back(NamedValues[VarName]);
    NamedValues[VarName] = Alloca;
  }

  CG.emitLocation(this);

  Value *BodyVal = Body->codegen(CG);
  if (!BodyVal)
    return nullptr;

  for (unsigned i = 0, e = VarNames.size(); i != e; ++i)
    NamedValues[VarNames[i].first] = OldBindings[i];

  return BodyVal;
}

Function *PrototypeAST::codegen(CodeGenContext &CG) {
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
  auto &P = *Proto;
  CG.getFunctionProtos()[Proto->getName()] = std::move(Proto);
  Function *TheFunction = getFunction(CG, P.getName());
  if (!TheFunction)
    return nullptr;

  if (P.isBinaryOp())
    CG.getBinopPrecedence()[P.getOperatorName()] = P.getBinaryPrecedence();

  BasicBlock *BB = BasicBlock::Create(CG.getContext(), "entry", TheFunction);
  CG.getBuilder().SetInsertPoint(BB);

  DISubprogram *SP = nullptr;
  DIFile *Unit = nullptr;
  unsigned LineNo = P.getLine();
  if (CG.hasDebugInfo()) {
    Unit = CG.getDIBuilder().createFile(CG.getCompileUnit()->getFilename(),
                                         CG.getCompileUnit()->getDirectory());
    SP = CG.getDIBuilder().createFunction(
        Unit, P.getName(), StringRef(), Unit, LineNo,
        CreateFunctionType(CG, TheFunction->arg_size()), LineNo,
        DINode::FlagPrototyped, DISubprogram::SPFlagDefinition);
    TheFunction->setSubprogram(SP);
    CG.getLexicalBlocks().push_back(SP);

    CG.emitLocation(nullptr);
  }

  auto &NamedValues = CG.getNamedValues();
  NamedValues.clear();
  unsigned ArgIdx = 0;
  for (auto &Arg : TheFunction->args()) {
    AllocaInst *Alloca = CreateEntryBlockAlloca(CG, TheFunction, Arg.getName());

    if (CG.hasDebugInfo()) {
      DILocalVariable *D = CG.getDIBuilder().createParameterVariable(
          SP, Arg.getName(), ++ArgIdx, Unit, LineNo, CG.getDoubleDIType(),
           true);
      CG.getDIBuilder().insertDeclare(
          Alloca, D, CG.getDIBuilder().createExpression(),
          DILocation::get(SP->getContext(), LineNo, 0, SP),
          CG.getBuilder().GetInsertBlock());
    }

    CG.getBuilder().CreateStore(&Arg, Alloca);
    NamedValues[std::string(Arg.getName())] = Alloca;
  }

  CG.emitLocation(Body.get());

  if (Value *RetVal = Body->codegen(CG)) {
    CG.getBuilder().CreateRet(RetVal);

    if (CG.hasDebugInfo())
      CG.getLexicalBlocks().pop_back();

    verifyFunction(*TheFunction);

    CG.getFPM().run(*TheFunction, CG.getFAM());

    return TheFunction;
  }

  TheFunction->eraseFromParent();

  if (P.isBinaryOp())
    CG.getBinopPrecedence().erase(P.getOperatorName());

  if (CG.hasDebugInfo())
    CG.getLexicalBlocks().pop_back();

  return nullptr;
}
