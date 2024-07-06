#include "ptlang_context_impl.h"

void ptlang_context_destory(ptlang_context *ctx)
{
    arrfree(ctx->scope);
    shfree(ctx->type_scope);
}

ptlang_ast_type ptlang_context_unname_type(ptlang_ast_type type, ptlang_context_type_scope *type_scope)
{

    while (type != NULL && ptlang_rc_deref(type).type == PTLANG_AST_TYPE_NAMED)
    {

        ptlang_context_type_scope *kv_pair = shgetp_null(type_scope, ptlang_rc_deref(type).content.name);
        if (kv_pair == NULL)
            return NULL;
        if (kv_pair->value.type == PTLANG_CONTEXT_TYPE_SCOPE_ENTRY_STRUCT)
        {
            break;
        }
        type = kv_pair->value.value.ptlang_type;
    }
    return type;
}

// ptlang_ast_type ptlang_context_copy_unname_type(ptlang_ast_type type, ptlang_context_type_scope
// *type_scope)
// {
//     return ptlang_ast_type_copy(ptlang_context_unname_type(type, type_scope));
// }

bool ptlang_context_type_equals(ptlang_ast_type type_1, ptlang_ast_type type_2,
                                ptlang_context_type_scope *type_scope)
{
    type_1 = ptlang_context_unname_type(type_1, type_scope);
    type_2 = ptlang_context_unname_type(type_2, type_scope);

    if (ptlang_rc_deref(type_1).type != ptlang_rc_deref(type_2).type)
        return false;
    switch (ptlang_rc_deref(type_1).type)
    {
    case PTLANG_AST_TYPE_VOID:
        return true;
    case PTLANG_AST_TYPE_INTEGER:
        return ptlang_rc_deref(type_1).content.integer.is_signed ==
                   ptlang_rc_deref(type_2).content.integer.is_signed &&
               ptlang_rc_deref(type_1).content.integer.size == ptlang_rc_deref(type_2).content.integer.size;
    case PTLANG_AST_TYPE_FLOAT:
        return ptlang_rc_deref(type_1).content.float_size == ptlang_rc_deref(type_2).content.float_size;
    case PTLANG_AST_TYPE_FUNCTION:
        if (!ptlang_context_type_equals(ptlang_rc_deref(type_1).content.function.return_type,
                                        ptlang_rc_deref(type_2).content.function.return_type, type_scope))
            return false;
        if (arrlenu(ptlang_rc_deref(type_1).content.function.parameters) !=
            arrlenu(ptlang_rc_deref(type_1).content.function.parameters))
            return false;
        for (size_t i = 0; i < arrlenu(ptlang_rc_deref(type_1).content.function.parameters); i++)
        {
            if (!ptlang_context_type_equals(ptlang_rc_deref(type_1).content.function.parameters[i],
                                            ptlang_rc_deref(type_2).content.function.parameters[i],
                                            type_scope))

                return false;
        }
        return true;
    case PTLANG_AST_TYPE_HEAP_ARRAY:
        return ptlang_context_type_equals(ptlang_rc_deref(type_1).content.heap_array.type,
                                          ptlang_rc_deref(type_2).content.heap_array.type, type_scope);
    case PTLANG_AST_TYPE_ARRAY:
        return ptlang_context_type_equals(ptlang_rc_deref(type_1).content.array.type,
                                          ptlang_rc_deref(type_2).content.array.type, type_scope) &&
               ptlang_rc_deref(type_1).content.array.len == ptlang_rc_deref(type_2).content.array.len;
    case PTLANG_AST_TYPE_REFERENCE:
        return ptlang_context_type_equals(ptlang_rc_deref(type_1).content.reference.type,
                                          ptlang_rc_deref(type_2).content.reference.type, type_scope) &&
               ptlang_rc_deref(type_1).content.reference.writable ==
                   ptlang_rc_deref(type_2).content.reference.writable;
    case PTLANG_AST_TYPE_NAMED:
        return 0 == strcmp(ptlang_rc_deref(type_1).content.name, ptlang_rc_deref(type_2).content.name);
    }
}

