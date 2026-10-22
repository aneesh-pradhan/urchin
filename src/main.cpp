#include "codegen.h"

#include <llvm/ADT/SmallString.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

static int usage() {
  std::cerr << "usage: urchin <file.urc> [-o <output>] [--emit-llvm]\n";
  return 2;
}

static void reportError(const std::string &path, const std::string &src, const urchin::CompileError &e) {
  std::cerr << path << ":" << e.loc.line << ":" << e.loc.col << ": error: " << e.what() << "\n";

  std::istringstream lines(src);
  std::string line;
  for (int i = 0; i < e.loc.line; i++)
    if (!std::getline(lines, line))
      return;
  std::string caret;
  for (int i = 0; i < e.loc.col - 1 && i < static_cast<int>(line.size()); i++)
    caret += line[i] == '\t' ? '\t' : ' ';
  std::cerr << "  " << line << "\n  " << caret << "^\n";
}

int main(int argc, char **argv) {
  std::string input, output;
  bool emitLLVM = false;

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "-o" && i + 1 < argc)
      output = argv[++i];
    else if (arg == "--emit-llvm")
      emitLLVM = true;
    else if (arg == "-h" || arg == "--help")
      return usage();
    else if (arg.size() > 1 && arg[0] == '-')
      return std::cerr << "urchin: unknown option '" << arg << "'\n", usage();
    else if (input.empty())
      input = arg;
    else
      return usage();
  }
  if (input.empty())
    return usage();

  std::ifstream in(input);
  if (!in) {
    std::cerr << "urchin: can't open '" << input << "'\n";
    return 1;
  }
  std::stringstream buf;
  buf << in.rdbuf();
  std::string src = buf.str();

  if (output.empty())
    output = fs::path(input).stem().string() + (emitLLVM ? ".ll" : "");
  if (output != "-" && fs::weakly_canonical(output) == fs::weakly_canonical(input)) {
    std::cerr << "urchin: refusing to overwrite the input file '" << input << "' (use -o)\n";
    return 1;
  }

  llvm::LLVMContext ctx;
  std::unique_ptr<llvm::Module> mod;
  try {
    mod = urchin::codegen(urchin::parse(urchin::lex(src)), ctx, input);
  } catch (const urchin::CompileError &e) {
    reportError(input, src, e);
    return 1;
  }
  mod->setTargetTriple(llvm::Triple(llvm::sys::getDefaultTargetTriple()));

  if (emitLLVM) {
    if (output == "-") {
      mod->print(llvm::outs(), nullptr);
      return 0;
    }
    std::error_code ec;
    llvm::raw_fd_ostream os(output, ec);
    if (ec) {
      std::cerr << "urchin: can't write '" << output << "': " << ec.message() << "\n";
      return 1;
    }
    mod->print(os, nullptr);
    return 0;
  }

  // Hand the IR to clang to optimize, compile, and link against libc.
  auto clang = llvm::sys::findProgramByName("clang");
  if (!clang) {
    std::cerr << "urchin: clang is needed on PATH to build executables (or use --emit-llvm)\n";
    return 1;
  }

  llvm::SmallString<128> irPath;
  int fd;
  if (auto ec = llvm::sys::fs::createTemporaryFile("urchin", "ll", fd, irPath)) {
    std::cerr << "urchin: can't create a temporary file: " << ec.message() << "\n";
    return 1;
  }
  {
    llvm::raw_fd_ostream os(fd, /*shouldClose=*/true);
    mod->print(os, nullptr);
  }

  std::vector<llvm::StringRef> args = {*clang, "-O2", "-x", "ir", irPath, "-o", output};
  std::string errMsg;
  int rc = llvm::sys::ExecuteAndWait(*clang, args, std::nullopt, {}, 0, 0, &errMsg);
  (void)llvm::sys::fs::remove(irPath);
  if (rc != 0) {
    std::cerr << "urchin: clang failed" << (errMsg.empty() ? "" : ": " + errMsg) << "\n";
    return 1;
  }
  return 0;
}
