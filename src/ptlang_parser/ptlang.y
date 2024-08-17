%code requires {
    #include "ptlang_ast.h"
    #include "ptlang_error.h"

    #ifndef YY_TYPEDEF_YY_SCANNER_T
    #define YY_TYPEDEF_YY_SCANNER_T
    typedef void* yyscan_t;
    #endif
}

%define parse.error detailed

%{
    #include "ptlang_parser_impl.h"
    #include "ptlang_utils.h"
    // #include "stb_ds.h"

    #define ppcp ptlang_parser_code_position
    #define ppcpft ptlang_parser_code_position_from_to
%}

%define api.prefix {ptlang_yy}

%union {
    char *str;
    ptlang_ast_type type;
    ptlang_ast_stmt stmt;
//    ptlang_ast_module module;
    ptlang_ast_func func;
    ptlang_ast_exp exp;
    ptlang_ast_decl decl;
    ptlang_ast_struct_def struct_def;
    ptlang_ast_decl *decl_list;
    ptlang_ast_type *type_list;
    ptlang_ast_exp *exp_list;
    ptlang_ast_struct_member_list struct_member_list;
    enum ptlang_ast_type_float_size float_size;
    ptlang_ast_ident ident;
    struct {ptlang_ast_type type; ptlang_ast_ident ident;} type_and_ident;
}

%define api.pure true
%locations

%glr-parser
%expect-rr 1
/* %lex-param {ptlang_ast_module out} {ptlang_error **syntax_errors} */
%lex-param {yyscan_t yyscanner}
%parse-param {ptlang_ast_module out} {ptlang_error **syntax_errors} {yyscan_t yyscanner}
/* %parse-param {ptlang_ast_module out} {ptlang_error **syntax_errors} */

%token PLUS "'+'"
%token MINUS "'-'"
%token STAR "'*'"
%token SLASH "'/'"
%token PERCENT "'%'"
%token LEQ "'>='"
%token GEQ "'<='"
%token EQ "'='"
%token EQEQ "'=='"
%token NEQ "'!='"
%token LESSER "'<'"
%token GREATER "'>'"
%token SEMICOLON "';'"
%token <str> IDENT "identifier"
%token <str> INT_TYPE "integer type"
%token <float_size> FLOAT_TYPE "floating point number type"
%token <str> INT_VAL "integer value with type suffix"
%token <str> INT "integer value"
%token <str> FLOAT_VAL "floating point number value"
%token OPEN_SQUARE_BRACKET "'['"
%token CLOSE_SQUARE_BRACKET "']'"
%token OPEN_BRACKET "'('"
%token CLOSE_BRACKET "')'"
%token OPEN_CURLY_BRACE "'{'"
%token CLOSE_CURLY_BRACE "'}'"
%token COMMA "','"
%token MOD "'mod'"
%token IF "'if'"
%token ELSE "'else'"
%token WHILE "'while'"
%token CONST "'const'"
%token RETURN "'return'"
%token RET_VAL "'retval'"
%token BREAK "'break'"
%token CONTINUE "'continue'"
%token TYPE_ALIAS "'typealias'"
%token STRUCT_DEF "'struct'"
%token EXPORT "'export'"
%token LEFT_SHIFT "'<<'"
%token RIGHT_SHIFT "'>>'"
%token AND "'&&'"
%token OR "'||'"
%token NOT "'!'"
%token AMPERSAND "'&'"
%token VERTICAL_BAR "'|'"
%token CIRCUMFLEX "'^'"
%token TILDE "'~'"
%token DOT "'.'"
%token QUESTION_MARK "'?'"
%token COLON "':'"
%token HASHTAG "'#'"
%token VOID "'void'"

%type <str> int_val
%type <type> type type_or_void void
%type <stmt> stmt block
//%type <module> module
%type <func> func function
%type <exp> exp const_exp
%type <decl> const_decl decl decl_statement module_decl declaration declaration_without_export non_const_decl
%type <decl_list> params one_or_more_params struct_members one_or_more_struct_members
%type <type_list> types one_or_more_types
%type <exp_list> exps one_or_more_exps
%type <struct_member_list> members one_or_more_members
%type <ident> ident
%type <struct_def> struct_def
%type <type_and_ident> type_and_ident

%destructor {
    ptlang_free($$);
} <str>

%destructor {
    if ($$ != NULL){
        ptlang_rc_remove_ref($$, ptlang_ast_type_destroy);
    }
} <type>

%destructor {
    ptlang_rc_remove_ref($$, ptlang_ast_stmt_destroy);
} <stmt>

%destructor {
    ptlang_rc_remove_ref($$, ptlang_ast_func_destroy);
} <func>

