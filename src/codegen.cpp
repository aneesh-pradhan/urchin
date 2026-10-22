#include "codegen.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include <map>

using namespace llvm;

namespace urchin {

namespace {

class CodeGen {
public:
  CodeGen(LLVMContext &ctx, const std::string &name)
      : ctx(ctx), mod(std::make_unique<Module>(name, ctx)), b(ctx), i64(Type::getInt64Ty(ctx)) {}

  std::unique_ptr<Module> run(const Program &prog) {
    // Declare every dive first so they can call each other in any order.
    for (const Dive &d : prog.dives)
      declare(d);
    for (const Dive &d : prog.dives)
      define(d);
    defineMain(prog.topLevel);
    return std::move(mod);
  }

private:
  LLVMContext &ctx;
  std::unique_ptr<Module> mod;
  IRBuilder<> b;
  Type *i64;

  std::vector<std::map<std::string, AllocaInst *>> scopes;
  bool inDive = false;
  FunctionCallee printfFn;
  Value *splashFmt = nullptr;

  // Prefix user dives so they can never clash with main, printf, or libc.
  static std::string mangle(const std::string &name) { return "urchin." + name; }

  llvm::Function *currentFn() { return b.GetInsertBlock()->getParent(); }

  void declare(const Dive &d) {
    if (mod->getFunction(mangle(d.name)))
      throw CompileError(d.loc, "there's already a dive called '" + d.name + "'");
    std::vector<Type *> params(d.params.size(), i64);
    auto *fn = llvm::Function::Create(FunctionType::get(i64, params, false), GlobalValue::InternalLinkage,
                                      mangle(d.name), *mod);
    unsigned idx = 0;
    for (Argument &arg : fn->args())
      arg.setName(d.params[idx++]);
  }

  void define(const Dive &d) {
    llvm::Function *fn = mod->getFunction(mangle(d.name));
    b.SetInsertPoint(BasicBlock::Create(ctx, "entry", fn));
    inDive = true;
    scopes.assign(1, {});

    for (Argument &arg : fn->args())
      b.CreateStore(&arg, declareVar(d.loc, arg.getName().str()));
    genStmts(d.body);

    // Falling off the end of a dive surfaces 0.
    if (!b.GetInsertBlock()->getTerminator())
      b.CreateRet(ConstantInt::get(i64, 0));
    verify(*fn);
  }

  void defineMain(const Block &topLevel) {
    auto *fn = llvm::Function::Create(FunctionType::get(b.getInt32Ty(), false), GlobalValue::ExternalLinkage,
                                      "main", *mod);
    b.SetInsertPoint(BasicBlock::Create(ctx, "entry", fn));
    inDive = false;
    scopes.assign(1, {});
    genStmts(topLevel);
    b.CreateRet(b.getInt32(0));
    verify(*fn);
  }

  void verify(llvm::Function &fn) {
    std::string err;
    raw_string_ostream os(err);
    if (verifyFunction(fn, &os))
      throw std::logic_error("internal compiler error: invalid IR in '" + fn.getName().str() + "':\n" + err);
  }

  AllocaInst *declareVar(Loc loc, const std::string &name) {
    if (scopes.back().count(name))
      throw CompileError(loc, "'" + name + "' is already stashed here");
    // Allocas go in the entry block so mem2reg can promote them to registers.
    BasicBlock &entry = currentFn()->getEntryBlock();
    IRBuilder<> tmp(&entry, entry.begin());
    AllocaInst *slot = tmp.CreateAlloca(i64, nullptr, name);
    scopes.back()[name] = slot;
    return slot;
  }

  AllocaInst *lookup(Loc loc, const std::string &name) {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
      auto found = it->find(name);
      if (found != it->end())
        return found->second;
    }
    throw CompileError(loc, "no otter has stashed anything called '" + name + "'");
  }

  void branchTo(BasicBlock *bb) {
    if (!b.GetInsertBlock()->getTerminator())
      b.CreateBr(bb);
  }

  void genStmts(const Block &block) {
    for (const StmtPtr &s : block)
      genStmt(*s);
  }

  void genBlock(const Block &block) {
    scopes.emplace_back();
    genStmts(block);
    scopes.pop_back();
  }

  void genStmt(const Stmt &stmt) {
    if (auto *s = dynamic_cast<const StashStmt *>(&stmt)) {
      // Evaluate first so `stash x = x + 1;` can read an outer x.
      Value *init = genExpr(*s->init);
      b.CreateStore(init, declareVar(s->loc, s->name));
    } else if (auto *s = dynamic_cast<const AssignStmt *>(&stmt)) {
      b.CreateStore(genExpr(*s->value), lookup(s->loc, s->name));
    } else if (auto *s = dynamic_cast<const IfStmt *>(&stmt)) {
      genIf(*s);
    } else if (auto *s = dynamic_cast<const PaddleStmt *>(&stmt)) {
      genPaddle(*s);
    } else if (auto *s = dynamic_cast<const SurfaceStmt *>(&stmt)) {
      if (!inDive)
        throw CompileError(s->loc, "you can only surface from inside a dive");
      b.CreateRet(genExpr(*s->value));
      // Anything after a surface is unreachable; give it a block to live in.
      b.SetInsertPoint(BasicBlock::Create(ctx, "after.surface", currentFn()));
    } else if (auto *s = dynamic_cast<const SplashStmt *>(&stmt)) {
      genSplash(*s);
    } else if (auto *s = dynamic_cast<const ExprStmt *>(&stmt)) {
      genExpr(*s->expr);
    }
  }

