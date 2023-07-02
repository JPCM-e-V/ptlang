#pragma once
#include "ptlang_ast.h"
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>

LLVMModuleRef ptlang_ir_builder_module(ptlang_ast_module module, LLVMTargetDataRef target_info);
