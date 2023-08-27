#ifndef PTLANG_ERROR_H
#define PTLANG_ERROR_H

#include "ptlang_ast.h"

typedef enum ptlang_error_type
{
    PTLANG_ERROR_SYNTAX,
    PTLANG_ERROR_TYPE_UNRESOLVABLE,
    PTLANG_ERROR_STRUCT_MEMBER_DUPLICATION,
    PTLANG_ERROR_STRUCT_RECURSION,
    PTLANG_ERROR_TYPE_UNDEFINED,
    PTLANG_ERROR_TYPE_MISMATCH,
    PTLANG_ERROR_NESTING_LEVEL_OUT_OF_RANGE,
    PTLANG_ERROR_VARIABLE_NAME_DUPLICATION,
    PTLANG_ERROR_CODE_UNREACHABLE,
} ptlang_error_type;

#define ptlang_error_type_name(error_type)                                                                   \
    error_type == PTLANG_ERROR_SYNTAX                       ? "Syntax Error"                                 \
    : error_type == PTLANG_ERROR_TYPE_UNRESOLVABLE          ? "Type Unresolvable Error"                      \
    : error_type == PTLANG_ERROR_STRUCT_MEMBER_DUPLICATION  ? "Struct Member Duplication"                    \
    : error_type == PTLANG_ERROR_STRUCT_RECURSION           ? "Struct Recursion Error"                       \
    : error_type == PTLANG_ERROR_TYPE_UNDEFINED             ? "Type Undefined Error"                         \
    : error_type == PTLANG_ERROR_TYPE_MISMATCH              ? "Type Mismatch Error"                          \
    : error_type == PTLANG_ERROR_NESTING_LEVEL_OUT_OF_RANGE ? "Nesting Level Out of Range Error"             \
    : error_type == PTLANG_ERROR_VARIABLE_NAME_DUPLICATION  ? "Variable Name Duplication"                    \
    : error_type == PTLANG_ERROR_CODE_UNREACHABLE           ? "Code Unreachable"                             \
                                                            : "Unkown Error"

typedef struct ptlang_error_s
{
    ptlang_error_type type;
    char *message;
    ptlang_ast_code_position pos;
} ptlang_error;

#endif
