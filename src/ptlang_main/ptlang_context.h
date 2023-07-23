#ifndef PTLANG_CONTEXT_H
#define PTLANG_CONTEXT_H

#include "ptlang_ast_impl.h"

typedef struct ptlang_context_type_scope_entry_s
{
    // LLVMTypeRef type;
    ptlang_ast_type ptlang_type;
} ptlang_context_type_scope_entry;

typedef struct ptlang_context_type_scope_s
{
    char *key;
    ptlang_context_type_scope_entry value;
} ptlang_context_type_scope;

typedef struct ptlang_context_s
{
    ptlang_context_type_scope *type_scope;
} ptlang_context;

static inline ptlang_ast_type ptlang_context_unname_type(ptlang_ast_type type,
                                                         ptlang_context_type_scope *type_scope)
{
    ptlang_ast_type new_type;
    while (type != NULL && type->type == PTLANG_AST_TYPE_NAMED)
    {
        new_type = shget(type_scope, type->content.name).ptlang_type;
        if (new_type == NULL)
        {
            break;
        }
        type = new_type;
    }
    return type;
}

#endif
