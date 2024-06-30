#pragma once

#include <llvm/BinaryFormat/Dwarf.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/Debug.h>

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
        ptlang_ir_builder_scope_variable *variables;
        ptlang_ir_builder_scope *parent;
    };

    typedef struct ptlang_ir_builder_struct_s
    {
        char *key;
        llvm::StructType *value;
    } ptlang_ir_builder_struct;

    typedef struct ptlang_ir_builder_context_s
    {
        llvm::IRBuilder<> builder;
        llvm::Module module_;
        llvm::LLVMContext &llvm_ctx;

        ptlang_context *ctx;

        ptlang_ir_builder_scope *scope;

        llvm::DIFile *di_file;
        llvm::DIScope *di_scope;

        ptlang_ir_builder_struct *structs;
    } ptlang_ir_builder_context;

    static void ptlang_ir_builder_module(ptlang_ast_module module, ptlang_ir_builder_context *ctx);

    static void ptlang_ir_builder_struct_defs(ptlang_ast_struct_def *struct_defs,
                                              ptlang_ir_builder_context *ctx);
    static llvm::Type *ptlang_ir_builder_type(ptlang_ast_type ast_type, ptlang_ir_builder_context *ctx);
    static void ptlang_ir_builder_context_destroy(ptlang_ir_builder_context *ctx);

    static llvm::GlobalVariable *ptlang_ir_builder_decl_decl(ptlang_ast_decl decl,
                                                             ptlang_ir_builder_context *ctx);

    llvm::Value *ptlang_ir_builder_scope_get(char *name, ptlang_ir_builder_scope *scope);

    static llvm::Function *ptlang_ir_builder_func_decl(ptlang_ast_func func, ptlang_ir_builder_context *ctx);

    static llvm::Constant *ptlang_ir_builder_exp_const(ptlang_ast_exp exp, ptlang_ir_builder_context *ctx);

    static llvm::DIType *ptlang_ir_builder_di_function_type(ptlang_ast_type ast_type,
                                                            ptlang_ir_builder_context *ctx);
    static llvm::DIType *ptlang_ir_builder_di_type(ptlang_ast_type ast_type, ptlang_ir_builder_context *ctx);
}
