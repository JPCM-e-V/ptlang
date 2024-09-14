#include "ptlang_ir_builder_impl.h"

#ifndef NULL
#    define NULL nullptr
#endif

typedef __SIZE_TYPE__ size_t;

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
            /*.di_file =*/llvm::DIFile::get(llvm_ctx, "test.ptl", "/tmp"),
        };

        ctx.di_scope = llvm::DICompileUnit::getDistinct(
            llvm_ctx, 0, ctx.di_file, "ptlang 0.0.0", false, "", 0, "",
            llvm::DICompileUnit::DebugEmissionKind::FullDebug, llvm::DICompositeTypeArray(),
            llvm::DIScopeArray(), llvm::DIGlobalVariableExpressionArray(), llvm::DIImportedEntityArray(),
            llvm::DIMacroNodeArray(), 0, false, false, llvm::DICompileUnit::DebugNameTableKind::Default,
            false, "", "");

        // ctx.ctx = context;

        // ctx.module_("name", llvm_ctx);

        ptlang_ir_builder_module(module, &ctx);

#ifndef NDEBUG
        bool broken_debug_info;
        ptlang_assert(!llvm::verifyModule(ctx.module_, &llvm::dbgs(), &broken_debug_info));
        ptlang_assert(!broken_debug_info);
