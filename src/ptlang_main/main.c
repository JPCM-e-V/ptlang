#include "ptlang_parser.h"
#include "ptlang_ir_builder.h"

#include <llvm-c/Analysis.h>
#include <llvm-c/Transforms/PassBuilder.h>

int main()
{
    ptlang_ast_module mod;
    ptlang_parser_parse(stdin, &mod);
    LLVMModuleRef llvmmod = ptlang_ir_builder_module(mod);

    // LLVMDumpModule(llvmmod);

    char *error = NULL;
    LLVMVerifyModule(llvmmod, LLVMAbortProcessAction, &error);
    LLVMDisposeMessage(error);

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

#ifdef WIN32
#define ASM_FILE "t.asm"
#define OBJ_FILE "t.obj"
#else
#define ASM_FILE "t.S"
#define OBJ_FILE "t.o"
#endif

    LLVMInitializeNativeAsmPrinter();

    LLVMTargetMachineEmitToFile(machine, llvmmod, ASM_FILE, LLVMAssemblyFile, &error);
    LLVMTargetMachineEmitToFile(machine, llvmmod, OBJ_FILE, LLVMObjectFile, &error);

    ptlang_ast_module_destroy(mod);
}
