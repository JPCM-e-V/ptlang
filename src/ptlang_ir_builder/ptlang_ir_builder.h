#pragma once
#include <llvm-c/Core.h>
#include "ptlang_ast.h"

LLVMModuleRef ptlang_ir_builder_module(ptlang_ast_module module);
