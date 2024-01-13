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
        // ptlang_verify_decl(module->declarations[i], 0, ctx, &errors);
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
            bool child_node_has_return_value = false;
            ptlang_verify_statement(statement->content.block.stmts[i], nesting_level, validate_return_type,
                                    wanted_return_type, new_scope_offset, &child_node_has_return_value,
                                    is_unreachable, ctx, errors);
            *has_return_value |= child_node_has_return_value;
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

        if (condition != NULL && (condition->ast_type->type != PTLANG_AST_TYPE_INTEGER ||
                                  condition->ast_type->content.integer.size != 1 ||
                                  condition->ast_type->content.integer.is_signed))
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
    ptlang_verify_decl_header(decl, scope_offset, ctx, errors);
    if (decl->init != NULL)
    {
        ptlang_verify_exp(decl->init, ctx, errors);
        ptlang_verify_check_implicit_cast(decl->init->ast_type, decl->type, decl->pos, ctx, errors);
    }
}

// Algo to init global vars:
// * Create nodes: one node for each global var and each (recursive) element / member of a global var
// 1 Parse references to create edges
// * Tarjan's algorithm (if cycle found, error)
// 2 Pick a Leaf
// * Eval the Leaf Initaliser
// * If Leaf is used as array index, substitute the Leaf as the index
// * Go to  2
// * If any array index was substituted, got to 1

// ----

