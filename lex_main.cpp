#include "Lexer.h"

#include <fstream>
#include <iostream>

static void printToken(const Lexer &Lex, int Tok) {
  switch (Tok) {
  case tok_def:
    std::cout << "def\n";
    break;
  case tok_extern:
    std::cout << "extern\n";
    break;
  case tok_identifier:
    std::cout << "identifier: " << Lex.getIdentifier() << "\n";
    break;
  case tok_number:
    std::cout << "number: " << Lex.getNumVal() << "\n";
    break;
  default:
    std::cout << "char: '" << static_cast<char>(Tok) << "'\n";
  }
}

int main(int argc, char **argv) {
  std::string Path = argc > 1 ? argv[1] : "test.ks";

  std::ifstream File(Path);
  if (!File) {
    std::cerr << "error: could not open file '" << Path << "'\n";
    return 1;
  }

  Lexer Lex(File);
  int Tok;
  while ((Tok = Lex.getTok()) != tok_eof)
    printToken(Lex, Tok);

  return 0;
}
