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
                                    .pos = *statement->content.block.stmts[i]->pos,
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
        ptlang_verify_exp(condition, ctx, errors);
        if (condition != NULL && condition->ast_type->type != PTLANG_AST_TYPE_INTEGER &&
            condition->ast_type->type != PTLANG_AST_TYPE_FLOAT)
        {
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_TYPE,
                                .pos = *condition->pos,
                                .message = "Control flow condition must be a float or an int.",
                            }));
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
        ptlang_verify_exp(statement->content.exp, ctx, errors);
        if (validate_return_type)
        {
            ptlang_ast_type return_type = statement->content.exp->ast_type;
            ptlang_verify_check_implicit_cast(return_type, wanted_return_type, statement->content.exp->pos,
                                              ctx, errors);
        }
        break;
    case PTLANG_AST_STMT_BREAK:
    case PTLANG_AST_STMT_CONTINUE:
        *has_return_value = false;

        if (statement->content.nesting_level == 0)
        {
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_NESTING_LEVEL_OUT_OF_RANGE,
                                .pos = *statement->pos,
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
                                .pos = *statement->pos,
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
                                .pos = *decl->pos,
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
    case PTLANG_AST_EXP_LENGTH:
    {
        return ptlang_verify_exp_is_mutable(exp->content.unary_operator, ctx);
    }
    case PTLANG_AST_EXP_DEREFERENCE:
    {
        return exp->content.unary_operator->ast_type == NULL ||
               exp->content.unary_operator->ast_type->content.reference.writable;
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
                NULL);
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
            *out = ptlang_ast_type_float(float_size, NULL);
        }
        return true;
    }
    return false;
}

