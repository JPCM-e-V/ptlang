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
    for (size_t i = 0; i < arrlenu(module->declarations); i++)
    {
        ptlang_verify_decl(module->declarations[i], 0, ctx, &errors);
    }
    ptlang_verify_functions(module->functions, ctx, &errors);
    return errors;
}

static void ptlang_verify_functions(ptlang_ast_func *functions, ptlang_context *ctx, ptlang_error **errors)
{
    for (size_t i = 0; i < arrlenu(functions); i++)
    {
        ptlang_verify_function(functions[i], ctx, errors);
    }
}

static void ptlang_verify_function(ptlang_ast_func function, ptlang_context *ctx, ptlang_error **errors)
{
    bool validate_return_type = ptlang_verify_type(function->return_type, ctx, errors);

    size_t function_scope_offset = arrlenu(ctx->scope);

    for (size_t i = 0; i < arrlenu(function->parameters); i++)
    {
        if (!ptlang_verify_type(function->parameters[i]->type, ctx, errors))
        {
            function->parameters[i]->type = NULL;
        }
        arrput(ctx->scope, function->parameters[i]);
    }

    size_t function_stmt_scope_offset = arrlenu(ctx->scope);

    bool has_return_value = false;
    bool is_unreachable = false;

    ptlang_verify_statement(function->stmt, 0, validate_return_type, function->return_type,
                            function_stmt_scope_offset, &has_return_value, &is_unreachable, ctx, errors);

    // arrsetlen(ctx->scope, function_stmt_scope_offset);
    arrsetlen(ctx->scope, function_scope_offset);
}

