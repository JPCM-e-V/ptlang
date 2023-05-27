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
    enum ptlang_ast_type_float_size float_size;
}

%define api.pure true
%locations

%glr-parser
%expect-rr 3

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
%token <str> INT_TYPE
%token <float_size> FLOAT_TYPE
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
%token ALLOC
%token LEFT_SHIFT
%token RIGHT_SHIFT
%token AND
%token OR
%token NOT
%token AMPERSAND
%token VERTICAL_BAR
%token CIRCUMFLEX
%token TILDE
%token DOT
%token QUESTION_MARK
%token COLON

%token DUMMY

%type <type> type
%type <stmt> stmt block
%type <module> module
%type <func> func
%type <exp> exp
%type <decl> decl non_const_decl
%type <decl_list> params one_or_more_params struct_members one_or_more_struct_members
%type <type_list> types one_or_more_types
%type <exp_list> exps one_or_more_exps
%type <str_exp_list> members one_or_more_members

%precedence single_if
%precedence ELSE

%right EQ
%right QUESTION_MARK COLON
%left OR
%left AND
%left VERTICAL_BAR
%left CIRCUMFLEX
%left AMPERSAND
%left EQEQ NEQ
%left LESSER LEQ GREATER GEQ
%left LEFT_SHIFT RIGHT_SHIFT
%left PLUS MINUS
%left STAR SLASH PERCENT
%right negation NOT TILDE reference dereference cast
%left DOT OPEN_SQUARE_BRACKET OPEN_BRACKET

/* %precedence array_type */


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

type: INT_TYPE { $$ = ptlang_parser_integer_type_of_string($1, &@1); }
    | FLOAT_TYPE { $$ = ptlang_ast_type_float($1); }
    | OPEN_BRACKET types CLOSE_BRACKET COLON type { $$ = ptlang_ast_type_function($5, $2); }
    | OPEN_SQUARE_BRACKET CLOSE_SQUARE_BRACKET type { $$ = ptlang_ast_type_heap_array($3); }
    | OPEN_SQUARE_BRACKET INT_VAL CLOSE_SQUARE_BRACKET type  { $$ = ptlang_ast_type_array($4, strtoul($2, NULL, 0)); free($2); } // TODO Overflow
    | AMPERSAND type %prec reference { $$ = ptlang_ast_type_reference($2, true); }
    | AMPERSAND CONST type %prec reference { $$ = ptlang_ast_type_reference($3, false); }
    | IDENT { $$ = ptlang_ast_type_named($1); }
    /* | DUMMY { $$ = NULL; } */

types: { $$ = ptlang_ast_type_list_new(); }
    | one_or_more_types { $$ = $1; }
    | one_or_more_types COMMA { $$ = $1; }

one_or_more_types: type { $$ = ptlang_ast_type_list_new(); ptlang_ast_type_list_add($$, $1); }
                 | one_or_more_types COMMA type { $$ = $1; ptlang_ast_type_list_add($$, $3); }


// []f128

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

