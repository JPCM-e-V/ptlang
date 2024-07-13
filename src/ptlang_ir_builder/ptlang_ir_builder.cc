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
        ptlang_ir_builder_fun_ctx fun_ctx = {ctx, llvm_func};

        llvm::BasicBlock::Create(ctx->llvm_ctx, "entry", llvm_func);

        ctx->builder.SetInsertPointPastAllocas(fun_ctx.func);

        ptlang_ir_builder_scope_init(ctx);

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
        // TODO end lifetimes

        for (size_t i = 0; i < shlenu(ctx->scope->variables); i++)
        {
            ptlang_ir_builder_scope_entry scope_entry = ctx->scope->variables[i].value;
            ctx->builder.CreateIntrinsic(
                llvm::Type::getVoidTy(ctx->llvm_ctx), llvm::Intrinsic::lifetime_end,
                {llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx->llvm_ctx),
                                        ctx->ctx->data_layout->getTypeStoreSize(scope_entry.type)),
                 scope_entry.ptr});
        }

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
            llvm::Type *member_types[2] = {
                llvm::PointerType::getUnqual(
                    ptlang_ir_builder_type(ptlang_rc_deref(ast_type).content.heap_array.type, ctx)),
                llvm::IntegerType::get(ctx->llvm_ctx, ctx->ctx->pointer_bytes >> 3)};
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
            return shget(ctx->structs, ptlang_rc_deref(ast_type).content.name);
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
        // TODO: typedef
        size_t type_name_len = ptlang_context_type_to_string(ast_type, NULL, ctx->ctx->type_scope);
        char *type_name_cstr = (char *)ptlang_malloc(type_name_len);
        ptlang_context_type_to_string(ast_type, type_name_cstr, ctx->ctx->type_scope);
        llvm::StringRef type_name = type_name_cstr;

        // ast_type = ptlang_context_unname_type(ast_type, ctx->ctx->type_scope);

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
        case ptlang_ast_exp_s::PTLANG_AST_EXP_VARIABLE:
        {
        }
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

                    indices[i] = llvm::ConstantInt::get(
                        llvm::IntegerType::get(ctx->llvm_ctx, ctx->ctx->pointer_bytes >> 3), j, false);

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
}