// returns false, if there was any error
static void ptlang_verify_statement(ptlang_ast_stmt statement, uint64_t nesting_level,
                                    bool validate_return_type, ptlang_ast_type wanted_return_type,
                                    size_t scope_offset, bool *has_return_value, bool *is_unreachable,
                                    ptlang_context *ctx, ptlang_error **errors)
{
    *has_return_value = false;
    switch (statement->type)
    {
    case PTLANG_AST_STMT_BLOCK:
    {
        size_t new_scope_offset = arrlenu(ctx->scope);
        for (size_t i = 0; i < arrlenu(statement->content.block.stmts); i++)
        {
            if (*is_unreachable)
            {
                arrput(*errors, ((ptlang_error){
                                    .type = PTLANG_ERROR_CODE_UNREACHABLE,
                                    .pos = statement->content.block.stmts[i]->pos,
                                    .message = "Dead code detected.",
                                }));
                *is_unreachable = false;
            }
            bool *child_node_has_return_value = false;
            ptlang_verify_statement(statement->content.block.stmts[i], nesting_level, validate_return_type,
                                    wanted_return_type, new_scope_offset, child_node_has_return_value,
                                    is_unreachable, ctx, errors);
            *has_return_value |= *child_node_has_return_value;
        }
        arrsetlen(ctx->scope, new_scope_offset);
        break;
    }
    case PTLANG_AST_STMT_EXP:
        ptlang_verify_exp(statement->content.exp, ctx, errors);
        break;

    case PTLANG_AST_STMT_DECL:
        ptlang_verify_decl(statement->content.decl, scope_offset, ctx, errors);
        break;
    case PTLANG_AST_STMT_IF:
    case PTLANG_AST_STMT_WHILE:
    case PTLANG_AST_STMT_IF_ELSE:
    {
        ptlang_ast_exp condition = statement->type == PTLANG_AST_STMT_IF_ELSE
                                       ? statement->content.control_flow2.condition
                                       : statement->content.control_flow.condition;
        if (ptlang_verify_exp(condition, ctx, errors))
        {
            if (condition->ast_type->type != PTLANG_AST_TYPE_INTEGER &&
                condition->ast_type->type != PTLANG_AST_TYPE_FLOAT)
            {
                arrput(*errors, ((ptlang_error){
                                    .type = PTLANG_ERROR_TYPE_MISMATCH,
                                    .pos = condition->pos,
                                    .message = "Control flow condition must be a float or an int.",
                                }));
            }
        }

        if (statement->type != PTLANG_AST_STMT_IF_ELSE)
        {

            ptlang_verify_statement(statement->content.control_flow.stmt,
                                    nesting_level + (statement->type == PTLANG_AST_STMT_WHILE),
                                    validate_return_type, wanted_return_type, scope_offset, has_return_value,
                                    is_unreachable, ctx, errors);
            *is_unreachable = false;
            *has_return_value = false;
        }
        else
        {
            bool child_nodes_have_return_value[2] = {false, false};
            bool child_nodes_are_unreachable[2] = {false, false};

            ptlang_verify_statement(statement->content.control_flow2.stmt[0], nesting_level,
                                    validate_return_type, wanted_return_type, scope_offset,
                                    &child_nodes_have_return_value[0], &child_nodes_are_unreachable[0], ctx,
                                    errors);
            ptlang_verify_statement(statement->content.control_flow2.stmt[1], nesting_level,
                                    validate_return_type, wanted_return_type, scope_offset,
                                    &child_nodes_have_return_value[1], &child_nodes_are_unreachable[1], ctx,
                                    errors);
            *has_return_value = child_nodes_have_return_value[0] && child_nodes_have_return_value[1];
            *is_unreachable = child_nodes_are_unreachable[0] && child_nodes_are_unreachable[1];
        }
        break;
    }
    case PTLANG_AST_STMT_RETURN:
        *is_unreachable = true;
    case PTLANG_AST_STMT_RET_VAL:
        *has_return_value = false;
        bool expression_correct = ptlang_verify_exp(statement->content.exp, ctx, errors);
        if (validate_return_type)
        {
            if (expression_correct)
            {
                ptlang_ast_type return_type = statement->content.exp->ast_type;
                ptlang_verify_check_cast(return_type, wanted_return_type, statement->content.exp->pos, ctx,
                                         errors);
            }
        }
        break;
    case PTLANG_AST_STMT_BREAK:
    case PTLANG_AST_STMT_CONTINUE:
        *has_return_value = false;

        if (statement->content.nesting_level == 0)
        {
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_NESTING_LEVEL_OUT_OF_RANGE,
                                .pos = statement->pos,
                                .message = "Nesting level must be a positive integer.",
                            }));
        }
        else if (statement->content.nesting_level > nesting_level)
        {
            size_t message_len = sizeof("Given nesting level 18446744073709551615 exceeds current nesting "
                                        "level of 18446744073709551615");
            char *message = ptlang_malloc(message_len);
            snprintf(message, message_len,
                     "Given nesting level %" PRIu64 " exceeds current nesting level of %" PRIu64,
                     statement->content.nesting_level, nesting_level);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_NESTING_LEVEL_OUT_OF_RANGE,
                                .pos = statement->pos,
                                .message = message,
                            }));
        }
        else
        {
            *is_unreachable = true;
        }

        break;
    }
}

static void ptlang_verify_decl(ptlang_ast_decl decl, size_t scope_offset, ptlang_context *ctx,
                               ptlang_error **errors)
{

    bool correct = ptlang_verify_type(decl->type, ctx, errors);

    for (size_t i = scope_offset; i < arrlenu(ctx->scope); i++)
    {
        if (strcmp(ctx->scope[i]->name.name, decl->name.name) == 0)
        {
            size_t message_len = sizeof("The variable name '' is already used in the current scope.") +
                                 strlen(decl->name.name);
            char *message = ptlang_malloc(message_len);
            snprintf(message, message_len, "The variable name '%s' is already used in the current scope.",
                     decl->name.name);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_STRUCT_MEMBER_DUPLICATION,
                                .pos = decl->pos,
                                .message = message,
                            }));
            correct = false;
            break;
        }
    }

    if (correct)
    {

        arrpush(ctx->scope, decl);
    }
}

