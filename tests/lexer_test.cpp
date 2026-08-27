#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Lexer.h"

#include <sstream>

TEST_CASE("Lexer recognizes keywords, identifiers and punctuation",
          "[lexer]") {
  std::istringstream In("def foo(x y)");
  Lexer Lex(In);

  REQUIRE(Lex.getTok() == tok_def);

  REQUIRE(Lex.getTok() == tok_identifier);
  REQUIRE(Lex.getIdentifier() == "foo");

  REQUIRE(Lex.getTok() == '(');

  REQUIRE(Lex.getTok() == tok_identifier);
  REQUIRE(Lex.getIdentifier() == "x");

  REQUIRE(Lex.getTok() == tok_identifier);
  REQUIRE(Lex.getIdentifier() == "y");

  REQUIRE(Lex.getTok() == ')');
  REQUIRE(Lex.getTok() == tok_eof);
}

TEST_CASE("Lexer parses numeric literals", "[lexer]") {
  std::istringstream In("3.14 42");
  Lexer Lex(In);

  REQUIRE(Lex.getTok() == tok_number);
  REQUIRE(Lex.getNumVal() == Catch::Approx(3.14));

  REQUIRE(Lex.getTok() == tok_number);
  REQUIRE(Lex.getNumVal() == Catch::Approx(42.0));
}

TEST_CASE("Lexer skips '#' comments through end of line", "[lexer]") {
  std::istringstream In("# a comment\n42");
  Lexer Lex(In);

  REQUIRE(Lex.getTok() == tok_number);
  REQUIRE(Lex.getNumVal() == Catch::Approx(42.0));
}
