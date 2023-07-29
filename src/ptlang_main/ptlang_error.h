#ifndef PTLANG_ERROR_H
#define PTLANG_ERROR_H

#include "ptlang_ast.h"

typedef enum ptlang_error_type
{
    PTLANG_ERROR_SYNTAX,
    PTLANG_ERROR_TYPE_UNRESOLVABLE,
    PTLANG_ERROR_STRUCT_MEMBER_DUPLICATION,
    PTLANG_ERROR_STRUCT_RECURSION,
    PTLANG_ERROR_TYPE_UNDEFINED
} ptlang_error_type;

#define ptlang_error_type_name(error_type)                                                                   \
    error_type == PTLANG_ERROR_SYNTAX                      ? "Syntax Error"                                  \
    : error_type == PTLANG_ERROR_TYPE_UNRESOLVABLE         ? "Type Unresolvable Error"                       \
    : error_type == PTLANG_ERROR_STRUCT_MEMBER_DUPLICATION ? "Struct Member Duplication"                     \
    : error_type == PTLANG_ERROR_STRUCT_RECURSION          ? "Struct Recursion Error"                        \
    : error_type == PTLANG_ERROR_TYPE_UNDEFINED            ? "Type Undefined Error"                          \
                                                           : "Unkown Error"

typedef struct ptlang_error_s
{
    ptlang_error_type type;
    char *message;
    ptlang_ast_code_position pos;
} ptlang_error;

#endif
