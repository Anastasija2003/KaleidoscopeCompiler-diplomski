#include "Lexer.h"

#include <cctype>
#include <cstdlib>
#include <unordered_map>

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
  static const std::unordered_map<std::string, int> Keywords = {
      {"def", tok_def},     {"extern", tok_extern}, {"if", tok_if},
      {"then", tok_then},   {"else", tok_else},     {"for", tok_for},
      {"in", tok_in},       {"binary", tok_binary}, {"unary", tok_unary},
      {"var", tok_var},
  };

  IdentifierStr = static_cast<char>(LastChar);
  while (isalnum((LastChar = readChar())))
    IdentifierStr += static_cast<char>(LastChar);

  auto It = Keywords.find(IdentifierStr);
  if (It != Keywords.end())
    return It->second;
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
