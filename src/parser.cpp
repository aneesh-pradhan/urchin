#include "parser.h"

#include <algorithm>

namespace urchin {

namespace {

template <class T> std::unique_ptr<T> node(Loc loc) {
  auto n = std::make_unique<T>();
  n->loc = loc;
  return n;
}

class Parser {
public:
  explicit Parser(const std::vector<Token> &toks) : toks(toks) {}

  Program parseProgram() {
    Program prog;
    while (peek().kind != TokKind::Eof) {
      if (check("dive"))
        prog.dives.push_back(parseDive());
      else
        prog.topLevel.push_back(parseStmt());
    }
    return prog;
  }

private:
  const std::vector<Token> &toks;
  size_t pos = 0;

  const Token &peek(size_t ahead = 0) const { return toks[std::min(pos + ahead, toks.size() - 1)]; }

  bool check(const char *text) const {
    const Token &t = peek();
    return (t.kind == TokKind::Keyword || t.kind == TokKind::Op) && t.text == text;
  }

  bool accept(const char *text) {
    if (!check(text))
      return false;
    pos++;
    return true;
  }

  [[noreturn]] void fail(const Token &t, const std::string &msg) const {
    std::string found = t.kind == TokKind::Eof ? "the end of the file" : "'" + t.text + "'";
    throw CompileError(t.loc, msg + ", but found " + found);
  }

  void expect(const char *text) {
    if (!accept(text))
      fail(peek(), std::string("expected '") + text + "'");
  }

  std::string expectIdent(const char *what) {
    if (peek().kind != TokKind::Ident)
      fail(peek(), std::string("expected ") + what);
    return toks[pos++].text;
  }

  Dive parseDive() {
    Dive d;
    d.loc = peek().loc;
    expect("dive");
    d.name = expectIdent("a name for this dive");
    expect("(");
    if (!check(")")) {
      do {
        expect("pebble");
        d.params.push_back(expectIdent("a parameter name"));
      } while (accept(","));
    }
    expect(")");
    expect("->");
    expect("pebble");
    d.body = parseBlock();
    return d;
  }

  Block parseBlock() {
    expect("{");
    Block block;
    while (!check("}")) {
      if (peek().kind == TokKind::Eof)
        fail(peek(), "expected '}' to close this block");
      block.push_back(parseStmt());
    }
    expect("}");
    return block;
  }

  StmtPtr parseStmt() {
    Loc loc = peek().loc;

    if (accept("stash")) {
      auto s = node<StashStmt>(loc);
      s->name = expectIdent("a name to stash");
      expect("=");
      s->init = parseExpr();
      expect(";");
      return s;
    }
    if (accept("if"))
      return parseIf(loc);
    if (accept("paddle")) {
      auto s = node<PaddleStmt>(loc);
      s->var = expectIdent("a loop variable");
      expect("from");
      s->from = parseExpr();
      expect("to");
      s->to = parseExpr();
      s->body = parseBlock();
      return s;
    }
    if (accept("surface")) {
      auto s = node<SurfaceStmt>(loc);
      s->value = parseExpr();
      expect(";");
      return s;
    }
    if (accept("splash")) {
      auto s = node<SplashStmt>(loc);
      s->value = parseExpr();
      expect(";");
      return s;
    }
    if (peek().kind == TokKind::Ident && peek(1).kind == TokKind::Op && peek(1).text == "=") {
      auto s = node<AssignStmt>(loc);
      s->name = toks[pos].text;
      pos += 2;
      s->value = parseExpr();
      expect(";");
      return s;
    }

    auto s = node<ExprStmt>(loc);
    s->expr = parseExpr();
    expect(";");
    return s;
  }

  StmtPtr parseIf(Loc loc) {
    auto s = node<IfStmt>(loc);
    s->cond = parseExpr();
    s->then = parseBlock();
    if (accept("else")) {
      if (check("if")) {
        Loc elifLoc = peek().loc;
        pos++;
        s->otherwise.push_back(parseIf(elifLoc));
      } else {
        s->otherwise = parseBlock();
      }
    }
    return s;
  }

  ExprPtr parseExpr() { return parseBinary(0); }

  // Lowest to highest precedence.
  ExprPtr parseBinary(size_t level) {
    static const std::vector<std::vector<std::string>> levels = {
        {"==", "!="},
        {"<", ">", "<=", ">="},
        {"+", "-"},
        {"*", "/", "%"},
    };
    if (level == levels.size())
      return parseUnary();

    ExprPtr lhs = parseBinary(level + 1);
    for (;;) {
      const Token &t = peek();
      const auto &ops = levels[level];
      if (t.kind != TokKind::Op || std::find(ops.begin(), ops.end(), t.text) == ops.end())
        return lhs;
      pos++;
      auto bin = node<BinaryExpr>(t.loc);
      bin->op = t.text;
      bin->lhs = std::move(lhs);
      bin->rhs = parseBinary(level + 1);
      lhs = std::move(bin);
    }
  }

  ExprPtr parseUnary() {
    Loc loc = peek().loc;
    if (accept("-")) {
      auto e = node<NegExpr>(loc);
      e->operand = parseUnary();
      return e;
    }
    return parsePrimary();
  }

  ExprPtr parsePrimary() {
    const Token &t = peek();
    if (t.kind == TokKind::Number) {
      pos++;
      auto e = node<NumberExpr>(t.loc);
      e->value = t.number;
      return e;
    }
    if (t.kind == TokKind::Ident) {
      pos++;
      if (accept("(")) {
        auto call = node<CallExpr>(t.loc);
        call->callee = t.text;
        if (!check(")")) {
          do {
            call->args.push_back(parseExpr());
          } while (accept(","));
        }
        expect(")");
        return call;
      }
      auto e = node<VarExpr>(t.loc);
      e->name = t.text;
      return e;
    }
    if (accept("(")) {
      ExprPtr e = parseExpr();
      expect(")");
      return e;
    }
    fail(t, "expected an expression");
  }
};

} // namespace

Program parse(const std::vector<Token> &toks) { return Parser(toks).parseProgram(); }

} // namespace urchin
