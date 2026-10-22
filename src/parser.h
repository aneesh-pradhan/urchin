#pragma once

#include "lexer.h"

#include <memory>

namespace urchin {

// Every value in Urchin is a pebble (a 64-bit signed integer).

struct Expr {
  Loc loc;
  virtual ~Expr() = default;
};
using ExprPtr = std::unique_ptr<Expr>;

struct NumberExpr : Expr {
  int64_t value = 0;
};
struct VarExpr : Expr {
  std::string name;
};
struct NegExpr : Expr {
  ExprPtr operand;
};
struct BinaryExpr : Expr {
  std::string op;
  ExprPtr lhs, rhs;
};
struct CallExpr : Expr {
  std::string callee;
  std::vector<ExprPtr> args;
};

struct Stmt {
  Loc loc;
  virtual ~Stmt() = default;
};
using StmtPtr = std::unique_ptr<Stmt>;
using Block = std::vector<StmtPtr>;

// stash x = expr;
struct StashStmt : Stmt {
  std::string name;
  ExprPtr init;
};
// x = expr;
struct AssignStmt : Stmt {
  std::string name;
  ExprPtr value;
};
// if cond { ... } else { ... }
struct IfStmt : Stmt {
  ExprPtr cond;
  Block then, otherwise;
};
// paddle i from a to b { ... }  (b is exclusive)
struct PaddleStmt : Stmt {
  std::string var;
  ExprPtr from, to;
  Block body;
};
// surface expr;  (return)
struct SurfaceStmt : Stmt {
  ExprPtr value;
};
// splash expr;  (print)
struct SplashStmt : Stmt {
  ExprPtr value;
};
struct ExprStmt : Stmt {
  ExprPtr expr;
};

// dive name(pebble a, pebble b) -> pebble { ... }
struct Dive {
  Loc loc;
  std::string name;
  std::vector<std::string> params;
  Block body;
};

struct Program {
  std::vector<Dive> dives;
  Block topLevel; // statements outside any dive run in main
};

Program parse(const std::vector<Token> &toks);

} // namespace urchin
