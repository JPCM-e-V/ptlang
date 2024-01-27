#ifndef PTLANG_PARSER_IMPL_H
#define PTLANG_PARSER_IMPL_H

#include "parser.h"
#include "ptlang_parser.h"
#include "ptlang_utils.h"

#include "stb_ds.h"

#include <errno.h>

#ifndef YYSTYPE
#    define YYSTYPE PTLANG_YYSTYPE
#endif

#ifndef YYLTYPE
#    define YYLTYPE PTLANG_YYLTYPE
#endif

#ifndef PTLANG_PARSER_DO_NOT_INCLUDE_LEXER_H
#    ifndef YY_NO_UNISTD_H
#        define YY_NO_UNISTD_H
#    endif
#    include "lexer.h"
#endif

void ptlang_yyerror(const PTLANG_YYLTYPE *yylloc, ptlang_ast_module out, ptlang_error **syntax_errors,
                    char const *message);

// str: [sSuU][1-9][0-9]{0,6}
ptlang_ast_type ptlang_parser_integer_type_of_string(char *str, const PTLANG_YYLTYPE *yylloc,
                                                     ptlang_error **syntax_errors);
uint64_t ptlang_parser_strtouint64(char *str, const PTLANG_YYLTYPE *yylloc, ptlang_error **syntax_errors);
ptlang_ast_code_position ptlang_parser_code_position(const PTLANG_YYLTYPE *yylloc);
ptlang_ast_code_position ptlang_parser_code_position_from_to(const PTLANG_YYLTYPE *from,
                                                             const PTLANG_YYLTYPE *to);

// extern ptlang_ast_module *ptlang_parser_module_out;

#endif
