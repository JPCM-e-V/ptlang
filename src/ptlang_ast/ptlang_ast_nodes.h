#ifndef PTLANG_AST_NODES_H
#define PTLANG_AST_NODES_H

#include "ptlang_rc.h"
#include <stdbool.h>
#include <stdint.h>

PTLANG_RC_DEFINE_REF_TYPE_ONLY(ptlang_ast_type);
// typedef struct { size_t ref_count; struct ptlang_ast_type_s content; } **ptlang_ast_type;
PTLANG_RC_DEFINE_REF_TYPE_ONLY(ptlang_ast_stmt);
PTLANG_RC_DEFINE_REF_TYPE_ONLY(ptlang_ast_module);
PTLANG_RC_DEFINE_REF_TYPE_ONLY(ptlang_ast_func);
PTLANG_RC_DEFINE_REF_TYPE_ONLY(ptlang_ast_exp);
PTLANG_RC_DEFINE_REF_TYPE_ONLY(ptlang_ast_decl);
PTLANG_RC_DEFINE_REF_TYPE_ONLY(ptlang_ast_struct_def);
typedef struct ptlang_ast_struct_member_s *ptlang_ast_struct_member_list;

typedef struct ptlang_ast_code_position_s
{
    uint64_t from_line;
    uint64_t from_column;
    uint64_t to_line;
    uint64_t to_column;
} ptlang_ast_code_position_s;

PTLANG_RC_DEFINE_REF_TYPE_ONLY(ptlang_ast_code_position);

typedef struct ptlang_ast_ident_s
{
    char *name;
    ptlang_ast_code_position pos;
} ptlang_ast_ident;

enum ptlang_ast_type_float_size
{
    PTLANG_AST_TYPE_FLOAT_16 = 16,
    PTLANG_AST_TYPE_FLOAT_32 = 32,
    PTLANG_AST_TYPE_FLOAT_64 = 64,
    PTLANG_AST_TYPE_FLOAT_128 = 128,
};

struct ptlang_ast_struct_def_s
{
    ptlang_ast_ident name;
    ptlang_ast_decl *members;
    ptlang_ast_code_position pos;
};

struct ptlang_ast_module_type_alias_s
{
    ptlang_ast_ident name;
    ptlang_ast_type type;
    ptlang_ast_code_position pos;
};

struct ptlang_ast_module_s
{
    ptlang_ast_func *functions;
    ptlang_ast_decl *declarations;
    ptlang_ast_struct_def *struct_defs;
    struct ptlang_ast_module_type_alias_s *type_aliases;
};

struct ptlang_ast_decl_s
{
    ptlang_ast_type type;
    ptlang_ast_exp init;
    ptlang_ast_ident name;
    bool writable;
    bool exported;
    ptlang_ast_code_position pos;
};

struct ptlang_ast_struct_member_s
{
    ptlang_ast_ident str;
    ptlang_ast_exp exp;
    ptlang_ast_code_position pos;
};
struct ptlang_ast_stmt_block_s
{
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
    ptlang_ast_code_position pos;
};

struct ptlang_ast_exp_binary_operator_s
{
    ptlang_ast_exp left_value;
    ptlang_ast_exp right_value;
};

struct ptlang_ast_exp_function_call_s
{
    ptlang_ast_exp function;
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
    ptlang_ast_ident type;
    ptlang_ast_struct_member_list members;
};

struct ptlang_ast_exp_array_s
{
    ptlang_ast_type type;
    ptlang_ast_exp *values;
};

