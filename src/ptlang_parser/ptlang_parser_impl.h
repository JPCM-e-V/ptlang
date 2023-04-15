#ifndef PTLANG_PARSER_IMPL_H
#define PTLANG_PARSER_IMPL_H

#include "ptlang_parser.h"

#include "parser.h"

#ifndef YYSTYPE
# define YYSTYPE PTLANG_YYSTYPE
#endif

#ifndef YYLTYPE
# define YYLTYPE PTLANG_YYLTYPE
#endif


#ifndef PTLANG_PARSER_DO_NOT_INCLUDE_LEXER_H
# ifndef YY_NO_UNISTD_H
#  define YY_NO_UNISTD_H
# endif
# include "lexer.h"
#endif

void ptlang_yyerror(const PTLANG_YYLTYPE *yylloc, char const *s);

// str: [sSuU][1-9][0-9]{0,6}
ptlang_ast_type ptlang_parser_integer_type_of_string(char *str, const PTLANG_YYLTYPE *yylloc);

extern ptlang_ast_module *ptlang_parser_module_out;

#endif
