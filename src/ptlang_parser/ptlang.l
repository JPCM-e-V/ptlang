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
%option warn
%option always-interactive
%option yylineno

%%
">="                    { return LEQ; }
"<="                    { return GEQ; }
"="                     { return EQ; }
"=="                    { return EQEQ; }
"!="                    { return NEQ; }
"<"                     { return LESSER; }
">"                     { return GREATER; }
";"                     { return SEMICOLON; }
"<<"                    { return LEFT_SHIFT; }
">>"                    { return RIGHT_SHIFT; }
"&&"                    { return AND; }
"||"                    { return OR; }
"!"                     { return NOT; }
"&"                     { return AMPERSAND; }
"|"                     { return VERTICAL_BAR; }
"^"                     { return CIRCUMFLEX; }
"~"                     { return TILDE; }
"."                     { return DOT; }
"?"                     { return QUESTION_MARK; }
":"                     { return COLON; }
[ \t\n]                 {}
"f16"                   { return F16; }
"f32"                   { return F32; }
"f64"                   { return F64; }
"f128"                  { return F128; }
"["                     { return OPEN_SQUARE_BRACKET; }
"]"                     { return CLOSE_SQUARE_BRACKET; }
"("                     { return OPEN_BRACKET; }
")"                     { return CLOSE_BRACKET; }
"{"                     { return OPEN_CURLY_BRACE; }
"}"                     { return CLOSE_CURLY_BRACE; }
","                     { return COMMA; }
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
