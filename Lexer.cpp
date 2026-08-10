#include "Lexer.h"

#include <cctype>
#include <cstdlib>

int Lexer::getTok() {
  while (isspace(LastChar))
    LastChar = readChar();

  if (isalpha(LastChar))
    return lexIdentifier();

  if (isdigit(LastChar) || LastChar == '.')
    return lexNumber();

  if (LastChar == '#') {
    skipComment();
    if (LastChar != EOF)
      return getTok();
  }

  if (LastChar == EOF)
    return tok_eof;

  int ThisChar = LastChar;
  LastChar = readChar();
  return ThisChar;
}

int Lexer::lexIdentifier() {
  IdentifierStr = static_cast<char>(LastChar);
  while (isalnum((LastChar = readChar())))
    IdentifierStr += static_cast<char>(LastChar);

  if (IdentifierStr == "def")
    return tok_def;
  if (IdentifierStr == "extern")
    return tok_extern;
  return tok_identifier;
}

int Lexer::lexNumber() {
  std::string NumStr;
  bool SeenDot = false;

  do {
    if (LastChar == '.') {
      if (SeenDot)
        break;
      SeenDot = true;
    }
    NumStr += static_cast<char>(LastChar);
    LastChar = readChar();
  } while (isdigit(LastChar) || LastChar == '.');

  char *End = nullptr;
  NumVal = strtod(NumStr.c_str(), &End);
  if (End == NumStr.c_str())
    return getTok();

  return tok_number;
}

void Lexer::skipComment() {
  do
    LastChar = readChar();
  while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');
}
