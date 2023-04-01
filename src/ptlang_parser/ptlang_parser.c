#include "ptlang_parser_impl.h"

void ptlang_yyerror(PTLANG_YYLTYPE *yylloc, char const *s)
{
    fprintf(stderr, "error from %d:%d to %d:%d : %s\n", yylloc->first_line, yylloc->first_column, yylloc->last_line, yylloc->last_column, s);
}

ptlang_ast_module *ptlang_parser_module_out;

void ptlang_parser_parse(FILE *file, ptlang_ast_module *out)
{
    ptlang_yyset_in(file);
    ptlang_parser_module_out = out;
    ptlang_yyparse();
}
