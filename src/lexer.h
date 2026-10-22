#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace urchin {

struct Loc {
  int line = 1;
  int col = 1;
};

struct CompileError : std::runtime_error {
  Loc loc;
  CompileError(Loc loc, const std::string &msg) : std::runtime_error(msg), loc(loc) {}
};

enum class TokKind { Eof, Ident, Number, Keyword, Op };

struct Token {
  TokKind kind;
  std::string text;
  int64_t number = 0;
  Loc loc;
};

std::vector<Token> lex(const std::string &src);

} // namespace urchin
