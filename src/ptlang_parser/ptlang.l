%{
    #ifndef PTLANG_PARSER_DO_NOT_INCLUDE_LEXER_H
    #define PTLANG_PARSER_DO_NOT_INCLUDE_LEXER_H
    #endif

    #include "ptlang_parser_impl.h"

    // #define SAVE_STR                              \
    //     {                                         \
    //         size_t str_len = strlen(yytext) + 1;  \
    //         yylval->str = malloc(str_len);        \
    //         memcpy(yylval->str, yytext, str_len); \
    //     }
    #define SAVE_STR                           \
    {                                            \
        yylval->str = malloc(strlen(yytext) + 1); \
        strcpy(yylval->str, yytext);              \
    }
%}

%option prefix="ptlang_yy"
%option noyywrap
%option bison-bridge
%option bison-locations

%option nounistd
%option nodefault
%option warn
%option always-interactive

%%
">="                    { return LEQ; }
"<="                    { return GEQ; }
"="                     { return EQ; }
"=="                    { return EQEQ; }
"!="                    { return NEQ; }
"<"                     { return LESSER; }
">"                     { return GREATER; }
";"                     { return SEMICOLON; }
[ \t\n]                 {}
"f16"                  { return F16; }
"f32"                  { return F32; }
"f64"                  { return F64; }
"f128"                 { return F128; }
"["                     { return OPEN_SQUARE_BRACKET; }
"]"                     { return CLOSE_SQUARE_BRACKET; }
"("                     { return OPEN_BRACKET; }
")"                     { return CLOSE_BRACKET; }
"{"                     { return OPEN_CURLY_BRACE; }
"}"                     { return CLOSE_CURLY_BRACE; }
"if"                    { return IF; }
"else"                  { return ELSE; }
"const"                 { return CONST; }
"return"                { return RETURN; }
"retval"                { return RET_VAL; }
"break"                 { return BREAK; }
"continue"              { return CONTINUE; }
"typealias"             { return TYPE_ALIAS; }
"struct"                { return STRUCT_DEF; }
"dummy"                 { return DUMMY; }
[a-zA-Z_][a-zA-Z_0-9]*  { SAVE_STR; return IDENT; }
<<EOF>>                 { return PTLANG_YYEOF; }
.                       { ptlang_yyerror(yylloc, "Unknown token\n"); yyterminate(); };
%%