exp: OPEN_BRACKET exp CLOSE_BRACKET { $$ = $2; }
   | exp EQ exp { $$ = ptlang_ast_exp_assignment_new($1, $3); }
   | exp PLUS exp { $$ = ptlang_ast_exp_addition_new($1, $3); }
   | exp MINUS exp { $$ = ptlang_ast_exp_subtraction_new($1, $3); }
   | MINUS exp %prec negation { $$ = ptlang_ast_exp_negation_new($2); }
   | exp STAR exp { $$ = ptlang_ast_exp_multiplication_new($1, $3); }
   | exp SLASH exp { $$ = ptlang_ast_exp_division_new($1, $3); }
   | exp PERCENT exp { $$ = ptlang_ast_exp_modulo_new($1, $3); }
   | exp EQEQ exp { $$ = ptlang_ast_exp_equal_new($1, $3); }
   | exp NEQ exp { $$ = ptlang_ast_exp_not_equal_new($1, $3); }
   | exp GREATER exp { $$ = ptlang_ast_exp_greater_new($1, $3); }
   | exp GEQ exp { $$ = ptlang_ast_exp_greater_equal_new($1, $3); }
   | exp LESSER exp { $$ = ptlang_ast_exp_less_new($1, $3); }
   | exp LEQ exp { $$ = ptlang_ast_exp_less_equal_new($1, $3); }
   | exp LEFT_SHIFT exp { $$ = ptlang_ast_exp_left_shift_new($1, $3); }
   | exp RIGHT_SHIFT exp { $$ = ptlang_ast_exp_right_shift_new($1, $3); }
   | exp AND exp { $$ = ptlang_ast_exp_and_new($1, $3); }
   | exp OR exp { $$ = ptlang_ast_exp_or_new($1, $3); }
   | NOT exp { $$ = ptlang_ast_exp_not_new($2); }
   | exp AMPERSAND exp { $$ = ptlang_ast_exp_bitwise_and_new($1, $3); }
   | exp VERTICAL_BAR exp { $$ = ptlang_ast_exp_bitwise_or_new($1, $3); }
   | exp CIRCUMFLEX exp { $$ = ptlang_ast_exp_bitwise_xor_new($1, $3); }
   | TILDE exp { $$ = ptlang_ast_exp_bitwise_inverse_new($2); }
   | exp OPEN_BRACKET exps CLOSE_BRACKET { $$ = ptlang_ast_exp_function_call_new($1, $3); }
   | IDENT { $$ = ptlang_ast_exp_variable_new($1); }
   | INT_VAL { $$ = ptlang_ast_exp_integer_new($1); }
   | FLOAT_VAL { $$ = ptlang_ast_exp_float_new($1); }
   | IDENT OPEN_CURLY_BRACE members CLOSE_CURLY_BRACE { $$ = ptlang_ast_exp_struct_new($1, $3); }
   | type OPEN_SQUARE_BRACKET CLOSE_SQUARE_BRACKET OPEN_CURLY_BRACE exps CLOSE_CURLY_BRACE { $$ = ptlang_ast_exp_array_new($1, $5); }
   | ALLOC OPEN_SQUARE_BRACKET exp CLOSE_SQUARE_BRACKET type { $$ = ptlang_ast_exp_heap_array_from_length_new($5, $3); }
   | exp QUESTION_MARK exp COLON exp { $$ = ptlang_ast_exp_ternary_operator_new($1, $3, $5); }
   | LESSER type GREATER exp %prec cast { $$ = ptlang_ast_exp_cast_new($2, $4); }
   | exp DOT IDENT { $$ = ptlang_ast_exp_struct_member_new($1, $3); }
   | exp OPEN_SQUARE_BRACKET exp CLOSE_SQUARE_BRACKET { $$ = ptlang_ast_exp_array_element_new($1, $3); }
   | AMPERSAND exp %prec reference { $$ = ptlang_ast_exp_reference_new(true, $2); }
   | AMPERSAND CONST exp %prec reference { $$ = ptlang_ast_exp_reference_new(false, $3); }
   | STAR exp %prec dereference { $$ = ptlang_ast_exp_dereference_new($2); }
   /* | DUMMY { $$ = NULL; } */

exps: { $$ = ptlang_ast_exp_list_new(); }
    | one_or_more_exps { $$ = $1; }
    | one_or_more_exps COMMA { $$ = $1; }

one_or_more_exps: exp { $$ = ptlang_ast_exp_list_new(); ptlang_ast_exp_list_add($$, $1); }
                | one_or_more_exps COMMA exp { $$ = $1; ptlang_ast_exp_list_add($$, $3); }

members: { $$ = ptlang_ast_str_exp_list_new(); }
       | one_or_more_members { $$ = $1; }
       | one_or_more_members COMMA { $$ = $1; }

one_or_more_members: IDENT exp { $$ = ptlang_ast_str_exp_list_new(); ptlang_ast_str_exp_list_add($$, $1, $2); }
                   | one_or_more_members COMMA IDENT exp { $$ = $1; ptlang_ast_str_exp_list_add($$, $3, $4); }
%%