#endif

        ctx.module_.print(llvm::dbgs(), NULL, false, true);

        // builder->Context

        // delete ctx.builder;
        // delete ctx.module_;
        // delete llvm_ctx;

        delete ctx.ctx->data_layout;

        shfree(global_scope.variables);

        ptlang_ir_builder_context_destroy(&ctx);
    }

    static void ptlang_ir_builder_module(ptlang_ast_module module, ptlang_ir_builder_context *ctx)
    {
        // Add external (memory) functions

        ctx->integer_ptrsize_type = llvm::IntegerType::get(ctx->llvm_ctx, ctx->ctx->pointer_bytes >> 3);

        llvm::Type *ptr_type = llvm::PointerType::getUnqual(ctx->llvm_ctx);

        ctx->malloc_func = ctx->module_.getOrInsertFunction(
            "malloc", llvm::FunctionType::get(ptr_type, ctx->integer_ptrsize_type, false));

        ctx->realloc_func = ctx->module_.getOrInsertFunction(
            "realloc", llvm::FunctionType::get(ptr_type, {ptr_type, ctx->integer_ptrsize_type}, false));

        ctx->free_func = ctx->module_.getOrInsertFunction(
            "free", llvm::FunctionType::get(llvm::Type::getVoidTy(ctx->llvm_ctx), ptr_type, false));

        // Building the module (structs, globals, functions)

        ptlang_ir_builder_struct_defs(ptlang_rc_deref(module).struct_defs, ctx);

        llvm::GlobalVariable **glob_vars = (llvm::GlobalVariable **)ptlang_malloc(
            sizeof(llvm::GlobalVariable *) * arrlenu(ptlang_rc_deref(module).declarations));

        for (size_t i = 0; i < arrlenu(ptlang_rc_deref(module).declarations); i++)
        {
            glob_vars[i] = ptlang_ir_builder_decl_decl(ptlang_rc_deref(module).declarations[i], ctx);
        }

        llvm::Function **functions = (llvm::Function **)ptlang_malloc(
            sizeof(llvm::Function *) * arrlenu(ptlang_rc_deref(module).functions));
        for (size_t i = 0; i < arrlenu(ptlang_rc_deref(module).functions); i++)
        {
            functions[i] = ptlang_ir_builder_func_decl(ptlang_rc_deref(module).functions[i], ctx);
        }

        for (size_t i = 0; i < arrlenu(ptlang_rc_deref(module).declarations); i++)
        {
            glob_vars[i]->setInitializer(ptlang_ir_builder_exp_const(
                ptlang_rc_deref(ptlang_rc_deref(module).declarations[i]).init, ctx));
        }

        for (size_t i = 0; i < arrlenu(ptlang_rc_deref(module).functions); i++)
        {
            ptlang_ir_builder_func_body(ptlang_rc_deref(module).functions[i], functions[i], ctx);
        }

        ptlang_free(glob_vars);
        ptlang_free(functions);
    }

    static void ptlang_ir_builder_struct_defs(ptlang_ast_struct_def *struct_defs,
                                              ptlang_ir_builder_context *ctx)
    {
        // Create Struct Types
        for (size_t i = 0; i < arrlenu(struct_defs); i++)
        {
            llvm::StringRef name = llvm::StringRef(ptlang_rc_deref(struct_defs[i]).name.name);
            llvm::StructType *llvmStructType = llvm::StructType::create(ctx->llvm_ctx, name);

            shput(ctx->structs, ptlang_rc_deref(struct_defs[i]).name.name,
                  ((struct ptlang_ir_builder_struct_entry_s){llvmStructType, struct_defs[i]}));
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
                .type->setBody(
                    llvm::ArrayRef(member_types, arrlenu(ptlang_rc_deref(struct_defs[i]).members)));

            ptlang_free(member_types);
        }
    }

    static llvm::GlobalVariable *ptlang_ir_builder_decl_decl(ptlang_ast_decl decl,
                                                             ptlang_ir_builder_context *ctx)
    {
        llvm::Type *type = ptlang_ir_builder_type(ptlang_rc_deref(decl).type, ctx);
        // llvm::GlobalVariable()
        llvm::GlobalVariable *var = new llvm::GlobalVariable(
            ctx->module_, type, !ptlang_rc_deref(decl).writable,
            ptlang_rc_deref(decl).exported ? llvm::GlobalValue::LinkageTypes::ExternalLinkage
                                           : llvm::GlobalValue::LinkageTypes::InternalLinkage,
            NULL, ptlang_rc_deref(decl).name.name);
        shput(ctx->scope->variables, ptlang_rc_deref(decl).name.name,
              (ptlang_ir_builder_scope_entry{var, type}));

        var->addDebugInfo(llvm::DIGlobalVariableExpression::get(
            ctx->llvm_ctx,
            llvm::DIGlobalVariable::get(ctx->llvm_ctx, ctx->di_scope, ptlang_rc_deref(decl).name.name,
                                        ptlang_rc_deref(decl).name.name, ctx->di_file,
                                        ptlang_rc_deref(ptlang_rc_deref(decl).pos).from_line,
                                        ptlang_ir_builder_di_type(ptlang_rc_deref(decl).type, ctx), false,
                                        true, NULL, NULL, 0, NULL),
            llvm::DIExpression::get(ctx->llvm_ctx, llvm::ArrayRef<uint64_t>())));
        return var;
    }

    static llvm::Function *ptlang_ir_builder_func_decl(ptlang_ast_func func, ptlang_ir_builder_context *ctx)
    {
        size_t param_count = arrlenu(ptlang_rc_deref(func).parameters);
        llvm::Type **param_types = (llvm::Type **)ptlang_malloc(sizeof(llvm::Type *) * param_count);
        for (size_t i = 0; i < param_count; i++)
        {
            param_types[i] =
                ptlang_ir_builder_type(ptlang_rc_deref(ptlang_rc_deref(func).parameters[i]).type, ctx);
        }
        llvm::FunctionType *function_type =
            llvm::FunctionType::get(ptlang_ir_builder_type(ptlang_rc_deref(func).return_type, ctx),
                                    llvm::ArrayRef(param_types, param_count), false);
        ptlang_free(param_types);

        llvm::Function *function = llvm::Function::Create(
            function_type,
            ptlang_rc_deref(func).exported ? llvm::GlobalValue::LinkageTypes::ExternalLinkage
                                           : llvm::GlobalValue::LinkageTypes::InternalLinkage,
            0, ptlang_rc_deref(func).name.name, &ctx->module_);

        shput(ctx->scope->variables, ptlang_rc_deref(func).name.name,
              (ptlang_ir_builder_scope_entry{function, function_type}));

        llvm::Argument *args = function->args().begin();

        for (size_t i = 0; i < param_count; i++)
        {
            args[i].setName(ptlang_rc_deref(ptlang_rc_deref(func).parameters[i]).name.name);
        }

        return function;
    }

    static void ptlang_ir_builder_func_body(ptlang_ast_func func, llvm::Function *llvm_func,
                                            ptlang_ir_builder_context *ctx)

    {

        (ctx->llvm_ctx, "entry", llvm_func);

        ptlang_ir_builder_scope_init(ctx);

        ptlang_ir_builder_fun_ctx fun_ctx = {ctx, llvm_func, ctx->scope};

        ctx->builder.SetInsertPointPastAllocas(fun_ctx.func);

        llvm::Type *return_type;

        if (ptlang_rc_deref(ptlang_rc_deref(func).return_type).type !=
            ptlang_ast_type_s::PTLANG_AST_TYPE_VOID)
        {
            return_type = ptlang_ir_builder_type(ptlang_rc_deref(func).return_type, ctx);
            fun_ctx.return_ptr = ctx->builder.CreateAlloca(return_type, NULL, "return_ptr");
        }

        ptlang_ir_builder_scope_entry *scope_entries = (ptlang_ir_builder_scope_entry *)ptlang_malloc(
            sizeof(ptlang_ir_builder_scope_entry) * arrlenu(ptlang_rc_deref(func).parameters));

        for (size_t i = 0; i < arrlenu(ptlang_rc_deref(func).parameters); i++)
        {

            llvm::Type *type =
                ptlang_ir_builder_type(ptlang_rc_deref(ptlang_rc_deref(func).parameters[i]).type, ctx);

            scope_entries[i] = {ctx->builder.CreateAlloca(type, NULL, "arg_ptr"), type};

            shput(ctx->scope->variables, ptlang_rc_deref(ptlang_rc_deref(func).parameters[i]).name.name,
                  scope_entries[i]);
        }
        for (size_t i = 0; i < arrlenu(ptlang_rc_deref(func).parameters); i++)
        {
            ctx->builder.CreateIntrinsic(
                llvm::Type::getVoidTy(ctx->llvm_ctx), llvm::Intrinsic::lifetime_start,
                {llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx->llvm_ctx),
                                        ctx->ctx->data_layout->getTypeStoreSize(scope_entries[i].type)),
                 scope_entries[i].ptr});

            ctx->builder.CreateStore(fun_ctx.func->getArg(i), scope_entries[i].ptr);
        }

        ptlang_free(scope_entries);

        fun_ctx.return_block = llvm::BasicBlock::Create(ctx->llvm_ctx, "return", llvm_func);

        // TODO build statement
        ptlang_ir_builder_stmt(ptlang_rc_deref(func).stmt, &fun_ctx);

        ctx->builder.CreateBr(fun_ctx.return_block);

        ctx->builder.SetInsertPoint(fun_ctx.return_block);

        ptlang_ir_builder_scope_deinit(ctx);

        if (ptlang_rc_deref(ptlang_rc_deref(func).return_type).type !=
            ptlang_ast_type_s::PTLANG_AST_TYPE_VOID)
        {
            llvm::Value *ret_val = ctx->builder.CreateLoad(return_type, fun_ctx.return_ptr, "return_ptr");
            ctx->builder.CreateRet(ret_val);
        }
        else
        {
            ctx->builder.CreateRetVoid();
        }
    }

    llvm::Value *ptlang_ir_builder_scope_get(char *name, ptlang_ir_builder_scope *scope)
    {
        while (scope != NULL)
        {
            ptlang_ir_builder_scope_variable *variable = shgetp_null(scope->variables, name);
            if (variable != NULL)
                return variable->value.ptr;
            scope = scope->parent;
        }
        abort();
    }

    static void ptlang_ir_builder_scope_deinit(ptlang_ir_builder_context *ctx)
    {
        ptlang_ir_builder_scope_end(ctx->scope, ctx);

        shfree(ctx->scope->variables);
        ctx->scope = ctx->scope->parent;
    }

    static llvm::Type *ptlang_ir_builder_type(ptlang_ast_type ast_type, ptlang_ir_builder_context *ctx)
    {
        ast_type = ptlang_context_unname_type(ast_type, ctx->ctx->type_scope);
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
            llvm::Type *member_types[2] = {llvm::PointerType::getUnqual(ptlang_ir_builder_type(
                                               ptlang_rc_deref(ast_type).content.heap_array.type, ctx)),
                                           ctx->integer_ptrsize_type};
            return llvm::StructType::get(ctx->llvm_ctx, llvm::ArrayRef(member_types, 2), false);
        }
        case ptlang_ast_type_s::PTLANG_AST_TYPE_ARRAY:
            return llvm::ArrayType::get(
                ptlang_ir_builder_type(ptlang_rc_deref(ast_type).content.array.type, ctx),
                ptlang_rc_deref(ast_type).content.array.len);
        case ptlang_ast_type_s::PTLANG_AST_TYPE_REFERENCE:
            return llvm::PointerType::getUnqual(
                ptlang_ir_builder_type(ptlang_rc_deref(ast_type).content.reference.type, ctx));
        case ptlang_ast_type_s::PTLANG_AST_TYPE_NAMED:
            return shget(ctx->structs, ptlang_rc_deref(ast_type).content.name).type;
        }
        abort();
    }

    static llvm::DIType *ptlang_ir_builder_di_function_type(ptlang_ast_type ast_type,
                                                            ptlang_ir_builder_context *ctx)
    {
        size_t num_args = arrlenu(ptlang_rc_deref(ast_type).content.function.parameters);
        llvm::Metadata **ret_and_params =
            (llvm::Metadata **)ptlang_malloc(sizeof(llvm::DIType *) * (num_args + 1));
        ret_and_params[0] =
            ptlang_ir_builder_di_type(ptlang_rc_deref(ast_type).content.function.return_type, ctx);
        for (size_t i = 0; i < num_args; i++)
        {
            ret_and_params[i + 1] =
                ptlang_ir_builder_di_type(ptlang_rc_deref(ast_type).content.function.parameters[i], ctx);
        }
        return llvm::DISubroutineType::get(ctx->llvm_ctx, llvm::DINode::FlagZero, llvm::dwarf::DW_CC_normal,
                                           llvm::DITypeRefArray(llvm::MDTuple::get(
                                               ctx->llvm_ctx, llvm::ArrayRef(ret_and_params, num_args + 1))));
    }

    static llvm::DIType *ptlang_ir_builder_di_type(ptlang_ast_type ast_type, ptlang_ir_builder_context *ctx)
    {
        size_t type_name_len = ptlang_context_type_to_string(ast_type, NULL, ctx->ctx->type_scope);
        char *type_name_cstr = (char *)ptlang_malloc(type_name_len + 1);
        ptlang_context_type_to_string(ast_type, type_name_cstr, ctx->ctx->type_scope);
        llvm::StringRef type_name = type_name_cstr;

        llvm::DIType *ret_type;

        switch (ptlang_rc_deref(ast_type).type)
        {
        case ptlang_ast_type_s::PTLANG_AST_TYPE_VOID:

            ret_type = NULL;
            break;
        case ptlang_ast_type_s::PTLANG_AST_TYPE_INTEGER:

            ret_type = llvm::DIBasicType::get(ctx->llvm_ctx, llvm::dwarf::DW_TAG_base_type, type_name,
                                              ptlang_rc_deref(ast_type).content.integer.size, 0,
                                              ptlang_rc_deref(ast_type).content.integer.is_signed
                                                  ? llvm::dwarf::DW_ATE_signed
                                                  : llvm::dwarf::DW_ATE_unsigned,
                                              llvm::DINode::FlagZero);
            break;

        case ptlang_ast_type_s::PTLANG_AST_TYPE_FLOAT:
            ret_type = llvm::DIBasicType::get(ctx->llvm_ctx, llvm::dwarf::DW_TAG_base_type, type_name,
                                              ptlang_rc_deref(ast_type).content.float_size, 0,
                                              llvm::dwarf::DW_ATE_float, llvm::DINode::FlagZero);
            break;

        case ptlang_ast_type_s::PTLANG_AST_TYPE_FUNCTION:
            ret_type = llvm::DIDerivedType::get(
                ctx->llvm_ctx, llvm::dwarf::DW_TAG_pointer_type, type_name, NULL, 0, NULL,
                ptlang_ir_builder_di_function_type(ast_type, ctx), (ctx->ctx->pointer_bytes) >> 3, 0, 0,
                std::nullopt, llvm::DINode::FlagZero);
            break;

        case ptlang_ast_type_s::PTLANG_AST_TYPE_HEAP_ARRAY:
        {
            llvm::DICompositeType *heap_array = llvm::DICompositeType::get(
                ctx->llvm_ctx, llvm::dwarf::DW_TAG_member, type_name, ctx->di_file,
                ptlang_rc_deref(ptlang_rc_deref(ast_type).pos).from_line, ctx->di_scope, NULL, 0, 0, 0,
                llvm::DINode::FlagZero, llvm::DINodeArray(), 0, NULL);

            llvm::Metadata *elements[2];

            ptlang_ast_type len_type = ptlang_ast_type_integer(false, ctx->ctx->pointer_bytes >> 3, NULL);
            elements[0] = llvm::DIDerivedType::get(ctx->llvm_ctx, llvm::dwarf::DW_TAG_member, "len", NULL, 0,
                                                   heap_array, ptlang_ir_builder_di_type(len_type, ctx), 0, 0,
                                                   0, std::nullopt, llvm::DINode::FlagZero);
            ptlang_rc_remove_ref(len_type, ptlang_ast_type_destroy);

            llvm::DIType *ptr_type = llvm::DIDerivedType::get(
                ctx->llvm_ctx, llvm::dwarf::DW_TAG_pointer_type, llvm::StringRef(), NULL, 0, NULL,
                ptlang_ir_builder_di_type(ptlang_rc_deref(ast_type).content.heap_array.type, ctx), 0, 0, 0,
                std::nullopt, llvm::DINode::FlagZero);
            elements[1] =
                llvm::DIDerivedType::get(ctx->llvm_ctx, llvm::dwarf::DW_TAG_member, "ptr", NULL, 0,
                                         heap_array, ptr_type, 0, 0, 0, std::nullopt, llvm::DINode::FlagZero);

            heap_array->replaceElements(
                llvm::DINodeArray(llvm::MDTuple::get(ctx->llvm_ctx, llvm::ArrayRef(elements, 2))));

            ret_type = heap_array;
            break;
        }

        case ptlang_ast_type_s::PTLANG_AST_TYPE_ARRAY:
        {
            llvm::Metadata *index_range =
                llvm::DISubrange::get(ctx->llvm_ctx, ptlang_rc_deref(ast_type).content.array.len, 0);
            ret_type = llvm::DICompositeType::get(
                ctx->llvm_ctx, llvm::dwarf::DW_TAG_array_type, type_name, NULL, 0, NULL, NULL, 0, 0, 0,
                llvm::DINode::FlagZero,
                llvm::DINodeArray(llvm::MDTuple::get(ctx->llvm_ctx, llvm::ArrayRef(index_range))), 0, NULL);
            break;
        }

        case ptlang_ast_type_s::PTLANG_AST_TYPE_REFERENCE:
        {
            ret_type = llvm::DIDerivedType::get(
                ctx->llvm_ctx, llvm::dwarf::DW_TAG_reference_type, type_name, ctx->di_file,
                ptlang_rc_deref(ptlang_rc_deref(ast_type).pos).from_line, NULL,
                ptlang_ir_builder_di_type(ptlang_rc_deref(ast_type).content.reference.type, ctx), 0, 0, 0,
                std::nullopt, llvm::DINode::FlagZero);
            break;
        }

        case ptlang_ast_type_s::PTLANG_AST_TYPE_NAMED:
        {

            ptlang_context_type_scope_entry entry =
                shget(ctx->ctx->type_scope, ptlang_rc_deref(ast_type).content.name);
            if (entry.type == ptlang_context_type_scope_entry_s::PTLANG_CONTEXT_TYPE_SCOPE_ENTRY_STRUCT)
            {
                llvm::DICompositeType *struct_ = llvm::DICompositeType::get(
                    ctx->llvm_ctx, llvm::dwarf::DW_TAG_member, type_name, ctx->di_file,
                    ptlang_rc_deref(ptlang_rc_deref(ast_type).pos).from_line, ctx->di_scope, NULL, 0, 0, 0,
                    llvm::DINode::FlagZero, llvm::DINodeArray(), 0, NULL);

                llvm::Metadata **elements = (llvm::Metadata **)ptlang_malloc(
                    sizeof(llvm::Metadata *) * arrlenu(ptlang_rc_deref(entry.value.struct_def).members));

                for (size_t i = 0; i < arrlenu(ptlang_rc_deref(entry.value.struct_def).members); i++)
                {
                    elements[i] = llvm::DIDerivedType::get(
                        ctx->llvm_ctx, llvm::dwarf::DW_TAG_member,
                        ptlang_rc_deref(ptlang_rc_deref(entry.value.struct_def).members[i]).name.name,
                        ctx->di_file,
                        ptlang_rc_deref(
                            ptlang_rc_deref(ptlang_rc_deref(entry.value.struct_def).members[i]).pos)
                            .from_line,
                        struct_,
                        ptlang_ir_builder_di_type(
                            ptlang_rc_deref(ptlang_rc_deref(entry.value.struct_def).members[i]).type, ctx),
                        0, 0, 0, std::nullopt, llvm::DINode::FlagZero);
                }

                struct_->replaceElements(llvm::DINodeArray(llvm::MDTuple::get(
                    ctx->llvm_ctx,
                    llvm::ArrayRef(elements, arrlenu(ptlang_rc_deref(entry.value.struct_def).members)))));

                ret_type = struct_;
            }
            else
            {
                ret_type = llvm::DIDerivedType::get(
                    ctx->llvm_ctx, llvm::dwarf::DW_TAG_typedef, type_name, ctx->di_file,
                    ptlang_rc_deref(ptlang_rc_deref(entry.value.ptlang_type).pos).from_line, ctx->di_scope,
                    ptlang_ir_builder_di_type(entry.value.ptlang_type, ctx), 0, 0, 0, std::nullopt,
                    llvm::DINode::FlagZero);
            }

            break;
        }
        }
        ptlang_free(type_name_cstr);
        return ret_type;
    }

    static llvm::Constant *ptlang_ir_builder_exp_const(ptlang_ast_exp exp, ptlang_ir_builder_context *ctx)
    {
        llvm::Type *type = ptlang_ir_builder_type(ptlang_rc_deref(exp).ast_type, ctx);
        switch (ptlang_rc_deref(exp).type)
        {
        // case ptlang_ast_exp_s::PTLANG_AST_EXP_VARIABLE:
        // {
        // }
        case ptlang_ast_exp_s::PTLANG_AST_EXP_INTEGER:
            return llvm::ConstantInt::get((llvm::IntegerType *)type,
                                          ptlang_rc_deref(exp).content.str_prepresentation, 10);
        case ptlang_ast_exp_s::PTLANG_AST_EXP_FLOAT:
            return llvm::ConstantFP::get(type, ptlang_rc_deref(exp).content.str_prepresentation);

        case ptlang_ast_exp_s::PTLANG_AST_EXP_STRUCT:
        {
            size_t val_count = arrlenu(ptlang_rc_deref(exp).content.struct_.members);
            llvm::Constant **vals = (llvm::Constant **)ptlang_malloc(sizeof(llvm::Constant *) * val_count);

            for (size_t i = 0; i < val_count; i++)
            {
                vals[i] =
                    ptlang_ir_builder_exp_const(ptlang_rc_deref(exp).content.struct_.members[i].exp, ctx);
            }

            llvm::Constant *const_struct =
                llvm::ConstantStruct::get((llvm::StructType *)type, llvm::ArrayRef(vals, val_count));
            ptlang_free(vals);
            return const_struct;
        }
        case ptlang_ast_exp_s::PTLANG_AST_EXP_ARRAY:
        {
            size_t val_count = arrlenu(ptlang_rc_deref(exp).content.array.values);
            llvm::Constant **vals = (llvm::Constant **)ptlang_malloc(sizeof(llvm::Constant *) * val_count);

            for (size_t i = 0; i < val_count; i++)
            {
                vals[i] = ptlang_ir_builder_exp_const(ptlang_rc_deref(exp).content.array.values[i], ctx);
            }

            llvm::Constant *const_array =
                llvm::ConstantArray::get((llvm::ArrayType *)type, llvm::ArrayRef(vals, val_count));
            ptlang_free(vals);
            return const_array;
        }

        case ptlang_ast_exp_s::PTLANG_AST_EXP_REFERENCE:
        {
            size_t depth = 0;
            ptlang_ast_exp current_exp = ptlang_rc_deref(exp).content.reference.value;
            for (; ptlang_rc_deref(current_exp).type != ptlang_ast_exp_s::PTLANG_AST_EXP_VARIABLE; depth++)
            {
                switch (ptlang_rc_deref(current_exp).type)
                {
                case ptlang_ast_exp_s::PTLANG_AST_EXP_STRUCT_MEMBER:
                    current_exp = ptlang_rc_deref(current_exp).content.struct_member.struct_;
                    break;
                case ptlang_ast_exp_s::PTLANG_AST_EXP_ARRAY_ELEMENT:
                    current_exp = ptlang_rc_deref(current_exp).content.array_element.array;
                    break;
                default:
                    abort();
                }
            }

            llvm::Constant **indices = (llvm::Constant **)ptlang_malloc(sizeof(llvm::Constant *) * depth);

            size_t i = depth - 1;
            current_exp = ptlang_rc_deref(exp).content.reference.value;
            for (; ptlang_rc_deref(current_exp).type != ptlang_ast_exp_s::PTLANG_AST_EXP_VARIABLE; i--)
            {
                switch (ptlang_rc_deref(current_exp).type)
                {
                case ptlang_ast_exp_s::PTLANG_AST_EXP_STRUCT_MEMBER:
                {
                    char *name = ptlang_rc_deref(current_exp).content.struct_member.member_name.name;
                    ptlang_ast_struct_def struct_def = ptlang_context_get_struct_def(
                        ptlang_rc_deref(
                            ptlang_rc_deref(ptlang_rc_deref(current_exp).content.struct_member.struct_)
                                .ast_type)
                            .content.name,
                        ctx->ctx->type_scope);
                    size_t j = 0;
                    while (0 !=
                           strcmp(name, ptlang_rc_deref(ptlang_rc_deref(struct_def).members[j]).name.name))
                        j++;

                    indices[i] = llvm::ConstantInt::get(ctx->integer_ptrsize_type, j, false);

                    current_exp = ptlang_rc_deref(current_exp).content.struct_member.struct_;
                    break;
                }
                case ptlang_ast_exp_s::PTLANG_AST_EXP_ARRAY_ELEMENT:

                    indices[i] = ptlang_ir_builder_exp_const(
                        ptlang_rc_deref(current_exp).content.array_element.index, ctx);

                    current_exp = ptlang_rc_deref(current_exp).content.array_element.array;
                    break;

                default:
                    abort();
                }
            }

            llvm::Constant *gep = llvm::ConstantExpr::getGetElementPtr(
                type,
                (llvm::Constant *)ptlang_ir_builder_scope_get(
                    ptlang_rc_deref(current_exp).content.str_prepresentation, ctx->scope),
                llvm::ArrayRef(indices, depth), true);

            ptlang_free(indices);
            return gep;
        }
        case ptlang_ast_exp_s::PTLANG_AST_EXP_BINARY:
        {
            uint32_t bit_size = ptlang_rc_deref(ptlang_rc_deref(exp).ast_type).type ==
                                        ptlang_ast_type_s::PTLANG_AST_TYPE_INTEGER
                                    ? ptlang_rc_deref(ptlang_rc_deref(exp).ast_type).content.integer.size
                                    : ptlang_rc_deref(ptlang_rc_deref(exp).ast_type).content.float_size;
            uint32_t byte_size = (bit_size - 1) / 8 + 1;

            // LLVMValueRef *bytes = ptlang_malloc(sizeof(LLVMValueRef) * byte_size);

            // LLVMTypeRef byte = LLVMInt8Type();

            llvm::Type *type = llvm::IntegerType::get(ctx->llvm_ctx, bit_size);

            llvm::Constant *value = llvm::ConstantInt::get(type, 0, false);
            llvm::Constant *one = llvm::ConstantInt::get(type, 1, false);

            for (uint32_t i = 0; i < byte_size; i++)
            {
                value = llvm::ConstantExpr::getShl(value, one);
                value = llvm::ConstantExpr::getAdd(
                    value,
                    llvm::ConstantInt::get(
                        type,
                        ptlang_rc_deref(exp).content.binary[ctx->ctx->is_big_endian ? i : byte_size - i - 1],
                        false));
            }

            value = llvm::ConstantExpr::getTruncOrBitCast(
                value, ptlang_ir_builder_type(ptlang_rc_deref(exp).ast_type, ctx));
            return value;
        }
        default:
            abort();
        }
    }

    static void ptlang_ir_builder_context_destroy(ptlang_ir_builder_context *ctx) { shfree(ctx->structs); }
    void ptlang_ir_builder_store_data_layout(ptlang_context *ctx)
    {
        ctx->data_layout = new llvm::DataLayout(
            "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128");
        ctx->is_big_endian = ctx->data_layout->isBigEndian();
        ctx->pointer_bytes = ctx->data_layout->getPointerSize();
    }

    static llvm::Value *ptlang_ir_builder_exp(ptlang_ast_exp exp, ptlang_ir_builder_fun_ctx *ctx)
    {
        llvm::Value *ptr = ptlang_ir_builder_exp_ptr(exp, ctx);
        if (ptr != NULL)
        {
            return ctx->ctx->builder.CreateLoad(
                ptlang_ir_builder_type(ptlang_rc_deref(exp).ast_type, ctx->ctx), ptr, "ptrtoval");
        }

        switch (ptlang_rc_deref(exp).type)
        {
        case ptlang_ast_exp_s::PTLANG_AST_EXP_ASSIGNMENT:
        {
            llvm::Value *val = ptlang_ir_builder_exp_and_cast(
                ptlang_rc_deref(exp).content.binary_operator.right_value,
                ptlang_rc_deref(ptlang_rc_deref(exp).content.binary_operator.left_value).ast_type, ctx);
            if (ptlang_rc_deref(ptlang_rc_deref(exp).content.binary_operator.left_value).type !=
                ptlang_ast_exp_s::PTLANG_AST_EXP_LENGTH)
            {
                llvm::Value *ptr =
                    ptlang_ir_builder_exp_ptr(ptlang_rc_deref(exp).content.binary_operator.left_value, ctx);
                ctx->ctx->builder.CreateStore(val, ptr);
            }
            else
            {
                // Set Length
                // TODO init functions

                llvm::BasicBlock *lens_not_equal_block =
                    llvm::BasicBlock::Create(ctx->ctx->llvm_ctx, "setlength_not_equal", ctx->func);
                llvm::BasicBlock *already_allocated_block =
                    llvm::BasicBlock::Create(ctx->ctx->llvm_ctx, "setlength_already_allocated", ctx->func);
                llvm::BasicBlock *malloc_block =
                    llvm::BasicBlock::Create(ctx->ctx->llvm_ctx, "setlength_malloc", ctx->func);
                llvm::BasicBlock *realloc_block =
                    llvm::BasicBlock::Create(ctx->ctx->llvm_ctx, "setlength_realloc", ctx->func);
                llvm::BasicBlock *free_block =
                    llvm::BasicBlock::Create(ctx->ctx->llvm_ctx, "setlength_free", ctx->func);
                llvm::BasicBlock *end_block =
                    llvm::BasicBlock::Create(ctx->ctx->llvm_ctx, "setlength_end", ctx->func);

                // Are new and old length equal?
                llvm::Type *heap_array_type = ptlang_ir_builder_type(
                    ptlang_rc_deref(ptlang_rc_deref(exp).content.binary_operator.left_value).ast_type,
                    ctx->ctx);
                llvm::Value *heap_array_ptr =
                    ptlang_ir_builder_exp_ptr(ptlang_rc_deref(exp).content.binary_operator.left_value, ctx);
                llvm::Value *len_ptr = ctx->ctx->builder.CreateStructGEP(heap_array_type, heap_array_ptr, 1,
                                                                         "setlength_old_len_ptr");
                llvm::Value *old_len = ctx->ctx->builder.CreateLoad(ctx->ctx->integer_ptrsize_type, len_ptr,
                                                                    "setlength_old_len");
                llvm::Value *new_len =
                    ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.binary_operator.right_value, ctx);
                llvm::Value *are_equal =
                    ctx->ctx->builder.CreateICmpEQ(old_len, new_len, "setlength_lens_are_equal");
                ctx->ctx->builder.CreateCondBr(are_equal, end_block, lens_not_equal_block);

                // Is old length zero ?
                ctx->ctx->builder.SetInsertPoint(lens_not_equal_block);
                llvm::Value *elements_ptr_ptr = ctx->ctx->builder.CreateStructGEP(
                    heap_array_type, heap_array_ptr, 0, "setlength_elements_ptr_ptr");
                ctx->ctx->builder.CreateStore(new_len, len_ptr); // Set new length
                llvm::Value *zero = llvm::ConstantInt::get(ctx->ctx->integer_ptrsize_type, 0, false);
                llvm::Value *old_len_is_zero =
                    ctx->ctx->builder.CreateICmpEQ(old_len, zero, "setlength_old_len_is_zero");
                ctx->ctx->builder.CreateCondBr(old_len_is_zero, malloc_block, already_allocated_block);

                // Is new length zero ?
                ctx->ctx->builder.SetInsertPoint(already_allocated_block);
                llvm::Value *old_elements_ptr =
                    ctx->ctx->builder.CreateLoad(llvm::PointerType::getUnqual(ctx->ctx->llvm_ctx),
                                                 elements_ptr_ptr, "setlength_old_elements_ptr");
                llvm::Value *new_len_is_zero =
                    ctx->ctx->builder.CreateICmpEQ(new_len, zero, "setlength_new_len_is_zero");
                ctx->ctx->builder.CreateCondBr(new_len_is_zero, free_block, realloc_block);

                // New alloc
                ctx->ctx->builder.SetInsertPoint(malloc_block);
                llvm::Value *malloc =
                    ctx->ctx->builder.CreateCall(ctx->ctx->malloc_func, new_len, "setlength_malloc");
                ctx->ctx->builder.CreateStore(elements_ptr_ptr, malloc);
                ctx->ctx->builder.CreateBr(end_block);

                // Realloc
                ctx->ctx->builder.SetInsertPoint(realloc_block);
                llvm::Value *realloc = ctx->ctx->builder.CreateCall(
                    ctx->ctx->realloc_func, {old_elements_ptr, new_len}, "setlength_malloc");
                ctx->ctx->builder.CreateStore(elements_ptr_ptr, realloc);
                ctx->ctx->builder.CreateBr(end_block);

                // Free
                ctx->ctx->builder.SetInsertPoint(free_block);
                ctx->ctx->builder.CreateCall(ctx->ctx->free_func, old_elements_ptr, "setlength_free");
                ctx->ctx->builder.CreateStore(
                    elements_ptr_ptr,
                    llvm::PoisonValue::get(llvm::PointerType::getUnqual(ctx->ctx->llvm_ctx)));
                ctx->ctx->builder.CreateBr(end_block);

                ctx->ctx->builder.SetInsertPoint(end_block);
            }
            return val;
        }
        case ptlang_ast_exp_s::PTLANG_AST_EXP_ADDITION:
        {
            llvm::Value *left_operand =
                ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.binary_operator.left_value, ctx);
            llvm::Value *right_operand =
                ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.binary_operator.right_value, ctx);
            if (ptlang_rc_deref(ptlang_rc_deref(exp).ast_type).type ==
                ptlang_ast_type_s::PTLANG_AST_TYPE_INTEGER)
            {
                bool is_signed = ptlang_rc_deref(ptlang_rc_deref(exp).ast_type).content.integer.is_signed;
                return ctx->ctx->builder.CreateAdd(left_operand, right_operand, "add", false, is_signed);
            }
            else
            { // PTLANG_AST_TYPE_FLOAT
                return ctx->ctx->builder.CreateFAdd(left_operand, right_operand, "fadd");
            }
        }
        case ptlang_ast_exp_s::PTLANG_AST_EXP_SUBTRACTION:
        {
            llvm::Value *left_operand =
                ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.binary_operator.left_value, ctx);
            llvm::Value *right_operand =
                ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.binary_operator.right_value, ctx);
            if (ptlang_rc_deref(ptlang_rc_deref(exp).ast_type).type ==
                ptlang_ast_type_s::PTLANG_AST_TYPE_INTEGER)
            {
                bool is_signed = ptlang_rc_deref(ptlang_rc_deref(exp).ast_type).content.integer.is_signed;
                return ctx->ctx->builder.CreateSub(left_operand, right_operand, "sub", false, is_signed);
            }
            else
            { // PTLANG_AST_TYPE_FLOAT
                return ctx->ctx->builder.CreateFSub(left_operand, right_operand, "fsub");
            }
        }
        case ptlang_ast_exp_s::PTLANG_AST_EXP_NEGATION:
        {
            llvm::Value *operand = ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.unary_operator, ctx);
            if (ptlang_rc_deref(ptlang_rc_deref(exp).ast_type).type ==
                ptlang_ast_type_s::PTLANG_AST_TYPE_INTEGER)
            {
                bool is_signed = ptlang_rc_deref(ptlang_rc_deref(exp).ast_type).content.integer.is_signed;
                return ctx->ctx->builder.CreateNeg(operand, "neg", false, is_signed);
            }
            else
            { // PTLANG_AST_TYPE_FLOAT
                return ctx->ctx->builder.CreateFNeg(operand, "fneg");
            }
        }
        case ptlang_ast_exp_s::PTLANG_AST_EXP_MULTIPLICATION:
        {
            llvm::Value *left_operand =
                ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.binary_operator.left_value, ctx);
            llvm::Value *right_operand =
                ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.binary_operator.right_value, ctx);
            if (ptlang_rc_deref(ptlang_rc_deref(exp).ast_type).type ==
                ptlang_ast_type_s::PTLANG_AST_TYPE_INTEGER)
            {
                bool is_signed = ptlang_rc_deref(ptlang_rc_deref(exp).ast_type).content.integer.is_signed;
                return ctx->ctx->builder.CreateMul(left_operand, right_operand, "add", false, is_signed);
            }
            else
            { // PTLANG_AST_TYPE_FLOAT
                return ctx->ctx->builder.CreateFMul(left_operand, right_operand, "fmul");
            }
        }
        case ptlang_ast_exp_s::PTLANG_AST_EXP_DIVISION:
        {
            llvm::Value *left_operand =
                ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.binary_operator.left_value, ctx);
            llvm::Value *right_operand =
                ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.binary_operator.right_value, ctx);
            if (ptlang_rc_deref(ptlang_rc_deref(exp).ast_type).type ==
                ptlang_ast_type_s::PTLANG_AST_TYPE_INTEGER)
            {
                if (ptlang_rc_deref(ptlang_rc_deref(exp).ast_type).content.integer.is_signed)
                    return ctx->ctx->builder.CreateSDiv(left_operand, right_operand, "div");
                else
                    return ctx->ctx->builder.CreateUDiv(left_operand, right_operand, "div");
            }
            else
            { // PTLANG_AST_TYPE_FLOAT
                return ctx->ctx->builder.CreateFDiv(left_operand, right_operand, "fdiv");
            }
        }
        case ptlang_ast_exp_s::PTLANG_AST_EXP_MODULO:
        {
            llvm::Type *type = ptlang_ir_builder_type(ptlang_rc_deref(exp).ast_type, ctx->ctx);
            llvm::Value *left_operand = ptlang_ir_builder_exp_and_cast(
                ptlang_rc_deref(exp).content.binary_operator.left_value, ptlang_rc_deref(exp).ast_type, ctx);
            llvm::Value *right_operand = ptlang_ir_builder_exp_and_cast(
                ptlang_rc_deref(exp).content.binary_operator.right_value, ptlang_rc_deref(exp).ast_type, ctx);
            if (ptlang_rc_deref(ptlang_rc_deref(exp).ast_type).type ==
                ptlang_ast_type_s::PTLANG_AST_TYPE_INTEGER)
            {
                if (ptlang_rc_deref(ptlang_rc_deref(exp).ast_type).content.integer.is_signed)
                {
                    llvm::Constant *zero = llvm::ConstantInt::get(type, 0, true);
                    llvm::Value *rem = ctx->ctx->builder.CreateSRem(left_operand, right_operand, "modrem");
                    llvm::Value *x_or = ctx->ctx->builder.CreateXor(left_operand, right_operand, "modxor");
                    llvm::Value *is_pos = ctx->ctx->builder.CreateICmpSGT(x_or, zero, "modispos");
                    llvm::Value *is_multiple = ctx->ctx->builder.CreateICmpEQ(rem, zero, "modismultiple");
                    llvm::Value *is_pos_or_multiple =
                        ctx->ctx->builder.CreateOr(is_pos, is_multiple, "modisposormultiple");

                    llvm::Value *rem_difference = ctx->ctx->builder.CreateSelect(
                        is_pos_or_multiple, zero, right_operand, "modremdifference");

                    return ctx->ctx->builder.CreateNSWAdd(rem, rem_difference, "mod");
                }
                else
                    return ctx->ctx->builder.CreateURem(left_operand, right_operand, "mod");
            }
            else
            {
                llvm::Value *mod_1 = ctx->ctx->builder.CreateFRem(left_operand, right_operand, "mod");
                llvm::Value *mod_2 = ctx->ctx->builder.CreateFRem(left_operand, right_operand, "mod_add");
                mod_2 = ctx->ctx->builder.CreateFRem(left_operand, right_operand, "mod_add_mod");
                llvm::Value *is_neg = ctx->ctx->builder.CreateFCmpOLT(
                    left_operand, llvm::ConstantFP::get(type, 0.), "mod_is_neg");
                return ctx->ctx->builder.CreateSelect(is_neg, mod_2, mod_1, "mod_select");
            }
        }
        case ptlang_ast_exp_s::PTLANG_AST_EXP_REMAINDER:
        {
            llvm::Value *left_operand =
                ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.binary_operator.left_value, ctx);
            llvm::Value *right_operand =
                ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.binary_operator.right_value, ctx);
            if (ptlang_rc_deref(ptlang_rc_deref(exp).ast_type).type ==
                ptlang_ast_type_s::PTLANG_AST_TYPE_INTEGER)
            {
                if (ptlang_rc_deref(ptlang_rc_deref(exp).ast_type).content.integer.is_signed)
                    return ctx->ctx->builder.CreateSRem(left_operand, right_operand, "rem");
                else
                    return ctx->ctx->builder.CreateURem(left_operand, right_operand, "rem");
            }
            else
            { // PTLANG_AST_TYPE_FLOAT
                return ctx->ctx->builder.CreateFRem(left_operand, right_operand, "frem");
            }
        }