static bool ptlang_verify_exp_is_mutable(ptlang_ast_exp exp, ptlang_context *ctx)
{
    switch (exp->type)
    {
    case PTLANG_AST_EXP_VARIABLE:
    {
        for (size_t i = 0; i < arrlenu(ctx->scope); i++)
        {
            if (strcmp(ctx->scope[i]->name.name, exp->content.str_prepresentation) == 0)
            {
                return ctx->scope[i]->writable;
            }
        }
        return true;
    }
    case PTLANG_AST_EXP_STRUCT_MEMBER:
    {
        return ptlang_verify_exp_is_mutable(exp->content.struct_member.struct_, ctx);
    }
    case PTLANG_AST_EXP_ARRAY_ELEMENT:
    {
        return ptlang_verify_exp_is_mutable(exp->content.array_element.array, ctx);
    }
    case PTLANG_AST_EXP_DEREFERENCE:
    {
        return exp->content.unary_operator->ast_type->content.reference.writable;
    }
    default:
        return false;
    }
}

static bool ptlang_verify_child_exp(ptlang_ast_exp left, ptlang_ast_exp right, ptlang_ast_type *out)
{
    if (left->ast_type == NULL || right->ast_type == NULL)
    {
        *out = NULL;
        return true;
    }
    if ((left->ast_type->type == PTLANG_AST_TYPE_INTEGER || left->ast_type->type == PTLANG_AST_TYPE_FLOAT) &&
        (right->ast_type->type == PTLANG_AST_TYPE_INTEGER || right->ast_type->type == PTLANG_AST_TYPE_FLOAT))
    {
        if (left->ast_type->type == PTLANG_AST_TYPE_INTEGER &&
            right->ast_type->type == PTLANG_AST_TYPE_INTEGER)
        {
            *out = ptlang_ast_type_integer(
                left->ast_type->content.integer.is_signed || right->ast_type->content.integer.is_signed,
                left->ast_type->content.integer.size > right->ast_type->content.integer.size
                    ? left->ast_type->content.integer.size
                    : right->ast_type->content.integer.size,
                (ptlang_ast_code_position){0});
        }
        else
        {
            uint32_t left_size = left->ast_type->type == PTLANG_AST_TYPE_INTEGER
                                     ? left->ast_type->content.integer.size
                                     : left->ast_type->content.float_size;
            uint32_t right_size = right->ast_type->type == PTLANG_AST_TYPE_INTEGER
                                      ? right->ast_type->content.integer.size
                                      : right->ast_type->content.float_size;

            uint32_t size = left_size > right_size ? left_size : right_size;
            enum ptlang_ast_type_float_size float_size;
            if (size <= PTLANG_AST_TYPE_FLOAT_16)
            {
                float_size = PTLANG_AST_TYPE_FLOAT_16;
            }
            else if (size <= PTLANG_AST_TYPE_FLOAT_32)
            {
                float_size = PTLANG_AST_TYPE_FLOAT_32;
            }
            else if (size <= PTLANG_AST_TYPE_FLOAT_64)
            {
                float_size = PTLANG_AST_TYPE_FLOAT_64;
            }
            else
            {
                float_size = PTLANG_AST_TYPE_FLOAT_128;
            }
            *out = ptlang_ast_type_float(float_size, (ptlang_ast_code_position){0});
        }
        return true;
    }
    return false;
}

