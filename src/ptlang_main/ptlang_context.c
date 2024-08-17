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
    if (type == NULL)
    {
        if (out != NULL)
        {
            memcpy(out, "Error Type", sizeof("Error Type"));
        }

        return sizeof("Error Type") - 1;
    }
    switch (ptlang_rc_deref(type).type)
    {
    case PTLANG_AST_TYPE_VOID:
        if (out != NULL)
        {
            memcpy(out, "void", sizeof("void"));
        }

        return sizeof("void") - 1;
    case PTLANG_AST_TYPE_INTEGER:
    {
        if (out != NULL)
        {
            return snprintf(out, sizeof("?8388607"), "%c%" PRIu32,
                            ptlang_rc_deref(type).content.integer.is_signed ? 's' : 'u',
                            ptlang_rc_deref(type).content.integer.size);
        }
        return sizeof("?8388607") - 1;
    }
    case PTLANG_AST_TYPE_FLOAT:
    {
        if (out != NULL)
        {
            return snprintf(out, sizeof("f128"), "f%d", ptlang_rc_deref(type).content.float_size);
        }
        return sizeof("f128") - 1;
    }
    case PTLANG_AST_TYPE_FUNCTION:
    {
        if (out != NULL)
        {
            size_t len = 0;
            out[len] = '(';
            len++;
            for (size_t i = 0; i < arrlenu(ptlang_rc_deref(type).content.function.parameters); i++)
            {
                len += ptlang_context_type_to_string(ptlang_rc_deref(type).content.function.parameters[i],
                                                     out + len, type_scope);
                if (i < arrlenu(ptlang_rc_deref(type).content.function.parameters) - 1)
                {
                    out[len] = ',';
                    len++;
                    out[len] = ' ';
                    len++;
                }
            }
            memcpy(&out[len], "): ", sizeof("): "));
            len += sizeof("): ") - 1;

            len += ptlang_context_type_to_string(ptlang_rc_deref(type).content.function.return_type, out,
                                                 type_scope);
            return len;
        }

        size_t size = sizeof("(): ") - 1 +
                      ptlang_context_type_to_string(ptlang_rc_deref(type).content.function.return_type, NULL,
                                                    type_scope);
        for (size_t i = 0; i < arrlenu(ptlang_rc_deref(type).content.function.parameters); i++)
        {
            size += ptlang_context_type_to_string(ptlang_rc_deref(type).content.function.parameters[i], NULL,
                                                  type_scope) +
                    sizeof(", ") - 1;
        }
        if (arrlenu(ptlang_rc_deref(type).content.function.parameters) != 0)
        {
            size -= sizeof(", ") - 1;
        }
        return size;
        break;
    }
    case PTLANG_AST_TYPE_HEAP_ARRAY:
    {

        if (out != NULL)
        {
            memcpy(out, "[]", sizeof("[]"));
            return 2 + ptlang_context_type_to_string(ptlang_rc_deref(type).content.array.type,
                                                     out + sizeof("[]") - 1, type_scope);
        }
        return ptlang_context_type_to_string(ptlang_rc_deref(type).content.array.type, out, type_scope) +
               sizeof("[]") - 1;
    }
    case PTLANG_AST_TYPE_ARRAY:
    {

        if (out != NULL)
        {
            size_t prefix_len = snprintf(out, sizeof("[18446744073709551615]"), "[%" PRIu64 "]",
                                         ptlang_rc_deref(type).content.array.len);
            return prefix_len + ptlang_context_type_to_string(ptlang_rc_deref(type).content.array.type,
                                                              out + prefix_len, type_scope);
        }

        return ptlang_context_type_to_string(ptlang_rc_deref(type).content.array.type, out, type_scope) +
               sizeof("[18446744073709551615]") - 1;
    }
    case PTLANG_AST_TYPE_REFERENCE:
    {

        if (out != NULL)
        {
            size_t len = 0;
            *out = '&';
            len++;
            if (!ptlang_rc_deref(type).content.reference.writable)
            {
                memcpy(&out[len], "const ", sizeof("const "));
                out += sizeof("const ") - 1;
            }
            len += ptlang_context_type_to_string(ptlang_rc_deref(type).content.reference.type, out + len,
                                                 type_scope);
            return len;
        }

        size_t size =
            ptlang_context_type_to_string(ptlang_rc_deref(type).content.reference.type, NULL, type_scope) +
            sizeof("&") - 1;
        if (!ptlang_rc_deref(type).content.reference.writable)
        {
            size += sizeof("const ") - 1;
        }
        return size;
    }
    case PTLANG_AST_TYPE_NAMED:
    {
        // ptlang_ast_type unnamed_type = ptlang_context_unname_type(type, type_scope);
        // if (unnamed_type == type)
        // {
        size_t size = strlen(ptlang_rc_deref(type).content.name);
        if (out != NULL)
        {
            memcpy(out, ptlang_rc_deref(type).content.name, size + 1);
        }
        return size;
        // }
        // else
        // {
        //     size = ptlang_context_type_to_string(unnamed_type, out, type_scope);
        // }
    }
    }
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
