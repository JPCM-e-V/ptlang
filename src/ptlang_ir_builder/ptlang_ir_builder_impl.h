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
#include <stddef.h>

    typedef struct ptlang_ir_builder_scope_entry_s
    {
        llvm::Value *ptr;
        llvm::Type *type;
    } ptlang_ir_builder_scope_entry;

    typedef struct ptlang_ir_builder_scope_variable_s
    {
        char *key;
        ptlang_ir_builder_scope_entry value;
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

    typedef struct ptlang_ir_builder_break_continue_entry_s ptlang_ir_builder_break_continue_entry;

    typedef struct ptlang_ir_builder_fun_ctx_s
    {
        ptlang_ir_builder_context *ctx;
        llvm::Function *func;
        ptlang_ir_builder_scope *func_scope;
        llvm::BasicBlock *return_block;
        llvm::Value *return_ptr;
        ptlang_ir_builder_break_continue_entry *break_continue;
    } ptlang_ir_builder_fun_ctx;

    struct ptlang_ir_builder_break_continue_entry_s
    {
        ptlang_ir_builder_break_continue_entry *parent;
        llvm::BasicBlock *break_target;
        llvm::BasicBlock *continue_target;
        ptlang_ir_builder_scope *scope;
    };

    static void ptlang_ir_builder_module(ptlang_ast_module module, ptlang_ir_builder_context *ctx);

    static void ptlang_ir_builder_struct_defs(ptlang_ast_struct_def *struct_defs,
                                              ptlang_ir_builder_context *ctx);
    static llvm::Type *ptlang_ir_builder_type(ptlang_ast_type ast_type, ptlang_ir_builder_context *ctx);
    static void ptlang_ir_builder_context_destroy(ptlang_ir_builder_context *ctx);

    static llvm::GlobalVariable *ptlang_ir_builder_decl_decl(ptlang_ast_decl decl,
                                                             ptlang_ir_builder_context *ctx);

    llvm::Value *ptlang_ir_builder_scope_get(char *name, ptlang_ir_builder_scope *scope);

#define ptlang_ir_builder_scope_init(ctx)                                                                    \
    ptlang_ir_builder_scope scope = {                                                                        \
        /*.variables=*/NULL,                                                                                 \
        /*.parent=*/(ctx)->scope,                                                                            \
    };                                                                                                       \
    (ctx)->scope = &scope;

    static void ptlang_ir_builder_scope_deinit(ptlang_ir_builder_context *ctx);

    static llvm::Function *ptlang_ir_builder_func_decl(ptlang_ast_func func, ptlang_ir_builder_context *ctx);
    static void ptlang_ir_builder_func_body(ptlang_ast_func func, llvm::Function *llvm_func,
                                            ptlang_ir_builder_context *ctx);

    static llvm::Constant *ptlang_ir_builder_exp_const(ptlang_ast_exp exp, ptlang_ir_builder_context *ctx);

    static llvm::Value *ptlang_ir_builder_exp(ptlang_ast_exp exp, ptlang_ir_builder_context *ctx);
    static llvm::Value *ptlang_ir_builder_exp_and_cast(ptlang_ast_exp exp, ptlang_ast_type type,
                                                       ptlang_ir_builder_context *ctx);
    static llvm::Value *ptlang_ir_builder_cast(llvm::Value *input, ptlang_ast_type from, ptlang_ast_type to,
                                               ptlang_ir_builder_context *ctx);
    static llvm::Value *ptlang_ir_builder_exp_ptr(ptlang_ast_exp exp, ptlang_ir_builder_context *ctx);
    static void ptlang_ir_builder_stmt(ptlang_ast_stmt stmt, ptlang_ir_builder_fun_ctx *ctx);

    static llvm::DIType *ptlang_ir_builder_di_function_type(ptlang_ast_type ast_type,
                                                            ptlang_ir_builder_context *ctx);
    static llvm::DIType *ptlang_ir_builder_di_type(ptlang_ast_type ast_type, ptlang_ir_builder_context *ctx);

    static void ptlang_ir_builder_scope_end(ptlang_ir_builder_scope *scope, ptlang_ir_builder_context *ctx);

    static void ptlang_ir_builder_scope_end_children(ptlang_ir_builder_scope *scope,
                                                     ptlang_ir_builder_context *ctx);
}