// returns true if the expression is correct
static bool ptlang_verify_exp(ptlang_ast_exp exp, ptlang_context *ctx, ptlang_error **errors)
{
    ptlang_ast_exp left = exp->content.binary_operator.left_value;
    ptlang_ast_exp right = exp->content.binary_operator.right_value;

    switch (exp->type)
    {
    case PTLANG_AST_EXP_ASSIGNMENT:
    {
        ptlang_verify_exp(left, ctx, errors);
        ptlang_verify_exp(right, ctx, errors);

        if (!ptlang_verify_exp_is_mutable(left, ctx))
        {
            size_t message_len = sizeof("Writing to unwritable expression.");
            char *message = ptlang_malloc(message_len);
            memcpy(message, "Writing to unwritable expression.", message_len);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_EXP_UNWRITABLE,
                                .pos = exp->pos,
                                .message = message,
                            }));
        }

        exp->ast_type = ptlang_ast_type_copy(left->ast_type);
        ptlang_verify_check_cast(right->ast_type, left->ast_type, exp->pos, ctx, errors);
        return false;
    }
    case PTLANG_AST_EXP_ADDITION:
    case PTLANG_AST_EXP_SUBTRACTION:
    case PTLANG_AST_EXP_MULTIPLICATION:
    case PTLANG_AST_EXP_DIVISION:
    case PTLANG_AST_EXP_MODULO:
    case PTLANG_AST_EXP_REMAINDER:
    {
        ptlang_verify_exp(left, ctx, errors);
        ptlang_verify_exp(right, ctx, errors);

        if (!ptlang_verify_child_exp(left, right, &exp->ast_type))
        {
            size_t message_len = sizeof("Operation arguments are incompatible.");
            char *message = ptlang_malloc(message_len);
            memcpy(message, "Operation arguments are incompatible.", message_len);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_TYPE_MISMATCH,
                                .pos = exp->pos,
                                .message = message,
                            }));
        }
        return false;
    }
    case PTLANG_AST_EXP_NEGATION:
    {
        return false;
    }
    case PTLANG_AST_EXP_EQUAL:
    {
        return false;
    }
    case PTLANG_AST_EXP_NOT_EQUAL:
    {
        return false;
    }
    case PTLANG_AST_EXP_GREATER:
    {
        return false;
    }
    case PTLANG_AST_EXP_GREATER_EQUAL:
    {
        return false;
    }
    case PTLANG_AST_EXP_LESS:
    {
        return false;
    }
    case PTLANG_AST_EXP_LESS_EQUAL:
    {
        return false;
    }
    case PTLANG_AST_EXP_LEFT_SHIFT:
    {
        return false;
    }
    case PTLANG_AST_EXP_RIGHT_SHIFT:
    {
        return false;
    }
    case PTLANG_AST_EXP_AND:
    {
        return false;
    }
    case PTLANG_AST_EXP_OR:
    {
        return false;
    }
    case PTLANG_AST_EXP_NOT:
    {
        return false;
    }
    case PTLANG_AST_EXP_BITWISE_AND:
    {
        return false;
    }
    case PTLANG_AST_EXP_BITWISE_OR:
    {
        return false;
    }
    case PTLANG_AST_EXP_BITWISE_XOR:
    {
        return false;
    }
    case PTLANG_AST_EXP_BITWISE_INVERSE:
    {
        return false;
    }
    case PTLANG_AST_EXP_LENGTH:
    {
        return false;
    }
    case PTLANG_AST_EXP_FUNCTION_CALL:
    {
        return false;
    }
    case PTLANG_AST_EXP_VARIABLE:
    {
        return false;
    }
    case PTLANG_AST_EXP_INTEGER:
    {
        return false;
    }
    case PTLANG_AST_EXP_FLOAT:
    {
        return false;
    }
    case PTLANG_AST_EXP_STRUCT:
    {
        return false;
    }
    case PTLANG_AST_EXP_ARRAY:
    {
        return false;
    }
    case PTLANG_AST_EXP_TERNARY:
    {
        return false;
    }
    case PTLANG_AST_EXP_CAST:
    {
        return false;
    }
    case PTLANG_AST_EXP_STRUCT_MEMBER:
    {
        return false;
    }
    case PTLANG_AST_EXP_ARRAY_ELEMENT:
    {
        return false;
    }
    case PTLANG_AST_EXP_REFERENCE:
    {
        return false;
    }
    case PTLANG_AST_EXP_DEREFERENCE:
    {
        return false;
    }
    }
}

