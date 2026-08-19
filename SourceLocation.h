#pragma once

// A 1-based line/column position in the source being compiled. Shared by
// Lexer (which produces one per token) and AST.h (which every AST node
// carries, for debug info / error messages) -- kept in its own header so
// neither has to pull in the other just for this.
struct SourceLocation {
  int Line;
  int Col;
};
