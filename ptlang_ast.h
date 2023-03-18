/* TODO
 * struct
 * statements
 * expression
 *
 */

#ifndef PTLANG_AST_H
#define PTLANG_AST_H

#include <stdint.h>
#include <stdbool.h>

typedef struct ptlang_ast_type_s *ptlang_ast_type;
typedef struct ptlang_ast_stmt_s *ptlang_ast_stmt;
typedef struct ptlang_ast_module_s *ptlang_ast_module;
typedef struct ptlang_ast_func_s *ptlang_ast_func;
typedef struct ptlang_ast_exp_s *ptlang_ast_exp;
typedef struct ptlang_ast_decl_s *ptlang_ast_decl;

struct ptlang_ast_module_struct_def_s
{
    char *name;
    uint64_t member_count;
    char **member_names;
    ptlang_ast_type *member_types;
};

struct ptlang_ast_module_type_alias_s
{
    char *name;
    ptlang_ast_type type;
};

struct ptlang_ast_module_s
{
    uint64_t function_count;
    ptlang_ast_func *functions;
    uint64_t declaration_count;
    ptlang_ast_decl *declarations;
    uint64_t struct_def_count;
    struct ptlang_ast_module_struct_def_s *struct_defs;
    uint64_t type_alias_count;
    struct ptlang_ast_module_type_alias_s *type_aliases;
};

struct ptlang_ast_decl_s
{
    ptlang_ast_type type;
    char *name;
    bool writable;
};

struct ptlang_ast_stmt_block_s
{
    uint64_t stmt_count;
    ptlang_ast_stmt *stmts;
};

struct ptlang_ast_stmt_control_flow_s
{
    ptlang_ast_exp condition;
    ptlang_ast_stmt stmt;
};

struct ptlang_ast_stmt_control_flow2_s
{
    ptlang_ast_exp condition;
    ptlang_ast_stmt stmt[2];
};

struct ptlang_ast_stmt_s
{
    enum
    {
        PTLANG_AST_STMT_BLOCK,
        PTLANG_AST_STMT_EXP,
        PTLANG_AST_STMT_DECL,
        PTLANG_AST_STMT_IF,
        PTLANG_AST_STMT_IF_ELSE,
        PTLANG_AST_STMT_WHILE,
        PTLANG_AST_STMT_RETURN,
        PTLANG_AST_STMT_RET_VAL,
        PTLANG_AST_STMT_BREAK,
        PTLANG_AST_STMT_CONTINUE,
    } type;
    union
    {
        struct ptlang_ast_stmt_block_s block;
        ptlang_ast_exp exp;
        ptlang_ast_decl decl;
        struct ptlang_ast_stmt_control_flow_s control_flow;
        struct ptlang_ast_stmt_control_flow2_s control_flow2;
        uint64_t nesting_level;
    } content;
};

struct ptlang_ast_exp_assignment_s
{
    char *variable_name;
    ptlang_ast_exp exp;
};

struct ptlang_ast_exp_binary_operator_s
{
    ptlang_ast_exp left_value;
    ptlang_ast_exp right_value;
};

struct ptlang_ast_exp_function_call_s
{
    char *function_name;
    uint64_t parameter_count;
    ptlang_ast_exp *parameters;
};

struct ptlang_ast_exp_ternary_operator_s
{
    ptlang_ast_exp condition;
    ptlang_ast_exp if_value;
    ptlang_ast_exp else_value;
};

struct ptlang_ast_exp_cast_s
{
    ptlang_ast_type type;
    ptlang_ast_exp value;
};

struct ptlang_ast_exp_struct_s
{
    ptlang_ast_type type;
    uint64_t length; // Not the len of the struct, but the amount of initalized struct members
    ptlang_ast_exp *values;
    char **names;
};

struct ptlang_ast_exp_array_s
{
    ptlang_ast_type type;
    uint64_t length;
    ptlang_ast_exp *values;
};

struct ptlang_ast_exp_heap_array_from_length_s
{
    ptlang_ast_type type;
    ptlang_ast_exp length;
};

