%{
    #include "ptlang_parser_impl.h"
%}

%define api.prefix {ptlang_yy}

%union {
    char *str;
    ptlang_ast_type type;
    ptlang_ast_stmt stmt;
    ptlang_ast_module module;
    ptlang_ast_func func;
    ptlang_ast_exp exp;
    ptlang_ast_decl decl;
    ptlang_ast_struct_def struct_def;
}

%token PLUS
%token MINUS
%token STAR
%token SLASH
%token PERCENT
%token LEQ
%token GEQ
%token EQ
%token EQEQ
%token NEQ
%token LESSER
%token GREATER
%token SEMICOLON
%token <str> IDENT
%token F16
%token F32
%token F64
%token F128
%token <str> UINT
%token <str> INT
%token <str> INT_VAL
%token <str> FLOAT_VAL
%token OPEN_SQUARE_BRACKET
%token CLOSE_SQUARE_BRACKET
%token OPEN_BRACKET
%token CLOSE_BRACKET
%token OPEN_CURLY_BRACE
%token CLOSE_CURLY_BRACE
%token START_STRUCT
%token COMMA
%token START_FUNC
%token ARROW
%token IF
%token ELSE
%token WHILE

%type <module> module
%type <func> func

%start file



%%

file: module { *ptlang_parser_module_out = $1; }

module: { $$ = ptlang_ast_module_new(); }
//       | module func { $$ = $1; ptlang_ast_module_add_function($$, $2); }

// func: type IDENT OPEN_BRACKET parameters CLOSE_BRACKET statement

// decls: { $$ = ptlang_ast_module_add_function(NULL, NULL) }
//      | type IDENT COMMA parameters { $$ = $4; ptlang_ast_func_add_parameter($$, $2, $1); }
//      | type IDENT { $$ = ptlang_ast_module_add_function(NULL, NULL); ptlang_ast_func_add_parameter($$, $2, $1); }

// parameters CLOSE_BRACKET statement { $$ = ptlang_ast_func }

// func: func_start
%%
