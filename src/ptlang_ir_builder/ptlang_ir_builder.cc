#include "ptlang_ir_builder_impl.h"

extern "C"
{

    void ptlang_ir_builder_dump_module(ptlang_ast_module module, ptlang_context *context)
    {
        llvm::LLVMContext llvm_ctx = llvm::LLVMContext();

        ptlang_ir_builder_scope global_scope = {};

        ptlang_ir_builder_context ctx = {
            /*.builder =*/llvm::IRBuilder<>(llvm_ctx),
            /*.module_ =*/llvm::Module("name", llvm_ctx),
            /*.llvm_ctx =*/llvm_ctx,
            /*.ctx =*/context,
            /*.scope =*/&global_scope,
        };

        // ctx.ctx = context;

        // ctx.module_("name", llvm_ctx);

        ptlang_ir_builder_module(module, &ctx);

        ctx.module_.print(llvm::dbgs(), NULL, false, true);

        // builder->Context

        // delete ctx.builder;
        // delete ctx.module_;
        // delete llvm_ctx;

        shfree(global_scope.variables);

        ptlang_ir_builder_context_destroy(&ctx);
    }

    static void ptlang_ir_builder_module(ptlang_ast_module module, ptlang_ir_builder_context *ctx)
    {
        ptlang_ir_builder_struct_defs(ptlang_rc_deref(module).struct_defs, ctx);

        for (size_t i = 0; i < arrlenu(ptlang_rc_deref(module).declarations); i++)
        {
            ptlang_ir_builder_decl_init(ptlang_rc_deref(module).declarations[i], ctx);
        }
    }

    static void ptlang_ir_builder_struct_defs(ptlang_ast_struct_def *struct_defs,
                                              ptlang_ir_builder_context *ctx)
    {
        // Create Struct Types
        for (size_t i = 0; i < arrlenu(struct_defs); i++)
        {
            llvm::StringRef name = llvm::StringRef(ptlang_rc_deref(struct_defs[i]).name.name);
            llvm::StructType *llvmStructType = llvm::StructType::create(ctx->llvm_ctx, name);
            shput(ctx->structs, ptlang_rc_deref(struct_defs[i]).name.name, llvmStructType);
        }

        // Fill Struct Types
        for (size_t i = 0; i < arrlenu(struct_defs); i++)
        {
            llvm::Type **member_types = (llvm::Type **)ptlang_malloc(
                sizeof(llvm::Type *) * arrlenu(ptlang_rc_deref(struct_defs[i]).members));
            for (size_t j = 0; j < arrlenu(ptlang_rc_deref(struct_defs[i]).members); j++)
            {
                member_types[j] = ptlang_ir_builder_type(
                    ptlang_rc_deref(ptlang_rc_deref(struct_defs[i]).members[j]).type, ctx);
            }
            shget(ctx->structs, ptlang_rc_deref(struct_defs[i]).name.name)
                ->setBody(llvm::ArrayRef(member_types, arrlenu(ptlang_rc_deref(struct_defs[i]).members)));

            ptlang_free(member_types);
        }
    }

    static void ptlang_ir_builder_decl_init(ptlang_ast_decl decl, ptlang_ir_builder_context *ctx)
    {
        // llvm::GlobalVariable()
        stbds_shput(ctx->scope->variables, ptlang_rc_deref(decl).name.name,
                    new llvm::GlobalVariable(
                        ctx->module_, ptlang_ir_builder_type(ptlang_rc_deref(decl).type, ctx),
                        !ptlang_rc_deref(decl).writable,
                        ptlang_rc_deref(decl).exported ? llvm::GlobalValue::LinkageTypes::ExternalLinkage
                                                       : llvm::GlobalValue::LinkageTypes::InternalLinkage,
                        NULL, ptlang_rc_deref(decl).name.name));
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

    static llvm::Type *ptlang_ir_builder_type(ptlang_ast_type ast_type, ptlang_ir_builder_context *ctx)
    {
        ast_type = ptlang_context_unname_type(ast_type, ctx->ctx->type_scope);
        // TODO
        switch (ptlang_rc_deref(ast_type).type)
        {
        case ptlang_ast_type_s::PTLANG_AST_TYPE_VOID:
            return llvm::Type::getVoidTy(ctx->llvm_ctx);
        case ptlang_ast_type_s::PTLANG_AST_TYPE_INTEGER:
            return llvm::IntegerType::get(ctx->llvm_ctx, ptlang_rc_deref(ast_type).content.integer.size);
        case ptlang_ast_type_s::PTLANG_AST_TYPE_FLOAT:
            switch (ptlang_rc_deref(ast_type).content.float_size)
            {
            case ptlang_ast_type_float_size::PTLANG_AST_TYPE_FLOAT_16:
                return llvm::Type::getHalfTy(ctx->llvm_ctx);
            case ptlang_ast_type_float_size::PTLANG_AST_TYPE_FLOAT_32:
                return llvm::Type::getFloatTy(ctx->llvm_ctx);
            case ptlang_ast_type_float_size::PTLANG_AST_TYPE_FLOAT_64:
                return llvm::Type::getDoubleTy(ctx->llvm_ctx);
            case ptlang_ast_type_float_size::PTLANG_AST_TYPE_FLOAT_128:
                return llvm::Type::getFP128Ty(ctx->llvm_ctx);
            }
        case ptlang_ast_type_s::PTLANG_AST_TYPE_FUNCTION:
        {
            size_t param_count = arrlenu(ptlang_rc_deref(ast_type).content.function.parameters);
            llvm::Type **param_types = (llvm::Type **)ptlang_malloc(sizeof(llvm::Type *) * param_count);
            for (size_t i = 0; i < param_count; i++)
            {
                param_types[i] =
                    ptlang_ir_builder_type(ptlang_rc_deref(ast_type).content.function.parameters[i], ctx);
            }
            llvm::Type *function_type = llvm::FunctionType::get(
                ptlang_ir_builder_type(ptlang_rc_deref(ast_type).content.function.return_type, ctx),
                llvm::ArrayRef(param_types, param_count), false);
            ptlang_free(param_types);
            return function_type;
        }
        case ptlang_ast_type_s::PTLANG_AST_TYPE_HEAP_ARRAY:
        {
            llvm::Type *member_types[2] = {
                llvm::PointerType::getUnqual(
                    ptlang_ir_builder_type(ptlang_rc_deref(ast_type).content.heap_array.type, ctx)),
                llvm::IntegerType::get(ctx->llvm_ctx, ctx->ctx->pointer_bytes >> 3)};
            return llvm::StructType::get(ctx->llvm_ctx, llvm::ArrayRef(member_types));
        }
        case ptlang_ast_type_s::PTLANG_AST_TYPE_ARRAY:
            return llvm::ArrayType::get(
                ptlang_ir_builder_type(ptlang_rc_deref(ast_type).content.array.type, ctx),
                ptlang_rc_deref(ast_type).content.array.len);
        case ptlang_ast_type_s::PTLANG_AST_TYPE_REFERENCE:
            return llvm::PointerType::getUnqual(
                ptlang_ir_builder_type(ptlang_rc_deref(ast_type).content.reference.type, ctx));
        case ptlang_ast_type_s::PTLANG_AST_TYPE_NAMED:
            return shget(ctx->structs, ptlang_rc_deref(ast_type).content.name);
        }
        abort();
    }

    static void ptlang_ir_builder_context_destroy(ptlang_ir_builder_context *ctx) { shfree(ctx->structs); }
}