%destructor {
    ptlang_rc_remove_ref($$, ptlang_ast_exp_destroy);
} <exp>

%destructor {
    ptlang_rc_remove_ref($$, ptlang_ast_decl_destroy);
} <decl>

%destructor {
    ptlang_ast_decl_list_destroy($$);
} <decl_list>

%destructor {
    ptlang_ast_type_list_destroy($$);
} <type_list>

%destructor {
    ptlang_ast_exp_list_destroy($$);
} <exp_list>

%destructor {
    ptlang_ast_struct_member_list_destroy($$);
} <struct_member_list>

%destructor {
    ptlang_ast_ident_destroy($$);
} <ident>

%destructor {
    ptlang_rc_remove_ref($$, ptlang_ast_struct_def_destroy);
} <struct_def>

%destructor {
    ptlang_rc_remove_ref($$.type, ptlang_ast_type_destroy);
    ptlang_ast_ident_destroy($$.ident);
} <type_and_ident>

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
%left STAR SLASH PERCENT MOD
%right negation NOT TILDE reference dereference cast HASHTAG
%left DOT OPEN_SQUARE_BRACKET OPEN_BRACKET

/* %precedence array_type */


%start module

%%

module:
      | module declaration { ptlang_ast_module_add_declaration(out, $2); }
      | module function { ptlang_ast_module_add_function(out, $2); }
      | module struct_def { ptlang_ast_module_add_struct_def(out, $2); }
      | module TYPE_ALIAS ident type SEMICOLON { ptlang_ast_module_add_type_alias(out, $3, $4, ppcpft(&@2, &@$)); }

declaration: module_decl SEMICOLON { $$ = $1; }

function: EXPORT func { $$ = $2; ptlang_ast_func_set_export($$, true); }
        | func { $$ = $1; }


ident: IDENT { $$ = (ptlang_ast_ident){$1, ppcp(&@1)}; }

struct_members: { $$ = NULL; }
      | one_or_more_struct_members { $$ = $1; }
      | one_or_more_struct_members COMMA { $$ = $1; }

one_or_more_struct_members: non_const_decl { $$ = NULL; arrput($$, $1); }
      | one_or_more_struct_members COMMA non_const_decl { $$ = $1; arrput($$, $3); }

func: type_and_ident OPEN_BRACKET params CLOSE_BRACKET stmt {$$ = ptlang_ast_func_new($1.ident, $1.type, $3, $5, false, ppcpft(&@$, &@4)); }
    | void ident OPEN_BRACKET params CLOSE_BRACKET stmt {$$ = ptlang_ast_func_new($2, $1, $4, $6, false, ppcpft(&@$, &@5));}


params: { $$ = NULL; }
      | one_or_more_params { $$ = $1; }
      | one_or_more_params COMMA { $$ = $1; }

one_or_more_params: non_const_decl { $$ = NULL; arrput($$, $1); }
                  | one_or_more_params COMMA non_const_decl { $$ = $1; arrput($$, $3); }

non_const_decl: type_and_ident { $$ = ptlang_ast_decl_new($1.type, $1.ident, true, ppcp(&@$)); }

type_and_ident: type ident { $$.type = $1; $$.ident = $2; }

const_decl: CONST type ident { $$ = ptlang_ast_decl_new($2, $3, false, ppcp(&@$));}

decl: non_const_decl { $$=$1; } 
    | const_decl { $$=$1; }

decl_statement: decl EQ exp { $$ = $1; ptlang_ast_decl_set_init($1, $3); }
    | non_const_decl { $$ = $1; }

declaration_without_export: decl EQ exp { $$ = $1; ptlang_ast_decl_set_init($$, $3); }
                         | non_const_decl { $$ = $1; }

module_decl: declaration_without_export { $$ = $1; }
           | EXPORT declaration_without_export { $$ = $2; ptlang_ast_decl_set_export($$, true); }

struct_def: STRUCT_DEF ident OPEN_CURLY_BRACE struct_members CLOSE_CURLY_BRACE
           { $$ = ptlang_ast_struct_def_new($2, $4, ppcpft(&@2, &@$)); }


