#pragma once

#include "parser.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

namespace urchin {

std::unique_ptr<llvm::Module> codegen(const Program &prog, llvm::LLVMContext &ctx, const std::string &moduleName);

} // namespace urchin
