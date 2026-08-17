#include "CodeGenContext.h"
#include "Lexer.h"
#include "Parser.h"

#include "llvm/Support/raw_ostream.h"

#include <fstream>
#include <iostream>

int main(int argc, char **argv) {
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

  Lexer Lex(*Input);
  CodeGenContext CG("kaleidoscope");
  Parser P(Lex, CG);
  P.run();

  // Dump everything that made it into the module (definitions and externs;
  // top-level expressions were erased right after printing their own IR).
  CG.getModule().print(llvm::errs(), nullptr);

  return 0;
}
