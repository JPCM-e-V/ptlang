#include "ptlang_parser.h"
#include "ptlang_ir_builder.h"

int main()
{
    ptlang_ast_module mod;
    ptlang_parser_parse(stdin, &mod);
    LLVMModuleRef llvmmod = ptlang_ir_builder_module(mod);

    LLVMDumpModule(llvmmod);

    ptlang_ast_module_destroy(mod);
}