size_t ptlang_context_type_to_string(ptlang_ast_type type, char *out, ptlang_context_type_scope *type_scope)
{
    size_t size;
    if (type == NULL)
    {
        size = sizeof("Error Type");
        if (out != NULL)
        {
            memcpy(out, "Error Type", size);
        }

        return size;
    }
    switch (ptlang_rc_deref(type).type)
    {
    case PTLANG_AST_TYPE_VOID:
        size = sizeof("void");
        if (out != NULL)
        {
            *(out + 0) = 'v';
            *(out + 1) = 'o';
            *(out + 2) = 'i';
            *(out + 3) = 'd';
        }
        break;
    case PTLANG_AST_TYPE_INTEGER:
    {
        size = sizeof("?8388607");
        if (out != NULL)
        {
            snprintf(out, size, "%c%" PRIu32, ptlang_rc_deref(type).content.integer.is_signed ? 's' : 'u',
                     ptlang_rc_deref(type).content.integer.size);
        }
        break;
    }
    case PTLANG_AST_TYPE_FLOAT:
    {
        size = sizeof("f128");
        if (out != NULL)
        {
            snprintf(out, size, "f%d", ptlang_rc_deref(type).content.float_size);
        }
        break;
    }
    case PTLANG_AST_TYPE_FUNCTION:
    {
        size = sizeof("(): ") - 1 +
               ptlang_context_type_to_string(ptlang_rc_deref(type).content.function.return_type, NULL,
                                             type_scope);
        for (size_t i = 0; i < arrlenu(ptlang_rc_deref(type).content.function.parameters); i++)
        {
            size += ptlang_context_type_to_string(ptlang_rc_deref(type).content.function.parameters[i], NULL,
                                                  type_scope) -
                    1 + sizeof(", ") - 1;
        }
        if (arrlenu(ptlang_rc_deref(type).content.function.parameters) != 0)
        {
            size -= sizeof(", ") - 1;
        }

        if (out != NULL)
        {
            *out = '(';
            out++;
            for (size_t i = 0; i < arrlenu(ptlang_rc_deref(type).content.function.parameters); i++)
            {
                out += ptlang_context_type_to_string(ptlang_rc_deref(type).content.function.parameters[i],
                                                     out, type_scope) -
                       1;
                if (i < arrlenu(ptlang_rc_deref(type).content.function.parameters) - 1)
                {
                    *out = ',';
                    out++;
                    *out = ' ';
                    out++;
                }
            }
            memcpy(out, "): ", sizeof("): ") - 1);
            out += sizeof("): ") - 1;

            ptlang_context_type_to_string(ptlang_rc_deref(type).content.function.return_type, out,
                                          type_scope);
        }

        break;
    }
    case PTLANG_AST_TYPE_HEAP_ARRAY:
    {

        size = ptlang_context_type_to_string(ptlang_rc_deref(type).content.array.type, out, type_scope) +
               sizeof("[]") - 1;

        if (out != NULL)
        {
            memcpy(out, "[]", sizeof("[]") - 1);
            ptlang_context_type_to_string(ptlang_rc_deref(type).content.array.type, out + sizeof("[]") - 1,
                                          type_scope);
        }
        break;
    }
    case PTLANG_AST_TYPE_ARRAY:
    {
        size_t element_size =
            ptlang_context_type_to_string(ptlang_rc_deref(type).content.array.type, out, type_scope);

        size = element_size + sizeof("[18446744073709551615]") - 1;

        if (out != NULL)
        {
            size_t prefix_len =
                snprintf(out, size - element_size, "[%" PRIu64 "]", ptlang_rc_deref(type).content.array.len);
            ptlang_context_type_to_string(ptlang_rc_deref(type).content.array.type, out + prefix_len,
                                          type_scope);
        }
        break;
    }
    case PTLANG_AST_TYPE_REFERENCE:
    {
        size = ptlang_context_type_to_string(ptlang_rc_deref(type).content.reference.type, NULL, type_scope) +
               sizeof("&") - 1;
        if (!ptlang_rc_deref(type).content.reference.writable)
        {
            size += sizeof("const ") - 1;
        }
        if (out != NULL)
        {
            *out = '&';
            out++;
            if (!ptlang_rc_deref(type).content.reference.writable)
            {
                memcpy(out + 1, "const ", sizeof("const ") - 1);
                out += sizeof("const ") - 1;
            }
            ptlang_context_type_to_string(ptlang_rc_deref(type).content.reference.type, out, type_scope);
        }

        break;
    }
    case PTLANG_AST_TYPE_NAMED:
    {
        // ptlang_ast_type unnamed_type = ptlang_context_unname_type(type, type_scope);
        // if (unnamed_type == type)
        // {
        size = strlen(ptlang_rc_deref(type).content.name) + 1;
        if (out != NULL)
        {
            memcpy(out, ptlang_rc_deref(type).content.name, size);
        }
        // }
        // else
        // {
        //     size = ptlang_context_type_to_string(unnamed_type, out, type_scope);
        // }
        break;
    }
    }
    return size;
}

ptlang_ast_struct_def ptlang_context_get_struct_def(char *name, ptlang_context_type_scope *type_scope)
{

    ptlang_ast_struct_def struct_def = NULL;
    while (true)
    {
        ptlang_context_type_scope_entry entry = shget(type_scope, name);
        if (entry.type == PTLANG_CONTEXT_TYPE_SCOPE_ENTRY_STRUCT)
        {
            struct_def = entry.value.struct_def;
            break;
        }
        if (entry.value.ptlang_type == NULL)
        {
            return NULL;
        }
        name = ptlang_rc_deref(entry.value.ptlang_type).content.name;
    }
    return struct_def;
}