type: INT_TYPE { $$ = ptlang_parser_integer_type_of_string($1, &@1, syntax_errors); }
    | FLOAT_TYPE { $$ = ptlang_ast_type_float($1, ppcp(&@$)); }
    | OPEN_BRACKET types CLOSE_BRACKET COLON type_or_void { $$ = ptlang_ast_type_function($5, $2, ppcp(&@$)); }
    | OPEN_SQUARE_BRACKET CLOSE_SQUARE_BRACKET type { $$ = ptlang_ast_type_heap_array($3, ppcp(&@$)); }
    | OPEN_SQUARE_BRACKET INT CLOSE_SQUARE_BRACKET type  { $$ = ptlang_ast_type_array($4, ptlang_parser_strtouint64($2, &@2, syntax_errors), ppcp(&@$)); ptlang_free($2); }
    | AMPERSAND type %prec reference { $$ = ptlang_ast_type_reference($2, true, ppcp(&@$)); }
    | AMPERSAND CONST type %prec reference { $$ = ptlang_ast_type_reference($3, false, ppcp(&@$)); }
    | IDENT { $$ = ptlang_ast_type_named($1, ppcp(&@$)); }
    | error { $$ = NULL; }
    /* | DUMMY { $$ = NULL; } */

type_or_void: type {$$ = $1;} | void { $$ = $1; }
void: VOID { $$ = ptlang_ast_type_void(ppcp(&@$)); }

types: { $$ = NULL; }
    | one_or_more_types { $$ = $1; }
    | one_or_more_types COMMA { $$ = $1; }

one_or_more_types: type { $$ = NULL; arrput($$, $1); }
                 | one_or_more_types COMMA type { $$ = $1; arrput($$, $3); }


// []f128

stmt: OPEN_CURLY_BRACE block CLOSE_CURLY_BRACE { $$ = $2; }
    | exp SEMICOLON { $$ = ptlang_ast_stmt_expr_new($1, ppcp(&@$)); }
    | decl_statement SEMICOLON { $$ = ptlang_ast_stmt_decl_new($1, ppcp(&@$)); }
    | IF OPEN_BRACKET exp CLOSE_BRACKET stmt %prec single_if { $$ = ptlang_ast_stmt_if_new($3, $5, ppcp(&@$)); }
    | IF OPEN_BRACKET exp CLOSE_BRACKET stmt ELSE stmt { $$ = ptlang_ast_stmt_if_else_new($3, $5, $7, ppcp(&@$)); }
    | WHILE OPEN_BRACKET exp CLOSE_BRACKET stmt { $$ = ptlang_ast_stmt_while_new($3, $5, ppcp(&@$)); }
    | RET_VAL exp SEMICOLON {$$ = ptlang_ast_stmt_ret_val_new($2, ppcp(&@$)); }
    | RETURN exp SEMICOLON {$$ = ptlang_ast_stmt_return_value_new($2, ppcp(&@$)); }
    | RETURN SEMICOLON {$$ = ptlang_ast_stmt_return_new( ppcp(&@$)); }
    | BREAK SEMICOLON { $$ = ptlang_ast_stmt_break_new(1, ppcp(&@$)); }
    | CONTINUE SEMICOLON { $$ = ptlang_ast_stmt_continue_new(1, ppcp(&@$)); }
    | BREAK INT SEMICOLON { $$ = ptlang_ast_stmt_break_new(ptlang_parser_strtouint64($2, &@2, syntax_errors), ppcp(&@$)); }
    | CONTINUE INT SEMICOLON { $$ = ptlang_ast_stmt_continue_new(ptlang_parser_strtouint64($2, &@2, syntax_errors), ppcp(&@$)); }

block: { $$ = ptlang_ast_stmt_block_new(ppcp(&@$)); }
     | block stmt { $$ = $1; ptlang_ast_stmt_block_add_stmt($$, $2); }


const_exp: int_val { $$ = ptlang_ast_exp_integer_new($1, ppcp(&@$)); }
         | FLOAT_VAL { $$ = ptlang_ast_exp_float_new($1, ppcp(&@$)); }