static void ptlang_verify_struct_defs(ptlang_ast_struct_def *struct_defs, ptlang_context *ctx,
                                      ptlang_error **errors)
{

    // Type aliases

    pltang_verify_struct_table *type_alias_table = NULL;

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

        pltang_verify_struct type_alias = pltang_verify_struct_create(struct_defs[i], ctx, errors);
        shput(type_alias_table, struct_defs[i]->name.name, type_alias);
    }

    pltang_verify_struct **candidates = NULL;

    for (size_t i = 0; i < arrlenu(struct_defs); i++)
    {
        pltang_verify_struct *type_alias = &shget(type_alias_table, struct_defs[i]->name.name);
        for (size_t j = 0; j < arrlenu(type_alias->referenced_types); j++)
        {
            size_t index = shgeti(type_alias_table, type_alias->referenced_types[j]);
            arrput(type_alias_table[index].value.referencing_types, type_alias);
        }
        if (arrlenu(type_alias->referenced_types) == 0)
        {
            arrput(candidates, type_alias);
        }
    }

    while (arrlenu(candidates) != 0)
    {
        pltang_verify_struct *type_alias = arrpop(candidates);

        // check if resolved
        if (type_alias->resolved)
            continue;
        type_alias->resolved = true;
        for (size_t i = 0; i < arrlenu(type_alias->referenced_types); i++)
        {
            if (!shget(type_alias_table, type_alias->referenced_types[i]).resolved)
            {
                type_alias->resolved = false;
                break;
            }
        }
        if (!type_alias->resolved)
            continue;

        // add referencing types to candidates
        for (size_t i = 0; i < arrlenu(type_alias->referencing_types); i++)
        {
            arrput(candidates, type_alias->referencing_types[i]);
        }
    }

    arrfree(candidates);

    for (size_t i = 0; i < shlen(type_alias_table); i++)
    {
        pltang_verify_struct type_alias = type_alias_table[i].value;
        if (!type_alias.resolved)
        {
            size_t message_len = sizeof("The struct  is recursive.") + strlen(type_alias.name);
            char *message = ptlang_malloc(message_len);
            snprintf(message, message_len, "The struct %s is recursive.", type_alias.name);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_STRUCT_RECURSION,
                                .pos = type_alias.pos,
                                .message = message,
                            }));
        }
        arrfree(type_alias.referenced_types);
        arrfree(type_alias.referencing_types);
    }

    shfree(type_alias_table);
}

static bool ptlang_verify_type(ptlang_ast_type type, ptlang_context *ctx, ptlang_error **errors)
{
    switch (type->type)
    {
    case PTLANG_AST_TYPE_VOID:
        return true;
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

    pltang_verify_type_alias_table *type_alias_table = NULL;

    // pltang_verify_type_alias primitive_alias = ;

    shput(type_alias_table, "",
          ((pltang_verify_type_alias){
              .name = "",
              .resolved = false,
          }));

    for (size_t i = 0; i < arrlenu(ast_module->type_aliases); i++)
    {
        pltang_verify_type_alias type_alias =
            pltang_verify_type_alias_create(ast_module->type_aliases[i], ctx);
        shput(type_alias_table, ast_module->type_aliases[i].name.name, type_alias);
    }

    for (size_t i = 0; i < arrlenu(ast_module->type_aliases); i++)
    {
        pltang_verify_type_alias *type_alias =
            &shget(type_alias_table, ast_module->type_aliases[i].name.name);
        for (size_t j = 0; j < arrlenu(type_alias->referenced_types); j++)
        {
            size_t index = shgeti(type_alias_table, type_alias->referenced_types[j].name);
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
                arrput(type_alias_table[index].value.referencing_types, type_alias);
            }
        }
    }

    pltang_verify_type_alias **candidates = NULL;
    arrput(candidates, &type_alias_table[0].value);
    while (arrlenu(candidates) != 0)
    {
        pltang_verify_type_alias *type_alias = arrpop(candidates);

        // check if resolved
        if (type_alias->resolved)
            continue;
        type_alias->resolved = true;
        for (size_t i = 0; i < arrlenu(type_alias->referenced_types); i++)
        {
            if (!shget(type_alias_table, type_alias->referenced_types[i].name).resolved)
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
                      .type = PTLANG_CONTEXT_TYPE_SCOPE_ENTRY_TYPEALIAS,
                      .value.ptlang_type = ptlang_context_unname_type(type_alias->type, ctx->type_scope),
                  }));
        }

        // add referencing types to candidates
        for (size_t i = 0; i < arrlenu(type_alias->referencing_types); i++)
        {
            arrput(candidates, type_alias->referencing_types[i]);
        }
    }

    arrfree(candidates);

    for (size_t i = 0; i < arrlenu(ast_module->type_aliases) + 1; i++)
    {
        pltang_verify_type_alias type_alias = type_alias_table[i].value;
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
            shput(ctx->type_scope, type_alias.name,
                  ((ptlang_context_type_scope_entry){
                      //   .type = pltang_verify_type(type_alias->type, ctx),
                      .type = PTLANG_CONTEXT_TYPE_SCOPE_ENTRY_TYPEALIAS,
                      .value.ptlang_type = NULL,
                  }));
        }
        arrfree(type_alias.referenced_types);
        arrfree(type_alias.referencing_types);
    }

    shfree(type_alias_table);
}