  void genIf(const IfStmt &s) {
    Value *cond = b.CreateICmpNE(genExpr(*s.cond), ConstantInt::get(i64, 0), "cond");
    llvm::Function *fn = currentFn();
    auto *thenBB = BasicBlock::Create(ctx, "then", fn);
    auto *elseBB = BasicBlock::Create(ctx, "else", fn);
    auto *mergeBB = BasicBlock::Create(ctx, "endif", fn);
    b.CreateCondBr(cond, thenBB, elseBB);

    b.SetInsertPoint(thenBB);
    genBlock(s.then);
    branchTo(mergeBB);

    b.SetInsertPoint(elseBB);
    genBlock(s.otherwise);
    branchTo(mergeBB);

    b.SetInsertPoint(mergeBB);
  }

  void genPaddle(const PaddleStmt &s) {
    Value *start = genExpr(*s.from);
    Value *end = genExpr(*s.to);

    scopes.emplace_back();
    AllocaInst *slot = declareVar(s.loc, s.var);
    b.CreateStore(start, slot);

    llvm::Function *fn = currentFn();
    auto *condBB = BasicBlock::Create(ctx, "paddle.cond", fn);
    auto *bodyBB = BasicBlock::Create(ctx, "paddle.body", fn);
    auto *stepBB = BasicBlock::Create(ctx, "paddle.step", fn);
    auto *afterBB = BasicBlock::Create(ctx, "paddle.end", fn);
    b.CreateBr(condBB);

    b.SetInsertPoint(condBB);
    Value *iv = b.CreateLoad(i64, slot, s.var);
    b.CreateCondBr(b.CreateICmpSLT(iv, end), bodyBB, afterBB);

    b.SetInsertPoint(bodyBB);
    genBlock(s.body);
    branchTo(stepBB);

    b.SetInsertPoint(stepBB);
    Value *next = b.CreateAdd(b.CreateLoad(i64, slot), ConstantInt::get(i64, 1), "next");
    b.CreateStore(next, slot);
    b.CreateBr(condBB);

    b.SetInsertPoint(afterBB);
    scopes.pop_back();
  }

  void genSplash(const SplashStmt &s) {
    if (!splashFmt) {
      splashFmt = b.CreateGlobalString("%lld\n", "splash.fmt", 0, mod.get());
      printfFn = mod->getOrInsertFunction("printf", FunctionType::get(b.getInt32Ty(), {b.getPtrTy()}, true));
    }
    b.CreateCall(printfFn, {splashFmt, genExpr(*s.value)});
  }

  Value *genExpr(const Expr &expr) {
    if (auto *e = dynamic_cast<const NumberExpr *>(&expr))
      return ConstantInt::get(i64, e->value, /*isSigned=*/true);
    if (auto *e = dynamic_cast<const VarExpr *>(&expr))
      return b.CreateLoad(i64, lookup(e->loc, e->name), e->name);
    if (auto *e = dynamic_cast<const NegExpr *>(&expr))
      return b.CreateNeg(genExpr(*e->operand), "neg");
    if (auto *e = dynamic_cast<const BinaryExpr *>(&expr))
      return genBinary(*e);
    if (auto *e = dynamic_cast<const CallExpr *>(&expr))
      return genCall(*e);
    throw std::logic_error("internal compiler error: unknown expression");
  }

  Value *genBinary(const BinaryExpr &e) {
    Value *l = genExpr(*e.lhs);
    Value *r = genExpr(*e.rhs);
    const std::string &op = e.op;
    if (op == "+") return b.CreateAdd(l, r, "add");
    if (op == "-") return b.CreateSub(l, r, "sub");
    if (op == "*") return b.CreateMul(l, r, "mul");
    if (op == "/") return b.CreateSDiv(l, r, "div");
    if (op == "%") return b.CreateSRem(l, r, "rem");

    // Comparisons produce 1 or 0.
    CmpInst::Predicate pred;
    if (op == "==") pred = CmpInst::ICMP_EQ;
    else if (op == "!=") pred = CmpInst::ICMP_NE;
    else if (op == "<") pred = CmpInst::ICMP_SLT;
    else if (op == ">") pred = CmpInst::ICMP_SGT;
    else if (op == "<=") pred = CmpInst::ICMP_SLE;
    else if (op == ">=") pred = CmpInst::ICMP_SGE;
    else throw std::logic_error("internal compiler error: unknown operator '" + op + "'");
    return b.CreateZExt(b.CreateICmp(pred, l, r, "cmp"), i64);
  }

  Value *genCall(const CallExpr &e) {
    llvm::Function *fn = mod->getFunction(mangle(e.callee));
    if (!fn)
      throw CompileError(e.loc, "there's no dive called '" + e.callee + "'");
    if (fn->arg_size() != e.args.size())
      throw CompileError(e.loc, "'" + e.callee + "' takes " + std::to_string(fn->arg_size()) + " pebble(s), but got " +
                                    std::to_string(e.args.size()));
    std::vector<Value *> args;
    for (const ExprPtr &arg : e.args)
      args.push_back(genExpr(*arg));
    return b.CreateCall(fn, args, "call");
  }
};

} // namespace

std::unique_ptr<Module> codegen(const Program &prog, LLVMContext &ctx, const std::string &moduleName) {
  return CodeGen(ctx, moduleName).run(prog);
}

} // namespace urchin
