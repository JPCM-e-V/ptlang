#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <llvm-c/Core.h>
#include "ptlang_ast_impl.h"

LLVMModuleRef ptlang_ir_builder_module(ptlang_ast_module module);