#define ptlang_ir_builder_exp_comparison(name, signed_pred, unsigned_pred, float_pred)                       \
    {                                                                                                        \
        llvm::Value *left_operand =                                                                          \
            ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.binary_operator.left_value, ctx);             \
        llvm::Value *right_operand =                                                                         \
            ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.binary_operator.right_value, ctx);            \
        if (ptlang_rc_deref(ptlang_rc_deref(exp).ast_type).type ==                                           \
            ptlang_ast_type_s::PTLANG_AST_TYPE_INTEGER)                                                      \
        {                                                                                                    \
            if (ptlang_rc_deref(ptlang_rc_deref(exp).ast_type).content.integer.is_signed)                    \
                return ctx->ctx->builder.CreateICmp(signed_pred, left_operand, right_operand, name);         \
            else                                                                                             \
                return ctx->ctx->builder.CreateICmp(unsigned_pred, left_operand, right_operand, name);       \
        }                                                                                                    \
        else                                                                                                 \
        {                                                                                                    \
            return ctx->ctx->builder.CreateFCmp(float_pred, left_operand, right_operand, name);              \
        }                                                                                                    \
    }
        case ptlang_ast_exp_s::PTLANG_AST_EXP_EQUAL:
            ptlang_ir_builder_exp_comparison("eq", llvm::ICmpInst::ICMP_EQ, llvm::ICmpInst::ICMP_EQ,
                                             llvm::FCmpInst::FCMP_OEQ);
        case ptlang_ast_exp_s::PTLANG_AST_EXP_NOT_EQUAL:
            ptlang_ir_builder_exp_comparison("ne", llvm::ICmpInst::ICMP_NE, llvm::ICmpInst::ICMP_NE,
                                             llvm::FCmpInst::FCMP_UNE);
        case ptlang_ast_exp_s::PTLANG_AST_EXP_GREATER:
            ptlang_ir_builder_exp_comparison("gt", llvm::ICmpInst::ICMP_SGT, llvm::ICmpInst::ICMP_UGT,
                                             llvm::FCmpInst::FCMP_OGT);
        case ptlang_ast_exp_s::PTLANG_AST_EXP_GREATER_EQUAL:
            ptlang_ir_builder_exp_comparison("ge", llvm::ICmpInst::ICMP_SGE, llvm::ICmpInst::ICMP_UGE,
                                             llvm::FCmpInst::FCMP_OGE);
        case ptlang_ast_exp_s::PTLANG_AST_EXP_LESS:
            ptlang_ir_builder_exp_comparison("lt", llvm::ICmpInst::ICMP_SLT, llvm::ICmpInst::ICMP_ULT,
                                             llvm::FCmpInst::FCMP_OLT);
        case ptlang_ast_exp_s::PTLANG_AST_EXP_LESS_EQUAL:
            ptlang_ir_builder_exp_comparison("le", llvm::ICmpInst::ICMP_SLE, llvm::ICmpInst::ICMP_ULE,
                                             llvm::FCmpInst::FCMP_OLE);

