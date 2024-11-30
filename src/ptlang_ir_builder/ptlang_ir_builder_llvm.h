
#include "ptlang_ir_builder.h"

#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/IRBuilder.h>

extern "C"
{

    typedef struct ptlang_ir_builder_scope_entry_s
    {
        llvm::Value *ptr;
        llvm::Type *type;
        bool direct;
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

    struct ptlang_ir_builder_struct_entry_s
    {
        llvm::StructType *type;
        ptlang_ast_struct_def def;
    };

    typedef struct ptlang_ir_builder_struct_s
    {
        char *key;
        struct ptlang_ir_builder_struct_entry_s value;
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

        llvm::Type *integer_ptrsize_type;

        llvm::FunctionCallee malloc_func;
        llvm::FunctionCallee realloc_func;
        llvm::FunctionCallee free_func;

    } ptlang_ir_builder_context;

    typedef struct ptlang_ir_builder_break_continue_entry_s ptlang_ir_builder_break_continue_entry;

    struct ptlang_ir_builder_break_continue_entry_s
    {
        ptlang_ir_builder_break_continue_entry *parent;
        llvm::BasicBlock *break_target;
        llvm::BasicBlock *continue_target;
        ptlang_ir_builder_scope *scope;
    };

    typedef struct ptlang_ir_builder_fun_ctx_s
    {
        ptlang_ir_builder_context *ctx;
        llvm::Function *func;
        ptlang_ir_builder_scope *func_scope;
        llvm::BasicBlock *return_block;
        llvm::Value *return_ptr;
        ptlang_ir_builder_break_continue_entry *break_continue;
    } ptlang_ir_builder_fun_ctx;

    llvm::Type *ptlang_ir_builder_type(ptlang_ast_type ast_type, ptlang_ir_builder_context *ctx);
    void ptlang_ir_builder_context_destroy(ptlang_ir_builder_context *ctx);
    llvm::Value *ptlang_ir_builder_exp(ptlang_ast_exp exp, ptlang_ir_builder_fun_ctx *ctx);
}

#define ptlang_ir_builder_make_ctx(variable, ptlang_context)                                                 \
    llvm::LLVMContext llvm_ctx = llvm::LLVMContext();                                                        \
                                                                                                             \
    ptlang_ir_builder_scope global_scope = {};                                                               \
                                                                                                             \
    ptlang_ir_builder_context variable = {                                                                   \
        /*.builder =*/llvm::IRBuilder<>(llvm_ctx),                                                           \
        /*.module_ =*/llvm::Module("name", llvm_ctx),                                                        \
        /*.llvm_ctx =*/llvm_ctx,                                                                             \
        /*.ctx =*/ptlang_context,                                                                            \
        /*.scope =*/&global_scope,                                                                           \
        /*.di_file =*/llvm::DIFile::get(llvm_ctx, "test.ptl", "/tmp"),                                       \
    };                                                                                                       \
                                                                                                             \
    {                                                                                                        \
                                                                                                             \
        variable.module_.setDataLayout(ptlang_context->target_machine->createDataLayout());                  \
        variable.module_.setTargetTriple(ptlang_context->target_machine->getTargetTriple().str());           \
                                                                                                             \
        variable.di_scope = llvm::DICompileUnit::getDistinct(                                                \
            llvm_ctx, 0, variable.di_file, "ptlang 0.0.0", false, "", 0, "",                                 \
            llvm::DICompileUnit::DebugEmissionKind::FullDebug, llvm::DICompositeTypeArray(),                 \
            llvm::DIScopeArray(), llvm::DIGlobalVariableExpressionArray(), llvm::DIImportedEntityArray(),    \
            llvm::DIMacroNodeArray(), 0, false, false, llvm::DICompileUnit::DebugNameTableKind::Default,     \
            false, "", "");                                                                                  \
                                                                                                             \
        /*Add external(memory) functions*/                                                                   \
                                                                                                             \
        variable.integer_ptrsize_type =                                                                      \
            llvm::IntegerType::get(variable.llvm_ctx, variable.ctx->pointer_bytes >> 3);                     \
                                                                                                             \
        llvm::Type *ptr_type = llvm::PointerType::getUnqual(variable.llvm_ctx);                              \
                                                                                                             \
        variable.malloc_func = variable.module_.getOrInsertFunction(                                         \
            "malloc", llvm::FunctionType::get(ptr_type, variable.integer_ptrsize_type, false));              \
                                                                                                             \
        variable.realloc_func = variable.module_.getOrInsertFunction(                                        \
            "realloc", llvm::FunctionType::get(ptr_type, {ptr_type, variable.integer_ptrsize_type}, false)); \
                                                                                                             \
        variable.free_func = variable.module_.getOrInsertFunction(                                           \
            "free", llvm::FunctionType::get(llvm::Type::getVoidTy(variable.llvm_ctx), ptr_type, false));     \
    }