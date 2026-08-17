CXX := clang++
CXXFLAGS := -std=c++17 -Wall -Wextra -g

# Homebrew's LLVM is keg-only (not symlinked into PATH) so llvm-config has
# to be found via its Cellar prefix.
LLVM_CONFIG := $(shell brew --prefix llvm 2>/dev/null)/bin/llvm-config
# -isystem (instead of llvm-config's own -I) marks LLVM's headers as system
# headers, so -Wall -Wextra below only reports warnings from our own code.
LLVM_CXXFLAGS := $(shell $(LLVM_CONFIG) --cxxflags | sed 's/-I/-isystem /g')
LLVM_LDFLAGS := $(shell $(LLVM_CONFIG) --ldflags)
LLVM_LIBS := $(shell $(LLVM_CONFIG) --libs core)
LLVM_SYSLIBS := $(shell $(LLVM_CONFIG) --system-libs)

COMMON_SRCS := Lexer.cpp
COMMON_HDRS := Lexer.h AST.h Parser.h

MAIN_SRCS := main.cpp Parser.cpp CodeGen.cpp $(COMMON_SRCS)
MAIN_HDRS := $(COMMON_HDRS) CodeGenContext.h
LEX_SRCS := lex_main.cpp $(COMMON_SRCS)

MAIN_TARGET := main
LEX_TARGET := lex
KS_FILE := test.ks

.PHONY: all run run-lex clean

all: $(MAIN_TARGET) $(LEX_TARGET)

# Only the parser/codegen binary needs LLVM; the lexer stays a fast,
# dependency-free build.
$(MAIN_TARGET): $(MAIN_SRCS) $(MAIN_HDRS)
	$(CXX) $(CXXFLAGS) $(LLVM_CXXFLAGS) -o $(MAIN_TARGET) $(MAIN_SRCS) \
		$(LLVM_LDFLAGS) $(LLVM_LIBS) $(LLVM_SYSLIBS)

$(LEX_TARGET): $(LEX_SRCS) Lexer.h
	$(CXX) $(CXXFLAGS) -o $(LEX_TARGET) $(LEX_SRCS)

# Runs the parser+codegen REPL against KS_FILE.
run: $(MAIN_TARGET)
	./$(MAIN_TARGET) $(KS_FILE)

# Runs the lexer's token dump against KS_FILE.
run-lex: $(LEX_TARGET)
	./$(LEX_TARGET) $(KS_FILE)

clean:
	rm -f $(MAIN_TARGET) $(LEX_TARGET)
