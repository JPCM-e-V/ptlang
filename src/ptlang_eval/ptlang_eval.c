#include "ptlang_eval_impl.h"

ptlang_ast_exp ptlang_eval_const_exp(ptlang_ast_exp exp, ptlang_context *ctx)
{
    // LLVMContextRef C = LLVMContextCreate();
    // LLVMModuleRef M = LLVMModuleCreateWithNameInContext("ptlang_eval", C);

    // // LLVMTypeRef type = ptlang_ir_builder_type(exp->ast_type, NULL, C);

    // LLVMTypeRef func_type =
    //     LLVMFunctionType(LLVMVoidTypeInContext(C), (LLVMTypeRef[]){LLVMPointerTypeInContext(C, 0)}, 1,
    //     false);

    // LLVMValueRef function = LLVMAddFunction(M, "main", func_type);

    // LLVMBuilderRef B = LLVMCreateBuilderInContext(C);

    // LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(C, function, "entry");

    // ptlang_ir_builder_build_context cxt = {
    //     .builder = B,
    //     .module = M,
    //     .function = function,
    //     .target_info = ctx->target_data_layout,
    // };
    // LLVMPositionBuilderAtEnd(B, entry);

    // LLVMValueRef value = ptlang_ir_builder_exp(exp, &cxt);
    // LLVMBuildStore(B, value, LLVMGetParam(function, 0));
    // LLVMBuildRetVoid(B);

    // LLVMLinkInInterpreter();

    // LLVMExecutionEngineRef ee;
    // // LLVMCreateExecutionEngineForModule(&ee, M, NULL);
    // // LLVMCreateJITCompilerForModule(&ee, M, )
    // LLVMCreateInterpreterForModule(&ee, M, NULL);

    uint32_t bit_size = ptlang_rc_deref(ptlang_rc_deref(exp).ast_type).type == PTLANG_AST_TYPE_INTEGER
                            ? ptlang_rc_deref(ptlang_rc_deref(exp).ast_type).content.integer.size
                            : ptlang_rc_deref(ptlang_rc_deref(exp).ast_type).content.float_size;

    uint8_t *binary = memset(ptlang_malloc((bit_size - 1) / 8 + 1), 0x6b, (bit_size - 1) / 8 + 1);

    // arrsetlen(binary, size);

    // LLVMGenericValueRef in_llvm_binary = LLVMCreateGenericValueOfPointer(binary);
    // LLVMRunFunction(ee, function, 1, &in_llvm_binary);
    // LLVMDisposeExecutionEngine(ee);

    // // LLVMDisposeModule(M);
    // LLVMContextDispose(C);

    return ptlang_ast_exp_binary_new(binary, exp);
}

uint32_t ptlang_eval_calc_byte_size(ptlang_ast_type type)
{
    // uint32_t bit_size =
    //     type->type == PTLANG_AST_TYPE_INTEGER ? type->content.integer.size : type->content.float_size;
    return (((ptlang_rc_deref(type).type == PTLANG_AST_TYPE_INTEGER
                  ? ptlang_rc_deref(type).content.integer.size
                  : ptlang_rc_deref(type).content.float_size) -
             1) >>
            3) +
           1;
}
