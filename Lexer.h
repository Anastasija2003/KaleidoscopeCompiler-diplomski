#pragma once

#include <istream>
#include <string>

enum Token {
  tok_eof = -1,

  tok_def = -2,
  tok_extern = -3,

  tok_identifier = -4,
  tok_number = -5,
};

class Lexer {
public:
  explicit Lexer(std::istream &In) : In(In) {}

  int getTok();

  const std::string &getIdentifier() const { return IdentifierStr; }
  double getNumVal() const { return NumVal; }

private:
  int readChar() { return In.get(); }

  int lexIdentifier();
  int lexNumber();
  void skipComment();

  std::istream &In;
  int LastChar = ' ';
  std::string IdentifierStr;
  double NumVal = 0;
};
