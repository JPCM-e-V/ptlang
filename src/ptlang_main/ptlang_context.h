#ifndef PTLANG_CONTEXT_H
#define PTLANG_CONTEXT_H

#include "ptlang_ast_impl.h"

typedef struct ptlang_context_type_scope_entry_s
{
    // LLVMTypeRef type;
    enum
    {
        PTLANG_CONTEXT_TYPE_SCOPE_ENTRY_TYPEALIAS,
        PTLANG_CONTEXT_TYPE_SCOPE_ENTRY_STRUCT,
    } type;
    union
    {
        ptlang_ast_type ptlang_type;
        ptlang_ast_struct_def struct_def;
    } value;
    size_t index; // seperate for structs and type aliases
} ptlang_context_type_scope_entry;

typedef struct ptlang_context_type_scope_s
{
    char *key;
    ptlang_context_type_scope_entry value;
} ptlang_context_type_scope;

typedef struct ptlang_context_s
{
    ptlang_context_type_scope *type_scope;
    ptlang_ast_decl
        *scope; // Impl of a scope: store len of this array, add new entries, reset len to previous len
} ptlang_context;

void pltang_context_destory(ptlang_context *ctx);
ptlang_ast_type ptlang_context_unname_type(ptlang_ast_type type, ptlang_context_type_scope *type_scope);

#endif