#undef ptlang_ir_builder_exp_comparison

        case ptlang_ast_exp_s::PTLANG_AST_EXP_LEFT_SHIFT:
        {
            llvm::Value *left_operand =
                ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.binary_operator.left_value, ctx);
            llvm::Value *right_operand =
                ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.binary_operator.right_value, ctx);
            bool is_signed = ptlang_rc_deref(ptlang_rc_deref(exp).ast_type).content.integer.is_signed;
            return ctx->ctx->builder.CreateShl(left_operand, right_operand, "shl", false, is_signed);
        }
        case ptlang_ast_exp_s::PTLANG_AST_EXP_RIGHT_SHIFT:
        {
            llvm::Value *left_operand =
                ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.binary_operator.left_value, ctx);
            llvm::Value *right_operand =
                ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.binary_operator.right_value, ctx);
            if (ptlang_rc_deref(ptlang_rc_deref(exp).ast_type).content.integer.is_signed)
                return ctx->ctx->builder.CreateAShr(left_operand, right_operand, "shr", true);
            else
                return ctx->ctx->builder.CreateLShr(left_operand, right_operand, "shr", true);
        }
        case ptlang_ast_exp_s::PTLANG_AST_EXP_AND:
        {
            llvm::Value *left_operand =
                ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.binary_operator.left_value, ctx);

            llvm::BasicBlock *start_block = ctx->ctx->builder.GetInsertBlock();

            llvm::BasicBlock *second_operand_block =
                llvm::BasicBlock::Create(ctx->ctx->llvm_ctx, "second_operand", ctx->func);
            llvm::BasicBlock *end_block = llvm::BasicBlock::Create(ctx->ctx->llvm_ctx, "end", ctx->func);

            ctx->ctx->builder.CreateCondBr(left_operand, second_operand_block, end_block);

            ctx->ctx->builder.SetInsertPoint(second_operand_block);
            llvm::Value *right_operand =
                ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.binary_operator.right_value, ctx);

            ctx->ctx->builder.SetInsertPoint(end_block);

            llvm::PHINode *phi = ctx->ctx->builder.CreatePHI(
                ptlang_ir_builder_type(ptlang_rc_deref(exp).ast_type, ctx->ctx), 2);
            phi->addIncoming(right_operand, second_operand_block);
            phi->addIncoming(llvm::ConstantInt::getFalse(ctx->ctx->llvm_ctx), start_block);
        }
        case ptlang_ast_exp_s::PTLANG_AST_EXP_OR:
        {
            llvm::Value *left_operand =
                ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.binary_operator.left_value, ctx);

            llvm::BasicBlock *start_block = ctx->ctx->builder.GetInsertBlock();

            llvm::BasicBlock *second_operand_block =
                llvm::BasicBlock::Create(ctx->ctx->llvm_ctx, "second_operand", ctx->func);
            llvm::BasicBlock *end_block = llvm::BasicBlock::Create(ctx->ctx->llvm_ctx, "end", ctx->func);

            ctx->ctx->builder.CreateCondBr(left_operand, end_block, second_operand_block);

            ctx->ctx->builder.SetInsertPoint(second_operand_block);
            llvm::Value *right_operand =
                ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.binary_operator.right_value, ctx);

            ctx->ctx->builder.SetInsertPoint(end_block);

            llvm::PHINode *phi = ctx->ctx->builder.CreatePHI(
                ptlang_ir_builder_type(ptlang_rc_deref(exp).ast_type, ctx->ctx), 2);
            phi->addIncoming(right_operand, second_operand_block);
            phi->addIncoming(llvm::ConstantInt::getTrue(ctx->ctx->llvm_ctx), start_block);
        }
        case ptlang_ast_exp_s::PTLANG_AST_EXP_NOT:
        case ptlang_ast_exp_s::PTLANG_AST_EXP_BITWISE_INVERSE:
        {
            llvm::Value *operand = ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.unary_operator, ctx);
            return ctx->ctx->builder.CreateNot(operand, "notorinverse");
        }

        case ptlang_ast_exp_s::PTLANG_AST_EXP_BITWISE_AND:
        {
            llvm::Value *left_operand =
                ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.binary_operator.left_value, ctx);
            llvm::Value *right_operand =
                ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.binary_operator.right_value, ctx);
            return ctx->ctx->builder.CreateAnd(left_operand, right_operand, "and");
        }
        case ptlang_ast_exp_s::PTLANG_AST_EXP_BITWISE_OR:
        {
            llvm::Value *left_operand =
                ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.binary_operator.left_value, ctx);
            llvm::Value *right_operand =
                ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.binary_operator.right_value, ctx);
            return ctx->ctx->builder.CreateOr(left_operand, right_operand, "or");
        }
        case ptlang_ast_exp_s::PTLANG_AST_EXP_BITWISE_XOR:
        {
            llvm::Value *left_operand =
                ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.binary_operator.left_value, ctx);
            llvm::Value *right_operand =
                ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.binary_operator.right_value, ctx);
            return ctx->ctx->builder.CreateXor(left_operand, right_operand, "xor");
        }
        break;
        case ptlang_ast_exp_s::PTLANG_AST_EXP_LENGTH:
            break;
        case ptlang_ast_exp_s::PTLANG_AST_EXP_FUNCTION_CALL:
        {
            llvm::FunctionType *function_type = (llvm::FunctionType *)ptlang_ir_builder_type(
                ptlang_rc_deref(ptlang_rc_deref(exp).content.function_call.function).ast_type, ctx->ctx);

            llvm::Value *func =
                ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.function_call.function, ctx);

            llvm::Value **args = (llvm::Value **)ptlang_malloc(
                sizeof(llvm::Value *) * arrlenu(ptlang_rc_deref(exp).content.function_call.parameters));

            for (size_t i = 0; i < arrlenu(ptlang_rc_deref(exp).content.function_call.parameters); i++)
            {
                args[i] =
                    ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.function_call.parameters[i], ctx);
            }

            llvm::Value *ret_val = ctx->ctx->builder.CreateCall(
                function_type, func,
                llvm::ArrayRef(args, arrlenu(ptlang_rc_deref(exp).content.function_call.parameters)),
                "funccall");

            ptlang_free(args);
            return ret_val;
        }
        case ptlang_ast_exp_s::PTLANG_AST_EXP_VARIABLE:
        {
            abort();
        }
        case ptlang_ast_exp_s::PTLANG_AST_EXP_INTEGER:
        case ptlang_ast_exp_s::PTLANG_AST_EXP_FLOAT:
        case ptlang_ast_exp_s::PTLANG_AST_EXP_BINARY:
        {
            return ptlang_ir_builder_exp_const(exp, ctx->ctx);
        }
        case ptlang_ast_exp_s::PTLANG_AST_EXP_STRUCT:
        {
            llvm::Value *struct_ = llvm::PoisonValue::get(
                shget(ctx->ctx->structs, ptlang_rc_deref(exp).content.struct_.type.name).type);
            for (size_t i = 0; i < arrlenu(ptlang_rc_deref(exp).content.struct_.members); i++)
            {
                struct_ = ctx->ctx->builder.CreateInsertValue(
                    struct_, ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.struct_.members[i].exp, ctx),
                    i, "buildstruct");
            }

            return struct_;
        }
        case ptlang_ast_exp_s::PTLANG_AST_EXP_ARRAY:
        {
            llvm::Value *arr = llvm::PoisonValue::get(
                ptlang_ir_builder_type(ptlang_rc_deref(exp).content.array.type, ctx->ctx));
            for (size_t i = 0; i < arrlenu(ptlang_rc_deref(exp).content.array.values); i++)
            {
                arr = ctx->ctx->builder.CreateInsertValue(
                    arr, ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.array.values[i], ctx), i,
                    "buildarray");
            }

            return arr;
        }
        case ptlang_ast_exp_s::PTLANG_AST_EXP_TERNARY:
        {
            llvm::Value *cond =
                ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.ternary_operator.condition, ctx);

            llvm::BasicBlock *ternary_then_block =
                llvm::BasicBlock::Create(ctx->ctx->llvm_ctx, "ternary_then", ctx->func);
            llvm::BasicBlock *ternary_else_block =
                llvm::BasicBlock::Create(ctx->ctx->llvm_ctx, "ternary_else", ctx->func);
            llvm::BasicBlock *ternary_end_block =
                llvm::BasicBlock::Create(ctx->ctx->llvm_ctx, "ternary_end", ctx->func);

            ctx->ctx->builder.CreateCondBr(cond, ternary_then_block, ternary_else_block);

            ctx->ctx->builder.SetInsertPoint(ternary_then_block);
            llvm::Value *then_val =
                ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.ternary_operator.if_value, ctx);
            ctx->ctx->builder.CreateBr(ternary_end_block);

            ctx->ctx->builder.SetInsertPoint(ternary_else_block);
            llvm::Value *else_val =
                ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.ternary_operator.else_value, ctx);
            ctx->ctx->builder.CreateBr(ternary_end_block);

            ctx->ctx->builder.SetInsertPoint(ternary_end_block);
            llvm::PHINode *phi = ctx->ctx->builder.CreatePHI(
                ptlang_ir_builder_type(ptlang_rc_deref(exp).ast_type, ctx->ctx), 2, "ternary_phi");

            phi->addIncoming(then_val, ternary_then_block);
            phi->addIncoming(else_val, ternary_else_block);
            return phi;
        }

            // ctx->ctx->builder.getinser
        case ptlang_ast_exp_s::PTLANG_AST_EXP_CAST:
            return ptlang_ir_builder_exp_and_cast(ptlang_rc_deref(exp).content.cast.value,
                                                  ptlang_rc_deref(exp).content.cast.type, ctx);
        case ptlang_ast_exp_s::PTLANG_AST_EXP_STRUCT_MEMBER:
        {
            ptlang_ast_type ast_type = ptlang_context_unname_type(
                ptlang_rc_deref(ptlang_rc_deref(exp).content.struct_member.struct_).ast_type,
                ctx->ctx->ctx->type_scope);

            ptlang_ast_struct_def def = shget(ctx->ctx->structs, ptlang_rc_deref(ast_type).content.name).def;

            unsigned int index = 0;
            for (; index < arrlenu(ptlang_rc_deref(def).members); index++)
            {
                if (strcmp(ptlang_rc_deref(ptlang_rc_deref(def).members[index]).name.name,
                           ptlang_rc_deref(exp).content.struct_member.member_name.name) == 0)
                {
                    break;
                }
            }

            return ctx->ctx->builder.CreateExtractValue(
                ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.struct_member.struct_, ctx), index,
                "structmember");
        }
        case ptlang_ast_exp_s::PTLANG_AST_EXP_ARRAY_ELEMENT:
        {
            llvm::BasicBlock *insert_point = ctx->ctx->builder.GetInsertBlock();

            ctx->ctx->builder.SetInsertPointPastAllocas(ctx->func);

            llvm::Type *arr_type = ptlang_ir_builder_type(
                ptlang_rc_deref(ptlang_rc_deref(exp).content.array_element.array).ast_type, ctx->ctx);

            llvm::Value *arr_ptr = ctx->ctx->builder.CreateAlloca(arr_type, NULL, "tmpalloca");

            ctx->ctx->builder.SetInsertPoint(insert_point);

            llvm::Value *arr_val =
                ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.array_element.array, ctx);

            ctx->ctx->builder.CreateIntrinsic(
                llvm::Type::getVoidTy(ctx->ctx->llvm_ctx), llvm::Intrinsic::lifetime_start,
                {llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx->ctx->llvm_ctx),
                                        ctx->ctx->ctx->data_layout->getTypeStoreSize(arr_type)),
                 arr_ptr});

            ctx->ctx->builder.CreateStore(arr_val, arr_ptr);

            llvm::Type *elem_type = ptlang_ir_builder_type(ptlang_rc_deref(exp).ast_type, ctx->ctx);

            llvm::Value *elem_ptr = ctx->ctx->builder.CreateInBoundsGEP(
                elem_type, arr_ptr,
                ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.array_element.index, ctx), "arrayelement");

            llvm::Value *elem_val = ctx->ctx->builder.CreateLoad(elem_type, elem_ptr, "loadtmpalloca");

            ctx->ctx->builder.CreateIntrinsic(
                llvm::Type::getVoidTy(ctx->ctx->llvm_ctx), llvm::Intrinsic::lifetime_end,
                {llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx->ctx->llvm_ctx),
                                        ctx->ctx->ctx->data_layout->getTypeStoreSize(arr_type)),
                 arr_ptr});
            return elem_val;
        }
        case ptlang_ast_exp_s::PTLANG_AST_EXP_REFERENCE:
            return ptlang_ir_builder_exp_ptr(ptlang_rc_deref(exp).content.reference.value, ctx);
        case ptlang_ast_exp_s::PTLANG_AST_EXP_DEREFERENCE:
            return ctx->ctx->builder.CreateLoad(
                ptlang_ir_builder_type(ptlang_rc_deref(exp).ast_type, ctx->ctx),
                ptlang_ir_builder_exp(ptlang_rc_deref(exp).content.unary_operator, ctx), "deref");
        }

        llvm::Type *type = llvm::IntegerType::get(ctx->ctx->llvm_ctx, 1);
        return llvm::ConstantInt::get(type, 0, false);
    }

    static llvm::Value *ptlang_ir_builder_exp_and_cast(ptlang_ast_exp exp, ptlang_ast_type type,
                                                       ptlang_ir_builder_fun_ctx *ctx)
    {
        llvm::Value *uncasted = ptlang_ir_builder_exp(exp, ctx);
        return ptlang_ir_builder_cast(uncasted, ptlang_rc_deref(exp).ast_type, type, ctx->ctx);
    }

    static llvm::Value *ptlang_ir_builder_cast(llvm::Value *input, ptlang_ast_type from, ptlang_ast_type to,
                                               ptlang_ir_builder_context *ctx)
    {
        // TODO
        return NULL;
    }

    static llvm::Value *ptlang_ir_builder_exp_ptr(ptlang_ast_exp exp, ptlang_ir_builder_fun_ctx *ctx)
    {
        // TODO
        return NULL;
    }

    static void ptlang_ir_builder_stmt(ptlang_ast_stmt stmt, ptlang_ir_builder_fun_ctx *ctx)
    {
        switch (ptlang_rc_deref(stmt).type)
        {
        case ptlang_ast_stmt_s::PTLANG_AST_STMT_BLOCK:
        {

            ptlang_ir_builder_scope_init(ctx->ctx);
            for (size_t i = 0; i < arrlenu(ptlang_rc_deref(stmt).content.block.stmts); i++)
            {
                ptlang_ir_builder_stmt(ptlang_rc_deref(stmt).content.block.stmts[i], ctx);
            }
            ptlang_ir_builder_scope_deinit(ctx->ctx);
            break;
        }
        case ptlang_ast_stmt_s::PTLANG_AST_STMT_EXP:
        {
            ptlang_ir_builder_exp(ptlang_rc_deref(stmt).content.exp, ctx);
            break;
        }
        case ptlang_ast_stmt_s::PTLANG_AST_STMT_DECL:
        {
            llvm::BasicBlock *insert_point = ctx->ctx->builder.GetInsertBlock();
            llvm::Type *type =
                ptlang_ir_builder_type((ptlang_rc_deref(ptlang_rc_deref(stmt).content.decl).type), ctx->ctx);
            ctx->ctx->builder.SetInsertPointPastAllocas(ctx->func);
            llvm::AllocaInst *ptr = ctx->ctx->builder.CreateAlloca(
                type, NULL, ptlang_rc_deref(ptlang_rc_deref(stmt).content.decl).name.name);
            ctx->ctx->builder.SetInsertPoint(insert_point);

            ptlang_ir_builder_scope_entry entry = {ptr, type};

            shput(ctx->ctx->scope->variables, ptlang_rc_deref(ptlang_rc_deref(stmt).content.decl).name.name,
                  entry);

            ctx->ctx->builder.CreateIntrinsic(
                llvm::Type::getVoidTy(ctx->ctx->llvm_ctx), llvm::Intrinsic::lifetime_start,
                {llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx->ctx->llvm_ctx),
                                        ctx->ctx->ctx->data_layout->getTypeStoreSize(type)),
                 ptr});

            llvm::Value *init =
                ptlang_ir_builder_exp(ptlang_rc_deref(ptlang_rc_deref(stmt).content.decl).init, ctx);
            ctx->ctx->builder.CreateStore(init, ptr);
            break;
        }
        case ptlang_ast_stmt_s::PTLANG_AST_STMT_IF:
        {
            llvm::Value *cond =
                ptlang_ir_builder_exp(ptlang_rc_deref(stmt).content.control_flow.condition, ctx);

            llvm::BasicBlock *then_block = llvm::BasicBlock::Create(ctx->ctx->llvm_ctx, "then", ctx->func);
            llvm::BasicBlock *endif_block = llvm::BasicBlock::Create(ctx->ctx->llvm_ctx, "endif", ctx->func);

            ctx->ctx->builder.CreateCondBr(cond, then_block, endif_block);
            ctx->ctx->builder.SetInsertPoint(then_block);

            ptlang_ir_builder_stmt(ptlang_rc_deref(stmt).content.control_flow.stmt, ctx);

            ctx->ctx->builder.CreateBr(endif_block);

            ctx->ctx->builder.SetInsertPoint(endif_block);
            break;
        }
        case ptlang_ast_stmt_s::PTLANG_AST_STMT_IF_ELSE:
        {
            llvm::Value *cond =
                ptlang_ir_builder_exp(ptlang_rc_deref(stmt).content.control_flow2.condition, ctx);

            llvm::BasicBlock *then_block = llvm::BasicBlock::Create(ctx->ctx->llvm_ctx, "then", ctx->func);
            llvm::BasicBlock *else_block = llvm::BasicBlock::Create(ctx->ctx->llvm_ctx, "else", ctx->func);
            llvm::BasicBlock *endif_block = llvm::BasicBlock::Create(ctx->ctx->llvm_ctx, "endif", ctx->func);

            ctx->ctx->builder.CreateCondBr(cond, then_block, else_block);
            ctx->ctx->builder.SetInsertPoint(then_block);

            ptlang_ir_builder_stmt(ptlang_rc_deref(stmt).content.control_flow2.stmt[0], ctx);

            ctx->ctx->builder.CreateBr(endif_block);

            ctx->ctx->builder.SetInsertPoint(else_block);

            ptlang_ir_builder_stmt(ptlang_rc_deref(stmt).content.control_flow2.stmt[1], ctx);

            ctx->ctx->builder.CreateBr(endif_block);

            ctx->ctx->builder.SetInsertPoint(endif_block);
            break;
        }
        case ptlang_ast_stmt_s::PTLANG_AST_STMT_WHILE:
        {
            llvm::BasicBlock *cond_block = llvm::BasicBlock::Create(ctx->ctx->llvm_ctx, "cond", ctx->func);
            llvm::BasicBlock *while_block = llvm::BasicBlock::Create(ctx->ctx->llvm_ctx, "while", ctx->func);
            llvm::BasicBlock *endwhile_block =
                llvm::BasicBlock::Create(ctx->ctx->llvm_ctx, "endwhile", ctx->func);

            ctx->ctx->builder.CreateBr(cond_block);
            ctx->ctx->builder.SetInsertPoint(cond_block);

            llvm::Value *cond =
                ptlang_ir_builder_exp(ptlang_rc_deref(stmt).content.control_flow.condition, ctx);

            ctx->ctx->builder.CreateCondBr(cond, while_block, endwhile_block);

            ctx->ctx->builder.SetInsertPoint(while_block);

            ptlang_ir_builder_break_continue_entry break_continue = {
                ctx->break_continue,
                endwhile_block,
                cond_block,
                ctx->ctx->scope,
            };
            ctx->break_continue = &break_continue;
            ptlang_ir_builder_stmt(ptlang_rc_deref(stmt).content.control_flow.stmt, ctx);
            ctx->break_continue = ctx->break_continue->parent;

            ctx->ctx->builder.CreateBr(cond_block);

            ctx->ctx->builder.SetInsertPoint(endwhile_block);
            break;
        }
        case ptlang_ast_stmt_s::PTLANG_AST_STMT_RETURN_VAL:
        {
            llvm::Value *ret_val = ptlang_ir_builder_exp(ptlang_rc_deref(stmt).content.exp, ctx);
            ctx->ctx->builder.CreateStore(ret_val, ctx->return_ptr);
        }
        case ptlang_ast_stmt_s::PTLANG_AST_STMT_RETURN:
        {

            ptlang_ir_builder_scope_end_children(ctx->func_scope, ctx->ctx);
            ctx->ctx->builder.CreateBr(ctx->return_block);
            ctx->ctx->builder.SetInsertPoint(
                llvm::BasicBlock::Create(ctx->ctx->llvm_ctx, "unreachable", ctx->func));
            break;
        }
        case ptlang_ast_stmt_s::PTLANG_AST_STMT_RET_VAL:
        {
            llvm::Value *ret_val = ptlang_ir_builder_exp(ptlang_rc_deref(stmt).content.exp, ctx);
            ctx->ctx->builder.CreateStore(ret_val, ctx->return_ptr);
            break;
        }
        case ptlang_ast_stmt_s::PTLANG_AST_STMT_BREAK:
        case ptlang_ast_stmt_s::PTLANG_AST_STMT_CONTINUE:
        {
            ptlang_ir_builder_break_continue_entry *break_continue = ctx->break_continue;
            for (uint64_t i = 0; i < ptlang_rc_deref(stmt).content.nesting_level - 1; i++)
            {
                break_continue = break_continue->parent;
            }
            ptlang_ir_builder_scope_end_children(break_continue->scope, ctx->ctx);

            ctx->ctx->builder.CreateBr(ptlang_rc_deref(stmt).type == ptlang_ast_stmt_s::PTLANG_AST_STMT_BREAK
                                           ? break_continue->break_target
                                           : break_continue->continue_target);

            ctx->ctx->builder.SetInsertPoint(
                llvm::BasicBlock::Create(ctx->ctx->llvm_ctx, "unreachable", ctx->func));
            break;
        }
        }
    }

    static void ptlang_ir_builder_scope_end(ptlang_ir_builder_scope *scope, ptlang_ir_builder_context *ctx)
    {
        for (size_t i = 0; i < shlenu(scope->variables); i++)
        {
            ptlang_ir_builder_scope_entry scope_entry = scope->variables[i].value;
            ctx->builder.CreateIntrinsic(
                llvm::Type::getVoidTy(ctx->llvm_ctx), llvm::Intrinsic::lifetime_end,
                {llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx->llvm_ctx),
                                        ctx->ctx->data_layout->getTypeStoreSize(scope_entry.type)),
                 scope_entry.ptr});
        }
    }

    static void ptlang_ir_builder_scope_end_children(ptlang_ir_builder_scope *scope,
                                                     ptlang_ir_builder_context *ctx)
    {
        for (ptlang_ir_builder_scope *cur_scope = ctx->scope; cur_scope != scope;
             cur_scope = cur_scope->parent)
            ptlang_ir_builder_scope_end(cur_scope, ctx);
    }
}
