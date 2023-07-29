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
    ptlang_verify_struct_defs(module->struct_defs, ctx, &errors);
    return errors;
}

static void ptlang_verify_struct_defs(ptlang_ast_struct_def *struct_defs, ptlang_context *ctx,
                                      ptlang_error **errors)
{

    // Type aliases

    pltang_verify_struct_table *struct_table = NULL;

    // pltang_verify_type_alias primitive_alias = ;

    for (size_t i = 0; i < arrlenu(struct_defs); i++)
    {
        for (size_t j = 0; j < arrlenu(struct_defs[i]->members); j++)
        {
            for (size_t k = 0; k < j; k++)
            {
                if (strcmp(struct_defs[i]->members[j]->name.name, struct_defs[i]->members[k]->name.name) == 0)
                {
                    size_t message_len = sizeof("The struct member name '' is already used.") +
                                         strlen(struct_defs[i]->members[j]->name.name);
                    char *message = ptlang_malloc(message_len);
                    snprintf(message, message_len, "The struct member name '%s' is already used.",
                             struct_defs[i]->members[j]->name.name);
                    arrput(*errors, ((ptlang_error){
                                        .type = PTLANG_ERROR_STRUCT_MEMBER_DUPLICATION,
                                        .pos = struct_defs[i]->members[j]->name.pos,
                                        .message = message,
                                    }));
                }
            }
        }

        pltang_verify_struct struct_ = pltang_verify_struct_create(struct_defs[i], ctx, errors);
        shput(struct_table, struct_defs[i]->name.name, struct_);
    }

    pltang_verify_struct **candidates = NULL;

    for (size_t i = 0; i < arrlenu(struct_defs); i++)
    {
        pltang_verify_struct *struct_ = &shget(struct_table, struct_defs[i]->name.name);
        for (size_t j = 0; j < arrlenu(struct_->referenced_types); j++)
        {
            size_t index = shgeti(struct_table, struct_->referenced_types[j]);
            arrput(struct_table[index].value.referencing_types, struct_);
        }
        if (arrlenu(struct_->referenced_types) == 0)
        {
            arrput(candidates, struct_);
        }
    }

    while (arrlenu(candidates) != 0)
    {
        pltang_verify_struct *struct_ = arrpop(candidates);

        // check if resolved
        if (struct_->resolved)
            continue;
        struct_->resolved = true;
        for (size_t i = 0; i < arrlenu(struct_->referenced_types); i++)
        {
            if (!shget(struct_table, struct_->referenced_types[i]).resolved)
            {
                struct_->resolved = false;
                break;
            }
        }
        if (!struct_->resolved)
            continue;

        // add referencing types to candidates
        for (size_t i = 0; i < arrlenu(struct_->referencing_types); i++)
        {
            arrput(candidates, struct_->referencing_types[i]);
        }
    }

    arrfree(candidates);

    for (size_t i = 0; i < shlen(struct_table); i++)
    {
        pltang_verify_struct struct_ = struct_table[i].value;
        if (!struct_.resolved)
        {
            size_t message_len = sizeof("The struct  is recursive.") + strlen(struct_.name);
            char *message = ptlang_malloc(message_len);
            snprintf(message, message_len, "The struct %s is recursive.", struct_.name);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_STRUCT_RECURSION,
                                .pos = struct_.pos,
                                .message = message,
                            }));
        }
        arrfree(struct_.referenced_types);
        arrfree(struct_.referencing_types);
    }

    shfree(struct_table);
}

static bool ptlang_verify_type(ptlang_ast_type type, ptlang_context *ctx, ptlang_error **errors)
{
    switch (type->type)
    {
    case PTLANG_AST_TYPE_FUNCTION:
    {
        bool correct = ptlang_verify_type(type->content.function.return_type, ctx, errors);
        for (size_t i = 0; i < arrlenu(type->content.function.parameters); i++)
        {
            correct = ptlang_verify_type(type->content.function.parameters[i], ctx, errors) && correct;
        }
        return correct;
    }
    case PTLANG_AST_TYPE_HEAP_ARRAY:
        return ptlang_verify_type(type->content.heap_array.type, ctx, errors);
    case PTLANG_AST_TYPE_ARRAY:
        return ptlang_verify_type(type->content.array.type, ctx, errors);
    case PTLANG_AST_TYPE_REFERENCE:
        return ptlang_verify_type(type->content.reference.type, ctx, errors);
    case PTLANG_AST_TYPE_NAMED:
        if (shgeti(ctx->type_scope, type->content.name) == -1)
        {
            size_t message_len = sizeof("The type  is not defined.") + strlen(type->content.name);
            char *message = ptlang_malloc(message_len);
            snprintf(message, message_len, "The type %s is not defined.", type->content.name);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_TYPE_UNDEFINED,
                                .pos = type->pos,
                                .message = message,
                            }));
            return false;
        }
        else
        {
            return true;
        }
    default:
        return true;
    }
}

