#include "ptlang_context.h"

void pltang_context_destory(ptlang_context *ctx) { shfree(ctx->type_scope); }

ptlang_ast_type ptlang_context_unname_type(ptlang_ast_type type, ptlang_context_type_scope *type_scope)
{

    while (type != NULL && type->type == PTLANG_AST_TYPE_NAMED)
    {

        ptlang_context_type_scope_entry entry = shget(type_scope, type->content.name);
        if (entry.type == PTLANG_CONTEXT_TYPE_SCOPE_ENTRY_STRUCT)
        {
            break;
        }
        type = entry.value.ptlang_type;
    }
    return type;
}

bool ptlang_context_type_equals(ptlang_ast_type type_1, ptlang_ast_type type_2, ptlang_context_type_scope *type_scope)
{
    type_1 = ptlang_context_unname_type(type_1, type_scope);
    type_2 = ptlang_context_unname_type(type_2, type_scope);
    
    if (type_1->type != type_2->type)
        return false;
    switch (type_1->type)
    {
    case PTLANG_AST_TYPE_VOID:
        return true;
    case PTLANG_AST_TYPE_INTEGER:
        return type_1->content.integer.is_signed == type_2->content.integer.is_signed &&
               type_1->content.integer.size == type_2->content.integer.size;
    case PTLANG_AST_TYPE_FLOAT:
        return type_1->content.float_size == type_2->content.float_size;
    case PTLANG_AST_TYPE_FUNCTION:
        if (!ptlang_context_type_equals(type_1->content.function.return_type,
                                    type_2->content.function.return_type, type_scope))
            return false;
        if (arrlenu(type_1->content.function.parameters) != arrlenu(type_1->content.function.parameters))
            return false;
        for (size_t i = 0; i < arrlenu(type_1->content.function.parameters); i++)
        {
            if (!ptlang_context_type_equals(type_1->content.function.parameters[i],
                                        type_2->content.function.parameters[i], type_scope))

                return false;
        }
        return true;
    case PTLANG_AST_TYPE_HEAP_ARRAY:
        return ptlang_context_type_equals(type_1->content.heap_array.type, type_2->content.heap_array.type, type_scope);
    case PTLANG_AST_TYPE_ARRAY:
        return ptlang_context_type_equals(type_1->content.array.type, type_2->content.array.type, type_scope) &&
               type_1->content.array.len == type_2->content.array.len;
    case PTLANG_AST_TYPE_REFERENCE:
        return ptlang_context_type_equals(type_1->content.reference.type, type_2->content.reference.type, type_scope) &&
               type_1->content.reference.writable == type_2->content.reference.writable;
    case PTLANG_AST_TYPE_NAMED:
        return 0 == strcmp(type_1->content.name, type_2->content.name);
    }
}
