#include "lexer.h"

#include <cctype>
#include <unordered_set>

namespace urchin {

static const std::unordered_set<std::string> keywords = {
    "dive", "pebble", "surface", "splash", "stash",
    "if",   "else",   "paddle",  "from",   "to",
};

static const char *twoCharOps[] = {"->", "==", "!=", "<=", ">="};
static const std::string oneCharOps = "+-*/%<>=(){},;";

std::vector<Token> lex(const std::string &src) {
  std::vector<Token> toks;
  size_t i = 0;
  Loc loc;

  auto advance = [&] {
    if (src[i] == '\n') {
      loc.line++;
      loc.col = 1;
    } else {
      loc.col++;
    }
    i++;
  };
  auto at = [&](size_t k) { return k < src.size() ? src[k] : '\0'; };

  while (i < src.size()) {
    char c = src[i];
    if (std::isspace(static_cast<unsigned char>(c))) {
      advance();
      continue;
    }

    Loc start = loc;

    // ~~ comments float like otters ~~
    if (c == '~' && at(i + 1) == '~') {
      advance();
      advance();
      while (i < src.size() && !(src[i] == '~' && at(i + 1) == '~'))
        advance();
      if (i >= src.size())
        throw CompileError(start, "this comment drifted out to sea (missing closing '~~')");
      advance();
      advance();
      continue;
    }

    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
      std::string word;
      while (std::isalnum(static_cast<unsigned char>(at(i))) || at(i) == '_') {
        word += src[i];
        advance();
      }
      toks.push_back({keywords.count(word) ? TokKind::Keyword : TokKind::Ident, word, 0, start});
      continue;
    }

    if (std::isdigit(static_cast<unsigned char>(c))) {
      std::string digits;
      while (std::isdigit(static_cast<unsigned char>(at(i)))) {
        digits += src[i];
        advance();
      }
      int64_t value;
      try {
        value = std::stoll(digits);
      } catch (const std::out_of_range &) {
        throw CompileError(start, "the pebble " + digits + " is too heavy to carry (max is 9223372036854775807)");
      }
      toks.push_back({TokKind::Number, digits, value, start});
      continue;
    }

    bool matched = false;
    for (const char *op : twoCharOps) {
      if (c == op[0] && at(i + 1) == op[1]) {
        advance();
        advance();
        toks.push_back({TokKind::Op, op, 0, start});
        matched = true;
        break;
      }
    }
    if (matched)
      continue;

    if (oneCharOps.find(c) != std::string::npos) {
      advance();
      toks.push_back({TokKind::Op, std::string(1, c), 0, start});
      continue;
    }

    throw CompileError(start, std::string("the otter doesn't know what '") + c + "' is");
  }

  toks.push_back({TokKind::Eof, "", 0, loc});
  return toks;
}

} // namespace urchin