struct ptlang_ast_exp_s
{
    enum
    {
        PTLANG_AST_EXP_ASSIGNMENT,
        PTLANG_AST_EXP_ADDITION,
        PTLANG_AST_EXP_SUBTRACTION,
        PTLANG_AST_EXP_NEGATION,
        PTLANG_AST_EXP_MULTIPLICATION,
        PTLANG_AST_EXP_DIVISION,
        PTLANG_AST_EXP_MODULO,
        PTLANG_AST_EXP_EQUAL,
        PTLANG_AST_EXP_GREATER,
        PTLANG_AST_EXP_GREATER_EQUAL,
        PTLANG_AST_EXP_LESS,
        PTLANG_AST_EXP_LESS_EQUAL,
        PTLANG_AST_EXP_LEFT_SHIFT,
        PTLANG_AST_EXP_RIGHT_SHIFT,
        PTLANG_AST_EXP_AND,
        PTLANG_AST_EXP_OR,
        PTLANG_AST_EXP_NOT,
        PTLANG_AST_EXP_BITWISE_AND,
        PTLANG_AST_EXP_BITWISE_OR,
        PTLANG_AST_EXP_BITWISE_XOR,
        PTLANG_AST_EXP_BITWISE_INVERSE, // xor -1
        PTLANG_AST_EXP_FUNCTION_CALL,
        PTLANG_AST_EXP_VARIABLE,
        PTLANG_AST_EXP_INTEGER,
        PTLANG_AST_EXP_FLOAT,
        PTLANG_AST_EXP_STRUCT,
        PTLANG_AST_EXP_ARRAY, // Can be both a heap and a non-heap array
        PTLANG_AST_EXP_HEAP_ARRAY_FROM_LENGTH,
        PTLANG_AST_EXP_TERNARY,
        PTLANG_AST_EXP_CAST,
    } type;
    union
    {
        struct ptlang_ast_exp_assignment_s assignment;
        struct ptlang_ast_exp_binary_operator_s binary_operator;
        ptlang_ast_exp unary_operator;
        struct ptlang_ast_exp_function_call_s function_call;
        struct ptlang_ast_exp_ternary_operator_s ternary_operator;
        char *str_prepresentation;
        struct ptlang_ast_exp_heap_array_from_length_s heap_array;
        struct ptlang_ast_exp_struct_s struct_;
        struct ptlang_ast_exp_array_s array;
        struct ptlang_ast_exp_cast_s cast;
    } content;
};

struct ptlang_ast_type_integer_s
{
    bool is_signed;
    uint32_t size;
};

struct ptlang_ast_type_function_s
{
    ptlang_ast_type return_type;
    uint64_t parameter_count;
    ptlang_ast_type *parameter;
};

struct ptlang_ast_type_heap_array_s
{
    ptlang_ast_type type;
};

struct ptlang_ast_type_array_s
{
    ptlang_ast_type type;
    uint64_t len;
};

struct ptlang_ast_type_reference_s
{
    ptlang_ast_type type;
    bool writable;
};

enum ptlang_ast_type_float_size
{
    PTLANG_AST_TYPE_FLOAT_16,
    PTLANG_AST_TYPE_FLOAT_32,
    PTLANG_AST_TYPE_FLOAT_64,
    PTLANG_AST_TYPE_FLOAT_128,
};

struct ptlang_ast_type_s
{
    enum
    {
        PTLANG_AST_TYPE_INTEGER,
        PTLANG_AST_TYPE_FLOAT,
        PTLANG_AST_TYPE_FUNCTION,
        PTLANG_AST_TYPE_HEAP_ARRAY,
        PTLANG_AST_TYPE_ARRAY,
        PTLANG_AST_TYPE_REFERENCE,
        PTLANG_AST_TYPE_STRUCT,
    } type;
    union
    {
        struct ptlang_ast_type_integer_s integer;
        enum ptlang_ast_type_float_size float_type;
        struct ptlang_ast_type_function_s function;
        struct ptlang_ast_type_heap_array_s heap_array;
        struct ptlang_ast_type_array_s array;
        struct ptlang_ast_type_reference_s reference;
        char *structure;
    } content;
};

struct ptlang_ast_func_s
{
    char *name;
    struct ptlang_ast_type_function_s type;
    char **parameter_names;
};

ptlang_ast_module ptlang_ast_module_new();

void ptlang_ast_module_add_function(ptlang_ast_module module, ptlang_ast_func function);
void ptlang_ast_module_add_declaration(ptlang_ast_module module, ptlang_ast_decl declaration);
void ptlang_ast_module_add_struct_def(ptlang_ast_module module, char *name, uint64_t member_count, char **member_names, ptlang_ast_type *member_types);
void ptlang_ast_module_add_type_alias(ptlang_ast_module module, char *name, ptlang_ast_type type);

ptlang_ast_func ptlang_ast_func_new(char *name, ptlang_ast_type return_type);
void ptlang_ast_func_add_parameter(ptlang_ast_func function, char *name, ptlang_ast_type type);

ptlang_ast_decl ptlang_ast_decl_new(ptlang_ast_type type, char *name, bool writable);

