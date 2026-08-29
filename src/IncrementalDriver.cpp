#include "IncrementalDriver.h"

#include "CallGraph.h"
#include "CodeGenContext.h"
#include "DirtySet.h"
#include "FunctionCache.h"
#include "FunctionHasher.h"
#include "ParsedProgram.h"
#include "Parser.h"

#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/ValueMapper.h"

#include <filesystem>
#include <map>
#include <set>
#include <vector>

using namespace llvm;

namespace {

std::unique_ptr<Module> extractFunctionModule(Function &F) {
  auto MiniModule = std::make_unique<Module>(F.getName(), F.getContext());
  MiniModule->setDataLayout(F.getParent()->getDataLayout());
  MiniModule->setTargetTriple(F.getParent()->getTargetTriple());

  ValueToValueMapTy VMap;

  for (auto &BB : F) {
    for (auto &I : BB) {
      auto *Call = dyn_cast<CallInst>(&I);
      if (!Call)
        continue;
      Function *Callee = Call->getCalledFunction();
      if (!Callee || Callee == &F || VMap.count(Callee))
        continue;
      Function *Decl =
          Function::Create(Callee->getFunctionType(), GlobalValue::ExternalLinkage,
                            Callee->getName(), MiniModule.get());
      VMap[Callee] = Decl;
    }
  }

  Function *NewF = Function::Create(F.getFunctionType(), F.getLinkage(),
                                     F.getName(), MiniModule.get());
  auto NewArgIt = NewF->arg_begin();
  for (auto &Arg : F.args()) {
    NewArgIt->setName(Arg.getName());
    VMap[&Arg] = &*NewArgIt++;
  }

  SmallVector<ReturnInst *, 4> Returns;
  CloneFunctionInto(NewF, &F, VMap, CloneFunctionChangeType::DifferentModule,
                     Returns);
  verifyFunction(*NewF);

  if (auto *DbgCU = MiniModule->getNamedMetadata("llvm.dbg.cu"))
    if (DbgCU->getNumOperands() == 0)
      MiniModule->eraseNamedMetadata(DbgCU);

  return MiniModule;
}

void writeBitcodeForFunction(Function &F, const std::string &Path) {
  auto MiniModule = extractFunctionModule(F);
  std::error_code EC;
  raw_fd_ostream Out(Path, EC, sys::fs::OF_None);
  WriteBitcodeToFile(*MiniModule, Out);
}

void emitObjectForFunction(Function &F, TargetMachine &TM,
                            const std::string &Path) {
  auto MiniModule = extractFunctionModule(F);

  std::error_code EC;
  raw_fd_ostream Out(Path, EC, sys::fs::OF_None);

  legacy::PassManager Pass;
  TM.addPassesToEmitFile(Pass, Out, nullptr, CodeGenFileType::ObjectFile);
  Pass.run(*MiniModule);
}

bool linkCachedFunction(CodeGenContext &CG, const FunctionCacheEntry &Entry) {
  auto BufOrErr = MemoryBuffer::getFile(Entry.BcPath);
  if (!BufOrErr)
    return false;

  auto ModOrErr = parseBitcodeFile((*BufOrErr)->getMemBufferRef(), CG.getContext());
  if (!ModOrErr) {
    consumeError(ModOrErr.takeError());
    return false;
  }

  return !Linker::linkModules(CG.getModule(), std::move(*ModOrErr));
}

} // namespace

void runIncrementalCompile(Parser &P, CodeGenContext &CG, TargetMachine &TM,
                            const std::string &CacheDir) {
  ParsedProgram Program = P.parseProgram();

  std::vector<std::string> Names;
  std::map<std::string, std::string> NewHashes;
  std::vector<const FunctionAST *> FnPtrs;
  for (auto &Fn : Program.Functions) {
    std::string Name = Fn->getName();
    NewHashes[Name] = hashFunction(*Fn);
    FnPtrs.push_back(Fn.get());
    Names.push_back(std::move(Name));
  }
  CallGraph NewGraph = buildCallGraph(FnPtrs);

  std::string LLVMVersion = LLVM_VERSION_STRING;
  std::string TargetTripleStr = TM.getTargetTriple().str();

  FunctionCache OldCache = FunctionCache::load(CacheDir);
  if (!OldCache.isCompatible(LLVMVersion, TargetTripleStr))
    OldCache = FunctionCache();

  std::set<std::string> Dirty = computeDirtySet(NewHashes, NewGraph, OldCache);

  for (auto &Proto : Program.Externs)
    Proto->codegen(CG);

  for (std::size_t i = 0; i < Program.Functions.size(); ++i) {
    const std::string &Name = Names[i];
    if (Dirty.count(Name))
      continue;

    auto It = OldCache.getFunctions().find(Name);
    bool Linked =
        It != OldCache.getFunctions().end() && linkCachedFunction(CG, It->second);
    if (!Linked)
      Dirty.insert(Name);
  }

  for (std::size_t i = 0; i < Program.Functions.size(); ++i)
    if (Dirty.count(Names[i]))
      Program.Functions[i]->codegen(CG);

  std::filesystem::create_directories(CacheDir);

  for (std::size_t i = 0; i < Program.Functions.size(); ++i) {
    const std::string &Name = Names[i];
    if (!Dirty.count(Name))
      continue;
    if (Function *F = CG.getModule().getFunction(Name))
      writeBitcodeForFunction(*F, CacheDir + "/" + Name + ".bc");
  }

  CG.runModuleInlining();

  FunctionCache NewCache;
  NewCache.setVersionInfo(LLVMVersion, TargetTripleStr);

  for (std::size_t i = 0; i < Program.Functions.size(); ++i) {
    const std::string &Name = Names[i];

    FunctionCacheEntry Entry;
    Entry.Hash = NewHashes[Name];
    auto GraphIt = NewGraph.find(Name);
    Entry.Callees = GraphIt != NewGraph.end() ? GraphIt->second
                                               : std::vector<std::string>{};
    Entry.BcPath = CacheDir + "/" + Name + ".bc";
    Entry.OPath = CacheDir + "/" + Name + ".o";

    if (Dirty.count(Name)) {
      if (Function *F = CG.getModule().getFunction(Name))
        emitObjectForFunction(*F, TM, Entry.OPath);
    }

    NewCache.getFunctions()[Name] = std::move(Entry);
  }

  NewCache.save(CacheDir);

  errs() << "Incremental: " << Dirty.size() << " dirty, "
         << (Program.Functions.size() - Dirty.size()) << " reused (of "
         << Program.Functions.size() << " functions)\n";
}
