#include "ptlang_eval_impl.h"

ptlang_ast_exp ptlang_eval_const_exp(ptlang_ast_exp exp)
{
    LLVMContextRef C = LLVMContextCreate();
    LLVMModuleRef M = LLVMModuleCreateWithNameInContext("ptlang_eval", C);

    LLVMTypeRef type = ptlang_ir_builder_type(exp->ast_type, NULL, C);

    LLVMTypeRef func_type =
        LLVMFunctionType(LLVMVoidTypeInContext(C), (LLVMTypeRef[]){LLVMPointerTypeInContext(C, 0)}, 1, false);

    LLVMValueRef function = LLVMAddFunction(M, "main", func_type);

    LLVMTypeRef size_func_type = LLVMFunctionType(LLVMInt64TypeInContext(C), NULL, 0, false);

    LLVMValueRef size_func = LLVMAddFunction(M, "size", size_func_type);

    LLVMBuilderRef B = LLVMCreateBuilderInContext(C);

    LLVMBasicBlockRef size_entry = LLVMAppendBasicBlockInContext(C, size_func, "size_entry");

    LLVMPositionBuilderAtEnd(B, size_entry);
    LLVMValueRef size_value = LLVMSizeOf(type);
    LLVMBuildRet(B, size_value);

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

    LLVMGenericValueRef in_llvm_size = LLVMRunFunction(ee, size_func, 0, NULL);
    unsigned long long size = LLVMGenericValueToInt(in_llvm_size, false);

    uint8_t *binary = NULL;

    arrsetlen(binary, size);

    LLVMGenericValueRef in_llvm_binary = LLVMCreateGenericValueOfPointer(binary);
    LLVMRunFunction(ee, function, 1, &in_llvm_binary);
    LLVMDisposeExecutionEngine(ee);

    LLVMDisposeModule(M);
    LLVMContextDispose(C);

    return ptlang_ast_exp_binary_new(binary, exp);
}