// u64 2_HOCH_11 = ${ 2<<11 };
//
// void a(){ ... }
// ():void fptr = a;
// u64 b = 1;
// u64 bb = 2;
// &u64 c = ${irgendeinmacrowaszurcompilezeiteinevoonmehrerenmoeglichenvariablenreferenciert()};
// u64 d = *c;
static void ptlang_verify_decl_header(ptlang_ast_decl decl, size_t scope_offset, ptlang_context *ctx,
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
                                .type = PTLANG_ERROR_VARIABLE_NAME_DUPLICATION,
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

        exp->ast_type = ptlang_context_copy_unname_type(left->ast_type, ctx->type_scope);
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
            exp->ast_type = ptlang_context_copy_unname_type(unary->ast_type, ctx->type_scope);
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

        exp->ast_type = ptlang_context_copy_unname_type(left->ast_type, ctx->type_scope);
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

        exp->ast_type = ptlang_context_copy_unname_type(unary->ast_type, ctx->type_scope);
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
                                    .type = PTLANG_ERROR_PARAMETER_COUNT,
                                    .pos = *exp->pos,
                                    .message = message,
                                }));
            }
            else
            {
                exp->ast_type = ptlang_context_copy_unname_type(
                    exp->content.function_call.function->ast_type->content.function.return_type,
                    ctx->type_scope);
                if (arrlenu(exp->content.function_call.parameters) >
                    arrlenu(exp->content.function_call.function->ast_type->content.function.parameters))
                {
                    size_t message_len = sizeof("These parameters are too many.");
                    char *message = ptlang_malloc(message_len);
                    memcpy(message, "These parameters are too many.", message_len);
                    arrput(
                        *errors,
                        ((ptlang_error){
                            .type = PTLANG_ERROR_PARAMETER_COUNT,
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
                    size_t message_len = sizeof("To few parameters.");
                    char *message = ptlang_malloc(message_len);
                    memcpy(message, "To few parameters.", message_len);
                    arrput(*errors, ((ptlang_error){
                                        .type = PTLANG_ERROR_PARAMETER_COUNT,
                                        .pos = *exp->pos,
                                        .message = message,
                                    }));
                }

                size_t checkable_parameters =
                    arrlenu(exp->content.function_call.function->ast_type->content.function.parameters) <
                            arrlenu(exp->content.function_call.parameters)
                        ? arrlenu(exp->content.function_call.function->ast_type->content.function.parameters)
                        : arrlenu(exp->content.function_call.parameters);

                for (size_t i = 0; i < checkable_parameters; i++)
                {
                    ptlang_verify_check_implicit_cast(
                        exp->content.function_call.parameters[i]->ast_type,
                        exp->content.function_call.function->ast_type->content.function.parameters[i],
                        exp->content.function_call.parameters[i]->pos, ctx, errors);
                }
            }
        }
        break;
    }
    case PTLANG_AST_EXP_VARIABLE:
    {
        ptlang_ast_decl decl = ptlang_decl_list_find_last(ctx->scope, exp->content.str_prepresentation);
        if (decl == NULL)
        {
            size_t message_len = sizeof("Unknown variable.");
            char *message = ptlang_malloc(message_len);
            memcpy(message, "Unknown variable.", message_len);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_UNKNOWN_VARIABLE_NAME,
                                .pos = *exp->pos,
                                .message = message,
                            }));
        }
        else
        {
            exp->ast_type = ptlang_context_copy_unname_type(decl->type, ctx->type_scope);
        }
        break;
    }
    case PTLANG_AST_EXP_INTEGER:
    {
        errno = 0;
        char *suffix_begin;
        uint64_t number = strtoull(exp->content.str_prepresentation, &suffix_begin, 10);
        bool overflow = errno == ERANGE;

        uint32_t bits = 64;
        bool is_signed = true;

        if ((*suffix_begin) != '\0')
        {
            bits = strtoul(suffix_begin + 1, NULL, 10);
            if (bits > (1 << 23))
            {
                size_t message_len = sizeof("Integer type is too big.");
                char *message = ptlang_malloc(message_len);
                memcpy(message, "Integer type is too big.", message_len);
                arrput(*errors, ((ptlang_error){
                                    .type = PTLANG_ERROR_INTEGER_SIZE,
                                    .pos = *exp->pos,
                                    .message = message,
                                }));
                break;
            }
            is_signed = (*suffix_begin) == 's' || (*suffix_begin) == 'S';
        }

        exp->ast_type = ptlang_ast_type_integer(is_signed, bits, NULL);

        if (bits < 64)
        {
            uint8_t usable_bits = bits - (is_signed ? 1 : 0);
            uint64_t to_big = 1 << usable_bits;
            overflow = number >= to_big;
        }

        if (overflow)
        {
            size_t message_len = sizeof("Integer is too big.");
            char *message = ptlang_malloc(message_len);
            memcpy(message, "Integer is too big.", message_len);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_INTEGER_SIZE,
                                .pos = *exp->pos,
                                .message = message,
                            }));
        }
        break;
    }
    case PTLANG_AST_EXP_FLOAT:
    {
        char *suffix_begin = strchr(exp->content.str_prepresentation, 'f');
        if (suffix_begin == NULL)
            suffix_begin = strchr(exp->content.str_prepresentation, 'F');

        enum ptlang_ast_type_float_size float_size = PTLANG_AST_TYPE_FLOAT_64;

        if (suffix_begin != NULL)
        {
            if (strcmp(suffix_begin + 1, "16") == 0)
            {
                float_size = PTLANG_AST_TYPE_FLOAT_16;
            }
            else if (strcmp(suffix_begin + 1, "32") == 0)
            {
                float_size = PTLANG_AST_TYPE_FLOAT_32;
            }
            else if (strcmp(suffix_begin + 1, "128") == 0)
            {
                float_size = PTLANG_AST_TYPE_FLOAT_128;
            }
        }
        exp->ast_type = ptlang_ast_type_float(float_size, NULL);

        break;
    }
    case PTLANG_AST_EXP_STRUCT:
    {
        char *name = exp->content.struct_.type.name;

        ptlang_ast_struct_def struct_def = NULL;
        while (struct_def == NULL)
        {
            ptlang_context_type_scope *entry = shgetp_null(ctx->type_scope, name);
            if (entry == NULL)
            {
                size_t message_len = sizeof("Unknown struct type.");
                char *message = ptlang_malloc(message_len);
                memcpy(message, "Unknown struct type.", message_len);
                arrput(*errors, ((ptlang_error){
                                    .type = PTLANG_ERROR_TYPE,
                                    .pos = *exp->pos,
                                    .message = message,
                                }));
                break;
            }
            else if (entry->value.type == PTLANG_CONTEXT_TYPE_SCOPE_ENTRY_TYPEALIAS)
            {
                if (entry->value.value.ptlang_type->type != PTLANG_AST_TYPE_NAMED)
                {
                    size_t message_len = sizeof("Type is not a struct.");
                    char *message = ptlang_malloc(message_len);
                    memcpy(message, "Type is not a struct.", message_len);
                    arrput(*errors, ((ptlang_error){
                                        .type = PTLANG_ERROR_TYPE,
                                        .pos = *exp->pos,
                                        .message = message,
                                    }));
                    break;
                }
                else
                {
                    name = entry->value.value.ptlang_type->content.name;
                }
            }
            else if (entry->value.type == PTLANG_CONTEXT_TYPE_SCOPE_ENTRY_STRUCT)
            {
                struct_def = entry->value.value.struct_def;
            }
        }

        if (struct_def == NULL)
        {
            break;
        }

        exp->ast_type = ptlang_ast_type_named(name, NULL);

        // ptlang_ast_type struct_type = ptlang_context_unname_type(entry->value, ctx->type_scope);
        // ptlang_ast_type member_type = NULL;

        // if (array_type != NULL)
        // {
        //     if (array_type->type != PTLANG_AST_TYPE_ARRAY)
        //     {
        //         // throw error
        //     }
        //     else
        //     {
        //         exp->ast_type = ptlang_context_copy_unname_type(array_type, ctx->type_scope);
        //         member_type = ptlang_context_unname_type(array_type->content.array.type, ctx->type_scope);
        //     }
        // }

        for (size_t i = 0; i < arrlenu(exp->content.struct_.members); i++)
        {

            for (size_t j = 0; j < arrlenu(exp->content.struct_.members); j++)
            {
                if (i == j)
                {
                    continue;
                }
                if (strcmp(exp->content.struct_.members[i].str.name,
                           exp->content.struct_.members[j].str.name) == 0)
                {
                    size_t message_len = sizeof("Duplicate member name.");
                    char *message = ptlang_malloc(message_len);
                    memcpy(message, "Duplicate member name.", message_len);
                    arrput(*errors, ((ptlang_error){
                                        .type = PTLANG_ERROR_TYPE,
                                        .pos = *exp->content.struct_.members[i].pos,
                                        .message = message,
                                    }));
                    break;
                }
            }

            ptlang_ast_type member_type_in_struct_def = NULL;
            bool found = false;
            for (size_t j = 0; j < arrlenu(struct_def->members); j++)
            {
                if (strcmp(exp->content.struct_.members[i].str.name, struct_def->members[j]->name.name) == 0)
                {
                    member_type_in_struct_def = struct_def->members[j]->type;
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                size_t message_len = sizeof("Unknown member name.");
                char *message = ptlang_malloc(message_len);
                memcpy(message, "Unknown member name.", message_len);
                arrput(*errors, ((ptlang_error){
                                    .type = PTLANG_ERROR_TYPE,
                                    .pos = *exp->content.struct_.members[i].pos,
                                    .message = message,
                                }));
                continue;
            }

            ptlang_verify_exp(exp->content.struct_.members[i].exp, ctx, errors);
            ptlang_verify_check_implicit_cast(exp->content.struct_.members[i].exp->ast_type,
                                              member_type_in_struct_def, exp->content.struct_.members[i].pos,
                                              ctx, errors);
        }

        break;
    }
    case PTLANG_AST_EXP_ARRAY:
    {
        ptlang_ast_type array_type = ptlang_context_unname_type(exp->content.array.type, ctx->type_scope);
        ptlang_ast_type member_type = NULL;

        if (array_type != NULL)
        {
            if (array_type->type != PTLANG_AST_TYPE_ARRAY)
            {
                // TODO : throw error
            }
            else
            {
                exp->ast_type = ptlang_context_copy_unname_type(array_type, ctx->type_scope);
                member_type = ptlang_context_unname_type(array_type->content.array.type, ctx->type_scope);
            }

            ptlang_error *too_many_values_error = NULL;
            for (size_t i = 0; i < arrlenu(exp->content.array.values); i++)
            {
                ptlang_verify_exp(exp->content.array.values[i], ctx, errors);
                ptlang_verify_check_implicit_cast(exp->content.array.values[i]->ast_type, member_type,
                                                  exp->content.array.values[i]->pos, ctx, errors);
                if (i >= array_type->content.array.len)
                {
                    if (too_many_values_error == NULL)
                    {
                        char *message = NULL;
                        ptlang_utils_build_str(
                            message, CONST_STR("An array of type "),
                            ptlang_verify_type_to_string(array_type, ctx->type_scope),
                            CONST_STR(" may not have more than "),
                            ptlang_utils_sprintf_alloc("%zu", array_type->content.array.len),
                            CONST_STR(" values."));
                        arrput(*errors, ((ptlang_error){
                                            .type = PTLANG_ERROR_VALUE_COUNT,
                                            .pos = *exp->content.array.values[i]->pos,
                                            .message = message,
                                        }));
                        too_many_values_error = &((*errors)[arrlenu(*errors) - 1]);
                    }
                    else
                    {
                        too_many_values_error->pos.to_column = exp->content.array.values[i]->pos->to_column;
                        too_many_values_error->pos.to_line = exp->content.array.values[i]->pos->to_line;
                    }
                }
            }
        }
        break;
    }
    case PTLANG_AST_EXP_TERNARY:
    {
        ptlang_ast_exp condition = exp->content.ternary_operator.condition;
        ptlang_ast_exp if_value = exp->content.ternary_operator.if_value;
        ptlang_ast_exp else_value = exp->content.ternary_operator.else_value;

        ptlang_verify_exp(condition, ctx, errors);
        ptlang_verify_exp(if_value, ctx, errors);
        ptlang_verify_exp(else_value, ctx, errors);

        ptlang_ast_type condition_type = ptlang_context_unname_type(condition->ast_type, ctx->type_scope);
        if (condition_type != NULL &&
            (condition_type->type != PTLANG_AST_TYPE_INTEGER || condition_type->content.integer.size != 1 ||
             condition_type->content.integer.is_signed))
        {
            arrpush(*errors,
                    ptlang_verify_generate_type_error("Ternary condition must be a u1, but is of type",
                                                      condition, ".", ctx->type_scope));
        }

        exp->ast_type = ptlang_verify_unify_types(if_value->ast_type, else_value->ast_type, ctx);

        break;
    }
    case PTLANG_AST_EXP_CAST:
    {
        ptlang_ast_type to = ptlang_context_copy_unname_type(exp->content.cast.type, ctx->type_scope);
        exp->ast_type = to;
        ptlang_verify_exp(exp->content.cast.value, ctx, errors);
        ptlang_ast_type from = ptlang_context_unname_type(exp->content.cast.value->ast_type, ctx->type_scope);
        if (!ptlang_verify_implicit_cast(from, to, ctx))
        {
            if (!((to->type == PTLANG_AST_TYPE_INTEGER || to->type == PTLANG_AST_TYPE_FLOAT) &&
                  (from->type == PTLANG_AST_TYPE_INTEGER || from->type == PTLANG_AST_TYPE_FLOAT)))
            {
                char *message = NULL;
                ptlang_utils_build_str(message, CONST_STR("A value of type"),
                                       ptlang_verify_type_to_string(from, ctx->type_scope),
                                       CONST_STR(" can not be casted to a "),
                                       ptlang_verify_type_to_string(to, ctx->type_scope), CONST_STR("."));

                arrput(*errors, ((ptlang_error){
                                    .type = PTLANG_ERROR_CAST_ILLEGAL,
                                    .pos = *exp->pos,
                                    .message = message,
                                }));
            }
        }
        break;
    }
    case PTLANG_AST_EXP_STRUCT_MEMBER:
    {
        ptlang_verify_exp(exp->content.struct_member.struct_, ctx, errors);
        ptlang_ast_type struct_exp_type =
            ptlang_context_unname_type(exp->content.struct_member.struct_->ast_type, ctx->type_scope);
        if (struct_exp_type != NULL)
        {
            if (struct_exp_type->type != PTLANG_AST_TYPE_NAMED ||
                shget(ctx->type_scope, struct_exp_type->content.name).type !=
                    PTLANG_CONTEXT_TYPE_SCOPE_ENTRY_STRUCT)
            {
                arrput(*errors, ptlang_verify_generate_type_error(
                                    "Only structs can have members. You can not access a member of a ",
                                    exp->content.struct_member.struct_, ".", ctx->type_scope));
            }
            else
            {

                ptlang_ast_decl member =
                    ptlang_decl_list_find_last(ctx->scope, exp->content.struct_member.member_name.name);
                if (member == NULL)
                {
                    char *message = NULL;
                    ptlang_utils_build_str(message, CONST_STR("The struct"),
                                           ptlang_verify_type_to_string(
                                               exp->content.struct_member.struct_->ast_type, ctx->type_scope),
                                           CONST_STR(" doesn't have the member "),
                                           CONST_STR(exp->content.struct_member.member_name.name),
                                           CONST_STR("."));
                    arrput(*errors, ((ptlang_error){
                                        .type = PTLANG_ERROR_UNKNOWN_MEMBER,
                                        .pos = *exp->pos,
                                        .message = message,
                                    }));
                }
                else
                {
                    exp->ast_type = ptlang_context_copy_unname_type(member->type, ctx->type_scope);
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
        if (index_type != NULL &&
            (index_type->type != PTLANG_AST_TYPE_INTEGER || index_type->content.integer.is_signed))
        {
            arrput(*errors, ptlang_verify_generate_type_error(
                                "Array index must have be a unsigned integer, but is of type ",
                                exp->content.array_element.index, ".", ctx->type_scope));
        }

        ptlang_ast_type array_type = exp->content.array_element.array->ast_type;
        if (array_type != NULL)
        {
            if (array_type->type == PTLANG_AST_TYPE_ARRAY)
            {
                exp->ast_type =
                    ptlang_context_copy_unname_type(array_type->content.array.type, ctx->type_scope);
            }
            else if (array_type->type == PTLANG_AST_TYPE_HEAP_ARRAY)
            {
                exp->ast_type =
                    ptlang_context_copy_unname_type(array_type->content.heap_array.type, ctx->type_scope);
            }
            else
            {
                arrput(*errors,
                       ptlang_verify_generate_type_error(
                           "Subscripted expression must have array or heap array type, but is of type ",
                           exp->content.array_element.array, ".", ctx->type_scope));
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
            size_t type_str_size = ptlang_context_type_to_string(unary->ast_type, NULL, ctx->type_scope);
            char *message = ptlang_malloc(sizeof("Dereferenced expression must be a reference, but is a ") -
                                          1 + type_str_size - 1 + sizeof("."));
            char *message_ptr = message;
            memcpy(message_ptr, "Dereferenced expression must be a reference, but is a ",
                   sizeof("Dereferenced expression must be a reference, but is a ") - 1);
            message_ptr += sizeof("Dereferenced expression must be a reference, but is a ") - 1;
            ptlang_context_type_to_string(unary->ast_type, message_ptr, ctx->type_scope);
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
            exp->ast_type =
                ptlang_context_copy_unname_type(unary->ast_type->content.reference.type, ctx->type_scope);
        }
        break;
    }
    case PTLANG_AST_EXP_BINARY:
    {
        abort();
    }
    }
}

static void ptlang_verify_struct_defs(ptlang_ast_struct_def *struct_defs, ptlang_context *ctx,
                                      ptlang_error **errors)
{
    for (size_t i = 0; i < arrlenu(struct_defs); i++)
    {
        for (size_t j = i; j < arrlenu(struct_defs); i++)
        {
            if (strcmp(struct_defs[i]->name.name, struct_defs[j]->name.name) == 0)
            {
                size_t message_size =
                    sizeof("Duplicate struct definition of ") + strlen(struct_defs[i]->name.name);

                char *message = ptlang_malloc(message_size);
                memcpy(message, "Duplicate struct definition of ",
                       sizeof("Duplicate struct definition of ") - 1);
                arrput(*errors, ((ptlang_error){
                                    .type = PTLANG_ERROR_STRUCT_MEMBER_DUPLICATION,
                                    .pos = *(struct_defs[i]->name.pos),
                                    .message = message,
                                }));
            }
        }
    }

    ptlang_utils_graph_node *nodes = NULL;
    for (size_t i = 0; i < arrlenu(struct_defs); i++)
    {
        arrpush(nodes, (struct node_s){.index = -1});
    }
    for (size_t i = 0; i < arrlenu(struct_defs); i++)
    {
        char **referenced_types =
            ptlang_verify_struct_get_referenced_types_from_struct_def(struct_defs[i], ctx, errors);
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
        size_t message_size = strlen(message) + 1;
        arrfree(message_components);

        for (; i < j; i++)
        {
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_STRUCT_RECURSION,
                                .pos = *struct_defs[cycles[i] - nodes]->pos,
                                .message = message,
                            }));
            if (i < j - 1)
            {
                char *new_message = ptlang_malloc(message_size);
                memcpy(new_message, message, message_size);
                message = new_message;
            }
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
        size_t *referenced_types = ptlang_verify_type_alias_get_referenced_types_from_ast_type(
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
        size_t message_size = strlen(message) + 1;
        arrfree(message_components);

        for (; i < j; i++)
        {
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_TYPE_UNRESOLVABLE,
                                .pos = *ast_module->type_aliases[cycles[i] - nodes].pos,
                                .message = message,
                            }));
            if (i < j - 1)
            {
                char *new_message = ptlang_malloc(message_size);
                memcpy(new_message, message, message_size);
                message = new_message;
            }
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
static size_t *ptlang_verify_type_alias_get_referenced_types_from_ast_type(ptlang_ast_type ast_type,
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
        return ptlang_verify_type_alias_get_referenced_types_from_ast_type(ast_type->content.array.type, ctx,
                                                                           errors, has_error);
    case PTLANG_AST_TYPE_HEAP_ARRAY:
        return ptlang_verify_type_alias_get_referenced_types_from_ast_type(ast_type->content.heap_array.type,
                                                                           ctx, errors, has_error);
    case PTLANG_AST_TYPE_REFERENCE:
        return ptlang_verify_type_alias_get_referenced_types_from_ast_type(ast_type->content.reference.type,
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
        referenced_types = ptlang_verify_type_alias_get_referenced_types_from_ast_type(
            ast_type->content.function.return_type, ctx, errors, has_error);
        for (size_t i = 0; i < arrlenu(ast_type->content.function.parameters); i++)
        {
            size_t *param_referenced_types = ptlang_verify_type_alias_get_referenced_types_from_ast_type(
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

static char **ptlang_verify_struct_get_referenced_types_from_struct_def(ptlang_ast_struct_def struct_def,
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

        if (type != NULL && type->type == PTLANG_AST_TYPE_NAMED)
        {
            // => must be struct
            arrput(referenced_types, type->content.name);
        }
    }
    return referenced_types;
}

// static ptlang_verify_struct ptlang_verify_struct_create(ptlang_ast_struct_def ast_struct_def,
//                                                         ptlang_context *ctx, ptlang_error **errors)
// {

//     return (struct ptlang_verify_struct_s){
//         .name = ast_struct_def->name.name,
//         .pos = ast_struct_def->pos,
//         .referenced_types =
//             ptlang_verify_struct_get_referenced_types_from_struct_def(ast_struct_def, ctx, errors),
//         .referencing_types = NULL,
//         .resolved = false,
//     };
//     // return type_alias;
// }

static ptlang_ast_type ptlang_verify_unify_types(ptlang_ast_type type1, ptlang_ast_type type2,
                                                 ptlang_context *ctx)
{
    type1 = ptlang_context_unname_type(type1, ctx->type_scope);
    type2 = ptlang_context_unname_type(type2, ctx->type_scope);

    if (type1 == NULL)
        return ptlang_ast_type_copy(type2);
    if (type2 == NULL)
        return ptlang_ast_type_copy(type1);

    if (ptlang_context_type_equals(type1, type2, ctx->type_scope))
        return ptlang_ast_type_copy(type1);

    if (type1->type == PTLANG_AST_TYPE_REFERENCE && type2->type == PTLANG_AST_TYPE_REFERENCE)
    {
        if (ptlang_context_type_equals(type1->content.reference.type, type2->content.reference.type,
                                       ctx->type_scope))
        {
            return ptlang_ast_type_reference(
                type1->content.reference.type,
                type1->content.reference.writable && type2->content.reference.writable, NULL);
        }
    }
    else if (type1->type == PTLANG_AST_TYPE_FLOAT && type2->type == PTLANG_AST_TYPE_FLOAT)
    {
        return ptlang_ast_type_float(type1->content.float_size >= type2->content.float_size
                                         ? type1->content.float_size
                                         : type2->content.float_size,
                                     NULL);
    }
    else if (type1->type == PTLANG_AST_TYPE_FLOAT && type2->type == PTLANG_AST_TYPE_INTEGER)
    {
        return ptlang_ast_type_copy(type1);
    }
    else if (type1->type == PTLANG_AST_TYPE_INTEGER && type2->type == PTLANG_AST_TYPE_FLOAT)
    {
        return ptlang_ast_type_copy(type2);
    }
    else if (type1->type == PTLANG_AST_TYPE_INTEGER && type2->type == PTLANG_AST_TYPE_INTEGER)
    {
        return ptlang_ast_type_integer(type1->content.integer.is_signed || type2->content.integer.is_signed,
                                       type1->content.integer.size > type2->content.integer.size
                                           ? type1->content.integer.size
                                           : type2->content.integer.size,
                                       NULL);
    }
    return ptlang_ast_type_void(NULL);
}

static bool ptlang_verify_implicit_cast(ptlang_ast_type from, ptlang_ast_type to, ptlang_context *ctx)
{
    to = ptlang_context_unname_type(to, ctx->type_scope);
    from = ptlang_context_unname_type(from, ctx->type_scope);
    if (from == NULL || to == NULL)
    {
        return true;
    }
    if ((from->type == PTLANG_AST_TYPE_FLOAT && to->type == PTLANG_AST_TYPE_FLOAT) ||
        (from->type == PTLANG_AST_TYPE_INTEGER &&
         (to->type == PTLANG_AST_TYPE_FLOAT || to->type == PTLANG_AST_TYPE_INTEGER)))
    {
        if (to->type == PTLANG_AST_EXP_INTEGER)
            return (to->content.integer.size >= from->content.integer.size);
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
        size_t to_len = ptlang_context_type_to_string(to, NULL, ctx->type_scope);
        size_t from_len = ptlang_context_type_to_string(from, NULL, ctx->type_scope);

        size_t message_len = to_len + from_len + sizeof(" cannot be casted to the expected type .");

        assert(message_len > 0);
        // Allocate memory for the message
        char *message = ptlang_malloc(message_len);
        char *message_ptr = message;

        // Generate the message
        message_ptr += ptlang_context_type_to_string(to, message_ptr, ctx->type_scope);
        memcpy(message_ptr, " cannot be casted to the expected type ",
               sizeof(" cannot be casted to the expected type ") - 1);
        message_ptr += sizeof(" cannot be casted to the expected type ") - 1;
        message_ptr += ptlang_context_type_to_string(from, message_ptr, ctx->type_scope);
        *message_ptr = '.';

        // Add the error to the errors array
        arrput(*errors, ((ptlang_error){
                            .type = PTLANG_ERROR_TYPE,
                            .pos = *pos,
                            .message = message,
                        }));
    }
}

static ptlang_utils_str ptlang_verify_type_to_string(ptlang_ast_type type,
                                                     ptlang_context_type_scope *type_scope)
{
    size_t str_size = ptlang_context_type_to_string(type, NULL, type_scope);
    assert(str_size > 0);
    char *type_str = ptlang_malloc(str_size);
    ptlang_context_type_to_string(type, type_str, type_scope);
    return ALLOCATED_STR(type_str);
}

static ptlang_error ptlang_verify_generate_type_error(char *before, ptlang_ast_exp exp, char *after,
                                                      ptlang_context_type_scope *type_scope)
{
    char *message = NULL;
    ptlang_utils_build_str(message, CONST_STR(before),
                           ptlang_verify_type_to_string(exp->ast_type, type_scope), CONST_STR(after));

    ptlang_error error = (ptlang_error){.type = PTLANG_ERROR_TYPE, .pos = *exp->pos, .message = message};
    return error;
}

static void ptlang_verify_exp_check_const(ptlang_ast_exp exp, ptlang_context *ctx, ptlang_error **errors)
{
    switch (exp->type)
    {
    case PTLANG_AST_EXP_ASSIGNMENT:
    {
        arrput(*errors, ((ptlang_error){
                            .type = PTLANG_ERROR_NON_CONST_GLOBAL_INITIATOR,
                            .pos = *exp->pos,
                            .message = "Assignments may not be used in global initiators.",
                        }));
        break;
    }

    case PTLANG_AST_EXP_ADDITION:
    case PTLANG_AST_EXP_SUBTRACTION:
    case PTLANG_AST_EXP_MULTIPLICATION:
    case PTLANG_AST_EXP_DIVISION:
    case PTLANG_AST_EXP_MODULO:
    case PTLANG_AST_EXP_REMAINDER:
    case PTLANG_AST_EXP_EQUAL:
    case PTLANG_AST_EXP_NOT_EQUAL:
    case PTLANG_AST_EXP_GREATER:
    case PTLANG_AST_EXP_GREATER_EQUAL:
    case PTLANG_AST_EXP_LESS:
    case PTLANG_AST_EXP_LESS_EQUAL:
    case PTLANG_AST_EXP_LEFT_SHIFT:
    case PTLANG_AST_EXP_RIGHT_SHIFT:
    case PTLANG_AST_EXP_AND:
    case PTLANG_AST_EXP_OR:
    case PTLANG_AST_EXP_BITWISE_AND:
    case PTLANG_AST_EXP_BITWISE_OR:
    case PTLANG_AST_EXP_BITWISE_XOR:
    {
        ptlang_verify_exp_check_const(exp->content.binary_operator.left_value, ctx, errors);
        ptlang_verify_exp_check_const(exp->content.binary_operator.right_value, ctx, errors);
        break;
    }
    case PTLANG_AST_EXP_NEGATION:
    case PTLANG_AST_EXP_NOT:
    case PTLANG_AST_EXP_BITWISE_INVERSE:
    case PTLANG_AST_EXP_LENGTH:
    case PTLANG_AST_EXP_DEREFERENCE:
    {
        ptlang_verify_exp_check_const(exp->content.unary_operator, ctx, errors);
        break;
    }
    case PTLANG_AST_EXP_FUNCTION_CALL:
    {
        arrput(*errors, ((ptlang_error){
                            .type = PTLANG_ERROR_NON_CONST_GLOBAL_INITIATOR,
                            .pos = *exp->pos,
                            .message = "Function calls may not be used in global initiators.",
                        }));
        break;
    }
    case PTLANG_AST_EXP_VARIABLE:
    {
        break;
    }
    case PTLANG_AST_EXP_INTEGER:
    case PTLANG_AST_EXP_FLOAT:
    {
        break;
    }
    case PTLANG_AST_EXP_STRUCT:
    {
        for (size_t i = 0; i < arrlenu(exp->content.struct_.members); i++)
        {
            ptlang_verify_exp_check_const(exp->content.struct_.members[i].exp, ctx, errors);
        }
        break;
    }
    case PTLANG_AST_EXP_ARRAY:
    {
        for (size_t i = 0; i < arrlenu(exp->content.array.values); i++)
        {
            ptlang_verify_exp_check_const(exp->content.array.values[i], ctx, errors);
        }
        break;
    }
    case PTLANG_AST_EXP_TERNARY:
    {

        ptlang_verify_exp_check_const(exp->content.ternary_operator.condition, ctx, errors);
        ptlang_verify_exp_check_const(exp->content.ternary_operator.if_value, ctx, errors);
        ptlang_verify_exp_check_const(exp->content.ternary_operator.else_value, ctx, errors);
        break;
    }
    case PTLANG_AST_EXP_CAST:
    {
        ptlang_verify_exp_check_const(exp->content.cast.value, ctx, errors);
        break;
    }
    case PTLANG_AST_EXP_STRUCT_MEMBER:
    {
        ptlang_verify_exp_check_const(exp->content.struct_member.struct_, ctx, errors);
        break;
    }
    case PTLANG_AST_EXP_ARRAY_ELEMENT:
    {
        ptlang_verify_exp_check_const(exp->content.array_element.array, ctx, errors);
        ptlang_verify_exp_check_const(exp->content.array_element.index, ctx, errors);
        break;
    }
    case PTLANG_AST_EXP_REFERENCE:
    {
        ptlang_verify_exp_check_const(exp->content.reference.value, ctx, errors);
        break;
    }
    }
}

static ptlang_ast_exp ptlang_verify_eval(ptlang_ast_exp exp, bool eval_fully, ptlang_context *ctx)
{
    ptlang_ast_exp substituted = ptlang_malloc(sizeof(struct ptlang_ast_exp_s));
    ptlang_ast_exp evaluated = NULL;
    switch (exp->type)
    {
    case PTLANG_AST_EXP_ADDITION:
    case PTLANG_AST_EXP_SUBTRACTION:
    case PTLANG_AST_EXP_MULTIPLICATION:
    case PTLANG_AST_EXP_DIVISION:
    case PTLANG_AST_EXP_MODULO:
    case PTLANG_AST_EXP_REMAINDER:
    case PTLANG_AST_EXP_EQUAL:
    case PTLANG_AST_EXP_NOT_EQUAL:
    case PTLANG_AST_EXP_GREATER:
    case PTLANG_AST_EXP_GREATER_EQUAL:
    case PTLANG_AST_EXP_LESS:
    case PTLANG_AST_EXP_LESS_EQUAL:
    case PTLANG_AST_EXP_LEFT_SHIFT:
    case PTLANG_AST_EXP_RIGHT_SHIFT:
    case PTLANG_AST_EXP_AND:
    case PTLANG_AST_EXP_OR:
    case PTLANG_AST_EXP_BITWISE_AND:
    case PTLANG_AST_EXP_BITWISE_OR:
    case PTLANG_AST_EXP_BITWISE_XOR:
    {
        ptlang_ast_exp left_value = ptlang_verify_eval(exp->content.binary_operator.left_value, true, ctx);
        ptlang_ast_exp right_value = ptlang_verify_eval(exp->content.binary_operator.right_value, true, ctx);
        *substituted = (struct ptlang_ast_exp_s){
            .type = exp->type,
            .content.binary_operator =
                {
                    .left_value = left_value,
                    .right_value = right_value,
                },
            .pos = ptlang_ast_code_position_copy(exp->pos),
            .ast_type = ptlang_ast_type_copy(exp->ast_type),
        };
        break;
    }
    case PTLANG_AST_EXP_LENGTH:
    {
        unsigned pointer_size_in_bytes = LLVMPointerSize(ctx->target_data_layout);
        uint8_t *binary = ptlang_malloc_zero(pointer_size_in_bytes);

        if (exp->content.unary_operator->ast_type->type == PTLANG_AST_TYPE_ARRAY)
        {
            uint8_t bytes =
                pointer_size_in_bytes < sizeof(exp->content.unary_operator->ast_type->content.array.len)
                    ? pointer_size_in_bytes
                    : sizeof(exp->content.unary_operator->ast_type->content.array.len);
            for (uint8_t i = 0; i < bytes; i++)
            {
                binary[i] = exp->content.unary_operator->ast_type->content.array.len >> (i << 3);
            }
        }

        evaluated = ptlang_ast_exp_binary_new(binary, exp);
    }
    case PTLANG_AST_EXP_NEGATION:
    case PTLANG_AST_EXP_NOT:
    case PTLANG_AST_EXP_BITWISE_INVERSE:
    {
        ptlang_ast_exp operand = ptlang_verify_eval(exp->content.unary_operator, true, ctx);
        *substituted = (struct ptlang_ast_exp_s){
            .type = exp->type,
            .content.unary_operator = operand,
            .pos = exp->pos,
            .ast_type = exp->ast_type,
        };
        break;
    }
    case PTLANG_AST_EXP_DEREFERENCE:
    {
        ptlang_ast_exp reference = ptlang_verify_eval(exp->content.unary_operator, eval_fully, ctx);
        // TODO recheck cycles
        evaluated = ptlang_verify_eval(reference->content.reference.value, eval_fully, ctx);
        break;
    }
    case PTLANG_AST_EXP_VARIABLE:
    {
        // assumes that global variable was already initialized
        evaluated = ptlang_verify_eval(
            ptlang_decl_list_find_last(ctx->scope, exp->content.str_prepresentation)->init, eval_fully, ctx);
        break;
    }
    case PTLANG_AST_EXP_INTEGER:
    case PTLANG_AST_EXP_FLOAT:
    {
        evaluated = ptlang_ast_exp_copy(exp);
        break;
    }
    case PTLANG_AST_EXP_STRUCT:
    {
        ptlang_ast_struct_def struct_def =
            ptlang_context_get_struct_def(exp->content.struct_.type.name, ctx->type_scope);
        ptlang_ast_struct_member_list struct_members = NULL;
        for (size_t i = 0; i < arrlenu(struct_def->members); i++)
        {
            size_t j;
            for (j = 0; j < arrlenu(exp->content.struct_.members); j++)
            {
                if (0 == strcmp(exp->content.struct_.members[j].str.name, struct_def->members[i]->name.name))
                    break;
            }
            struct ptlang_ast_struct_member_s member = (struct ptlang_ast_struct_member_s){
                .exp = j < arrlenu(exp->content.struct_.members)
                           ? (eval_fully ? ptlang_verify_eval(exp->content.struct_.members[j].exp, true, ctx)
                                         : ptlang_ast_exp_copy(exp->content.struct_.members[j].exp))
                           : ptlang_verify_get_default_value(struct_def->members[i]->type, ctx),
                .str = ptlang_ast_ident_copy(struct_def->members[i]->name),
                .pos = NULL,
            };

            arrpush(struct_members, member);
        }
        evaluated = ptlang_ast_exp_struct_new(ptlang_ast_ident_copy(exp->content.struct_.type),
                                              struct_members, ptlang_ast_code_position_copy(exp->pos));

        break;
    }
    case PTLANG_AST_EXP_ARRAY:
    {
        ptlang_ast_exp *array_values = NULL;
        for (size_t i = 0; i < exp->content.array.type->content.array.len; i++)
        {
            if (i < arrlenu(exp->content.array.values))
                arrpush(array_values, eval_fully ? ptlang_verify_eval(exp->content.array.values[i], true, ctx)
                                                 : ptlang_ast_exp_copy(exp->content.array.values[i]));
            else
                arrpush(array_values,
                        ptlang_verify_get_default_value(exp->content.array.type->content.array.type, ctx));
        }
        evaluated = ptlang_ast_exp_array_new(
            ptlang_context_copy_unname_type(exp->content.array.type, ctx->type_scope), array_values,
            ptlang_ast_code_position_copy(exp->pos));
        break;
    }
    case PTLANG_AST_EXP_TERNARY:
    {
        ptlang_ast_exp condition = ptlang_verify_eval(exp->content.ternary_operator.condition, true, ctx);
        // TODO recheck cycles
        if (condition->content.binary[0] == 0)
        {
            ptlang_ast_exp else_value =
                ptlang_verify_eval(exp->content.ternary_operator.else_value, eval_fully, ctx);
            evaluated = else_value;
        }
        else
        {
            ptlang_ast_exp if_value =
                ptlang_verify_eval(exp->content.ternary_operator.if_value, eval_fully, ctx);
            evaluated = if_value;
        }

        break;
    }
    case PTLANG_AST_EXP_CAST:
    {
        ptlang_ast_exp value = ptlang_verify_eval(exp->content.cast.value, eval_fully, ctx);
        *substituted = (struct ptlang_ast_exp_s){
            .type = exp->type,
            .content.cast.type = exp->content.cast.type,
            .content.cast.value = value,
            .pos = exp->pos,
            .ast_type = exp->ast_type,
        };
        break;
    }
    case PTLANG_AST_EXP_STRUCT_MEMBER:
    {
        ptlang_ast_exp struct_ = ptlang_verify_eval(exp->content.struct_member.struct_, false, ctx);
        // cycles should already have been checked
        for (size_t i = 0; i < arrlenu(struct_->content.struct_.members); i++)
        {
            if (0 == strcmp(struct_->content.struct_.members[i].str.name,
                            exp->content.struct_member.member_name.name))
            {
                evaluated = ptlang_verify_eval(struct_->content.struct_.members[i].exp, eval_fully, ctx);
            }
        }
        break;
    }
    // TODO other types
    case PTLANG_AST_EXP_ARRAY_ELEMENT:
    {
        ptlang_ast_exp index = ptlang_verify_eval(exp->content.array_element.index, true, ctx);
        // TODO: recheck cycles

        enum LLVMByteOrdering byteOrdering = LLVMByteOrder(ctx->target_data_layout);
        // size_t num = 0;
        // uint8_t signum = 0;
        // if (index->ast_type->content.integer.is_signed &&
        //     (index->content.binary[byteOrdering == LLVMBigEndian ? 0 : arrlenu(index->content.binary) - 1]
        //     &
        //      0x80))
        //     signum = 0xFF;

        // for (size_t i = sizeof(size_t) - 1; i >= 0; i--)
        // {
        //     num <<= 8;
        //     if (i < arrlenu(index->content.binary))
        //         num +=
        //             index->content
        //                 .binary[byteOrdering == LLVMBigEndian ? arrlenu(index->content.binary) - i - 1 :
        //                 i];
        //     else
        //         num += signum;
        // }

        ptlang_ast_exp array = ptlang_verify_eval(exp->content.array_element.array, false, ctx);

        size_t num = ptlang_verify_binary_to_unsigned(index, ctx);

        if ((index->ast_type->content.integer.is_signed &&
             (index->content.binary[byteOrdering == LLVMBigEndian ? 0 : arrlenu(index->content.binary) - 1] &
              0x80)) ||
            num >= arrlenu(array->content.array.values))
        {
            // TODO throw error
        }

        evaluated = ptlang_verify_eval(array->content.array.values[num], eval_fully, ctx);
        break;
    }
    case PTLANG_AST_EXP_REFERENCE:
    {
        evaluated = ptlang_ast_exp_copy(exp);
        break;
    }
    }

    if (evaluated == NULL)
        evaluated = ptlang_eval_const_exp(substituted);

    ptlang_free(substituted);
    return evaluated;
}

ptlang_ast_exp ptlang_verify_get_default_value(ptlang_ast_type type, ptlang_context *ctx)
{
    switch (type->type)
    {
    case PTLANG_AST_TYPE_VOID:
    case PTLANG_AST_TYPE_HEAP_ARRAY:
    case PTLANG_AST_TYPE_FUNCTION:
    case PTLANG_AST_TYPE_REFERENCE:
        return NULL;
    }
    type = ptlang_context_copy_unname_type(type, ctx->type_scope);
    ptlang_ast_exp default_value = ptlang_malloc(sizeof(struct ptlang_ast_exp_s));
    switch (type->type)
    {
    case PTLANG_AST_TYPE_INTEGER:
    {
        *default_value = (struct ptlang_ast_exp_s){
            .type = PTLANG_AST_EXP_BINARY,
            .content.binary = ptlang_malloc_zero(((type->content.integer.size - 1) >> 3) + 1),
        };
        break;
    }
    case PTLANG_AST_TYPE_FLOAT:
    {
        *default_value = (struct ptlang_ast_exp_s){
            .type = PTLANG_AST_EXP_BINARY,
            .content.binary = ptlang_malloc_zero(type->content.float_size >> 3),
        };
        break;
    }
    case PTLANG_AST_TYPE_ARRAY:
    {
        ptlang_ast_exp *array_values = NULL;
        for (size_t i = 0; i < type->content.array.len; i++)
        {
            ptlang_ast_exp element_default = ptlang_verify_get_default_value(type->content.array.type, ctx);
            arrpush(array_values, element_default);
        }
        *default_value = (struct ptlang_ast_exp_s){
            .type = PTLANG_AST_EXP_ARRAY,
            .content.array.type = ptlang_context_copy_unname_type(type->content.array.type, ctx->type_scope),
            .content.array.values = array_values,
        };
        break;
    }
    case PTLANG_AST_TYPE_NAMED:
    {
        // Get struct def
        ptlang_ast_struct_def struct_def = ptlang_context_get_struct_def(type->content.name, ctx->type_scope);

        // Init members
        ptlang_ast_struct_member_list struct_members = NULL;
        for (size_t i = 0; i < arrlenu(struct_def->members); i++)
        {
            struct ptlang_ast_struct_member_s member_default = (struct ptlang_ast_struct_member_s){
                .exp = ptlang_verify_get_default_value(struct_def->members[i]->type, ctx),
                .str = ptlang_ast_ident_copy(struct_def->members[i]->name),
                .pos = NULL,
            };

            arrpush(struct_members, member_default);
        }

        *default_value = (struct ptlang_ast_exp_s){
            .type = PTLANG_AST_EXP_STRUCT,
            .content.struct_.type.name = NULL,
            .content.struct_.type.pos = NULL,
            .content.struct_.members = struct_members,
        };

        size_t str_size = strlen(type->content.name) + 1;
        default_value->content.struct_.type.name = ptlang_malloc(str_size);
        memcpy(default_value->content.struct_.type.name, type->content.name, str_size);
        break;
    }
    }
    default_value->ast_type = type;
    default_value->pos = NULL;
    return default_value;
}

static size_t ptlang_verify_calc_node_count(ptlang_ast_type type, ptlang_context_type_scope *type_scope)
{
    type = ptlang_context_unname_type(type, type_scope);
    // the node of a array / struct itself refers to the the reference to this array / struct
    switch (type->type)
    {

    case PTLANG_AST_TYPE_VOID:
        return 0;
    case PTLANG_AST_TYPE_ARRAY:
        return 1 +
               type->content.array.len * ptlang_verify_calc_node_count(type->content.array.type, type_scope);
    case PTLANG_AST_TYPE_NAMED:
    {
        size_t nodes = 1;
        ptlang_ast_struct_def struct_def = ptlang_context_get_struct_def(type->content.name, type_scope);
        for (size_t i = 0; i < arrlenu(struct_def->members); i++)
        {
            nodes += ptlang_verify_calc_node_count(struct_def->members[i]->type, type_scope);
        }
        return nodes;
    }
    default:
        return 1;
    }
}

static void ptlang_verify_label_nodes(ptlang_ast_exp path_exp, ptlang_utils_graph_node *node,
                                      ptlang_context_type_scope *type_scope)
{
    node->data = path_exp;
    node++;
    switch (path_exp->ast_type->type)
    {
    case PTLANG_AST_TYPE_ARRAY:
    {
        for (size_t i = 0; i < path_exp->ast_type->content.array.len; i++)
        {
            char *index_str_repr = ptlang_malloc(21);
            snprintf(index_str_repr, 21, "%" PRIu64, i);
            ptlang_ast_exp index = ptlang_ast_exp_integer_new(index_str_repr, NULL);
            ptlang_ast_exp array_element = ptlang_ast_exp_array_element_new(
                ptlang_ast_exp_copy(path_exp), index, ptlang_ast_code_position_copy(path_exp->pos));
            ptlang_verify_label_nodes(array_element, node, type_scope);
        }
        break;
    }

    case PTLANG_AST_TYPE_NAMED:
    {
        ptlang_ast_struct_def struct_def =
            ptlang_context_get_struct_def(path_exp->ast_type->content.name, type_scope);
        for (size_t i = 0; i < arrlenu(struct_def->members); i++)
        {
            ptlang_ast_exp struct_member = ptlang_ast_exp_struct_member_new(
                ptlang_ast_exp_copy(path_exp), ptlang_ast_ident_copy(struct_def->members[i]->name),
                ptlang_ast_code_position_copy(path_exp->pos));
            ptlang_verify_label_nodes(struct_member, node, type_scope);
        }
        break;
    }
    default:
        break;
    }
}

static void ptlang_verify_eval_globals(ptlang_ast_module module, ptlang_context *ctx)
{
    ptlang_verify_node_table node_table = NULL;
    // ptlang_verify_decl_table decl_table = NULL;
    ptlang_utils_graph_node *nodes = NULL;

    for (size_t i = 0; i < arrlenu(module->declarations); i++)
    {
        // shput(decl_table, module->declarations[i]->name.name, module->declarations[i]);
        size_t decl_node_count =
            ptlang_verify_calc_node_count(module->declarations[i]->type, ctx->type_scope);
        for (size_t j = 0; j < decl_node_count; j++)
        {
            arrpush(nodes, (struct node_s){.index = -1});
            if (j == 0)
                shput(node_table, module->declarations[i]->name.name, &nodes[arrlen(nodes) - 1]);
        }
    }
    for (size_t i = 0; i < arrlenu(module->declarations); i++)
    {
        ptlang_utils_graph_node *node = shget(node_table, module->declarations[i]->name.name);
        size_t var_name_size = strlen(module->declarations[i]->name.name) + 1;
        char *var_name = ptlang_malloc(var_name_size);
        memcpy(var_name, module->declarations[i]->name.name, var_name_size);
        ptlang_ast_exp global_var = ptlang_ast_exp_variable_new(
            var_name, ptlang_ast_code_position_copy(module->declarations[i]->name.pos));
        ptlang_verify_label_nodes(global_var, node, ctx->type_scope);
        if (module->declarations[i]->init != NULL)
            ptlang_verify_build_graph(node, module->declarations[i]->init, node_table, ctx);
    }

    ptlang_utils_graph_node **cycles = ptlang_utils_find_cycles(nodes);

    for (size_t i = 0; i < arrlenu(module->declarations); i++)
    {
        ptlang_utils_graph_node *node = shget(node_table, module->declarations[i]->name.name);
        if (module->declarations[i]->init == NULL || node->in_cycle)
            module->declarations[i]->init =
                ptlang_verify_get_default_value(module->declarations[i]->type, ctx);
    }

    // TODO error messages
}

/**
 * @returns `false` if cycles have to be rechecked
 */
static bool ptlang_verify_build_graph(ptlang_utils_graph_node *node, ptlang_ast_exp exp,
                                      ptlang_verify_node_table node_table, ptlang_context *ctx)
{
    switch (exp->type)
    {
    case PTLANG_AST_EXP_ADDITION:
    case PTLANG_AST_EXP_SUBTRACTION:
    case PTLANG_AST_EXP_NEGATION:
    case PTLANG_AST_EXP_MULTIPLICATION:
    case PTLANG_AST_EXP_DIVISION:
    case PTLANG_AST_EXP_MODULO:
    case PTLANG_AST_EXP_REMAINDER:
    case PTLANG_AST_EXP_EQUAL:
    case PTLANG_AST_EXP_NOT_EQUAL:
    case PTLANG_AST_EXP_GREATER:
    case PTLANG_AST_EXP_GREATER_EQUAL:
    case PTLANG_AST_EXP_LESS:
    case PTLANG_AST_EXP_LESS_EQUAL:
    case PTLANG_AST_EXP_LEFT_SHIFT:
    case PTLANG_AST_EXP_RIGHT_SHIFT:
    case PTLANG_AST_EXP_AND:
    case PTLANG_AST_EXP_OR:
        ptlang_verify_build_graph(node, exp->content.binary_operator.left_value, node_table, ctx);
        ptlang_verify_build_graph(node, exp->content.binary_operator.right_value, node_table, ctx);
        break;
    case PTLANG_AST_EXP_NOT:
    case PTLANG_AST_EXP_BITWISE_AND:
    case PTLANG_AST_EXP_BITWISE_OR:
    case PTLANG_AST_EXP_BITWISE_XOR:
    case PTLANG_AST_EXP_BITWISE_INVERSE:
        ptlang_verify_build_graph(node, exp->content.unary_operator, node_table, ctx);
        break;
    case PTLANG_AST_EXP_LENGTH:
        break;
    case PTLANG_AST_EXP_VARIABLE:
        ptlang_verify_add_dependency(node, ptlang_verify_get_node(exp, node_table, ctx), exp, node_table,
                                     ctx);
        break;
    case PTLANG_AST_EXP_INTEGER:
    case PTLANG_AST_EXP_FLOAT:
        break;
    case PTLANG_AST_EXP_STRUCT:
        for (size_t i = 0; i < arrlenu(exp->content.struct_.members); i++)
        {
            ptlang_ast_type struct_type = exp->ast_type;
            ptlang_ast_struct_def struct_def = ptlang_context_get_struct_def(
                ptlang_context_unname_type(struct_type->content.name, ctx->type_scope), ctx->type_scope);

            size_t offset;
            for (offset = 0; offset < arrlenu(struct_def->members); offset++)
            {
                if (strcmp(struct_def->members[offset]->name.name,
                           exp->content.struct_.members[i].str.name) == 0)
                {
                    ptlang_verify_build_graph(node + 1 + offset, exp->content.struct_.members[i].exp,
                                              node_table, ctx);
                }
            }
        }
        break;
    case PTLANG_AST_EXP_ARRAY:
        for (size_t i = 0; i < arrlenu(exp->content.array.values); i++)
        {
            ptlang_verify_build_graph(node + 1 + i, exp->content.array.values[i], node_table, ctx);
        }
        break;
    case PTLANG_AST_EXP_TERNARY:
        ptlang_verify_build_graph(node, exp->content.ternary_operator.condition, node_table, ctx);
        return false;
    case PTLANG_AST_EXP_CAST:
        ptlang_verify_build_graph(node, exp->content.cast.value, node_table, ctx);
    case PTLANG_AST_EXP_STRUCT_MEMBER:
        ptlang_verify_build_graph(node, exp->content.struct_member.struct_, node_table, ctx);
        return false;
    case PTLANG_AST_EXP_ARRAY_ELEMENT:
        ptlang_verify_build_graph(node, exp->content.array_element.array, node_table, ctx);
        ptlang_verify_build_graph(node, exp->content.array_element.index, node_table, ctx);
        return false;
    case PTLANG_AST_EXP_REFERENCE:
        break;
    case PTLANG_AST_EXP_DEREFERENCE:
        ptlang_verify_build_graph(node, exp->content.unary_operator, node_table, ctx);
        return false;
    case PTLANG_AST_EXP_BINARY:
        break;
    }
    return true;
}

static void ptlang_verify_add_dependency(ptlang_utils_graph_node *from, ptlang_utils_graph_node *to,
                                         ptlang_ast_exp exp, ptlang_verify_node_table node_table,
                                         ptlang_context *ctx)
{
    size_t node_count = ptlang_verify_calc_node_count(exp->ast_type, ctx->type_scope);
    for (size_t i = 0; i < node_count; i++)
    {
        arrpush(from[i].edges_to, &to[i]);
    }
}

static ptlang_utils_graph_node *
ptlang_verify_get_node(ptlang_ast_exp exp, ptlang_verify_node_table node_table, ptlang_context *ctx)
{
    switch (exp->type)
    {
    case PTLANG_AST_EXP_VARIABLE:
    {
        return shget(node_table, exp->content.str_prepresentation);
    }
    case PTLANG_AST_EXP_STRUCT_MEMBER:
    {
        ptlang_utils_graph_node *base_node =
            ptlang_verify_get_node(exp->content.struct_member.struct_, node_table, ctx);
        ptlang_ast_type struct_type = exp->content.struct_member.struct_->ast_type;
        ptlang_ast_struct_def struct_def = ptlang_context_get_struct_def(
            ptlang_context_unname_type(struct_type->content.name, ctx->type_scope), ctx->type_scope);

        size_t offset;
        for (offset = 0; offset < arrlenu(struct_def->members); offset++)
        {
            if (strcmp(struct_def->members[offset]->name.name, exp->content.struct_member.member_name.name) ==
                0)
            {
                break;
            }
        }
        return base_node + offset + 1;
    }
    case PTLANG_AST_EXP_ARRAY_ELEMENT:
    {
        ptlang_utils_graph_node *base_node =
            ptlang_verify_get_node(exp->content.array_element.array, node_table, ctx);
        return base_node + 1 + ptlang_verify_binary_to_unsigned(exp->content.array_element.index, ctx);
    }
    default:
        return NULL;
    }
}

static size_t ptlang_verify_binary_to_unsigned(ptlang_ast_exp binary, ptlang_context *ctx)
{

    enum LLVMByteOrdering byteOrdering = LLVMByteOrder(ctx->target_data_layout);
    size_t num = 0;

    for (size_t i = sizeof(size_t) - 1; i >= 0; i--)
    {
        num <<= 8;
        if (i < arrlenu(binary->content.binary))
            num += binary->content
                       .binary[byteOrdering == LLVMBigEndian ? arrlenu(binary->content.binary) - i - 1 : i];
    }
    return num;
}