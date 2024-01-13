#pragma once
#include "ptlang_ast.h"
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>

LLVMModuleRef ptlang_ir_builder_module(ptlang_ast_module module, LLVMTargetDataRef target_info);

typedef struct ptlang_ir_builder_type_scope_s ptlang_ir_builder_type_scope;
typedef struct ptlang_ir_builder_scope_s ptlang_ir_builder_scope;

typedef struct ptlang_ir_builder_struct_def_s ptlang_ir_builder_struct_def;

typedef struct ptlang_ir_builder_build_context_s
{
    LLVMBuilderRef builder;
    LLVMModuleRef module;
    LLVMValueRef function;
    ptlang_ir_builder_type_scope *type_scope;
    ptlang_ir_builder_scope *scope;
    uint64_t scope_number;
    ptlang_ir_builder_scope **scopes;
    LLVMValueRef return_ptr;
    LLVMBasicBlockRef return_block;
    struct break_and_continue_block *break_and_continue_blocks;
    ptlang_ast_type return_type;
    ptlang_ir_builder_struct_def *struct_defs;
    LLVMTargetDataRef target_info;
    LLVMValueRef malloc_func;
    LLVMValueRef realloc_func;
    LLVMValueRef free_func;
} ptlang_ir_builder_build_context;

LLVMTypeRef ptlang_ir_builder_type(ptlang_ast_type type, ptlang_ir_builder_build_context *ctx,
                                   LLVMContextRef C);

LLVMValueRef ptlang_ir_builder_exp(ptlang_ast_exp exp, ptlang_ir_builder_build_context *ctx);