static void ptlang_verify_type_resolvability(ptlang_ast_module ast_module, ptlang_context *ctx,
                                             ptlang_error **errors)
{
    // add structs to typescope

    for (size_t i = 0; i < arrlenu(ast_module->struct_defs); i++)
    {
        shput(ctx->type_scope, ast_module->struct_defs[i]->name.name,
              ((ptlang_context_type_scope_entry){.type = PTLANG_CONTEXT_TYPE_SCOPE_ENTRY_STRUCT,
                                                 .value.struct_def = ast_module->struct_defs[i]}));
    }

    // Type aliases

    pltang_verify_type_alias_table *struct_table = NULL;

    // pltang_verify_type_alias primitive_alias = ;

    shput(struct_table, "",
          ((pltang_verify_type_alias){
              .name = "",
              .resolved = false,
          }));

    for (size_t i = 0; i < arrlenu(ast_module->type_aliases); i++)
    {
        pltang_verify_type_alias struct_ = pltang_verify_type_alias_create(ast_module->type_aliases[i], ctx);
        shput(struct_table, ast_module->type_aliases[i].name.name, struct_);
    }

    for (size_t i = 0; i < arrlenu(ast_module->type_aliases); i++)
    {
        pltang_verify_type_alias *struct_ = &shget(struct_table, ast_module->type_aliases[i].name.name);
        for (size_t j = 0; j < arrlenu(struct_->referenced_types); j++)
        {
            size_t index = shgeti(struct_table, struct_->referenced_types[j].name);
            if (index == -1)
            {
                size_t message_len =
                    sizeof("The type  is not defined.") + strlen(struct_->referenced_types[j].name);
                char *message = ptlang_malloc(message_len);
                snprintf(message, message_len, "The type %s is not defined.",
                         struct_->referenced_types[j].name);
                arrput(*errors, ((ptlang_error){
                                    .type = PTLANG_ERROR_TYPE_UNDEFINED,
                                    .pos = struct_->referenced_types[j].pos,
                                    .message = message,
                                }));
            }
            else
            {
                arrput(struct_table[index].value.referencing_types, struct_);
            }
        }
    }

    pltang_verify_type_alias **candidates = NULL;
    arrput(candidates, &struct_table[0].value);
    while (arrlenu(candidates) != 0)
    {
        pltang_verify_type_alias *struct_ = arrpop(candidates);

        // check if resolved
        if (struct_->resolved)
            continue;
        struct_->resolved = true;
        for (size_t i = 0; i < arrlenu(struct_->referenced_types); i++)
        {
            if (!shget(struct_table, struct_->referenced_types[i].name).resolved)
            {
                struct_->resolved = false;
                break;
            }
        }
        if (!struct_->resolved)
            continue;

        // add type to type scope
        if (*struct_->name != '\0')
        {
            shput(ctx->type_scope, struct_->name,
                  ((ptlang_context_type_scope_entry){
                      //   .type = pltang_verify_type(struct_->type, ctx),
                      .type = PTLANG_CONTEXT_TYPE_SCOPE_ENTRY_TYPEALIAS,
                      .value.ptlang_type = ptlang_context_unname_type(struct_->type, ctx->type_scope),
                  }));
        }

        // add referencing types to candidates
        for (size_t i = 0; i < arrlenu(struct_->referencing_types); i++)
        {
            arrput(candidates, struct_->referencing_types[i]);
        }
    }

    arrfree(candidates);

    for (size_t i = 0; i < arrlenu(ast_module->type_aliases) + 1; i++)
    {
        pltang_verify_type_alias struct_ = struct_table[i].value;
        if (!struct_.resolved)
        {
            size_t message_len = sizeof("The type  could not be resolved.") + strlen(struct_.name);
            char *message = ptlang_malloc(message_len);
            snprintf(message, message_len, "The type %s could not be resolved.", struct_.name);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_TYPE_UNRESOLVABLE,
                                .pos = struct_.pos,
                                .message = message,
                            }));
        }
        arrfree(struct_.referenced_types);
        arrfree(struct_.referencing_types);
    }

    shfree(struct_table);
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
    // pltang_verify_type_alias struct_ = ptlang_malloc(sizeof(struct pltang_verify_type_alias_s));

    return (struct pltang_verify_type_alias_s){
        .name = ast_type_alias.name.name,
        .pos = ast_type_alias.pos,
        .type = ast_type_alias.type,
        .referenced_types =
            pltang_verify_type_alias_get_referenced_types_from_ast_type(ast_type_alias.type, ctx),
        .referencing_types = NULL,
        .resolved = false,
    };
    // return struct_;
}

static char **pltang_verify_struct_get_referenced_types_from_struct_def(ptlang_ast_struct_def struct_def,
                                                                        ptlang_context *ctx,
                                                                        ptlang_error **errors)
{
    char **referenced_types = NULL;

    for (size_t i = 0; i < arrlenu(struct_def->members); i++)
    {
        ptlang_ast_type type = struct_def->members[i]->type;
        if (!ptlang_verify_type(type, ctx, errors))
            continue;
        type = ptlang_context_unname_type(type, ctx->type_scope);
        while (type->type == PTLANG_AST_TYPE_ARRAY)
        {
            type = type->content.array.type;
            type = ptlang_context_unname_type(type, ctx->type_scope);
        }

        if (type->type == PTLANG_AST_TYPE_NAMED)
        {
            // => must be struct
            arrput(referenced_types, type->content.name);
        }
    }
    return referenced_types;
}

static pltang_verify_struct pltang_verify_struct_create(ptlang_ast_struct_def ast_struct_def,
                                                        ptlang_context *ctx, ptlang_error **errors)
{

    return (struct pltang_verify_struct_s){
        .name = ast_struct_def->name.name,
        .pos = ast_struct_def->pos,
        .referenced_types =
            pltang_verify_struct_get_referenced_types_from_struct_def(ast_struct_def, ctx, errors),
        .referencing_types = NULL,
        .resolved = false,
    };
    // return struct_;
}
