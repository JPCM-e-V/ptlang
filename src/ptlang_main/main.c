#include "ptlang_parser.h"
#include "ptlang_ir_builder.h"

#include <llvm-c/Analysis.h>
#include <llvm-c/Transforms/PassBuilder.h>

int main()
{
    ptlang_ast_module mod;
    ptlang_parser_parse(stdin, &mod);
    LLVMModuleRef llvmmod = ptlang_ir_builder_module(mod);

    char *error =NULL;
    LLVMVerifyModule(llvmmod, LLVMAbortProcessAction, &error);
    LLVMDisposeMessage(error);

    // LLVMDumpModule(llvmmod);



    printf("\n ============= unopt =============\n\n");

    LLVMDumpModule(llvmmod);

    // #ifndef NOPT
    LLVMPassBuilderOptionsRef pbo = LLVMCreatePassBuilderOptions();

    char *triple = LLVMGetDefaultTargetTriple();

    LLVMInitializeNativeTarget();

    LLVMTargetRef target;
    LLVMGetTargetFromTriple(triple, &target, NULL);

    LLVMTargetMachineRef machine = LLVMCreateTargetMachine(target, triple, "generic", "", LLVMCodeGenLevelDefault, LLVMRelocDefault, LLVMCodeModelDefault);

    LLVMErrorRef err = LLVMRunPasses(llvmmod, "default<O3>", machine, pbo);

    if (err != LLVMErrorSuccess)
    {
        fprintf(stderr, "ERROR lll.c Just search this string asdfghjkl\n");
        fprintf(stderr, "%s\n", LLVMGetErrorMessage(err));
    }

    // #endif

    printf("\n ============== opt ==============\n\n");

    // printf("%p", err);

    LLVMDumpModule(llvmmod);

    printf("\n ============== end ==============\n\n");

    ptlang_ast_module_destroy(mod);
}
