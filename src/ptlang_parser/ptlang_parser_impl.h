#ifndef PTLANG_PARSER_IMPL_H
#define PTLANG_PARSER_IMPL_H

#include "ptlang_parser.h"

#include "parser.h"

#ifndef YYSTYPE
#define YYSTYPE PTLANG_YYSTYPE
#endif
#ifndef YYLTYPE
#define YYLTYPE PTLANG_YYLTYPE
#endif

#ifndef PTLANG_PARSER_DO_NOT_INCLUDE_LEXER_H
#include "lexer.h"
#endif

void ptlang_yyerror(PTLANG_YYLTYPE *yylloc, char const *s);

extern ptlang_ast_module *ptlang_parser_module_out;

#endif
