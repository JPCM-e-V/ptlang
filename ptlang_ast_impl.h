#ifndef PTLANG_AST_IMPL_H
#define PTLANG_AST_IMPL_H

#include "ptlang_ast.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

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
    ptlang_ast_exp assignable;
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

struct ptlang_ast_exp_struct_member_s
{
    ptlang_ast_exp struct_;
    char* member_name;
};

struct ptlang_ast_exp_array_element_s
{
    ptlang_ast_exp array;
    ptlang_ast_exp index;
};

struct ptlang_ast_exp_reference_s
{
    bool writable;
    ptlang_ast_exp value;
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
        PTLANG_AST_EXP_STRUCT_MEMBER,
        PTLANG_AST_EXP_ARRAY_ELEMENT,
        PTLANG_AST_EXP_REFERENCE,
        PTLANG_AST_EXP_DEREFERENCE,
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
        struct ptlang_ast_exp_struct_member_s struct_member;
        struct ptlang_ast_exp_array_element_s array_element;
        struct ptlang_ast_exp_reference_s reference;
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
    ptlang_ast_type *parameters;
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
        enum ptlang_ast_type_float_size float_size;
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

#endif
