#include "ptlang_ir_builder_impl.h"

extern "C"
{

    void ptlang_ir_builder_dump_module(ptlang_ast_module module, ptlang_context *context)
    {
        llvm::LLVMContext llvm_ctx = llvm::LLVMContext();
        ptlang_ir_builder_context ctx = {
            /*.builder =*/llvm::IRBuilder<>(llvm_ctx),
            /*.module_ =*/llvm::Module("name", llvm_ctx),
            /*.ctx =*/context,
        };

        // ctx.ctx = context;

        // ctx.module_("name", llvm_ctx);

        // builder->Context

        // delete ctx.builder;
        // delete ctx.module_;
        // delete llvm_ctx;
    }

    void ptlang_ir_builder_module(ptlang_ast_module module, ptlang_ir_builder_context *ctx)
    {
        ptlang_ir_builder_struct_defs(ptlang_rc_deref(module).struct_defs, ctx);
    }

    void ptlang_ir_builder_struct_defs(ptlang_ast_struct_def *struct_defs, ptlang_ir_builder_context *ctx)
    {
        // Create Struct Types
        for (size_t i = 0; i < arrlenu(struct_defs); i++)
        {
            llvm::StringRef name = llvm::StringRef(ptlang_rc_deref(struct_defs[i]).name.name);
            llvm::StructType *llvmStructType = llvm::StructType::create(ctx->builder.getContext(), name);
        }

        // Fill Struct Types
        for (size_t i = 0; i < arrlenu(struct_defs); i++)
        {
            for (size_t j = 0; j < arrlenu(ptlang_rc_deref(struct_defs[i]).members); j++)
            {
                llvm::Type *type = ptlang_ir_builder_type(
                    ptlang_rc_deref(ptlang_rc_deref(struct_defs[i]).members[j]).type, ctx);
            }
        }
    }

    llvm::Value *ptlang_ir_builder_scope_get(char *name, ptlang_ir_builder_scope *scope)
    {
        while (scope != NULL)
        {
            ptlang_ir_builder_scope_variable *variable = shgetp_null(scope->variables, name);
            if (variable != NULL)
                return variable->value;
            scope = scope->parent;
        }
        abort();
    }

    llvm::Type *ptlang_ir_builder_type(ptlang_ast_type ast_type, ptlang_ir_builder_context *ctx)
    {
        ast_type = ptlang_context_unname_type(ast_type, ctx->ctx->type_scope);
        // TODO
        switch (ptlang_rc_deref(ast_type).type)
        {
        case ptlang_ast_type_s::PTLANG_AST_TYPE_VOID:
            return llvm::Type::getVoidTy(ctx->builder.getContext());
        case ptlang_ast_type_s::PTLANG_AST_TYPE_INTEGER:
            break;
        case ptlang_ast_type_s::PTLANG_AST_TYPE_FLOAT:
            break;
        case ptlang_ast_type_s::PTLANG_AST_TYPE_FUNCTION:
            break;
        case ptlang_ast_type_s::PTLANG_AST_TYPE_HEAP_ARRAY:
            break;
        case ptlang_ast_type_s::PTLANG_AST_TYPE_ARRAY:
            break;
        case ptlang_ast_type_s::PTLANG_AST_TYPE_REFERENCE:
            break;
        case ptlang_ast_type_s::PTLANG_AST_TYPE_NAMED:
            return shget(ctx->structs, ptlang_rc_deref(ast_type).content.name);
        }
        abort();
    }
}