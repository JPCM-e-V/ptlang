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
