#include "CodeGenContext.h"
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

// "Library" functions Kaleidoscope code can call via 'extern'. The JIT
// resolves them by searching the running process's own exported symbols,
// which is why they need extern "C" (no name mangling) and DLLEXPORT.
extern "C" DLLEXPORT double putchard(double X) {
  fputc(static_cast<char>(X), stderr);
  return 0;
}

extern "C" DLLEXPORT double printd(double X) {
  fprintf(stderr, "%f\n", X);
  return 0;
}

int main(int argc, char **argv) {
  // Register the host CPU as a JIT-able backend. Must happen before the
  // JIT is created.
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
  Parser P(Lex, CG, *JIT);
  P.run();

  return 0;
}