ptlang_ast_type ptlang_ast_type_integer(bool is_signed, uint32_t size);
ptlang_ast_type ptlang_ast_type_float(enum ptlang_ast_type_float_size size);
ptlang_ast_type ptlang_ast_type_function(ptlang_ast_type return_type, uint64_t parameter_count, ptlang_ast_type *parameter);
ptlang_ast_type ptlang_ast_type_function_new();
void ptlang_ast_type_function_set_return_type(ptlang_ast_type function_type, ptlang_ast_type return_type);
void ptlang_ast_type_function_add_parameter(ptlang_ast_type function_type, ptlang_ast_type patameter);
ptlang_ast_type ptlang_ast_type_heap_array(ptlang_ast_type element_type);
ptlang_ast_type ptlang_ast_type_array(ptlang_ast_type element_type, uint64_t len);
ptlang_ast_type ptlang_ast_type_reference(ptlang_ast_type type, bool writable);

ptlang_ast_exp ptlang_ast_exp_assignment_new(char *variable_name, ptlang_ast_exp exp);
ptlang_ast_exp ptlang_ast_exp_addition_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value);
ptlang_ast_exp ptlang_ast_exp_subtraction_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value);
ptlang_ast_exp ptlang_ast_exp_multiplication_new(ptlang_ast_exp value);
ptlang_ast_exp ptlang_ast_exp_division_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value);
ptlang_ast_exp ptlang_ast_exp_modulo_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value);
ptlang_ast_exp ptlang_ast_exp_equal_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value);
ptlang_ast_exp ptlang_ast_exp_greater_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value);
ptlang_ast_exp ptlang_ast_exp_greater_equal_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value);
ptlang_ast_exp ptlang_ast_exp_less_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value);
ptlang_ast_exp ptlang_ast_exp_less_equal_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value);
ptlang_ast_exp ptlang_ast_exp_left_shift_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value);
ptlang_ast_exp ptlang_ast_exp_right_shift_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value);
ptlang_ast_exp ptlang_ast_exp_and_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value);
ptlang_ast_exp ptlang_ast_exp_or_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value);
ptlang_ast_exp ptlang_ast_exp_not_new(ptlang_ast_exp value);
ptlang_ast_exp ptlang_ast_exp_bitwise_and_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value);
ptlang_ast_exp ptlang_ast_exp_bitwise_or_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value);
ptlang_ast_exp ptlang_ast_exp_bitwise_xor_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value);
ptlang_ast_exp ptlang_ast_exp_bitwise_inverse_new(ptlang_ast_exp value);
ptlang_ast_exp ptlang_ast_exp_function_call_new(char *function_name);
void ptlang_ast_exp_function_call_add_parameter(ptlang_ast_exp exp_function_call, ptlang_ast_exp parameter);
ptlang_ast_exp ptlang_ast_exp_variable_new(char *str_prepresentation);
ptlang_ast_exp ptlang_ast_exp_integer_new(char *str_prepresentation);
ptlang_ast_exp ptlang_ast_exp_float_new(char *str_prepresentation);
ptlang_ast_exp ptlang_ast_exp_struct_new(ptlang_ast_type type);
void ptlang_ast_exp_struct_add_value(ptlang_ast_exp exp_struct, char *name, ptlang_ast_exp value);
ptlang_ast_exp ptlang_ast_exp_array_new(ptlang_ast_type type);
void ptlang_ast_exp_array_add_value(ptlang_ast_exp exp_array, ptlang_ast_exp value);
ptlang_ast_exp ptlang_ast_exp_heap_array_from_length_new(ptlang_ast_type type, ptlang_ast_exp length);
ptlang_ast_exp ptlang_ast_exp_ternary_operator_new(ptlang_ast_exp condition, ptlang_ast_exp if_value, ptlang_ast_exp else_value);
ptlang_ast_exp ptlang_ast_exp_cast_new(ptlang_ast_type type, ptlang_ast_exp value);

ptlang_ast_stmt ptlang_ast_stmt_block_new();
void ptlang_ast_stmt_block_add_stmt(ptlang_ast_stmt block_stmt, ptlang_ast_stmt stmt);
ptlang_ast_stmt ptlang_ast_stmt_expr_new(ptlang_ast_exp exp);
ptlang_ast_stmt ptlang_ast_stmt_decl_new(ptlang_ast_decl decl);
ptlang_ast_stmt ptlang_ast_stmt_if_new(ptlang_ast_exp condition, ptlang_ast_stmt stmt);
ptlang_ast_stmt ptlang_ast_stmt_if_else_new(ptlang_ast_exp condition, ptlang_ast_stmt if_stmt, ptlang_ast_stmt else_stmt);
ptlang_ast_stmt ptlang_ast_stmt_while_new(ptlang_ast_exp condition, ptlang_ast_stmt stmt);
ptlang_ast_stmt ptlang_ast_stmt_return_new(ptlang_ast_exp return_value);
ptlang_ast_stmt ptlang_ast_stmt_ret_val_new(ptlang_ast_exp return_value);
ptlang_ast_stmt ptlang_ast_stmt_break_new(uint64_t nesting_level);
ptlang_ast_stmt ptlang_ast_stmt_continue_new(uint64_t nesting_level);

#endif
