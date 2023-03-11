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

struct ptlang_ast_module_s
{
    ptlang_ast_func *functions;
};

struct ptlang_ast_func_s
{
};

struct ptlang_ast_stmt_decl_s
{
    ptlang_ast_type type;
    char *name;
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
        ptlang_ast_stmt *block;
        ptlang_ast_exp exp;
        struct ptlang_ast_stmt_decl_s decl;
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
        PTLANG_AST_CAST,
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

struct ptlang_ast_type_struct_s
{
    uint64_t member_count;
    char **names;
    ptlang_ast_type *types;
};

struct ptlang_ast_type_reference_s
{
    ptlang_ast_type type;
    bool writable;
};

struct ptlang_ast_type_s
{
    enum
    {
        PTLANG_AST_TYPE_INTEGER,
        PTLANG_AST_TYPE_FLOAT,
        PTLANG_AST_TYPE_FUNCTION,
        PTLANG_AST_TYPE_POINTER,
        PTLANG_AST_TYPE_STRUCT,
    } type;
    union
    {
        struct ptlang_ast_type_integer_s integer;
        enum
        {
            PTLANG_AST_TYPE_FLOAT_16,
            PTLANG_AST_TYPE_FLOAT_32,
            PTLANG_AST_TYPE_FLOAT_64,
            PTLANG_AST_TYPE_FLOAT_128,
        } float_type;
        struct ptlang_ast_type_function_s function;
        struct ptlang_ast_type_heap_array_s heap_array;
        struct ptlang_ast_type_array_s array;
        struct ptlang_ast_type_reference_s reference;
        struct ptlang_ast_type_struct_s structure;
    } content;
};

#endif
