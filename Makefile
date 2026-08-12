CXX := clang++
CXXFLAGS := -std=c++17 -Wall -Wextra -g

COMMON_SRCS := Lexer.cpp
COMMON_HDRS := Lexer.h AST.h Parser.h

MAIN_SRCS := main.cpp Parser.cpp $(COMMON_SRCS)
LEX_SRCS := lex_main.cpp $(COMMON_SRCS)

MAIN_TARGET := main
LEX_TARGET := lex
KS_FILE := test.ks

.PHONY: all run run-lex clean

all: $(MAIN_TARGET) $(LEX_TARGET)

$(MAIN_TARGET): $(MAIN_SRCS) $(COMMON_HDRS)
	$(CXX) $(CXXFLAGS) -o $(MAIN_TARGET) $(MAIN_SRCS)

$(LEX_TARGET): $(LEX_SRCS) Lexer.h
	$(CXX) $(CXXFLAGS) -o $(LEX_TARGET) $(LEX_SRCS)

# Runs the parser REPL against KS_FILE.
run: $(MAIN_TARGET)
	./$(MAIN_TARGET) $(KS_FILE)

# Runs the lexer's token dump against KS_FILE.
run-lex: $(LEX_TARGET)
	./$(LEX_TARGET) $(KS_FILE)

clean:
	rm -f $(MAIN_TARGET) $(LEX_TARGET)
