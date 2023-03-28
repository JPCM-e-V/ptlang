%{
    #ifndef PTLANG_PARSER_DO_NOT_INCLUDE_LEXER_H
    #define PTLANG_PARSER_DO_NOT_INCLUDE_LEXER_H
    #endif
    #include "ptlang_parser_impl.h"
%}

%option noyywrap
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
"$f16"                  { return F16; }
"$f32"                  { return F32; }
"$f64"                  { return F64; }
"$f128"                 { return F128; }
"["                     { return OPEN_SQUARE_BRACKET; }
"]"                     { return CLOSE_SQUARE_BRACKET; }
"("                     { return OPEN_BRACKET; }
")"                     { return CLOSE_BRACKET; }
"{"                     { return OPEN_CURLY_BRACE; }
"}"                     { return CLOSE_CURLY_BRACE; }
"${"                    { return START_STRUCT; }
"$("                    { return START_FUNC; }
"->"                    { return ARROW; }
"?"                     { return IF; }
":"                     { return ELSE; }
<<EOF>>                 { return PTLANG_YYEOF; }
.                       { printf("Unknown token\n"); yyterminate(); };
%%