struct ptlang_ast_exp_struct_member_s
{
    ptlang_ast_exp struct_;
    ptlang_ast_ident member_name;
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
        PTLANG_AST_EXP_REMAINDER,
        PTLANG_AST_EXP_EQUAL,
        PTLANG_AST_EXP_NOT_EQUAL,
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
        PTLANG_AST_EXP_LENGTH,
        PTLANG_AST_EXP_FUNCTION_CALL,
        PTLANG_AST_EXP_VARIABLE,
        PTLANG_AST_EXP_INTEGER,
        PTLANG_AST_EXP_FLOAT,
        PTLANG_AST_EXP_STRUCT,
        PTLANG_AST_EXP_ARRAY,
        PTLANG_AST_EXP_TERNARY,
        PTLANG_AST_EXP_CAST,
        PTLANG_AST_EXP_STRUCT_MEMBER,
        PTLANG_AST_EXP_ARRAY_ELEMENT,
        PTLANG_AST_EXP_REFERENCE,
        PTLANG_AST_EXP_DEREFERENCE,
        PTLANG_AST_EXP_BINARY,
    } type;
    union
    {
        struct ptlang_ast_exp_binary_operator_s binary_operator;
        ptlang_ast_exp unary_operator;
        struct ptlang_ast_exp_function_call_s function_call;
        struct ptlang_ast_exp_ternary_operator_s ternary_operator;
        char *str_prepresentation;
        struct ptlang_ast_exp_struct_s struct_;
        struct ptlang_ast_exp_array_s array;
        struct ptlang_ast_exp_cast_s cast;
        struct ptlang_ast_exp_struct_member_s struct_member;
        struct ptlang_ast_exp_array_element_s array_element;
        struct ptlang_ast_exp_reference_s reference;
        uint8_t *binary;
    } content;
    ptlang_ast_code_position pos;
    ptlang_ast_type ast_type; // use only after verify
};

struct ptlang_ast_type_integer_s
{
    bool is_signed;
    uint32_t size;
};

struct ptlang_ast_type_function_s
{
    ptlang_ast_type return_type;
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
        PTLANG_AST_TYPE_VOID,
        PTLANG_AST_TYPE_INTEGER,
        PTLANG_AST_TYPE_FLOAT,
        PTLANG_AST_TYPE_FUNCTION,
        PTLANG_AST_TYPE_HEAP_ARRAY,
        PTLANG_AST_TYPE_ARRAY,
        PTLANG_AST_TYPE_REFERENCE,
        PTLANG_AST_TYPE_NAMED,
    } type;
    union
    {
        struct ptlang_ast_type_integer_s integer;
        enum ptlang_ast_type_float_size float_size;
        struct ptlang_ast_type_function_s function;
        struct ptlang_ast_type_heap_array_s heap_array;
        struct ptlang_ast_type_array_s array;
        struct ptlang_ast_type_reference_s reference;
        char *name;
    } content;
    ptlang_ast_code_position pos;
};

struct ptlang_ast_func_s
{
    ptlang_ast_ident name;
    ptlang_ast_type return_type;
    ptlang_ast_decl *parameters;
    ptlang_ast_stmt stmt;
    bool exported;
    ptlang_ast_code_position pos;
};


PTLANG_RC_DEFINE_REF_STRUCT(struct ptlang_ast_type_s, ptlang_ast_type);
PTLANG_RC_DEFINE_REF_STRUCT(struct ptlang_ast_stmt_s, ptlang_ast_stmt);
PTLANG_RC_DEFINE_REF_STRUCT(struct ptlang_ast_module_s, ptlang_ast_module);
PTLANG_RC_DEFINE_REF_STRUCT(struct ptlang_ast_func_s, ptlang_ast_func);
PTLANG_RC_DEFINE_REF_STRUCT(struct ptlang_ast_exp_s, ptlang_ast_exp);
PTLANG_RC_DEFINE_REF_STRUCT(struct ptlang_ast_decl_s, ptlang_ast_decl);
PTLANG_RC_DEFINE_REF_STRUCT(struct ptlang_ast_struct_def_s, ptlang_ast_struct_def);
PTLANG_RC_DEFINE_REF_STRUCT(ptlang_ast_code_position_s, ptlang_ast_code_position);
#endif
