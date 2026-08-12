#include "Lexer.h"
#include "Parser.h"

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
  Parser P(Lex);
  P.run();

  return 0;
}
