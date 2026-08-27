#include "CodeGenContext.h"
#include "JITModuleSink.h"
#include "KaleidoscopeJIT.h"
#include "Lexer.h"
#include "Parser.h"

#include "llvm/Support/TargetSelect.h"

#include <fstream>
#include <iostream>

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

extern "C" DLLEXPORT double putchard(double X) {
  fputc(static_cast<char>(X), stderr);
  return 0;
}

extern "C" DLLEXPORT double printd(double X) {
  fprintf(stderr, "%f\n", X);
  return 0;
}

int main(int argc, char **argv) {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();

  std::ifstream File;
  std::istream *Input = &std::cin;

  if (argc > 1) {
    File.open(argv[1]);
    if (!File) {
      std::cerr << "error: could not open file '" << argv[1] << "'\n";
      return 1;
    }
    Input = &File;
  }

  llvm::ExitOnError ExitOnErr;
  auto JIT = ExitOnErr(llvm::orc::KaleidoscopeJIT::Create());

  Lexer Lex(*Input);
  CodeGenContext CG("kaleidoscope", JIT->getDataLayout());
  JITModuleSink Sink(*JIT);
  Parser P(Lex, CG, &Sink);
  P.run();

  return 0;
}
