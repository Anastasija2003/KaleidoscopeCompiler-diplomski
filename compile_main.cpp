#include "CodeGenContext.h"
#include "Lexer.h"
#include "Parser.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/TargetParser/Triple.h"

#include <fstream>
#include <iostream>
#include <vector>

// kcc - Static (ahead-of-time) Kaleidoscope compiler. Unlike main.cpp
// (the JIT REPL), this reads a whole .ks file, compiles every definition
// into one running Module (no JIT, no per-definition module swap -- see
// Parser's nullptr-JIT mode), and at the end emits that Module as a
// native object file that can be linked like any other .o.
//
// Usage: kcc [-g] [input.ks] [output.o]
//   -g  emit DWARF debug info, so the resulting object can be stepped
//       through (source lines, local variables) in lldb/gdb.
int main(int argc, char **argv) {
  bool EmitDebugInfo = false;
  std::vector<std::string> PositionalArgs;
  for (int i = 1; i < argc; ++i) {
    std::string Arg = argv[i];
    if (Arg == "-g")
      EmitDebugInfo = true;
    else
      PositionalArgs.push_back(Arg);
  }

  std::ifstream File;
  std::istream *Input = &std::cin;
  std::string InputPath = "<stdin>";
  std::string OutputPath = "output.o";

  if (!PositionalArgs.empty()) {
    InputPath = PositionalArgs[0];
    File.open(InputPath);
    if (!File) {
      std::cerr << "error: could not open file '" << InputPath << "'\n";
      return 1;
    }
    Input = &File;
  }
  if (PositionalArgs.size() > 1)
    OutputPath = PositionalArgs[1];

  // Register every target LLVM was built with, so lookupTarget below can
  // find whichever one matches the host triple.
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmParsers();
  llvm::InitializeAllAsmPrinters();

  auto TargetTripleStr = llvm::sys::getDefaultTargetTriple();
  llvm::Triple TargetTriple(TargetTripleStr);

  std::string Error;
  const llvm::Target *Target =
      llvm::TargetRegistry::lookupTarget(TargetTripleStr, Error);
  if (!Target) {
    llvm::errs() << Error;
    return 1;
  }

  llvm::TargetOptions Opt;
  auto *TheTargetMachine = Target->createTargetMachine(
      TargetTriple, "generic", "", Opt, llvm::Reloc::PIC_);

  Lexer Lex(*Input);
  // The module's data layout has to match the target we're compiling
  // for, not the JIT's -- there is no JIT here.
  CodeGenContext CG("kaleidoscope", TheTargetMachine->createDataLayout());
  CG.getModule().setTargetTriple(TargetTriple);

  if (EmitDebugInfo)
    CG.enableDebugInfo(InputPath, ".");

  // No JIT passed: Parser accumulates every definition into CG's single
  // module instead of handing modules off one at a time.
  Parser P(Lex, CG);
  P.run();

  // No-op if -g wasn't passed.
  CG.finalizeDebugInfo();

  std::error_code EC;
  llvm::raw_fd_ostream Dest(OutputPath, EC, llvm::sys::fs::OF_None);
  if (EC) {
    llvm::errs() << "Could not open file: " << EC.message();
    return 1;
  }

  llvm::legacy::PassManager Pass;
  if (TheTargetMachine->addPassesToEmitFile(
          Pass, Dest, nullptr, llvm::CodeGenFileType::ObjectFile)) {
    llvm::errs() << "TargetMachine can't emit a file of this type\n";
    return 1;
  }

  Pass.run(CG.getModule());
  Dest.flush();

  llvm::outs() << "Wrote " << OutputPath << "\n";
  return 0;
}
