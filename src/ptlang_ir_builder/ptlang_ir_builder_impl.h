#pragma once

#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/BinaryFormat/Dwarf.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/TargetParser/Host.h>

#include "stb_ds.h"
#include <llvm/IR/NoFolder.h>

#define FOLDER llvm::NoFolder
#include "ptlang_eval.h"
#include "ptlang_ir_builder_llvm.h"

extern "C"
{
#include <stddef.h>
    static void ptlang_ir_builder_module(ptlang_ast_module module, ptlang_ir_builder_context *ctx);

    static void ptlang_ir_builder_struct_defs(ptlang_ast_struct_def *struct_defs,
                                              ptlang_ir_builder_context *ctx);

    static llvm::GlobalVariable *ptlang_ir_builder_decl_decl(ptlang_ast_decl decl,
                                                             ptlang_ir_builder_context *ctx);

    ptlang_ir_builder_scope_entry *ptlang_ir_builder_scope_get(char *name, ptlang_ir_builder_scope *scope);
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

    static llvm::Value *ptlang_ir_builder_exp_and_cast(ptlang_ast_exp exp, ptlang_ast_type type,
                                                       ptlang_ir_builder_fun_ctx *ctx);
    static llvm::Value *ptlang_ir_builder_cast(llvm::Value *input, ptlang_ast_type from, ptlang_ast_type to,
                                               ptlang_ir_builder_context *ctx);
    static llvm::Value *ptlang_ir_builder_exp_ptr(ptlang_ast_exp exp, ptlang_ir_builder_fun_ctx *ctx);
    static void ptlang_ir_builder_stmt(ptlang_ast_stmt stmt, ptlang_ir_builder_fun_ctx *ctx);

    static llvm::DIType *ptlang_ir_builder_di_function_type(ptlang_ast_type ast_type,
                                                            ptlang_ir_builder_context *ctx);
    static llvm::DIType *ptlang_ir_builder_di_type(ptlang_ast_type ast_type, ptlang_ir_builder_context *ctx);

    static void ptlang_ir_builder_scope_end(ptlang_ir_builder_scope *scope, ptlang_ir_builder_context *ctx);

    static void ptlang_ir_builder_scope_end_children(ptlang_ir_builder_scope *scope,
                                                     ptlang_ir_builder_context *ctx);

    static unsigned int ptlang_ir_builder_get_struct_index(char *member_name, ptlang_ast_type type,
                                                           ptlang_ir_builder_context *ctx);
    static llvm::StructType *ptlang_ir_builder_get_heap_array_struct(ptlang_ast_type ast_type,
                                                                     ptlang_ir_builder_context *ctx);
}
