#include "src/ptlang_parser/ptlang_parser.h"

int main()
{
    ptlang_ast_module mod;
    ptlang_parser_parse(stdin, &mod);
    printf("hi\n");
}
