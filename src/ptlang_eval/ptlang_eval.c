#include "ptlang_eval_impl.h"

ptlang_ast_exp ptlang_eval_const_exp(ptlang_ast_exp exp)
{
    LLVMContextRef C = LLVMContextCreate();
    LLVMModuleRef M = LLVMModuleCreateWithNameInContext("ptlang_eval", C);

    // LLVMTypeRef type = ptlang_ir_builder_type(exp->ast_type, NULL, C);

    LLVMTypeRef func_type =
        LLVMFunctionType(LLVMVoidTypeInContext(C), (LLVMTypeRef[]){LLVMPointerTypeInContext(C, 0)}, 1, false);

    LLVMValueRef function = LLVMAddFunction(M, "main", func_type);

    LLVMBuilderRef B = LLVMCreateBuilderInContext(C);

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(C, function, "entry");

    ptlang_ir_builder_build_context cxt = {
        .builder = B,
        .module = M,
        .function = function,
    };
    LLVMPositionBuilderAtEnd(B, entry);

    LLVMValueRef value = ptlang_ir_builder_exp(exp, &cxt);
    LLVMBuildStore(B, value, LLVMGetParam(function, 0));
    LLVMBuildRetVoid(B);
    LLVMLinkInInterpreter();

    LLVMExecutionEngineRef ee;
    LLVMCreateExecutionEngineForModule(&ee, M, NULL);

    uint32_t bit_size = exp->ast_type->type == PTLANG_AST_TYPE_INTEGER ? exp->ast_type->content.integer.size
                                                                       : exp->ast_type->content.float_size;

    uint8_t *binary = ptlang_malloc((bit_size - 1) / 8 + 1);

    // arrsetlen(binary, size);

    LLVMGenericValueRef in_llvm_binary = LLVMCreateGenericValueOfPointer(binary);
    LLVMRunFunction(ee, function, 1, &in_llvm_binary);
    LLVMDisposeExecutionEngine(ee);

    LLVMDisposeModule(M);
    LLVMContextDispose(C);

    return ptlang_ast_exp_binary_new(binary, exp);
}
