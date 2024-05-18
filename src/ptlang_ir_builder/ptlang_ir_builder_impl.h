#pragma once

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"

#include "stb_ds.h"

#include "ptlang_ir_builder.h"
extern "C"
{

    typedef struct ptlang_ir_builder_scope_variable_s
    {
        char *key;
        llvm::Value *value;
    } ptlang_ir_builder_scope_variable;

    typedef struct ptlang_ir_builder_scope_s ptlang_ir_builder_scope;
    struct ptlang_ir_builder_scope_s
    {
        ptlang_ir_builder_scope *parent;
        ptlang_ir_builder_scope_variable *variables;
    };

    typedef struct ptlang_ir_builder_struct_s
    {
        char *key;
        llvm::Type *value;
    } ptlang_ir_builder_struct;

    typedef struct ptlang_ir_builder_context_s
    {
        llvm::IRBuilder<> builder;
        llvm::Module module_;

        ptlang_context *ctx;

        ptlang_ir_builder_scope *scope;

        ptlang_ir_builder_struct *structs;
    } ptlang_ir_builder_context;

    void ptlang_ir_builder_struct_defs(ptlang_ast_struct_def *struct_defs, ptlang_ir_builder_context *ctx);
    llvm::Type *ptlang_ir_builder_type(ptlang_ast_type ast_type, ptlang_ir_builder_context *ctx);
}