// returns true if the expression is correct
static void ptlang_verify_exp(ptlang_ast_exp exp, ptlang_context *ctx, ptlang_error **errors)
{
    ptlang_ast_exp unary = exp->content.unary_operator;
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
                                .pos = *exp->pos,
                                .message = message,
                            }));
        }

        exp->ast_type = ptlang_ast_type_copy(left->ast_type);
        ptlang_verify_check_implicit_cast(right->ast_type, left->ast_type, exp->pos, ctx, errors);
        break;
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
                                .type = PTLANG_ERROR_TYPE,
                                .pos = *exp->pos,
                                .message = message,
                            }));
        }
        break;
    }
    case PTLANG_AST_EXP_NEGATION:
    {
        ptlang_verify_exp(unary, ctx, errors);

        if (unary->ast_type == NULL)
        {
            break;
        }

        if (unary->ast_type->type == PTLANG_AST_TYPE_INTEGER ||
            unary->ast_type->type == PTLANG_AST_TYPE_FLOAT)
        {
            exp->ast_type = ptlang_ast_type_copy(unary->ast_type);
        }
        else
        {
            size_t message_len = sizeof("Bad operant type for negation.");
            char *message = ptlang_malloc(message_len);
            memcpy(message, "Bad operant type for negation.", message_len);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_TYPE,
                                .pos = *exp->pos,
                                .message = message,
                            }));
        }
        break;
    }
    case PTLANG_AST_EXP_EQUAL:
    case PTLANG_AST_EXP_NOT_EQUAL:
    {
        ptlang_verify_exp(left, ctx, errors);
        ptlang_verify_exp(right, ctx, errors);

        if ((left->ast_type != NULL && left->ast_type->type != PTLANG_AST_TYPE_INTEGER &&
             left->ast_type->type != PTLANG_AST_TYPE_FLOAT &&
             left->ast_type->type != PTLANG_AST_TYPE_REFERENCE) ||
            (right->ast_type != NULL && right->ast_type->type != PTLANG_AST_TYPE_INTEGER &&
             right->ast_type->type != PTLANG_AST_TYPE_FLOAT &&
             right->ast_type->type != PTLANG_AST_TYPE_REFERENCE))
        {
            size_t message_len = sizeof("The operant types must be numbers or references.");
            char *message = ptlang_malloc(message_len);
            memcpy(message, "The operant types must be numbers or references.", message_len);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_TYPE,
                                .pos = *exp->pos,
                                .message = message,
                            }));
        }
        else if (left->ast_type != NULL && right->ast_type != NULL &&
                 (left->ast_type->type == PTLANG_AST_TYPE_REFERENCE) ==
                     (right->ast_type->type == PTLANG_AST_TYPE_REFERENCE))
        {
            size_t message_len = sizeof("The operant types must either be both numbers or both references.");
            char *message = ptlang_malloc(message_len);
            memcpy(message, "The operant types must either be both numbers or both references.", message_len);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_TYPE,
                                .pos = *exp->pos,
                                .message = message,
                            }));
        }

        exp->ast_type = ptlang_ast_type_integer(false, 1, NULL);
        break;
    }
    case PTLANG_AST_EXP_GREATER:
    case PTLANG_AST_EXP_GREATER_EQUAL:
    case PTLANG_AST_EXP_LESS:
    case PTLANG_AST_EXP_LESS_EQUAL:
    {
        ptlang_verify_exp(left, ctx, errors);
        ptlang_verify_exp(right, ctx, errors);

        if ((left->ast_type != NULL && left->ast_type->type != PTLANG_AST_TYPE_INTEGER &&
             left->ast_type->type != PTLANG_AST_TYPE_FLOAT) ||
            (right->ast_type != NULL && right->ast_type->type != PTLANG_AST_TYPE_INTEGER &&
             right->ast_type->type != PTLANG_AST_TYPE_FLOAT))
        {
            size_t message_len = sizeof("Bad operant type for comparison.");
            char *message = ptlang_malloc(message_len);
            memcpy(message, "Bad operant type for comparison.", message_len);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_TYPE,
                                .pos = *exp->pos,
                                .message = message,
                            }));
        }
        exp->ast_type = ptlang_ast_type_integer(false, 1, NULL);
        break;
    }
    case PTLANG_AST_EXP_LEFT_SHIFT:
    case PTLANG_AST_EXP_RIGHT_SHIFT:
    {
        ptlang_verify_exp(left, ctx, errors);
        ptlang_verify_exp(right, ctx, errors);

        if ((left->ast_type != NULL && left->ast_type->type != PTLANG_AST_TYPE_INTEGER) ||
            (right->ast_type != NULL && right->ast_type->type != PTLANG_AST_TYPE_INTEGER))
        {
            size_t message_len = sizeof("Bad operant type for shift.");
            char *message = ptlang_malloc(message_len);
            memcpy(message, "Bad operant type for shift.", message_len);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_TYPE,
                                .pos = *exp->pos,
                                .message = message,
                            }));
        }

        exp->ast_type = ptlang_ast_type_copy(left->ast_type);
        break;
    }
    case PTLANG_AST_EXP_AND:
    case PTLANG_AST_EXP_OR:
    {
        ptlang_verify_exp(left, ctx, errors);
        ptlang_verify_exp(right, ctx, errors);

        if ((left->ast_type != NULL && left->ast_type->type != PTLANG_AST_TYPE_INTEGER &&
             left->ast_type->type != PTLANG_AST_TYPE_FLOAT) ||
            (right->ast_type != NULL && right->ast_type->type != PTLANG_AST_TYPE_INTEGER &&
             right->ast_type->type != PTLANG_AST_TYPE_FLOAT))
        {
            size_t message_len = sizeof("Bad operant type for boolean operation.");
            char *message = ptlang_malloc(message_len);
            memcpy(message, "Bad operant type for boolean operation.", message_len);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_TYPE,
                                .pos = *exp->pos,
                                .message = message,
                            }));
        }

        exp->ast_type = ptlang_ast_type_integer(false, 1, NULL);
        break;
    }
    case PTLANG_AST_EXP_NOT:
    {
        ptlang_verify_exp(unary, ctx, errors);

        if (unary->ast_type != NULL && unary->ast_type->type != PTLANG_AST_TYPE_INTEGER &&
            unary->ast_type->type != PTLANG_AST_TYPE_FLOAT)
        {
            size_t message_len = sizeof("Bad operant type for boolean operation.");
            char *message = ptlang_malloc(message_len);
            memcpy(message, "Bad operant type for boolean operation.", message_len);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_TYPE,
                                .pos = *exp->pos,
                                .message = message,
                            }));
        }

        exp->ast_type = ptlang_ast_type_integer(false, 1, NULL);
        break;
    }
    case PTLANG_AST_EXP_BITWISE_AND:
    case PTLANG_AST_EXP_BITWISE_OR:
    case PTLANG_AST_EXP_BITWISE_XOR:
    {
        ptlang_verify_exp(left, ctx, errors);
        ptlang_verify_exp(right, ctx, errors);

        if ((left->ast_type != NULL && left->ast_type->type != PTLANG_AST_TYPE_INTEGER) ||
            (right->ast_type != NULL && right->ast_type->type != PTLANG_AST_TYPE_INTEGER))
        {
            size_t message_len = sizeof("Bad operant type for bitwise operation.");
            char *message = ptlang_malloc(message_len);
            memcpy(message, "Bad operant type for bitwise operation.", message_len);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_TYPE,
                                .pos = *exp->pos,
                                .message = message,
                            }));
        }

        ptlang_verify_child_exp(left, right, &exp->ast_type);

        break;
    }
    case PTLANG_AST_EXP_BITWISE_INVERSE:
    {
        ptlang_verify_exp(unary, ctx, errors);
        if (unary->ast_type != NULL && unary->ast_type->type != PTLANG_AST_TYPE_INTEGER)
        {
            size_t message_len = sizeof("Bad operant type for bitwise operation.");
            char *message = ptlang_malloc(message_len);
            memcpy(message, "Bad operant type for bitwise operation.", message_len);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_TYPE,
                                .pos = *exp->pos,
                                .message = message,
                            }));
        }

        exp->ast_type = ptlang_ast_type_copy(unary->ast_type);
        break;
    }
    case PTLANG_AST_EXP_LENGTH:
    {
        ptlang_verify_exp(unary, ctx, errors);
        exp->ast_type = ptlang_ast_type_integer(true, LLVMPointerSize(ctx->target_data_layout), NULL);
        break;
    }
    case PTLANG_AST_EXP_FUNCTION_CALL:
    {
        ptlang_verify_exp(exp->content.function_call.function, ctx, errors);
        for (size_t i = 0; i < arrlen(exp->content.function_call.parameters); i++)
        {
            ptlang_verify_exp(exp->content.function_call.parameters[i], ctx, errors);
        }
        if (exp->content.function_call.function->ast_type != NULL)
        {
            if (exp->content.function_call.function->ast_type->type != PTLANG_AST_TYPE_FUNCTION)
            {
                size_t message_len = sizeof("Function call is not a function.");
                char *message = ptlang_malloc(message_len);
                memcpy(message, "Function call is not a function.", message_len);
                arrput(*errors, ((ptlang_error){
                                    .type = PTLANG_ERROR_TYPE,
                                    .pos = *exp->pos,
                                    .message = message,
                                }));
            }
            else
            {
                exp->ast_type = ptlang_ast_type_copy(
                    exp->content.function_call.function->ast_type->content.function.return_type);
                if (arrlenu(exp->content.function_call.parameters) >
                    arrlenu(exp->content.function_call.function->ast_type->content.function.parameters))
                {
                    size_t message_len = sizeof("These parameters are to many.");
                    char *message = ptlang_malloc(message_len);
                    memcpy(message, "These parameters are to many.", message_len);
                    arrput(
                        *errors,
                        ((ptlang_error){
                            .type = PTLANG_ERROR_TYPE,
                            .pos.from_line =
                                exp->content.function_call
                                    .parameters[arrlenu(exp->content.function_call.function->ast_type->content
                                                            .function.parameters)]
                                    ->pos->from_line,
                            .pos.from_column =
                                exp->content.function_call
                                    .parameters[arrlenu(exp->content.function_call.function->ast_type->content
                                                            .function.parameters)]
                                    ->pos->from_column,
                            .pos.to_line = arrlast(exp->content.function_call.parameters)->pos->to_line,
                            .pos.to_column = arrlast(exp->content.function_call.parameters)->pos->to_column,
                            .message = message,
                        }));
                }
                else if (arrlenu(exp->content.function_call.parameters) <
                         arrlenu(exp->content.function_call.function->ast_type->content.function.parameters))
                {
                    size_t message_len = sizeof("These parameters are to many.");
                    char *message = ptlang_malloc(message_len);
                    memcpy(message, "These parameters are to many.", message_len);
                    arrput(*errors, ((ptlang_error){
                                        .type = PTLANG_ERROR_TYPE,
                                        .pos = *exp->pos,
                                        .message = message,
                                    }));
                }

                for (size_t i = 0; i < arrlen(exp->content.function_call.parameters); i++)
                {
                    if (i >=
                        arrlenu(exp->content.function_call.function->ast_type->content.function.parameters))
                    {
                        size_t message_len = sizeof("Too many .");
                        char *message = ptlang_malloc(message_len);
                        memcpy(message, "Too many .", message_len);
                        arrput(*errors, ((ptlang_error){
                                            .type = PTLANG_ERROR_TYPE,
                                            .pos = *exp->pos,
                                            .message = message,
                                        }));
                    }
                }
            }
        }
        break;
    }
    case PTLANG_AST_EXP_VARIABLE:
    {
        break;
    }
    case PTLANG_AST_EXP_INTEGER:
    {
        break;
    }
    case PTLANG_AST_EXP_FLOAT:
    {
        break;
    }
    case PTLANG_AST_EXP_STRUCT:
    {
        break;
    }
    case PTLANG_AST_EXP_ARRAY:
    {
        break;
    }
    case PTLANG_AST_EXP_TERNARY:
    {
        break;
    }
    case PTLANG_AST_EXP_CAST:
    {
        ptlang_ast_type to = exp->content.cast.type;
        exp->ast_type = to;
        ptlang_verify_exp(exp->content.cast.value, ctx, errors);
        ptlang_ast_type from = exp->content.cast.value->ast_type;
        if (! ptlang_verify_implicit_cast(from, to, ctx))
        {
            // todo
        }
        break;
    }
    case PTLANG_AST_EXP_STRUCT_MEMBER:
    {
        ptlang_verify_exp(exp->content.struct_member.struct_, ctx, errors);
        ptlang_ast_type struct_exp_type = exp->content.struct_member.struct_->ast_type;
        if (struct_exp_type != NULL)
        {
            if (struct_exp_type->type != PTLANG_AST_TYPE_NAMED)
            {
                // throw type error
            }
            else
            {
                ptlang_context_type_scope_entry struct_type_entry =
                    shget(ctx->type_scope, struct_exp_type->content.name);
                if (struct_type_entry.type != PTLANG_CONTEXT_TYPE_SCOPE_ENTRY_STRUCT)
                {
                    // throw type error
                }
                else
                {
                    ptlang_ast_decl member =
                        ptlang_decl_list_find_last(ctx->scope, exp->content.struct_member.member_name.name);
                    if (member == NULL)
                    {
                        // throw member not found error
                    }
                    else
                    {
                        exp->ast_type = ptlang_ast_type_copy(member->type);
                    }
                }
            }
        }
        break;
    }
    case PTLANG_AST_EXP_ARRAY_ELEMENT:
    {
        ptlang_verify_exp(exp->content.array_element.index, ctx, errors);
        ptlang_verify_exp(exp->content.array_element.array, ctx, errors);

        ptlang_ast_type index_type = exp->content.array_element.index->ast_type;
        if (index_type != NULL && index_type->type != PTLANG_AST_TYPE_INTEGER &&
            !index_type->content.integer.is_signed)
        {
            // throw type error
        }

        ptlang_ast_type array_type = exp->content.array_element.array->ast_type;
        if (array_type != NULL)
        {
            if (array_type->type == PTLANG_AST_TYPE_ARRAY)
            {
                exp->ast_type = ptlang_ast_type_copy(array_type->content.array.type);
            }
            else if (array_type->type == PTLANG_AST_TYPE_HEAP_ARRAY)
            {
                exp->ast_type = ptlang_ast_type_copy(array_type->content.heap_array.type);
            }
            else
            {
                // throw type error
            }
        };
    }

    break;

    case PTLANG_AST_EXP_REFERENCE:
    {
        ptlang_verify_exp(exp->content.reference.value, ctx, errors);
        if (exp->content.reference.writable &&
            !ptlang_verify_exp_is_mutable(exp->content.reference.value, ctx))
        {
            char *message =
                ptlang_malloc(sizeof("Can not create a writable reference of an unmutable expression."));
            memcpy(message, "Can not create a writable reference of an unmutable expression.",
                   sizeof("Can not create a writable reference of an unmutable expression."));
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_EXP_UNWRITABLE,
                                .pos = *exp->content.reference.value->pos,
                                .message = message,
                            }));
        }

        ptlang_ast_type referenced_type = exp->content.reference.value->ast_type;
        if (referenced_type != NULL)
        {
            exp->ast_type = ptlang_ast_type_reference(referenced_type, exp->content.reference.writable, NULL);
        }

        break;
    }
    case PTLANG_AST_EXP_DEREFERENCE:
    {
        ptlang_verify_exp(unary, ctx, errors);
        if (unary->ast_type != NULL)
        {
            size_t type_str_size = ptlang_ast_type_to_string(unary->ast_type, NULL);
            char *message = ptlang_malloc(sizeof("Dereferenced expression must be a reference, but is a ") -
                                          1 + type_str_size - 1 + sizeof("."));
            char *message_ptr = message;
            memcpy(message_ptr, "Dereferenced expression must be a reference, but is a ",
                   sizeof("Dereferenced expression must be a reference, but is a ") - 1);
            message_ptr += sizeof("Dereferenced expression must be a reference, but is a ") - 1;
            ptlang_ast_type_to_string(unary->ast_type, message_ptr);
            *message_ptr += type_str_size;
            memcpy(message_ptr, ".", 2);
            {
                arrput(*errors, ((ptlang_error){
                                    .type = PTLANG_ERROR_TYPE,
                                    .pos = *unary->pos,
                                    .message = message,
                                }));
                break;
            }
            exp->ast_type = ptlang_ast_type_copy(unary->ast_type->content.reference.type);
        }
        break;
    }
    }
}

