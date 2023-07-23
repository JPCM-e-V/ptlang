#include "ptlang_verify_impl.h"

// Type not found
// Var not foud
// Type mismatch
// modification (or creation of writable reference) of content of a non-writable reference
// modification of const
// double use of heap array
// use of reference after use of heap array
// double use of variable name (in same scope)
// double use of type name
// var use before declaration
// assignment to unassignable exp

// struct a { s64 attr; }
// & const a ptr;
// (*ptr).attr = 1;
// & const s64 attrpttr = & const ((*ptr).attr);

ptlang_error *ptlang_verify_module(ptlang_ast_module module, ptlang_context *ctx)
{
    ptlang_error *errors = NULL;
    ptlang_verify_type_resolvability(module, ctx, &errors);
    return errors;
}

static void ptlang_verify_type_resolvability(ptlang_ast_module ast_module, ptlang_context *ctx,
                                             ptlang_error **errors)
{

    // Type aliases

    pltang_verify_type_alias_table *alias_table = NULL;

    // pltang_verify_type_alias primitive_alias = ;

    shput(alias_table, "",
          ((pltang_verify_type_alias){
              .name = "",
              .resolved = false,
          }));

    for (size_t i = 0; i < arrlenu(ast_module->type_aliases); i++)
    {
        pltang_verify_type_alias type_alias =
            pltang_verify_type_alias_create(ast_module->type_aliases[i], ctx);
        shput(alias_table, ast_module->type_aliases[i].name.name, type_alias);
    }

    for (size_t i = 0; i < arrlenu(ast_module->type_aliases); i++)
    {
        pltang_verify_type_alias *type_alias = &shget(alias_table, ast_module->type_aliases[i].name.name);
        for (size_t j = 0; j < arrlenu(type_alias->referenced_types); j++)
        {
            size_t index = shgeti(alias_table, type_alias->referenced_types[j].name);
            if (index == -1)
            {
                size_t message_len =
                    sizeof("The type  is not defined.") + strlen(type_alias->referenced_types[j].name);
                char *message = ptlang_malloc(message_len);
                snprintf(message, message_len, "The type %s is not defined.",
                         type_alias->referenced_types[j].name);
                arrput(*errors, ((ptlang_error){
                                    .type = PTLANG_ERROR_TYPE_UNDEFINED,
                                    .pos = type_alias->referenced_types[j].pos,
                                    .message = message,
                                }));
            }
            else
            {
                arrput(alias_table[index].value.referencing_types, type_alias);
            }
        }
    }

    pltang_verify_type_alias **alias_candidates = NULL;
    arrput(alias_candidates, &alias_table[0].value);
    while (arrlenu(alias_candidates) != 0)
    {
        pltang_verify_type_alias *type_alias = arrpop(alias_candidates);

        // check if resolved
        if (type_alias->resolved)
            continue;
        type_alias->resolved = true;
        for (size_t i = 0; i < arrlenu(type_alias->referenced_types); i++)
        {
            if (!shget(alias_table, type_alias->referenced_types[i].name).resolved)
            {
                type_alias->resolved = false;
                break;
            }
        }
        if (!type_alias->resolved)
            continue;

        // add type to type scope
        if (*type_alias->name != '\0')
        {
            shput(ctx->type_scope, type_alias->name,
                  ((ptlang_context_type_scope_entry){
                      //   .type = pltang_verify_type(type_alias->type, ctx),
                      .ptlang_type = ptlang_context_unname_type(type_alias->type, ctx->type_scope),
                  }));
        }

        // add referencing types to candidates
        for (size_t i = 0; i < arrlenu(type_alias->referencing_types); i++)
        {
            arrput(alias_candidates, type_alias->referencing_types[i]);
        }
    }

    arrfree(alias_candidates);

    for (size_t i = 0; i < arrlenu(ast_module->type_aliases) + 1; i++)
    {
        pltang_verify_type_alias type_alias = alias_table[i].value;
        if (!type_alias.resolved)
        {
            size_t message_len = sizeof("The type  could not be resolved.") + strlen(type_alias.name);
            char *message = ptlang_malloc(message_len);
            snprintf(message, message_len, "The type %s could not be resolved.", type_alias.name);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_TYPE_UNRESOLVABLE,
                                .pos = type_alias.pos,
                                .message = message,
                            }));
        }
        arrfree(type_alias.referenced_types);
        arrfree(type_alias.referencing_types);
    }

    shfree(alias_table);
}

static ptlang_ast_ident *pltang_verify_type_alias_get_referenced_types_from_ast_type(ptlang_ast_type ast_type,
                                                                                     ptlang_context *ctx)
{
    ptlang_ast_ident *referenced_types = NULL;
    switch (ast_type->type)
    {
    case PTLANG_AST_TYPE_INTEGER:
    case PTLANG_AST_TYPE_FLOAT:
        arrput(referenced_types, ((ptlang_ast_ident){.name = "", .pos = ((ptlang_ast_code_position){0})}));
        break;
    case PTLANG_AST_TYPE_ARRAY:
        return pltang_verify_type_alias_get_referenced_types_from_ast_type(ast_type->content.array.type, ctx);
    case PTLANG_AST_TYPE_HEAP_ARRAY:
        return pltang_verify_type_alias_get_referenced_types_from_ast_type(ast_type->content.heap_array.type,
                                                                           ctx);
    case PTLANG_AST_TYPE_REFERENCE:
        return pltang_verify_type_alias_get_referenced_types_from_ast_type(ast_type->content.reference.type,
                                                                           ctx);
    case PTLANG_AST_TYPE_NAMED:
        if (shgeti(ctx->type_scope, ast_type->content.name) == -1)
            arrput(referenced_types,
                   ((ptlang_ast_ident){.name = ast_type->content.name, .pos = ast_type->pos}));
        else
            arrput(referenced_types,
                   ((ptlang_ast_ident){.name = "", .pos = ((ptlang_ast_code_position){0})}));
        break;
    case PTLANG_AST_TYPE_FUNCTION:
        referenced_types = pltang_verify_type_alias_get_referenced_types_from_ast_type(
            ast_type->content.function.return_type, ctx);
        for (size_t i = 0; i < arrlenu(ast_type->content.function.parameters); i++)
        {
            ptlang_ast_ident *param_referenced_types =
                pltang_verify_type_alias_get_referenced_types_from_ast_type(
                    ast_type->content.function.parameters[i], ctx);
            for (size_t j = 0; j < arrlenu(param_referenced_types); j++)
            {
                arrput(referenced_types, param_referenced_types[j]);
            }
            arrfree(param_referenced_types);
        }
        break;
    }
    return referenced_types;
}

static pltang_verify_type_alias
pltang_verify_type_alias_create(struct ptlang_ast_module_type_alias_s ast_type_alias, ptlang_context *ctx)
{
    // pltang_verify_type_alias type_alias = ptlang_malloc(sizeof(struct pltang_verify_type_alias_s));

    return (struct pltang_verify_type_alias_s){
        .name = ast_type_alias.name.name,
        .pos = ast_type_alias.pos,
        .type = ast_type_alias.type,
        .referenced_types =
            pltang_verify_type_alias_get_referenced_types_from_ast_type(ast_type_alias.type, ctx),
        .referencing_types = NULL,
        .resolved = false,
    };
    // return type_alias;
}
