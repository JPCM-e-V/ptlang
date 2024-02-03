#include "ptlang_parser_impl.h"

// ptlang_ast_module *ptlang_parser_module_out;
// ptlang_error *syntax_errors;

static void ptlang_parser_code_position_pt(ptlang_ast_code_position pos, const PTLANG_YYLTYPE *yylloc)
{
    *pos = ((ptlang_ast_code_position_s){
        .from_line = yylloc->first_line,
        .from_column = yylloc->first_column,
        .to_line = yylloc->last_line,
        .to_column = yylloc->last_column,
    });
}

void ptlang_yyerror(const PTLANG_YYLTYPE *yylloc, ptlang_ast_module out, ptlang_error **syntax_errors,
                    yyscan_t yyscanner, char const *message)
{
    // fprintf(stderr, "error from %d:%d to %d:%d : %s\n", yylloc->first_line, yylloc->first_column,
    //         yylloc->last_line, yylloc->last_column, s);

    size_t message_len = strlen(message) + 1;
    char *message_n = malloc(message_len);
    memcpy(message_n, message, message_len);

    arrput(*syntax_errors, ((ptlang_error){
                               .type = PTLANG_ERROR_SYNTAX,
                               //    .pos = *ptlang_parser_code_position(yylloc),
                               .message = message_n,
                           }));

    ptlang_parser_code_position_pt(&arrlast(*syntax_errors).pos, yylloc);

    // yyterminate();
}

ptlang_ast_code_position ptlang_parser_code_position_from_to(const PTLANG_YYLTYPE *from,
                                                             const PTLANG_YYLTYPE *to)
{
    ptlang_ast_code_position pos = ptlang_malloc(sizeof(*pos));
    *pos = ((ptlang_ast_code_position_s){
        .from_line = from->first_line,
        .from_column = from->first_column,
        .to_line = to->last_line,
        .to_column = to->last_column,
    });
    return pos;
}
ptlang_ast_code_position ptlang_parser_code_position(const PTLANG_YYLTYPE *yylloc)
{
    ptlang_ast_code_position pos = ptlang_malloc(sizeof(*pos));
    ptlang_parser_code_position_pt(pos, yylloc);
    return pos;
}

ptlang_error *ptlang_parser_parse(FILE *file, ptlang_ast_module *out)
{
#ifndef NDEBUG
    // ptlang_yydebug = 1;
#endif
    ptlang_error *syntax_errors = NULL;
    *out = ptlang_ast_module_new();

    struct ptlang_lexer_extra_data extra = {
        .out = *out,
        .syntax_errors = &syntax_errors,
    };

    yyscan_t scanner;
    ptlang_yylex_init_extra(&extra, &scanner);

    ptlang_yyset_in(file, scanner);
    // ptlang_parser_module_out = out;
    ptlang_yyparse(*out, &syntax_errors, scanner);

    ptlang_yylex_destroy(scanner);

    return syntax_errors;
}

// str: [sSuU][1-9][0-9]{0,6}
ptlang_ast_type ptlang_parser_integer_type_of_string(char *str, const PTLANG_YYLTYPE *yylloc,
                                                     ptlang_error **syntax_errors)
{
    bool is_signed = str[0] == 's' || str[0] == 'S';
    uint32_t size = strtoul(str + sizeof(char), NULL, 10);

    if (size > 1 << 23)
    {
#define max_msg_len sizeof("Size of Integer must be below 8388608, but is XXXXXXX.")
        char msg[max_msg_len];
        snprintf(msg, max_msg_len, "Size of integer must be below 8388608, but is %s.", str + sizeof(char));
#undef max_msg_len
        ptlang_free(str);
        ptlang_yyerror(yylloc, NULL, syntax_errors, NULL, msg);
    }
    else
    {
        ptlang_free(str);
    }
    return ptlang_ast_type_integer(is_signed, size, ptlang_parser_code_position(yylloc));
}

uint64_t ptlang_parser_strtouint64(char *str, const PTLANG_YYLTYPE *yylloc, ptlang_error **syntax_errors)
{
    uint64_t val = strtoull(str, NULL, 0);
    if (errno == ERANGE)
    {
        ptlang_yyerror(yylloc, NULL, syntax_errors, NULL, "Integer must be below 2^64.");
    }
    return val;
}