static void ptlang_verify_struct_defs(ptlang_ast_struct_def *struct_defs, ptlang_context *ctx,
                                      ptlang_error **errors)
{
    ptlang_utils_graph_node *nodes = NULL;
    for (size_t i = 0; i < arrlenu(struct_defs); i++)
    {
        arrpush(nodes, (struct node_s){.index = -1});
    }
    for (size_t i = 0; i < arrlenu(struct_defs); i++)
    {
        char **referenced_types =
            pltang_verify_struct_get_referenced_types_from_struct_def(struct_defs[i], ctx, errors);
        for (size_t j = 0; j < arrlenu(referenced_types); j++)
        {
            arrpush(nodes[i].edges_to, &nodes[shget(ctx->type_scope, referenced_types[j]).index]);
        }
        arrfree(referenced_types);
    }

    ptlang_utils_graph_node **cycles = ptlang_utils_find_cycles(nodes);

    for (size_t i = 0, lowlink = 0; i < arrlenu(cycles);)
    {
        lowlink = cycles[i]->lowlink;
        char **message_components = NULL;
        size_t j;
        for (j = i; j < arrlenu(cycles) && lowlink == cycles[j]->lowlink; j++)
        {
            arrpush(message_components, struct_defs[cycles[j] - nodes]->name.name);
            arrpush(message_components, ", ");
        }
        arrpop(message_components);

        if (j == i + 1)
        {
            arrins(message_components, 0, "The struct ");
            arrpush(message_components, " is recursive.");
        }
        else
        {
            arrins(message_components, 0, "The structs ");
            arrpush(message_components, " are recursive.");
        }

        char *message = ptlang_utils_build_str_from_stb_arr(message_components);
        arrfree(message_components);

        for (; i < j; i++)
        {
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_STRUCT_RECURSION,
                                .pos = *struct_defs[cycles[i] - nodes]->pos,
                                .message = message,
                            }));
            if (i < j - 1)
                message = strdup(message);
        }
    }
    ptlang_utils_graph_free(nodes);
    arrfree(cycles);
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
                                .pos = *type->pos,
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

    // add type aliases to type_store

    ptlang_utils_graph_node *nodes = NULL;

    for (size_t i = 0; i < arrlenu(ast_module->type_aliases); i++)
    {
        shput(ctx->type_scope, ast_module->type_aliases[i].name.name,
              ((ptlang_context_type_scope_entry){.type = PTLANG_CONTEXT_TYPE_SCOPE_ENTRY_TYPEALIAS,
                                                 .value.ptlang_type = ast_module->type_aliases[i].type,
                                                 .index = i}));
        arrpush(nodes, (struct node_s){.index = -1});
    }

    // add structs to typescope

    for (size_t i = 0; i < arrlenu(ast_module->struct_defs); i++)
    {
        shput(ctx->type_scope, ast_module->struct_defs[i]->name.name,
              ((ptlang_context_type_scope_entry){.type = PTLANG_CONTEXT_TYPE_SCOPE_ENTRY_STRUCT,
                                                 .value.struct_def = ast_module->struct_defs[i],
                                                 .index = i}));
    }

    for (size_t i = 0; i < arrlenu(ast_module->type_aliases); i++)
    {
        bool is_error_type = false;
        size_t *referenced_types = pltang_verify_type_alias_get_referenced_types_from_ast_type(
            ast_module->type_aliases[i].type, ctx, errors, &is_error_type);

        if (is_error_type)
            shget(ctx->type_scope, ast_module->type_aliases[i].name.name).value.ptlang_type = NULL;

        for (size_t j = 0; j < arrlenu(referenced_types); j++)
        {
            arrpush(nodes[i].edges_to, &nodes[referenced_types[j]]);
        }

        arrfree(referenced_types);
    }

    ptlang_utils_graph_node **cycles = ptlang_utils_find_cycles(nodes);

    for (size_t i = 0; i < arrlenu(ast_module->type_aliases); i++)
    {
        if (nodes[i].in_cycle)
        {
            shget(ctx->type_scope, ast_module->type_aliases[i].name.name).value.ptlang_type = NULL;
        }
    }

    for (size_t i = 0, lowlink = 0; i < arrlenu(cycles);)
    {
        lowlink = cycles[i]->lowlink;
        char **message_components = NULL;
        size_t j;
        for (j = i; j < arrlenu(cycles) && lowlink == cycles[j]->lowlink; j++)
        {
            arrpush(message_components, ast_module->type_aliases[cycles[j] - nodes].name.name);
            arrpush(message_components, ", ");
        }
        arrpop(message_components);

        if (j == i + 1)
        {
            arrins(message_components, 0, "The type alias ");
            arrpush(message_components, " contains a self reference.");
        }
        else
        {
            arrins(message_components, 0, "The type aliases ");
            arrpush(message_components, " form a circular definition.");
        }

        char *message = ptlang_utils_build_str_from_stb_arr(message_components);
        arrfree(message_components);

        for (; i < j; i++)
        {
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_TYPE_UNRESOLVABLE,
                                .pos = *ast_module->type_aliases[cycles[i] - nodes].pos,
                                .message = message,
                            }));
            if (i < j - 1)
                message = strdup(message);
        }
    }

    ptlang_utils_graph_free(nodes);
    arrfree(cycles);

    for (size_t i = 0; i < arrlenu(ast_module->type_aliases); i++)
    {
        shget(ctx->type_scope, ast_module->type_aliases[i].name.name).value.ptlang_type =
            ptlang_context_unname_type(ast_module->type_aliases[i].type, ctx->type_scope);
    }
}

