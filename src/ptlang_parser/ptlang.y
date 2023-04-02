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
    ptlang_ast_decl_list decl_list;
    ptlang_ast_type_list type_list;
    ptlang_ast_exp_list exp_list;
    ptlang_ast_str_exp_list str_exp_list;
}

%define api.pure full
%locations

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
%token COMMA
%token IF
%token ELSE
%token WHILE
%token CONST
%token RETURN
%token RET_VAL
%token BREAK
%token CONTINUE
%token TYPE_ALIAS
%token STRUCT_DEF
%token DUMMY

%type <type> type
%type <stmt> stmt block
%type <module> module
%type <func> func
%type <exp> exp
%type <decl> decl non_const_decl
%type <struct_def> struct_def
%type <decl_list> params one_or_more_params struct_members one_or_more_struct_members
%type <type_list> param_types
%type <exp_list> param_values
%type <str_exp_list> members

%precedence single_if
%precedence ELSE

%start file

%%

file: module { *ptlang_parser_module_out = $1; }

module: { $$ = ptlang_ast_module_new(); }
      | module func { $$ = $1; ptlang_ast_module_add_function($$, $2); }
      | module decl SEMICOLON { $$ = $1; ptlang_ast_module_add_declaration($$, $2); }
      | module STRUCT_DEF IDENT OPEN_CURLY_BRACE struct_members CLOSE_CURLY_BRACE
        { $$ = $1; ptlang_ast_module_add_struct_def($$, ptlang_ast_struct_def_new($3, $5)); }
      | module TYPE_ALIAS IDENT type { $$ = $1; ptlang_ast_module_add_type_alias($$, $3, $4); }

struct_members: { $$ = ptlang_ast_decl_list_new(); }
      | one_or_more_struct_members { $$ = $1; }
      | one_or_more_struct_members COMMA { $$ = $1; }

one_or_more_struct_members: non_const_decl { $$ = ptlang_ast_decl_list_new(); ptlang_ast_decl_list_add($$, $1); }
      | one_or_more_struct_members COMMA non_const_decl { $$ = $1; ptlang_ast_decl_list_add($$, $3); }

func: type IDENT OPEN_BRACKET params CLOSE_BRACKET stmt { $$ = ptlang_ast_func_new($2, $1, $4, $6); }
    | IDENT OPEN_BRACKET params CLOSE_BRACKET stmt { $$ = ptlang_ast_func_new($1, NULL, $3, $5); }

params: { $$ = ptlang_ast_decl_list_new(); }
      | one_or_more_params { $$ = $1; }
      | one_or_more_params COMMA { $$ = $1; }

one_or_more_params: decl { $$ = ptlang_ast_decl_list_new(); ptlang_ast_decl_list_add($$, $1); }
                  | one_or_more_params COMMA decl { $$ = $1; ptlang_ast_decl_list_add($$, $3); }

non_const_decl: type IDENT { $$ = ptlang_ast_decl_new($1, $2, true); }

decl: non_const_decl {$$ = $1;}
    | CONST type IDENT { $$ = ptlang_ast_decl_new($2, $3, false); }

type: DUMMY { $$ = NULL; }

stmt: OPEN_CURLY_BRACE block CLOSE_CURLY_BRACE { $$ = $2; }
    | exp SEMICOLON { $$ = ptlang_ast_stmt_expr_new($1); }
    | decl SEMICOLON { $$ = ptlang_ast_stmt_decl_new($1); }
    | IF OPEN_BRACKET exp CLOSE_BRACKET stmt %prec single_if { $$ = ptlang_ast_stmt_if_new($3, $5); }
    | IF OPEN_BRACKET exp CLOSE_BRACKET stmt ELSE stmt { $$ = ptlang_ast_stmt_if_else_new($3, $5, $7); }
    | WHILE OPEN_BRACKET exp CLOSE_BRACKET stmt { $$ = ptlang_ast_stmt_while_new($3, $5); }
    | RET_VAL exp SEMICOLON {$$ = ptlang_ast_stmt_ret_val_new($2); }
    | RETURN exp SEMICOLON {$$ = ptlang_ast_stmt_return_new($2); }
    | RETURN SEMICOLON {$$ = ptlang_ast_stmt_return_new(NULL); }
    | BREAK SEMICOLON { $$ = ptlang_ast_stmt_break_new(0); }
    | CONTINUE SEMICOLON { $$ = ptlang_ast_stmt_continue_new(0); }

block: { $$ = ptlang_ast_stmt_block_new(); }
     | block stmt { $$ = $1; ptlang_ast_stmt_block_add_stmt($$, $2); }

exp: DUMMY { $$ = NULL; }

// decls: { $$ = ptlang_ast_module_add_function(NULL, NULL) }
//      | type IDENT COMMA parameters { $$ = $4; ptlang_ast_func_add_parameter($$, $2, $1); }
//      | type IDENT { $$ = ptlang_ast_module_add_function(NULL, NULL); ptlang_ast_func_add_parameter($$, $2, $1); }

// parameters CLOSE_BRACKET statement { $$ = ptlang_ast_func }

// func: func_start
%%