exp: OPEN_BRACKET exp CLOSE_BRACKET { $$ = $2; }
   | exp EQ exp { $$ = ptlang_ast_exp_assignment_new($1, $3, ppcp(&@$)); }
   | exp PLUS exp { $$ = ptlang_ast_exp_addition_new($1, $3, ppcp(&@$)); }
   | exp MINUS exp { $$ = ptlang_ast_exp_subtraction_new($1, $3, ppcp(&@$)); }
   | MINUS exp %prec negation { $$ = ptlang_ast_exp_negation_new($2, ppcp(&@$)); }
   | exp STAR exp { $$ = ptlang_ast_exp_multiplication_new($1, $3, ppcp(&@$)); }
   | exp SLASH exp { $$ = ptlang_ast_exp_division_new($1, $3, ppcp(&@$)); }
   | exp MOD exp { $$ = ptlang_ast_exp_modulo_new($1, $3, ppcp(&@$)); }
   | exp PERCENT exp { $$ = ptlang_ast_exp_remainder_new($1, $3, ppcp(&@$)); }
   | exp EQEQ exp { $$ = ptlang_ast_exp_equal_new($1, $3, ppcp(&@$)); }
   | exp NEQ exp { $$ = ptlang_ast_exp_not_equal_new($1, $3, ppcp(&@$)); }
   | exp GREATER exp { $$ = ptlang_ast_exp_greater_new($1, $3, ppcp(&@$)); }
   | exp GEQ exp { $$ = ptlang_ast_exp_greater_equal_new($1, $3, ppcp(&@$)); }
   | exp LESSER exp { $$ = ptlang_ast_exp_less_new($1, $3, ppcp(&@$)); }
   | exp LEQ exp { $$ = ptlang_ast_exp_less_equal_new($1, $3, ppcp(&@$)); }
   | exp LEFT_SHIFT exp { $$ = ptlang_ast_exp_left_shift_new($1, $3, ppcp(&@$)); }
   | exp RIGHT_SHIFT exp { $$ = ptlang_ast_exp_right_shift_new($1, $3, ppcp(&@$)); }
   | exp AND exp { $$ = ptlang_ast_exp_and_new($1, $3, ppcp(&@$)); }
   | exp OR exp { $$ = ptlang_ast_exp_or_new($1, $3, ppcp(&@$)); }
   | NOT exp { $$ = ptlang_ast_exp_not_new($2, ppcp(&@$)); }
   | exp AMPERSAND exp { $$ = ptlang_ast_exp_bitwise_and_new($1, $3, ppcp(&@$)); }
   | exp VERTICAL_BAR exp { $$ = ptlang_ast_exp_bitwise_or_new($1, $3, ppcp(&@$)); }
   | exp CIRCUMFLEX exp { $$ = ptlang_ast_exp_bitwise_xor_new($1, $3, ppcp(&@$)); }
   | TILDE exp { $$ = ptlang_ast_exp_bitwise_inverse_new($2, ppcp(&@$)); }
   | HASHTAG exp { $$ = ptlang_ast_exp_length_new($2, ppcp(&@$)); }
   | exp OPEN_BRACKET exps CLOSE_BRACKET { $$ = ptlang_ast_exp_function_call_new($1, $3, ppcp(&@$)); }
   | IDENT { $$ = ptlang_ast_exp_variable_new($1, ppcp(&@$)); }
   | ident OPEN_CURLY_BRACE members CLOSE_CURLY_BRACE { $$ = ptlang_ast_exp_struct_new($1, $3, ppcp(&@$)); }
   | OPEN_SQUARE_BRACKET CLOSE_SQUARE_BRACKET type OPEN_CURLY_BRACE exps CLOSE_CURLY_BRACE { $$ = ptlang_ast_exp_array_new($3, $5, ppcp(&@$)); }
   | exp QUESTION_MARK exp COLON exp { $$ = ptlang_ast_exp_ternary_operator_new($1, $3, $5, ppcp(&@$)); }
   | LESSER type GREATER exp %prec cast { $$ = ptlang_ast_exp_cast_new($2, $4, ppcp(&@$)); }
   | exp DOT ident { $$ = ptlang_ast_exp_struct_member_new($1, $3, ppcp(&@$)); }
   | exp OPEN_SQUARE_BRACKET exp CLOSE_SQUARE_BRACKET { $$ = ptlang_ast_exp_array_element_new($1, $3, ppcp(&@$)); }
   | AMPERSAND exp %prec reference { $$ = ptlang_ast_exp_reference_new(true, $2, ppcp(&@$)); }
   | AMPERSAND CONST exp %prec reference { $$ = ptlang_ast_exp_reference_new(false, $3, ppcp(&@$)); }
   | STAR exp %prec dereference { $$ = ptlang_ast_exp_dereference_new($2, ppcp(&@$)); }
   | const_exp { $$ = $1; }
   /* | DUMMY { $$ = NULL; } */

exps: { $$ = NULL; }
    | one_or_more_exps { $$ = $1; }
    | one_or_more_exps COMMA { $$ = $1; }

one_or_more_exps: exp { $$ = NULL; arrput($$, $1); }
                | one_or_more_exps COMMA exp { $$ = $1; arrput($$, $3); }

members: { $$ = NULL; }
       | one_or_more_members { $$ = $1; }
       | one_or_more_members COMMA { $$ = $1; }

one_or_more_members: ident exp { $$ = NULL; ptlang_ast_struct_member_list_add(&$$, $1, $2, ppcp(&@$)); }
                   | one_or_more_members COMMA ident exp { $$ = $1; ptlang_ast_struct_member_list_add(&$$, $3, $4, ppcpft(&@3,&@$)); }

int_val: INT { $$ = $1; }
       | INT_VAL { $$ = $1; }
%%