static ptlang_ast_ident *pltang_verify_type_alias_get_referenced_types_from_ast_type(ptlang_ast_type ast_type,
                                                                                     ptlang_context *ctx)
{
    ptlang_ast_ident *referenced_types = NULL;
    switch (ast_type->type)
    {
    case PTLANG_AST_TYPE_VOID:
        abort();
        break;
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
    // return type_alias;
}

static bool ptlang_verify_cast(ptlang_ast_type from, ptlang_ast_type to, ptlang_context *ctx)
{
    if (from == NULL || to == NULL)
    {
        return true;
    }
    to = ptlang_context_unname_type(to, ctx->type_scope);
    from = ptlang_context_unname_type(from, ctx->type_scope);
    if ((to->type == PTLANG_AST_TYPE_FLOAT || to->type == PTLANG_AST_TYPE_INTEGER) &&
        (from->type == PTLANG_AST_TYPE_FLOAT || from->type == PTLANG_AST_TYPE_INTEGER))
    {
        return true;
    }
    if (to->type == PTLANG_AST_TYPE_REFERENCE && from->type == PTLANG_AST_TYPE_REFERENCE)
    {
        if (ptlang_ast_type_equals(to->content.reference.type, from->content.reference.type) &&
            !to->content.reference.writable)
            return true;
    }
    return ptlang_ast_type_equals(to, from);
}

static void ptlang_verify_check_cast(ptlang_ast_type from, ptlang_ast_type to, ptlang_ast_code_position pos,
                                     ptlang_context *ctx, ptlang_error **errors)
{
    if (!ptlang_verify_cast(from, to, ctx))
    {
        // Calculate the message length
        size_t to_len = ptlang_ast_type_to_string(to, NULL);
        size_t from_len = ptlang_ast_type_to_string(from, NULL);

        size_t message_len = to_len + from_len + sizeof(" cannot be casted to the expected type .");

        // Allocate memory for the message
        char *message = ptlang_malloc(message_len);
        char *message_ptr = message;

        // Generate the message
        message_ptr += ptlang_ast_type_to_string(to, message_ptr);
        memcpy(message_ptr, " cannot be casted to the expected type ",
               sizeof(" cannot be casted to the expected type ") - 1);
        message_ptr += sizeof(" cannot be casted to the expected type ") - 1;
        message_ptr += ptlang_ast_type_to_string(from, message_ptr);
        *message_ptr = '.';

        // Add the error to the errors array
        arrput(*errors, ((ptlang_error){
                            .type = PTLANG_ERROR_TYPE_MISMATCH,
                            .pos = pos,
                            .message = message,
                        }));
    }
}
