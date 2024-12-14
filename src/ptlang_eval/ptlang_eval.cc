#include "ptlang_eval_impl.h"

extern "C"
{
    ptlang_ast_exp ptlang_eval_const_exp(ptlang_ast_exp exp, ptlang_context *ctx)
    {
        // LLVMContextRef C = LLVMContextCreate();
        // LLVMModuleRef M = LLVMModuleCreateWithNameInContext("ptlang_eval", C);

        ptlang_ir_builder_make_ctx(ir_ctx, ctx);

        // // LLVMTypeRef type = ptlang_ir_builder_type(exp->ast_type, NULL, C);
        llvm::Type *type = ptlang_ir_builder_type(ptlang_rc_deref(exp).ast_type, &ir_ctx);

        // LLVMTypeRef func_type =
        //     LLVMFunctionType(LLVMVoidTypeInContext(C), (LLVMTypeRef[]){LLVMPointerTypeInContext(C, 0)}, 1,
        //     false);
        llvm::FunctionType *function_type =
            llvm::FunctionType::get(llvm::Type::getVoidTy(ir_ctx.llvm_ctx),
                                    llvm::ArrayRef<llvm::Type *>(llvm::PointerType::getUnqual(type)), false);

        // LLVMValueRef function = LLVMAddFunction(M, "main", func_type);

        llvm::Function *function = llvm::Function::Create(
            function_type, llvm::GlobalValue::LinkageTypes::ExternalLinkage, 0, "eval_func", &ir_ctx.module_);

        // LLVMBuilderRef B = LLVMCreateBuilderInContext(C);

        // LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(C, function, "entry");
        llvm::BasicBlock::Create(ir_ctx.llvm_ctx, "entry", function);

        // ptlang_ir_builder_build_context cxt = {
        //     .builder = B,
        //     .module = M,
        //     .function = function,
        //     .target_info = ctx->target_data_layout,
        // };
        ptlang_ir_builder_fun_ctx fun_ctx = {&ir_ctx, function, ir_ctx.scope};

        uint32_t byte_size = ptlang_eval_calc_byte_size(ptlang_rc_deref(exp).ast_type);

        // LLVMPositionBuilderAtEnd(B, entry);
        ir_ctx.builder.SetInsertPointPastAllocas(fun_ctx.func);

        // LLVMValueRef value = ptlang_ir_builder_exp(exp, &cxt);
        llvm::Value *value = ptlang_ir_builder_exp(exp, &fun_ctx);
        llvm::Constant *dyn_casted = llvm::dyn_cast<llvm::Constant>(value);
        if (dyn_casted != NULL)
        {
            llvm::Value *global = new llvm::GlobalVariable(
                type, true, llvm::GlobalValue::LinkageTypes::InternalLinkage, dyn_casted, "evaled_const");
            // ir_ctx.builder.CreateMemCpy(fun_ctx.func->getArg(0), std::nullopt , global, std::nullopt,
            // llvm::ConstantInt::get(ir_ctx.integer_ptrsize_type, byte_size, false));
            ir_ctx.builder.CreateMemCpy(fun_ctx.func->getArg(0), std::nullopt, global, std::nullopt,
                                        byte_size);
        }
        else
        {
            // llvm::dyn_cast
            // LLVMBuildStore(B, value, LLVMGetParam(function, 0));
            ir_ctx.builder.CreateStore(value, fun_ctx.func->getArg(0));
        }

        // LLVMBuildRetVoid(B);
        ir_ctx.builder.CreateRetVoid();

        // LLVMLinkInInterpreter();

        // LLVMExecutionEngineRef ee;
        // llvm::ExecutionEngine ee = llvm::ExecutionEngine::

#if 1
        ir_ctx.module_.print(llvm::dbgs(), NULL, false, true);
#endif
#ifndef NDEBUG
        bool broken_debug_info;
        ptlang_assert(!llvm::verifyModule(ir_ctx.module_, &llvm::dbgs(), &broken_debug_info));
        ptlang_assert(!broken_debug_info);
#endif

        // llvm::clone_module

        llvm::EngineBuilder eb = llvm::EngineBuilder(llvm::CloneModule(ir_ctx.module_));

        eb.setEngineKind(llvm::EngineKind::Interpreter);
        std::string err;
        eb.setErrorStr(&err);
        llvm::ExecutionEngine *ee = eb.create();
        if (!ee)
        {
            llvm::errs() << err;
            abort();
        }

        // // LLVMCreateExecutionEngineForModule(&ee, M, NULL);
        // // LLVMCreateJITCompilerForModule(&ee, M, )
        // LLVMCreateInterpreterForModule(&ee, M, NULL);

        // uint32_t bit_size =
        //     ptlang_rc_deref(ptlang_rc_deref(exp).ast_type).type ==
        //     ptlang_ast_type_s::PTLANG_AST_TYPE_INTEGER
        //         ? ptlang_rc_deref(ptlang_rc_deref(exp).ast_type).content.integer.size
        //         : ptlang_rc_deref(ptlang_rc_deref(exp).ast_type).content.float_size;

        uint8_t *binary = (uint8_t *)memset(ptlang_malloc(byte_size), 0x6b, byte_size);

        // arrsetlen(binary, size);

        // LLVMGenericValueRef in_llvm_binary = LLVMCreateGenericValueOfPointer(binary);
        // LLVMRunFunction(ee, function, 1, &in_llvm_binary);
        llvm::GenericValue in_llvm_bin = llvm::GenericValue(binary);
        ee->runFunction(function, in_llvm_bin);
        // LLVMDisposeExecutionEngine(ee);

        // // LLVMDisposeModule(M);
        // LLVMContextDispose(C);

        delete ee;

        // for (uint32_t i = 0; i < byte_size; i++)
        // {

        //     printf("%x\n", binary[i]);
        // }
        // printf("end\n");

        ptlang_ir_builder_context_destroy(&ir_ctx);

        return ptlang_ast_exp_binary_new(binary, exp);
    }

    uint32_t ptlang_eval_calc_byte_size(ptlang_ast_type type)
    {
        // uint32_t bit_size =
        //     type->type == PTLANG_AST_TYPE_INTEGER ? type->content.integer.size : type->content.float_size;
        return (((ptlang_rc_deref(type).type == ptlang_ast_type_s::PTLANG_AST_TYPE_INTEGER
                      ? ptlang_rc_deref(type).content.integer.size
                      : ptlang_rc_deref(type).content.float_size) -
                 1) >>
                3) +
               1;
    }
}