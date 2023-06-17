%{
    #ifndef PTLANG_PARSER_DO_NOT_INCLUDE_LEXER_H
    #define PTLANG_PARSER_DO_NOT_INCLUDE_LEXER_H
    #endif

    #include "ptlang_parser_impl.h"

    #define SAVE_STR                              \
        {                                         \
            size_t str_len = strlen(yytext) + 1;  \
            yylval->str = malloc(str_len);        \
            memcpy(yylval->str, yytext, str_len); \
        }

    #define SAVE_FLOAT_SIZE(size) yylval->float_size = PTLANG_AST_TYPE_FLOAT_##size

    // https://stackoverflow.com/a/22125500
    #define YY_USER_ACTION                              \
        {                                               \
            yylloc->first_line = yylloc->last_line;     \
            yylloc->first_column = yylloc->last_column; \
            for (int i = 0; yytext[i] != '\0'; i++)     \
            {                                           \
                if (yytext[i] == '\n')                  \
                {                                       \
                    yylloc->last_line++;                \
                    yylloc->last_column = 1;            \
                }                                       \
                else                                    \
                {                                       \
                    yylloc->last_column++;              \
                }                                       \
            }                                           \
        }
%}

%option prefix="ptlang_yy"
%option noyywrap
%option bison-bridge
%option bison-locations

%option nounistd
%option nodefault
%option noinput
%option nounput
%option warn
%option always-interactive
%option yylineno

%%
"+"                                                                     { return PLUS; }
"-"                                                                     { return MINUS; }
"*"                                                                     { return STAR; }
"/"                                                                     { return SLASH; }
"%"                                                                     { return PERCENT; }
">="                                                                    { return LEQ; }
"<="                                                                    { return GEQ; }
"="                                                                     { return EQ; }
"=="                                                                    { return EQEQ; }
"!="                                                                    { return NEQ; }
"<"                                                                     { return LESSER; }
">"                                                                     { return GREATER; }
";"                                                                     { return SEMICOLON; }
"<<"                                                                    { return LEFT_SHIFT; }
">>"                                                                    { return RIGHT_SHIFT; }
"&&"                                                                    { return AND; }
"||"                                                                    { return OR; }
"!"                                                                     { return NOT; }
"&"                                                                     { return AMPERSAND; }
"|"                                                                     { return VERTICAL_BAR; }
"^"                                                                     { return CIRCUMFLEX; }
"~"                                                                     { return TILDE; }
"."                                                                     { return DOT; }
"?"                                                                     { return QUESTION_MARK; }
":"                                                                     { return COLON; }
"#"                                                                     { return HASHTAG; }
[ \t\n]                                                                 {}
[0-9]+([sSuU][1-9][0-9]{0,6})?                                          { SAVE_STR; return INT_VAL; }
([0-9]*\.[0-9]+|[0-9]+\.[0-9]*)([eE][-+]?[0-9]+)?([fF](16|32|64|128))?  { SAVE_STR; return FLOAT_VAL; }
[sSuU][1-9][0-9]{0,6}                                                   { SAVE_STR; return INT_TYPE; }
[fF]16                                                                  { SAVE_FLOAT_SIZE(16); return FLOAT_TYPE; }
[fF]32                                                                  { SAVE_FLOAT_SIZE(32); return FLOAT_TYPE; }
[fF]64                                                                  { SAVE_FLOAT_SIZE(64); return FLOAT_TYPE; }
[fF]128                                                                 { SAVE_FLOAT_SIZE(128); return FLOAT_TYPE; }
"["                                                                     { return OPEN_SQUARE_BRACKET; }
"]"                                                                     { return CLOSE_SQUARE_BRACKET; }
"("                                                                     { return OPEN_BRACKET; }
")"                                                                     { return CLOSE_BRACKET; }
"{"                                                                     { return OPEN_CURLY_BRACE; }
"}"                                                                     { return CLOSE_CURLY_BRACE; }
","                                                                     { return COMMA; }
"mod"                                                                   { return MOD; }
"if"                                                                    { return IF; }
"else"                                                                  { return ELSE; }
"while"                                                                 { return WHILE; }
"const"                                                                 { return CONST; }
"return"                                                                { return RETURN; }
"retval"                                                                { return RET_VAL; }
"break"                                                                 { return BREAK; }
"continue"                                                              { return CONTINUE; }
"typealias"                                                             { return TYPE_ALIAS; }
"struct"                                                                { return STRUCT_DEF; }
[a-zA-Z_][a-zA-Z_0-9]*                                                  { SAVE_STR; return IDENT; }
<<EOF>>                                                                 { return PTLANG_YYEOF; }
.                                                                       { ptlang_yyerror(yylloc, "Unknown token\n"); yyterminate(); };
%%