/**
 * @param has_error is set to true if an undefined type is referenced
 * @return ptrdiff_t* returns the indices of all referenced type aliases in the type_alias_table
 */
static size_t *pltang_verify_type_alias_get_referenced_types_from_ast_type(ptlang_ast_type ast_type,
                                                                           ptlang_context *ctx,
                                                                           ptlang_error **errors,
                                                                           bool *has_error)
{
    size_t *referenced_types = NULL;
    switch (ast_type->type)
    {
    case PTLANG_AST_TYPE_VOID:
        abort(); // syntax error
        break;
    case PTLANG_AST_TYPE_INTEGER:
    case PTLANG_AST_TYPE_FLOAT:
        break;
    case PTLANG_AST_TYPE_ARRAY:
        return pltang_verify_type_alias_get_referenced_types_from_ast_type(ast_type->content.array.type, ctx,
                                                                           errors, has_error);
    case PTLANG_AST_TYPE_HEAP_ARRAY:
        return pltang_verify_type_alias_get_referenced_types_from_ast_type(ast_type->content.heap_array.type,
                                                                           ctx, errors, has_error);
    case PTLANG_AST_TYPE_REFERENCE:
        return pltang_verify_type_alias_get_referenced_types_from_ast_type(ast_type->content.reference.type,
                                                                           ctx, errors, has_error);
    case PTLANG_AST_TYPE_NAMED:
    {
        ptrdiff_t index = shgeti(ctx->type_scope, ast_type->content.name);
        if (index != -1)
        {
            if (ctx->type_scope[index].value.type == PTLANG_CONTEXT_TYPE_SCOPE_ENTRY_TYPEALIAS)
                arrput(referenced_types, ctx->type_scope[index].value.index);
        }
        else
        {
            size_t message_len = sizeof("The type  is not defined.") + strlen(ast_type->content.name);
            char *message = ptlang_malloc(message_len);
            snprintf(message, message_len, "The type %s is not defined.", ast_type->content.name);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_TYPE_UNDEFINED,
                                .pos = *ast_type->pos,
                                .message = message,
                            }));
            *has_error = true;
        }
        break;
    }
    case PTLANG_AST_TYPE_FUNCTION:
        referenced_types = pltang_verify_type_alias_get_referenced_types_from_ast_type(
            ast_type->content.function.return_type, ctx, errors, has_error);
        for (size_t i = 0; i < arrlenu(ast_type->content.function.parameters); i++)
        {
            size_t *param_referenced_types = pltang_verify_type_alias_get_referenced_types_from_ast_type(
                ast_type->content.function.parameters[i], ctx, errors, has_error);
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
        while (type != NULL && type->type == PTLANG_AST_TYPE_ARRAY)
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

// static pltang_verify_struct pltang_verify_struct_create(ptlang_ast_struct_def ast_struct_def,
//                                                         ptlang_context *ctx, ptlang_error **errors)
// {

//     return (struct pltang_verify_struct_s){
//         .name = ast_struct_def->name.name,
//         .pos = ast_struct_def->pos,
//         .referenced_types =
//             pltang_verify_struct_get_referenced_types_from_struct_def(ast_struct_def, ctx, errors),
//         .referencing_types = NULL,
//         .resolved = false,
//     };
//     // return type_alias;
// }

static bool ptlang_verify_implicit_cast(ptlang_ast_type from, ptlang_ast_type to, ptlang_context *ctx)
{
    if (from == NULL || to == NULL)
    {
        return true;
    }
    to = ptlang_context_unname_type(to, ctx->type_scope);
    from = ptlang_context_unname_type(from, ctx->type_scope);
    if ((from->type == PTLANG_AST_TYPE_FLOAT && to->type == PTLANG_AST_TYPE_FLOAT) ||
        (from->type == PTLANG_AST_TYPE_INTEGER &&
         (to->type == PTLANG_AST_TYPE_FLOAT || to->type == PTLANG_AST_TYPE_INTEGER)))
    {
        if (to->type == PTLANG_AST_EXP_INTEGER)
            return (to->content.integer.size >= from->content.integer.size &&
                    to->content.integer.is_signed == from->content.integer.is_signed) ||
                   (to->content.integer.size > from->content.integer.size && to->content.integer.is_signed);
        return true;
    }
    if (to->type == PTLANG_AST_TYPE_REFERENCE && from->type == PTLANG_AST_TYPE_REFERENCE)
    {
        if (ptlang_context_type_equals(to->content.reference.type, from->content.reference.type,
                                       ctx->type_scope) &&
            !to->content.reference.writable)
            return true;
    }
    return ptlang_context_type_equals(to, from, ctx->type_scope);
}

static void ptlang_verify_check_implicit_cast(ptlang_ast_type from, ptlang_ast_type to,
                                              ptlang_ast_code_position pos, ptlang_context *ctx,
                                              ptlang_error **errors)
{
    if (!ptlang_verify_implicit_cast(from, to, ctx))
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
                            .type = PTLANG_ERROR_TYPE,
                            .pos = *pos,
                            .message = message,
                        }));
    }
}
