#include "ptlang_context.h"
#include "ptlang_error.h"
#include "ptlang_ir_builder.h"
#include "ptlang_parser.h"
#include "ptlang_verify.h"

#include <inttypes.h>

#include <stb_ds.h>

void print_error(ptlang_error error)
{
    fprintf(stderr, "%s from %" PRIu64 ":%" PRIu64 " to %" PRIu64 ":%" PRIu64 " : %s\n",
            ptlang_error_type_name(error.type), error.pos.from_line, error.pos.from_column, error.pos.to_line,
            error.pos.to_column, error.message);
}

void handle_errors(ptlang_error *errors)
{
    for (size_t i = 0; i < arrlenu(errors); i++)
    {
        print_error(errors[i]);
        ptlang_free(errors[i].message);
    }
    arrfree(errors);
}

int main(void)
{
    ptlang_ast_module mod;

    // char *triple = LLVMGetDefaultTargetTriple();

    // LLVMInitializeNativeTarget();

    // LLVMTargetRef target;
    // LLVMGetTargetFromTriple(triple, &target, NULL);

    // LLVMTargetMachineRef machine = LLVMCreateTargetMachine(
    //     target, triple, "generic", "", LLVMCodeGenLevelDefault, LLVMRelocDefault, LLVMCodeModelDefault);

    // // LLVMCreateTargetDataLayout
    // LLVMTargetDataRef target_data_layout = LLVMCreateTargetDataLayout(machine);

    ptlang_context ctx = {
        .is_big_endian = false,
        .pointer_bytes = 8,
    };
    ptlang_error *syntax_errors = ptlang_parser_parse(stdin, &mod);

    bool errors = false;

    if (arrlenu(syntax_errors) != 0)
    {
        handle_errors(syntax_errors);
        errors = true;
        // ptlang_rc_remove_ref(mod, ptlang_ast_module_destroy);
        // exit(1);
    }
    ptlang_error *semantic_errors = ptlang_verify_module(mod, &ctx);
    if (arrlenu(semantic_errors) != 0)
    {
        handle_errors(semantic_errors);
        errors = true;
    }

    if(errors) { 
        ptlang_rc_remove_ref(mod, ptlang_ast_module_destroy);
        exit(1);
    }

    ptlang_ir_builder_dump_module(mod, &ctx);

    ptlang_context_destory(&ctx);

    // LLVMModuleRef llvmmod = ptlang_ir_builder_module(mod, target_data_layout);
    // LLVMDumpModule(llvmmod);
    // LLVMDisposeTargetData(target_data_layout);
    // char *error = NULL;
    // LLVMVerifyModule(llvmmod, LLVMAbortProcessAction, &error);
    // LLVMDisposeMessage(error);

    // LLVMSetTarget(llvmmod, triple);

    // LLVMDisposeMessage(triple);

    // printf("\n ============= unopt =============\n\n");

    // LLVMDumpModule(llvmmod);

    // // #ifndef NOPT
    // LLVMPassBuilderOptionsRef pbo = LLVMCreatePassBuilderOptions();

    // LLVMErrorRef err = LLVMRunPasses(llvmmod, "default<O3>", machine, pbo);
    // // err = LLVMRunPasses(llvmmod, "default<O3>", machine, pbo);

    // LLVMDisposePassBuilderOptions(pbo);

    // if (err != LLVMErrorSuccess)
    // {
    // fprintf(stderr, "ERROR lll.c Just search this string asdfghjkl\n");
    // fprintf(stderr, "%s\n", LLVMGetErrorMessage(err));
    // }

    // // #endif

    // printf("\n ============== opt ==============\n\n");

    // // printf("%p", err);

    // LLVMDumpModule(llvmmod);

    // printf("\n ============== end ==============\n\n");

    // #ifdef WIN32
    // #    define ASM_FILE "t.asm"
    // #    define OBJ_FILE "t.obj"
    // #else
    // #    define ASM_FILE "t.S"
    // #    define OBJ_FILE "t.o"
    // #endif

    // LLVMInitializeNativeAsmPrinter();

    // LLVMTargetMachineEmitToFile(machine, llvmmod, ASM_FILE, LLVMAssemblyFile, &error);
    // LLVMTargetMachineEmitToFile(machine, llvmmod, OBJ_FILE, LLVMObjectFile, &error);

    // LLVMDisposeTargetMachine(machine);

    ptlang_rc_remove_ref(mod, ptlang_ast_module_destroy);

    // LLVMDisposeModule(llvmmod);
}
