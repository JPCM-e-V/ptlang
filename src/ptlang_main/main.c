#include "ptlang_parser.h"

int main()
{
    ptlang_ast_module mod;
    ptlang_parser_parse(stdin, &mod);
    ptlang_ast_module_destroy(mod);
}
