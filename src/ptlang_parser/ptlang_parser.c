#include "ptlang_parser_impl.h"

void ptlang_yyerror(const PTLANG_YYLTYPE *yylloc, char const *s)
{
    fprintf(stderr, "error from %d:%d to %d:%d : %s\n", yylloc->first_line, yylloc->first_column,
            yylloc->last_line, yylloc->last_column, s);
}

ptlang_ast_module *ptlang_parser_module_out;

void ptlang_parser_parse(FILE *file, ptlang_ast_module *out)
{
#ifndef NDEBUG
    // ptlang_yydebug = 1;
#endif

    ptlang_yyset_in(file);
    ptlang_parser_module_out = out;
    ptlang_yyparse();
}

// str: [sSuU][1-9][0-9]{0,6}
ptlang_ast_type ptlang_parser_integer_type_of_string(char *str, const PTLANG_YYLTYPE *yylloc)
{
    bool is_signed = str[0] == 's' || str[0] == 'S';
    uint32_t size = strtoul(str + sizeof(char), NULL, 10);

    free(str);

    if (size > 1 << 23)
    {
#define max_msg_len sizeof("Size of Integer must be below 8388608, but is XXXXXXX.")
        char msg[max_msg_len];
        snprintf(msg, max_msg_len, "Size of integer must be below 8388608, but is %u.", size);
        ptlang_yyerror(yylloc, msg);
    }
    return ptlang_ast_type_integer(is_signed, size);
}
