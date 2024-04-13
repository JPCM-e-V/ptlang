#define asdf
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
    ptlang_verify_struct_defs(ptlang_rc_deref(module).struct_defs, ctx, &errors);
    ptlang_verify_global_decls(ptlang_rc_deref(module).declarations, ctx, &errors);
    ptlang_verify_eval_globals(module, ctx, &errors);
    ptlang_verify_functions(ptlang_rc_deref(module).functions, ctx, &errors);
    return errors;
}

static void ptlang_verify_global_decls(ptlang_ast_decl *declarations, ptlang_context *ctx,
                                       ptlang_error **errors)
{
    for (size_t i = 0; i < arrlenu(declarations); i++)
    {
        ptlang_verify_decl_header(declarations[i], 0, ctx, errors);
    }
    for (size_t i = 0; i < arrlenu(declarations); i++)
    {
        ptlang_verify_decl_init(declarations[i], 0, ctx, errors);
        if (ptlang_rc_deref(declarations[i]).init != NULL)
            ptlang_verify_exp_check_const(ptlang_rc_deref(declarations[i]).init, ctx, errors);
    }
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
    bool validate_return_type = ptlang_verify_type(ptlang_rc_deref(function).return_type, ctx, errors);

    size_t function_scope_offset = arrlenu(ctx->scope);

    for (size_t i = 0; i < arrlenu(ptlang_rc_deref(function).parameters); i++)
    {
        if (!ptlang_verify_type(ptlang_rc_deref(ptlang_rc_deref(function).parameters[i]).type, ctx, errors))
        {
            ptlang_rc_deref(ptlang_rc_deref(function).parameters[i]).type = NULL;
        }
        arrput(ctx->scope, ptlang_rc_deref(function).parameters[i]);
    }

    size_t function_stmt_scope_offset = arrlenu(ctx->scope);

    bool has_return_value = false;
    bool is_unreachable = false;

    ptlang_verify_statement(ptlang_rc_deref(function).stmt, 0, validate_return_type,
                            ptlang_rc_deref(function).return_type, function_stmt_scope_offset,
                            &has_return_value, &is_unreachable, ctx, errors);

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
    switch (ptlang_rc_deref(statement).type)
    {
    case PTLANG_AST_STMT_BLOCK:
    {
        size_t new_scope_offset = arrlenu(ctx->scope);
        for (size_t i = 0; i < arrlenu(ptlang_rc_deref(statement).content.block.stmts); i++)
        {
            if (*is_unreachable)
            {
                arrput(*errors,
                       ((ptlang_error){
                           .type = PTLANG_ERROR_CODE_UNREACHABLE,
                           .pos = ptlang_rc_deref(
                               ptlang_rc_deref(ptlang_rc_deref(statement).content.block.stmts[i]).pos),
                           .message = "Dead code detected.",
                       }));
                *is_unreachable = false;
            }
            bool child_node_has_return_value = false;
            ptlang_verify_statement(ptlang_rc_deref(statement).content.block.stmts[i], nesting_level,
                                    validate_return_type, wanted_return_type, new_scope_offset,
                                    &child_node_has_return_value, is_unreachable, ctx, errors);
            *has_return_value |= child_node_has_return_value;
        }
        arrsetlen(ctx->scope, new_scope_offset);
        break;
    }
    case PTLANG_AST_STMT_EXP:
        ptlang_verify_exp(ptlang_rc_deref(statement).content.exp, ctx, errors);
        break;

    case PTLANG_AST_STMT_DECL:
        ptlang_verify_decl(ptlang_rc_deref(statement).content.decl, scope_offset, ctx, errors);
        break;
    case PTLANG_AST_STMT_IF:
    case PTLANG_AST_STMT_WHILE:
    case PTLANG_AST_STMT_IF_ELSE:
    {
        ptlang_ast_exp condition = ptlang_rc_deref(statement).type == PTLANG_AST_STMT_IF_ELSE
                                       ? ptlang_rc_deref(statement).content.control_flow2.condition
                                       : ptlang_rc_deref(statement).content.control_flow.condition;
        ptlang_verify_exp(condition, ctx, errors);

        if (condition != NULL &&
            (ptlang_rc_deref(ptlang_rc_deref(condition).ast_type).type != PTLANG_AST_TYPE_INTEGER ||
             ptlang_rc_deref(ptlang_rc_deref(condition).ast_type).content.integer.size != 1 ||
             ptlang_rc_deref(ptlang_rc_deref(condition).ast_type).content.integer.is_signed))
        {
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_TYPE,
                                .pos = ptlang_rc_deref(ptlang_rc_deref(condition).pos),
                                .message = "Control flow condition must be a float or an int.",
                            }));
        }

        if (ptlang_rc_deref(statement).type != PTLANG_AST_STMT_IF_ELSE)
        {

            ptlang_verify_statement(ptlang_rc_deref(statement).content.control_flow.stmt,
                                    nesting_level +
                                        (ptlang_rc_deref(statement).type == PTLANG_AST_STMT_WHILE),
                                    validate_return_type, wanted_return_type, scope_offset, has_return_value,
                                    is_unreachable, ctx, errors);
            *is_unreachable = false;
            *has_return_value = false;
        }
        else
        {
            bool child_nodes_have_return_value[2] = {false, false};
            bool child_nodes_are_unreachable[2] = {false, false};

            ptlang_verify_statement(ptlang_rc_deref(statement).content.control_flow2.stmt[0], nesting_level,
                                    validate_return_type, wanted_return_type, scope_offset,
                                    &child_nodes_have_return_value[0], &child_nodes_are_unreachable[0], ctx,
                                    errors);
            ptlang_verify_statement(ptlang_rc_deref(statement).content.control_flow2.stmt[1], nesting_level,
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
        ptlang_verify_exp(ptlang_rc_deref(statement).content.exp, ctx, errors);
        if (validate_return_type)
        {
            ptlang_ast_type return_type = ptlang_rc_deref(ptlang_rc_deref(statement).content.exp).ast_type;
            ptlang_verify_check_implicit_cast(return_type, wanted_return_type,
                                              ptlang_rc_deref(ptlang_rc_deref(statement).content.exp).pos,
                                              ctx, errors);
        }
        break;
    case PTLANG_AST_STMT_BREAK:
    case PTLANG_AST_STMT_CONTINUE:
        *has_return_value = false;

        if (ptlang_rc_deref(statement).content.nesting_level == 0)
        {
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_NESTING_LEVEL_OUT_OF_RANGE,
                                .pos = ptlang_rc_deref(ptlang_rc_deref(statement).pos),
                                .message = "Nesting level must be a positive integer.",
                            }));
        }
        else if (ptlang_rc_deref(statement).content.nesting_level > nesting_level)
        {
            size_t message_len = sizeof("Given nesting level 18446744073709551615 exceeds current nesting "
                                        "level of 18446744073709551615");
            char *message = ptlang_malloc(message_len);
            snprintf(message, message_len,
                     "Given nesting level %" PRIu64 " exceeds current nesting level of %" PRIu64,
                     ptlang_rc_deref(statement).content.nesting_level, nesting_level);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_NESTING_LEVEL_OUT_OF_RANGE,
                                .pos = ptlang_rc_deref(ptlang_rc_deref(statement).pos),
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
    ptlang_verify_decl_init(decl, scope_offset, ctx, errors);
}

static void ptlang_verify_decl_init(ptlang_ast_decl decl, size_t scope_offset, ptlang_context *ctx,
                                    ptlang_error **errors)
{
    if (ptlang_rc_deref(decl).init != NULL)
    {
        ptlang_verify_exp(ptlang_rc_deref(decl).init, ctx, errors);
        ptlang_verify_check_implicit_cast(ptlang_rc_deref(ptlang_rc_deref(decl).init).ast_type,
                                          ptlang_rc_deref(decl).type, ptlang_rc_deref(decl).pos, ctx, errors);
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

    bool correct = ptlang_verify_type(ptlang_rc_deref(decl).type, ctx, errors);

    for (size_t i = scope_offset; i < arrlenu(ctx->scope); i++)
    {
        if (strcmp(ptlang_rc_deref(ctx->scope[i]).name.name, ptlang_rc_deref(decl).name.name) == 0)
        {
            size_t message_len = sizeof("The variable name '' is already used in the current scope.") +
                                 strlen(ptlang_rc_deref(decl).name.name);
            char *message = ptlang_malloc(message_len);
            snprintf(message, message_len, "The variable name '%s' is already used in the current scope.",
                     ptlang_rc_deref(decl).name.name);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_VARIABLE_NAME_DUPLICATION,
                                .pos = ptlang_rc_deref(ptlang_rc_deref(decl).pos),
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
    switch (ptlang_rc_deref(exp).type)
    {
    case PTLANG_AST_EXP_VARIABLE:
    {
        for (size_t i = 0; i < arrlenu(ctx->scope); i++)
        {
            if (strcmp(ptlang_rc_deref(ctx->scope[i]).name.name,
                       ptlang_rc_deref(exp).content.str_prepresentation) == 0)
            {
                return ptlang_rc_deref(ctx->scope[i]).writable;
            }
        }
        return true;
    }
    case PTLANG_AST_EXP_STRUCT_MEMBER:
    {
        return ptlang_verify_exp_is_mutable(ptlang_rc_deref(exp).content.struct_member.struct_, ctx);
    }
    case PTLANG_AST_EXP_ARRAY_ELEMENT:
    {
        return ptlang_verify_exp_is_mutable(ptlang_rc_deref(exp).content.array_element.array, ctx);
    }
    case PTLANG_AST_EXP_LENGTH:
    {
        return ptlang_verify_exp_is_mutable(ptlang_rc_deref(exp).content.unary_operator, ctx);
    }
    case PTLANG_AST_EXP_DEREFERENCE:
    {
        return ptlang_rc_deref(ptlang_rc_deref(exp).content.unary_operator).ast_type == NULL ||
               ptlang_rc_deref(ptlang_rc_deref(ptlang_rc_deref(exp).content.unary_operator).ast_type)
                   .content.reference.writable;
    }
    default:
        return false;
    }
}

static bool ptlang_verify_child_exp(ptlang_ast_exp left, ptlang_ast_exp right, ptlang_ast_type *out)
{
    if (ptlang_rc_deref(left).ast_type == NULL || ptlang_rc_deref(right).ast_type == NULL)
    {
        *out = NULL;
        return true;
    }
    if ((ptlang_rc_deref(ptlang_rc_deref(left).ast_type).type == PTLANG_AST_TYPE_INTEGER ||
         ptlang_rc_deref(ptlang_rc_deref(left).ast_type).type == PTLANG_AST_TYPE_FLOAT) &&
        (ptlang_rc_deref(ptlang_rc_deref(right).ast_type).type == PTLANG_AST_TYPE_INTEGER ||
         ptlang_rc_deref(ptlang_rc_deref(right).ast_type).type == PTLANG_AST_TYPE_FLOAT))
    {
        if (ptlang_rc_deref(ptlang_rc_deref(left).ast_type).type == PTLANG_AST_TYPE_INTEGER &&
            ptlang_rc_deref(ptlang_rc_deref(right).ast_type).type == PTLANG_AST_TYPE_INTEGER)
        {
            *out = ptlang_ast_type_integer(
                ptlang_rc_deref(ptlang_rc_deref(left).ast_type).content.integer.is_signed ||
                    ptlang_rc_deref(ptlang_rc_deref(right).ast_type).content.integer.is_signed,
                ptlang_rc_deref(ptlang_rc_deref(left).ast_type).content.integer.size >
                        ptlang_rc_deref(ptlang_rc_deref(right).ast_type).content.integer.size
                    ? ptlang_rc_deref(ptlang_rc_deref(left).ast_type).content.integer.size
                    : ptlang_rc_deref(ptlang_rc_deref(right).ast_type).content.integer.size,
                NULL);
        }
        else
        {
            uint32_t left_size =
                ptlang_rc_deref(ptlang_rc_deref(left).ast_type).type == PTLANG_AST_TYPE_INTEGER
                    ? ptlang_rc_deref(ptlang_rc_deref(left).ast_type).content.integer.size
                    : ptlang_rc_deref(ptlang_rc_deref(left).ast_type).content.float_size;
            uint32_t right_size =
                ptlang_rc_deref(ptlang_rc_deref(right).ast_type).type == PTLANG_AST_TYPE_INTEGER
                    ? ptlang_rc_deref(ptlang_rc_deref(right).ast_type).content.integer.size
                    : ptlang_rc_deref(ptlang_rc_deref(right).ast_type).content.float_size;

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

    ptlang_ast_exp unary = ptlang_rc_deref(exp).content.unary_operator;
    ptlang_ast_exp left = ptlang_rc_deref(exp).content.binary_operator.left_value;
    ptlang_ast_exp right = ptlang_rc_deref(exp).content.binary_operator.right_value;

    switch (ptlang_rc_deref(exp).type)
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
                                .pos = ptlang_rc_deref(ptlang_rc_deref(exp).pos),
                                .message = message,
                            }));
        }

        ptlang_rc_deref(exp).ast_type =
            ptlang_rc_add_ref(ptlang_context_unname_type(ptlang_rc_deref(left).ast_type, ctx->type_scope));
        ptlang_verify_check_implicit_cast(ptlang_rc_deref(right).ast_type, ptlang_rc_deref(left).ast_type,
                                          ptlang_rc_deref(exp).pos, ctx, errors);
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

        if (!ptlang_verify_child_exp(left, right, &ptlang_rc_deref(exp).ast_type))
        {
            size_t message_len = sizeof("Operation arguments are incompatible.");
            char *message = ptlang_malloc(message_len);
            memcpy(message, "Operation arguments are incompatible.", message_len);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_TYPE,
                                .pos = ptlang_rc_deref(ptlang_rc_deref(exp).pos),
                                .message = message,
                            }));
        }
        break;
    }
    case PTLANG_AST_EXP_NEGATION:
    {
        ptlang_verify_exp(unary, ctx, errors);

        if (ptlang_rc_deref(unary).ast_type == NULL)
        {
            break;
        }

        if (ptlang_rc_deref(ptlang_rc_deref(unary).ast_type).type == PTLANG_AST_TYPE_INTEGER ||
            ptlang_rc_deref(ptlang_rc_deref(unary).ast_type).type == PTLANG_AST_TYPE_FLOAT)
        {
            ptlang_rc_deref(exp).ast_type = ptlang_rc_add_ref(
                ptlang_context_unname_type(ptlang_rc_deref(unary).ast_type, ctx->type_scope));
        }
        else
        {
            size_t message_len = sizeof("Bad operant type for negation.");
            char *message = ptlang_malloc(message_len);
            memcpy(message, "Bad operant type for negation.", message_len);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_TYPE,
                                .pos = ptlang_rc_deref(ptlang_rc_deref(exp).pos),
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

        if ((ptlang_rc_deref(left).ast_type != NULL &&
             ptlang_rc_deref(ptlang_rc_deref(left).ast_type).type != PTLANG_AST_TYPE_INTEGER &&
             ptlang_rc_deref(ptlang_rc_deref(left).ast_type).type != PTLANG_AST_TYPE_FLOAT &&
             ptlang_rc_deref(ptlang_rc_deref(left).ast_type).type != PTLANG_AST_TYPE_REFERENCE) ||
            (ptlang_rc_deref(right).ast_type != NULL &&
             ptlang_rc_deref(ptlang_rc_deref(right).ast_type).type != PTLANG_AST_TYPE_INTEGER &&
             ptlang_rc_deref(ptlang_rc_deref(right).ast_type).type != PTLANG_AST_TYPE_FLOAT &&
             ptlang_rc_deref(ptlang_rc_deref(right).ast_type).type != PTLANG_AST_TYPE_REFERENCE))
        {
            size_t message_len = sizeof("The operant types must be numbers or references.");
            char *message = ptlang_malloc(message_len);
            memcpy(message, "The operant types must be numbers or references.", message_len);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_TYPE,
                                .pos = ptlang_rc_deref(ptlang_rc_deref(exp).pos),
                                .message = message,
                            }));
        }
        else if (ptlang_rc_deref(left).ast_type != NULL && ptlang_rc_deref(right).ast_type != NULL &&
                 (ptlang_rc_deref(ptlang_rc_deref(left).ast_type).type == PTLANG_AST_TYPE_REFERENCE) ==
                     (ptlang_rc_deref(ptlang_rc_deref(right).ast_type).type == PTLANG_AST_TYPE_REFERENCE))
        {
            size_t message_len = sizeof("The operant types must either be both numbers or both references.");
            char *message = ptlang_malloc(message_len);
            memcpy(message, "The operant types must either be both numbers or both references.", message_len);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_TYPE,
                                .pos = ptlang_rc_deref(ptlang_rc_deref(exp).pos),
                                .message = message,
                            }));
        }

        ptlang_rc_deref(exp).ast_type = ptlang_ast_type_integer(false, 1, NULL);
        break;
    }
    case PTLANG_AST_EXP_GREATER:
    case PTLANG_AST_EXP_GREATER_EQUAL:
    case PTLANG_AST_EXP_LESS:
    case PTLANG_AST_EXP_LESS_EQUAL:
    {
        ptlang_verify_exp(left, ctx, errors);
        ptlang_verify_exp(right, ctx, errors);

        if ((ptlang_rc_deref(left).ast_type != NULL &&
             ptlang_rc_deref(ptlang_rc_deref(left).ast_type).type != PTLANG_AST_TYPE_INTEGER &&
             ptlang_rc_deref(ptlang_rc_deref(left).ast_type).type != PTLANG_AST_TYPE_FLOAT) ||
            (ptlang_rc_deref(right).ast_type != NULL &&
             ptlang_rc_deref(ptlang_rc_deref(right).ast_type).type != PTLANG_AST_TYPE_INTEGER &&
             ptlang_rc_deref(ptlang_rc_deref(right).ast_type).type != PTLANG_AST_TYPE_FLOAT))
        {
            size_t message_len = sizeof("Bad operant type for comparison.");
            char *message = ptlang_malloc(message_len);
            memcpy(message, "Bad operant type for comparison.", message_len);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_TYPE,
                                .pos = ptlang_rc_deref(ptlang_rc_deref(exp).pos),
                                .message = message,
                            }));
        }
        ptlang_rc_deref(exp).ast_type = ptlang_ast_type_integer(false, 1, NULL);
        break;
    }
    case PTLANG_AST_EXP_LEFT_SHIFT:
    case PTLANG_AST_EXP_RIGHT_SHIFT:
    {
        ptlang_verify_exp(left, ctx, errors);
        ptlang_verify_exp(right, ctx, errors);

        if ((ptlang_rc_deref(left).ast_type != NULL &&
             ptlang_rc_deref(ptlang_rc_deref(left).ast_type).type != PTLANG_AST_TYPE_INTEGER) ||
            (ptlang_rc_deref(right).ast_type != NULL &&
             ptlang_rc_deref(ptlang_rc_deref(right).ast_type).type != PTLANG_AST_TYPE_INTEGER))
        {
            size_t message_len = sizeof("Bad operant type for shift.");
            char *message = ptlang_malloc(message_len);
            memcpy(message, "Bad operant type for shift.", message_len);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_TYPE,
                                .pos = ptlang_rc_deref(ptlang_rc_deref(exp).pos),
                                .message = message,
                            }));
        }

        ptlang_rc_deref(exp).ast_type =
            ptlang_rc_add_ref(ptlang_context_unname_type(ptlang_rc_deref(left).ast_type, ctx->type_scope));
        break;
    }
    case PTLANG_AST_EXP_AND:
    case PTLANG_AST_EXP_OR:
    {
        ptlang_verify_exp(left, ctx, errors);
        ptlang_verify_exp(right, ctx, errors);

        if ((ptlang_rc_deref(left).ast_type != NULL &&
             ptlang_rc_deref(ptlang_rc_deref(left).ast_type).type != PTLANG_AST_TYPE_INTEGER &&
             ptlang_rc_deref(ptlang_rc_deref(left).ast_type).type != PTLANG_AST_TYPE_FLOAT) ||
            (ptlang_rc_deref(right).ast_type != NULL &&
             ptlang_rc_deref(ptlang_rc_deref(right).ast_type).type != PTLANG_AST_TYPE_INTEGER &&
             ptlang_rc_deref(ptlang_rc_deref(right).ast_type).type != PTLANG_AST_TYPE_FLOAT))
        {
            size_t message_len = sizeof("Bad operant type for boolean operation.");
            char *message = ptlang_malloc(message_len);
            memcpy(message, "Bad operant type for boolean operation.", message_len);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_TYPE,
                                .pos = ptlang_rc_deref(ptlang_rc_deref(exp).pos),
                                .message = message,
                            }));
        }

        ptlang_rc_deref(exp).ast_type = ptlang_ast_type_integer(false, 1, NULL);
        break;
    }
    case PTLANG_AST_EXP_NOT:
    {
        ptlang_verify_exp(unary, ctx, errors);

        if (ptlang_rc_deref(unary).ast_type != NULL &&
            ptlang_rc_deref(ptlang_rc_deref(unary).ast_type).type != PTLANG_AST_TYPE_INTEGER &&
            ptlang_rc_deref(ptlang_rc_deref(unary).ast_type).type != PTLANG_AST_TYPE_FLOAT)
        {
            size_t message_len = sizeof("Bad operant type for boolean operation.");
            char *message = ptlang_malloc(message_len);
            memcpy(message, "Bad operant type for boolean operation.", message_len);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_TYPE,
                                .pos = ptlang_rc_deref(ptlang_rc_deref(exp).pos),
                                .message = message,
                            }));
        }

        ptlang_rc_deref(exp).ast_type = ptlang_ast_type_integer(false, 1, NULL);
        break;
    }
    case PTLANG_AST_EXP_BITWISE_AND:
    case PTLANG_AST_EXP_BITWISE_OR:
    case PTLANG_AST_EXP_BITWISE_XOR:
    {
        ptlang_verify_exp(left, ctx, errors);
        ptlang_verify_exp(right, ctx, errors);

        if ((ptlang_rc_deref(left).ast_type != NULL &&
             ptlang_rc_deref(ptlang_rc_deref(left).ast_type).type != PTLANG_AST_TYPE_INTEGER) ||
            (ptlang_rc_deref(right).ast_type != NULL &&
             ptlang_rc_deref(ptlang_rc_deref(right).ast_type).type != PTLANG_AST_TYPE_INTEGER))
        {
            size_t message_len = sizeof("Bad operant type for bitwise operation.");
            char *message = ptlang_malloc(message_len);
            memcpy(message, "Bad operant type for bitwise operation.", message_len);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_TYPE,
                                .pos = ptlang_rc_deref(ptlang_rc_deref(exp).pos),
                                .message = message,
                            }));
        }

        ptlang_verify_child_exp(left, right, &ptlang_rc_deref(exp).ast_type);

        break;
    }
    case PTLANG_AST_EXP_BITWISE_INVERSE:
    {
        ptlang_verify_exp(unary, ctx, errors);
        if (ptlang_rc_deref(unary).ast_type != NULL &&
            ptlang_rc_deref(ptlang_rc_deref(unary).ast_type).type != PTLANG_AST_TYPE_INTEGER)
        {
            size_t message_len = sizeof("Bad operant type for bitwise operation.");
            char *message = ptlang_malloc(message_len);
            memcpy(message, "Bad operant type for bitwise operation.", message_len);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_TYPE,
                                .pos = ptlang_rc_deref(ptlang_rc_deref(exp).pos),
                                .message = message,
                            }));
        }

        ptlang_rc_deref(exp).ast_type =
            ptlang_rc_add_ref(ptlang_context_unname_type(ptlang_rc_deref(unary).ast_type, ctx->type_scope));
        break;
    }
    case PTLANG_AST_EXP_LENGTH:
    {
        ptlang_verify_exp(unary, ctx, errors);
        ptlang_rc_deref(exp).ast_type =
            ptlang_ast_type_integer(true, LLVMPointerSize(ctx->target_data_layout), NULL);
        break;
    }
    case PTLANG_AST_EXP_FUNCTION_CALL:
    {
        ptlang_verify_exp(ptlang_rc_deref(exp).content.function_call.function, ctx, errors);
        for (size_t i = 0; i < arrlen(ptlang_rc_deref(exp).content.function_call.parameters); i++)
        {
            ptlang_verify_exp(ptlang_rc_deref(exp).content.function_call.parameters[i], ctx, errors);
        }
        if (ptlang_rc_deref(ptlang_rc_deref(exp).content.function_call.function).ast_type != NULL)
        {
            if (ptlang_rc_deref(ptlang_rc_deref(ptlang_rc_deref(exp).content.function_call.function).ast_type)
                    .type != PTLANG_AST_TYPE_FUNCTION)
            {
                size_t message_len = sizeof("Function call is not a function.");
                char *message = ptlang_malloc(message_len);
                memcpy(message, "Function call is not a function.", message_len);
                arrput(*errors, ((ptlang_error){
                                    .type = PTLANG_ERROR_PARAMETER_COUNT,
                                    .pos = ptlang_rc_deref(ptlang_rc_deref(exp).pos),
                                    .message = message,
                                }));
            }
            else
            {
                ptlang_rc_deref(exp).ast_type = ptlang_rc_add_ref(ptlang_context_unname_type(
                    ptlang_rc_deref(
                        ptlang_rc_deref(ptlang_rc_deref(exp).content.function_call.function).ast_type)
                        .content.function.return_type,
                    ctx->type_scope));
                if (arrlenu(ptlang_rc_deref(exp).content.function_call.parameters) >
                    arrlenu(ptlang_rc_deref(
                                ptlang_rc_deref(ptlang_rc_deref(exp).content.function_call.function).ast_type)
                                .content.function.parameters))
                {
                    size_t message_len = sizeof("These parameters are too many.");
                    char *message = ptlang_malloc(message_len);
                    memcpy(message, "These parameters are too many.", message_len);
                    arrput(*errors,
                           ((ptlang_error){
                               .type = PTLANG_ERROR_PARAMETER_COUNT,
                               .pos.from_line =
                                   ptlang_rc_deref(
                                       ptlang_rc_deref(
                                           ptlang_rc_deref(exp).content.function_call.parameters[arrlenu(
                                               ptlang_rc_deref(
                                                   ptlang_rc_deref(
                                                       ptlang_rc_deref(exp).content.function_call.function)
                                                       .ast_type)
                                                   .content.function.parameters)])
                                           .pos)
                                       .from_line,
                               .pos.from_column =
                                   ptlang_rc_deref(
                                       ptlang_rc_deref(
                                           ptlang_rc_deref(exp).content.function_call.parameters[arrlenu(
                                               ptlang_rc_deref(
                                                   ptlang_rc_deref(
                                                       ptlang_rc_deref(exp).content.function_call.function)
                                                       .ast_type)
                                                   .content.function.parameters)])
                                           .pos)
                                       .from_column,
                               .pos.to_line =
                                   ptlang_rc_deref(
                                       ptlang_rc_deref(
                                           arrlast(ptlang_rc_deref(exp).content.function_call.parameters))
                                           .pos)
                                       .to_line,
                               .pos.to_column =
                                   ptlang_rc_deref(
                                       ptlang_rc_deref(
                                           arrlast(ptlang_rc_deref(exp).content.function_call.parameters))
                                           .pos)
                                       .to_column,
                               .message = message,
                           }));
                }
                else if (arrlenu(ptlang_rc_deref(exp).content.function_call.parameters) <
                         arrlenu(ptlang_rc_deref(
                                     ptlang_rc_deref(ptlang_rc_deref(exp).content.function_call.function)
                                         .ast_type)
                                     .content.function.parameters))
                {
                    size_t message_len = sizeof("To few parameters.");
                    char *message = ptlang_malloc(message_len);
                    memcpy(message, "To few parameters.", message_len);
                    arrput(*errors, ((ptlang_error){
                                        .type = PTLANG_ERROR_PARAMETER_COUNT,
                                        .pos = ptlang_rc_deref(ptlang_rc_deref(exp).pos),
                                        .message = message,
                                    }));
                }

                size_t checkable_parameters =
                    arrlenu(ptlang_rc_deref(
                                ptlang_rc_deref(ptlang_rc_deref(exp).content.function_call.function).ast_type)
                                .content.function.parameters) <
                            arrlenu(ptlang_rc_deref(exp).content.function_call.parameters)
                        ? arrlenu(ptlang_rc_deref(
                                      ptlang_rc_deref(ptlang_rc_deref(exp).content.function_call.function)
                                          .ast_type)
                                      .content.function.parameters)
                        : arrlenu(ptlang_rc_deref(exp).content.function_call.parameters);

                for (size_t i = 0; i < checkable_parameters; i++)
                {
                    ptlang_verify_check_implicit_cast(
                        ptlang_rc_deref(ptlang_rc_deref(exp).content.function_call.parameters[i]).ast_type,
                        ptlang_rc_deref(
                            ptlang_rc_deref(ptlang_rc_deref(exp).content.function_call.function).ast_type)
                            .content.function.parameters[i],
                        ptlang_rc_deref(ptlang_rc_deref(exp).content.function_call.parameters[i]).pos, ctx,
                        errors);
                }
            }
        }
        break;
    }
    case PTLANG_AST_EXP_VARIABLE:
    {
        ptlang_ast_decl decl =
            ptlang_decl_list_find_last(ctx->scope, ptlang_rc_deref(exp).content.str_prepresentation);
        if (decl == NULL)
        {
            size_t message_len = sizeof("Unknown variable.");
            char *message = ptlang_malloc(message_len);
            memcpy(message, "Unknown variable.", message_len);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_UNKNOWN_VARIABLE_NAME,
                                .pos = ptlang_rc_deref(ptlang_rc_deref(exp).pos),
                                .message = message,
                            }));
        }
        else
        {
            ptlang_rc_deref(exp).ast_type =
                ptlang_rc_add_ref(ptlang_context_unname_type(ptlang_rc_deref(decl).type, ctx->type_scope));
        }
        break;
    }
    case PTLANG_AST_EXP_INTEGER:
    {
        errno = 0;
        char *suffix_begin;
        uint64_t number = strtoull(ptlang_rc_deref(exp).content.str_prepresentation, &suffix_begin, 10);
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
                                    .pos = ptlang_rc_deref(ptlang_rc_deref(exp).pos),
                                    .message = message,
                                }));
                break;
            }
            is_signed = (*suffix_begin) == 's' || (*suffix_begin) == 'S';
        }

        ptlang_rc_deref(exp).ast_type = ptlang_ast_type_integer(is_signed, bits, NULL);

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
                                .pos = ptlang_rc_deref(ptlang_rc_deref(exp).pos),
                                .message = message,
                            }));
        }
        break;
    }
    case PTLANG_AST_EXP_FLOAT:
    {
        char *suffix_begin = strchr(ptlang_rc_deref(exp).content.str_prepresentation, 'f');
        if (suffix_begin == NULL)
            suffix_begin = strchr(ptlang_rc_deref(exp).content.str_prepresentation, 'F');

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
        ptlang_rc_deref(exp).ast_type = ptlang_ast_type_float(float_size, NULL);

        break;
    }
    case PTLANG_AST_EXP_STRUCT:
    {
        char *name = ptlang_rc_deref(exp).content.struct_.type.name;

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
                                    .pos = ptlang_rc_deref(ptlang_rc_deref(exp).pos),
                                    .message = message,
                                }));
                break;
            }
            else if (entry->value.type == PTLANG_CONTEXT_TYPE_SCOPE_ENTRY_TYPEALIAS)
            {
                if (ptlang_rc_deref(entry->value.value.ptlang_type).type != PTLANG_AST_TYPE_NAMED)
                {
                    size_t message_len = sizeof("Type is not a struct.");
                    char *message = ptlang_malloc(message_len);
                    memcpy(message, "Type is not a struct.", message_len);
                    arrput(*errors, ((ptlang_error){
                                        .type = PTLANG_ERROR_TYPE,
                                        .pos = ptlang_rc_deref(ptlang_rc_deref(exp).pos),
                                        .message = message,
                                    }));
                    break;
                }
                else
                {
                    name = ptlang_rc_deref(entry->value.value.ptlang_type).content.name;
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

        size_t name_size = strlen(name) + 1;
        char *name_copy = ptlang_malloc(name_size);
        memcpy(name_copy, name, name_size);

        ptlang_rc_deref(exp).ast_type = ptlang_ast_type_named(name_copy, NULL);

        // ptlang_ast_type struct_type = ptlang_context_unname_type(entry->value, ctx->type_scope);
        // ptlang_ast_type member_type = NULL;

        // if (array_type != NULL)
        // {
        //     if (ptlang_rc_deref(array_type).type != PTLANG_AST_TYPE_ARRAY)
        //     {
        //         // throw error
        //     }
        //     else
        //     {
        //         ptlang_rc_deref(exp).ast_type = ptlang_context_unname_type(array_type,
        //         ctx->type_scope); member_type =
        //         ptlang_context_unname_type(ptlang_rc_deref(array_type).content.array.type,
        //         ctx->type_scope);
        //     }
        // }

        for (size_t i = 0; i < arrlenu(ptlang_rc_deref(exp).content.struct_.members); i++)
        {

            for (size_t j = 0; j < arrlenu(ptlang_rc_deref(exp).content.struct_.members); j++)
            {
                if (i == j)
                {
                    continue;
                }
                if (strcmp(ptlang_rc_deref(exp).content.struct_.members[i].str.name,
                           ptlang_rc_deref(exp).content.struct_.members[j].str.name) == 0)
                {
                    size_t message_len = sizeof("Duplicate member name.");
                    char *message = ptlang_malloc(message_len);
                    memcpy(message, "Duplicate member name.", message_len);
                    arrput(*errors,
                           ((ptlang_error){
                               .type = PTLANG_ERROR_TYPE,
                               .pos = ptlang_rc_deref(ptlang_rc_deref(exp).content.struct_.members[i].pos),
                               .message = message,
                           }));
                    break;
                }
            }

            ptlang_ast_type member_type_in_struct_def = NULL;
            bool found = false;
            for (size_t j = 0; j < arrlenu(ptlang_rc_deref(struct_def).members); j++)
            {
                if (strcmp(ptlang_rc_deref(exp).content.struct_.members[i].str.name,
                           ptlang_rc_deref(ptlang_rc_deref(struct_def).members[j]).name.name) == 0)
                {
                    member_type_in_struct_def = ptlang_rc_deref(ptlang_rc_deref(struct_def).members[j]).type;
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                size_t message_len = sizeof("Unknown member name.");
                char *message = ptlang_malloc(message_len);
                memcpy(message, "Unknown member name.", message_len);
                arrput(*errors,
                       ((ptlang_error){
                           .type = PTLANG_ERROR_TYPE,
                           .pos = ptlang_rc_deref(ptlang_rc_deref(exp).content.struct_.members[i].pos),
                           .message = message,
                       }));
                continue;
            }

            ptlang_verify_exp(ptlang_rc_deref(exp).content.struct_.members[i].exp, ctx, errors);
            ptlang_verify_check_implicit_cast(
                ptlang_rc_deref(ptlang_rc_deref(exp).content.struct_.members[i].exp).ast_type,
                member_type_in_struct_def, ptlang_rc_deref(exp).content.struct_.members[i].pos, ctx, errors);
        }

        break;
    }
    case PTLANG_AST_EXP_ARRAY:
    {
        ptlang_ast_type member_type =
            ptlang_context_unname_type(ptlang_rc_deref(exp).content.array.type, ctx->type_scope);
        // ptlang_ast_type member_type = NULL;

        if (member_type != NULL)
        {
            ptlang_rc_deref(exp).ast_type = ptlang_ast_type_array(
                ptlang_rc_add_ref(ptlang_context_unname_type(member_type, ctx->type_scope)),
                arrlenu(ptlang_rc_deref(exp).content.array.values), NULL);
            // member_type = ptlang_context_unname_type(ptlang_rc_deref(array_type).content.array.type,
            // ctx->type_scope);

            // ptlang_error *too_many_values_error = NULL;
            for (size_t i = 0; i < arrlenu(ptlang_rc_deref(exp).content.array.values); i++)
            {
                ptlang_verify_exp(ptlang_rc_deref(exp).content.array.values[i], ctx, errors);
                ptlang_verify_check_implicit_cast(
                    ptlang_rc_deref(ptlang_rc_deref(exp).content.array.values[i]).ast_type, member_type,
                    ptlang_rc_deref(ptlang_rc_deref(exp).content.array.values[i]).pos, ctx, errors);
                // if (i >= ptlang_rc_deref(member_type).content.array.len)
                // {
                //     if (too_many_values_error == NULL)
                //     {
                //         char *message = NULL;
                //         ptlang_utils_build_str(
                //             message, CONST_STR("An array of type "),
                //             ptlang_verify_type_to_string(member_type, ctx->type_scope),
                //             CONST_STR(" may not have more than "),
                //             ptlang_utils_sprintf_alloc("%zu",
                //             ptlang_rc_deref(member_type).content.array.len), CONST_STR(" values."));
                //         arrput(*errors, ((ptlang_error){
                //                             .type = PTLANG_ERROR_VALUE_COUNT,
                //                             .pos = *ptlang_rc_deref(exp).content.array.values[i]->pos,
                //                             .message = message,
                //                         }));
                //         too_many_values_error = &((*errors)[arrlenu(*errors) - 1]);
                //     }
                //     else
                //     {
                //         too_many_values_error->pos.to_column =
                //         ptlang_rc_deref(exp).content.array.values[i]->pos->to_column;
                //         too_many_values_error->pos.to_line =
                //         ptlang_rc_deref(exp).content.array.values[i]->pos->to_line;
                //     }
                // }
            }
        }
        break;
    }
    case PTLANG_AST_EXP_TERNARY:
    {
        ptlang_ast_exp condition = ptlang_rc_deref(exp).content.ternary_operator.condition;
        ptlang_ast_exp if_value = ptlang_rc_deref(exp).content.ternary_operator.if_value;
        ptlang_ast_exp else_value = ptlang_rc_deref(exp).content.ternary_operator.else_value;

        ptlang_verify_exp(condition, ctx, errors);
        ptlang_verify_exp(if_value, ctx, errors);
        ptlang_verify_exp(else_value, ctx, errors);

        ptlang_ast_type condition_type =
            ptlang_context_unname_type(ptlang_rc_deref(condition).ast_type, ctx->type_scope);
        if (condition_type != NULL && (ptlang_rc_deref(condition_type).type != PTLANG_AST_TYPE_INTEGER ||
                                       ptlang_rc_deref(condition_type).content.integer.size != 1 ||
                                       ptlang_rc_deref(condition_type).content.integer.is_signed))
        {
            arrpush(*errors,
                    ptlang_verify_generate_type_error("Ternary condition must be a u1, but is of type",
                                                      condition, ".", ctx->type_scope));
        }

        ptlang_rc_deref(exp).ast_type = ptlang_verify_unify_types(ptlang_rc_deref(if_value).ast_type,
                                                                  ptlang_rc_deref(else_value).ast_type, ctx);

        break;
    }
    case PTLANG_AST_EXP_CAST:
    {
        ptlang_ast_type to = ptlang_rc_add_ref(
            ptlang_context_unname_type(ptlang_rc_deref(exp).content.cast.type, ctx->type_scope));
        ptlang_rc_deref(exp).ast_type = to;
        ptlang_verify_exp(ptlang_rc_deref(exp).content.cast.value, ctx, errors);
        ptlang_ast_type from = ptlang_context_unname_type(
            ptlang_rc_deref(ptlang_rc_deref(exp).content.cast.value).ast_type, ctx->type_scope);
        if (!ptlang_verify_implicit_cast(from, to, ctx))
        {
            if (!((ptlang_rc_deref(to).type == PTLANG_AST_TYPE_INTEGER ||
                   ptlang_rc_deref(to).type == PTLANG_AST_TYPE_FLOAT) &&
                  (ptlang_rc_deref(from).type == PTLANG_AST_TYPE_INTEGER ||
                   ptlang_rc_deref(from).type == PTLANG_AST_TYPE_FLOAT)))
            {
                char *message = NULL;
                ptlang_utils_build_str(message, CONST_STR("A value of type"),
                                       ptlang_verify_type_to_string(from, ctx->type_scope),
                                       CONST_STR(" can not be casted to a "),
                                       ptlang_verify_type_to_string(to, ctx->type_scope), CONST_STR("."));

                arrput(*errors, ((ptlang_error){
                                    .type = PTLANG_ERROR_CAST_ILLEGAL,
                                    .pos = ptlang_rc_deref(ptlang_rc_deref(exp).pos),
                                    .message = message,
                                }));
            }
        }
        break;
    }
    case PTLANG_AST_EXP_STRUCT_MEMBER:
    {
        ptlang_verify_exp(ptlang_rc_deref(exp).content.struct_member.struct_, ctx, errors);
        ptlang_ast_type struct_exp_type = ptlang_context_unname_type(
            ptlang_rc_deref(ptlang_rc_deref(exp).content.struct_member.struct_).ast_type, ctx->type_scope);
        if (struct_exp_type != NULL)
        {
            if (ptlang_rc_deref(struct_exp_type).type != PTLANG_AST_TYPE_NAMED ||
                shget(ctx->type_scope, ptlang_rc_deref(struct_exp_type).content.name).type !=
                    PTLANG_CONTEXT_TYPE_SCOPE_ENTRY_STRUCT)
            {
                arrput(*errors,
                       ptlang_verify_generate_type_error(
                           "Only structs can have members. You can not access a member of a ",
                           ptlang_rc_deref(exp).content.struct_member.struct_, ".", ctx->type_scope));
            }
            else
            {
                ptlang_ast_struct_def struct_def = ptlang_context_get_struct_def(
                    ptlang_rc_deref(struct_exp_type).content.name, ctx->type_scope);

                ptlang_ast_decl member = NULL;
                for (size_t i = 0; i < arrlenu(ptlang_rc_deref(struct_def).members); i++)
                {
                    if (strcmp(ptlang_rc_deref(ptlang_rc_deref(struct_def).members[i]).name.name,
                               ptlang_rc_deref(exp).content.struct_member.member_name.name) == 0)
                    {
                        member = ptlang_rc_deref(struct_def).members[i];
                        break;
                    };
                }
                if (member == NULL)
                {
                    char *message = NULL;
                    ptlang_utils_build_str(
                        message, CONST_STR("The struct "),
                        ptlang_verify_type_to_string(
                            ptlang_rc_deref(ptlang_rc_deref(exp).content.struct_member.struct_).ast_type,
                            ctx->type_scope),
                        CONST_STR(" doesn't have the member "),
                        CONST_STR(ptlang_rc_deref(exp).content.struct_member.member_name.name),
                        CONST_STR("."));
                    arrput(*errors, ((ptlang_error){
                                        .type = PTLANG_ERROR_UNKNOWN_MEMBER,
                                        .pos = ptlang_rc_deref(ptlang_rc_deref(exp).pos),
                                        .message = message,
                                    }));
                }
                else
                {
                    ptlang_rc_deref(exp).ast_type = ptlang_rc_add_ref(
                        ptlang_context_unname_type(ptlang_rc_deref(member).type, ctx->type_scope));
                }
            }
        }
        break;
    }
    case PTLANG_AST_EXP_ARRAY_ELEMENT:
    {
        ptlang_verify_exp(ptlang_rc_deref(exp).content.array_element.index, ctx, errors);
        ptlang_verify_exp(ptlang_rc_deref(exp).content.array_element.array, ctx, errors);

        ptlang_ast_type index_type =
            ptlang_rc_deref(ptlang_rc_deref(exp).content.array_element.index).ast_type;
        if (index_type != NULL && (ptlang_rc_deref(index_type).type != PTLANG_AST_TYPE_INTEGER ||
                                   ptlang_rc_deref(index_type).content.integer.is_signed))
        {
            arrput(*errors, ptlang_verify_generate_type_error(
                                "Array index must be an unsigned integer, but is of type ",
                                ptlang_rc_deref(exp).content.array_element.index, ".", ctx->type_scope));
        }

        ptlang_ast_type array_type =
            ptlang_rc_deref(ptlang_rc_deref(exp).content.array_element.array).ast_type;
        if (array_type != NULL)
        {
            if (ptlang_rc_deref(array_type).type == PTLANG_AST_TYPE_ARRAY)
            {
                ptlang_rc_deref(exp).ast_type = ptlang_rc_add_ref(ptlang_context_unname_type(
                    ptlang_rc_deref(array_type).content.array.type, ctx->type_scope));
            }
            else if (ptlang_rc_deref(array_type).type == PTLANG_AST_TYPE_HEAP_ARRAY)
            {
                ptlang_rc_deref(exp).ast_type = ptlang_rc_add_ref(ptlang_context_unname_type(
                    ptlang_rc_deref(array_type).content.heap_array.type, ctx->type_scope));
            }
            else
            {
                arrput(*errors,
                       ptlang_verify_generate_type_error(
                           "Subscripted expression must have array or heap array type, but is of type ",
                           ptlang_rc_deref(exp).content.array_element.array, ".", ctx->type_scope));
            }
        };
    }

    break;

    case PTLANG_AST_EXP_REFERENCE:
    {
        ptlang_verify_exp(ptlang_rc_deref(exp).content.reference.value, ctx, errors);
        if (ptlang_rc_deref(exp).content.reference.writable &&
            !ptlang_verify_exp_is_mutable(ptlang_rc_deref(exp).content.reference.value, ctx))
        {
            char *message =
                ptlang_malloc(sizeof("Can not create a writable reference of an unmutable expression."));
            memcpy(message, "Can not create a writable reference of an unmutable expression.",
                   sizeof("Can not create a writable reference of an unmutable expression."));
            arrput(
                *errors,
                ((ptlang_error){
                    .type = PTLANG_ERROR_EXP_UNWRITABLE,
                    .pos = ptlang_rc_deref(ptlang_rc_deref(ptlang_rc_deref(exp).content.reference.value).pos),
                    .message = message,
                }));
        }

        ptlang_ast_type referenced_type =
            ptlang_rc_deref(ptlang_rc_deref(exp).content.reference.value).ast_type;
        if (referenced_type != NULL)
        {
            ptlang_rc_deref(exp).ast_type = ptlang_ast_type_reference(
                referenced_type, ptlang_rc_deref(exp).content.reference.writable, NULL);
        }

        break;
    }
    case PTLANG_AST_EXP_DEREFERENCE:
    {
        ptlang_verify_exp(unary, ctx, errors);
        if (ptlang_rc_deref(unary).ast_type != NULL)
        {
            size_t type_str_size =
                ptlang_context_type_to_string(ptlang_rc_deref(unary).ast_type, NULL, ctx->type_scope);
            char *message = ptlang_malloc(sizeof("Dereferenced expression must be a reference, but is a ") -
                                          1 + type_str_size - 1 + sizeof("."));
            char *message_ptr = message;
            memcpy(message_ptr, "Dereferenced expression must be a reference, but is a ",
                   sizeof("Dereferenced expression must be a reference, but is a ") - 1);
            message_ptr += sizeof("Dereferenced expression must be a reference, but is a ") - 1;
            ptlang_context_type_to_string(ptlang_rc_deref(unary).ast_type, message_ptr, ctx->type_scope);
            *message_ptr += type_str_size;
            memcpy(message_ptr, ".", 2);
            {
                arrput(*errors, ((ptlang_error){
                                    .type = PTLANG_ERROR_TYPE,
                                    .pos = ptlang_rc_deref(ptlang_rc_deref(unary).pos),
                                    .message = message,
                                }));
                break;
            }
            ptlang_rc_deref(exp).ast_type = ptlang_rc_add_ref(ptlang_context_unname_type(
                ptlang_rc_deref(ptlang_rc_deref(unary).ast_type).content.reference.type, ctx->type_scope));
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
        for (size_t j = i + 1; j < arrlenu(struct_defs); j++)
        {
            ptlang_ast_struct_def struct_def_i = struct_defs[i];
            ptlang_ast_struct_def struct_def_j = struct_defs[j];
            if (strcmp(ptlang_rc_deref(struct_def_i).name.name, ptlang_rc_deref(struct_def_j).name.name) == 0)
            {
                size_t name_len = strlen(ptlang_rc_deref(struct_defs[i]).name.name);

                size_t message_size = sizeof("Duplicate struct definition of ") + name_len;

                char *message = ptlang_malloc(message_size);
                memcpy(message, "Duplicate struct definition of ",
                       sizeof("Duplicate struct definition of ") - 1);
                memcpy(message + sizeof("Duplicate struct definition of ") - 1,
                       ptlang_rc_deref(struct_defs[i]).name.name, name_len);
                message[message_size - 1] = '\0';
                arrput(*errors, ((ptlang_error){
                                    .type = PTLANG_ERROR_STRUCT_MEMBER_DUPLICATION,
                                    .pos = ptlang_rc_deref(ptlang_rc_deref(struct_defs[i]).name.pos),
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
            arrpush(message_components, ptlang_rc_deref(struct_defs[cycles[j] - nodes]).name.name);
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

        char *message = ptlang_utils_build_str_from_char_arr(message_components);
        size_t message_size = strlen(message) + 1;
        arrfree(message_components);

        for (; i < j; i++)
        {
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_STRUCT_RECURSION,
                                .pos = ptlang_rc_deref(ptlang_rc_deref(struct_defs[cycles[i] - nodes]).pos),
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
    switch (ptlang_rc_deref(type).type)
    {
    case PTLANG_AST_TYPE_VOID:
        return true;
    case PTLANG_AST_TYPE_FUNCTION:
    {
        bool correct = ptlang_verify_type(ptlang_rc_deref(type).content.function.return_type, ctx, errors);
        for (size_t i = 0; i < arrlenu(ptlang_rc_deref(type).content.function.parameters); i++)
        {
            correct = ptlang_verify_type(ptlang_rc_deref(type).content.function.parameters[i], ctx, errors) &&
                      correct;
        }
        return correct;
    }
    case PTLANG_AST_TYPE_HEAP_ARRAY:
        return ptlang_verify_type(ptlang_rc_deref(type).content.heap_array.type, ctx, errors);
    case PTLANG_AST_TYPE_ARRAY:
        return ptlang_verify_type(ptlang_rc_deref(type).content.array.type, ctx, errors);
    case PTLANG_AST_TYPE_REFERENCE:
        return ptlang_verify_type(ptlang_rc_deref(type).content.reference.type, ctx, errors);
    case PTLANG_AST_TYPE_NAMED:
        if (shgeti(ctx->type_scope, ptlang_rc_deref(type).content.name) == -1)
        {
            size_t message_len =
                sizeof("The type  is not defined.") + strlen(ptlang_rc_deref(type).content.name);
            char *message = ptlang_malloc(message_len);
            snprintf(message, message_len, "The type %s is not defined.", ptlang_rc_deref(type).content.name);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_TYPE_UNDEFINED,
                                .pos = ptlang_rc_deref((ptlang_rc_deref(type).pos)),
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

    for (size_t i = 0; i < arrlenu(ptlang_rc_deref(ast_module).type_aliases); i++)
    {
        shput(ctx->type_scope, ptlang_rc_deref(ast_module).type_aliases[i].name.name,
              ((ptlang_context_type_scope_entry){.type = PTLANG_CONTEXT_TYPE_SCOPE_ENTRY_TYPEALIAS,
                                                 .value.ptlang_type =
                                                     ptlang_rc_deref(ast_module).type_aliases[i].type,
                                                 .index = i}));
        arrpush(nodes, (struct node_s){.index = -1});
    }

    // add structs to typescope

    for (size_t i = 0; i < arrlenu(ptlang_rc_deref(ast_module).struct_defs); i++)
    {
        shput(
            ctx->type_scope, ptlang_rc_deref(ptlang_rc_deref(ast_module).struct_defs[i]).name.name,
            ((ptlang_context_type_scope_entry){.type = PTLANG_CONTEXT_TYPE_SCOPE_ENTRY_STRUCT,
                                               .value.struct_def = ptlang_rc_deref(ast_module).struct_defs[i],
                                               .index = i}));
    }

    for (size_t i = 0; i < arrlenu(ptlang_rc_deref(ast_module).type_aliases); i++)
    {
        bool is_error_type = false;
        size_t *referenced_types = ptlang_verify_type_alias_get_referenced_types_from_ast_type(
            ptlang_rc_deref(ast_module).type_aliases[i].type, ctx, errors, &is_error_type);

        if (is_error_type)
            shget(ctx->type_scope, ptlang_rc_deref(ast_module).type_aliases[i].name.name).value.ptlang_type =
                NULL;

        for (size_t j = 0; j < arrlenu(referenced_types); j++)
        {
            arrpush(nodes[i].edges_to, &nodes[referenced_types[j]]);
        }

        arrfree(referenced_types);
    }

    ptlang_utils_graph_node **cycles = ptlang_utils_find_cycles(nodes);

    for (size_t i = 0; i < arrlenu(ptlang_rc_deref(ast_module).type_aliases); i++)
    {
        if (nodes[i].in_cycle)
        {
            shget(ctx->type_scope, ptlang_rc_deref(ast_module).type_aliases[i].name.name).value.ptlang_type =
                NULL;
        }
    }

    for (size_t i = 0, lowlink = 0; i < arrlenu(cycles);)
    {
        lowlink = cycles[i]->lowlink;
        char **message_components = NULL;
        size_t j;
        for (j = i; j < arrlenu(cycles) && lowlink == cycles[j]->lowlink; j++)
        {
            arrpush(message_components,
                    ptlang_rc_deref(ast_module).type_aliases[cycles[j] - nodes].name.name);
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

        char *message = ptlang_utils_build_str_from_char_arr(message_components);
        size_t message_size = strlen(message) + 1;
        arrfree(message_components);

        for (; i < j; i++)
        {
            arrput(
                *errors,
                ((ptlang_error){
                    .type = PTLANG_ERROR_TYPE_UNRESOLVABLE,
                    .pos = ptlang_rc_deref(ptlang_rc_deref(ast_module).type_aliases[cycles[i] - nodes].pos),
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

    for (size_t i = 0; i < arrlenu(ptlang_rc_deref(ast_module).type_aliases); i++)
    {
        shget(ctx->type_scope, ptlang_rc_deref(ast_module).type_aliases[i].name.name).value.ptlang_type =
            ptlang_context_unname_type(ptlang_rc_deref(ast_module).type_aliases[i].type, ctx->type_scope);
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
    switch (ptlang_rc_deref(ast_type).type)
    {
    case PTLANG_AST_TYPE_VOID:
        abort(); // syntax error
        break;
    case PTLANG_AST_TYPE_INTEGER:
    case PTLANG_AST_TYPE_FLOAT:
        break;
    case PTLANG_AST_TYPE_ARRAY:
        return ptlang_verify_type_alias_get_referenced_types_from_ast_type(
            ptlang_rc_deref(ast_type).content.array.type, ctx, errors, has_error);
    case PTLANG_AST_TYPE_HEAP_ARRAY:
        return ptlang_verify_type_alias_get_referenced_types_from_ast_type(
            ptlang_rc_deref(ast_type).content.heap_array.type, ctx, errors, has_error);
    case PTLANG_AST_TYPE_REFERENCE:
        return ptlang_verify_type_alias_get_referenced_types_from_ast_type(
            ptlang_rc_deref(ast_type).content.reference.type, ctx, errors, has_error);
    case PTLANG_AST_TYPE_NAMED:
    {
        ptrdiff_t index = shgeti(ctx->type_scope, ptlang_rc_deref(ast_type).content.name);
        if (index != -1)
        {
            if (ctx->type_scope[index].value.type == PTLANG_CONTEXT_TYPE_SCOPE_ENTRY_TYPEALIAS)
                arrput(referenced_types, ctx->type_scope[index].value.index);
        }
        else
        {
            size_t message_len =
                sizeof("The type  is not defined.") + strlen(ptlang_rc_deref(ast_type).content.name);
            char *message = ptlang_malloc(message_len);
            snprintf(message, message_len, "The type %s is not defined.",
                     ptlang_rc_deref(ast_type).content.name);
            arrput(*errors, ((ptlang_error){
                                .type = PTLANG_ERROR_TYPE_UNDEFINED,
                                .pos = ptlang_rc_deref(ptlang_rc_deref(ast_type).pos),
                                .message = message,
                            }));
            *has_error = true;
        }
        break;
    }
    case PTLANG_AST_TYPE_FUNCTION:
        referenced_types = ptlang_verify_type_alias_get_referenced_types_from_ast_type(
            ptlang_rc_deref(ast_type).content.function.return_type, ctx, errors, has_error);
        for (size_t i = 0; i < arrlenu(ptlang_rc_deref(ast_type).content.function.parameters); i++)
        {
            size_t *param_referenced_types = ptlang_verify_type_alias_get_referenced_types_from_ast_type(
                ptlang_rc_deref(ast_type).content.function.parameters[i], ctx, errors, has_error);
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

    for (size_t i = 0; i < arrlenu(ptlang_rc_deref(struct_def).members); i++)
    {
        ptlang_ast_type type = ptlang_rc_deref(ptlang_rc_deref(struct_def).members[i]).type;
        if (!ptlang_verify_type(type, ctx, errors))
            continue;
        type = ptlang_context_unname_type(type, ctx->type_scope);
        while (type != NULL && ptlang_rc_deref(type).type == PTLANG_AST_TYPE_ARRAY)
        {
            type = ptlang_rc_deref(type).content.array.type;
            type = ptlang_context_unname_type(type, ctx->type_scope);
        }

        if (type != NULL && ptlang_rc_deref(type).type == PTLANG_AST_TYPE_NAMED)
        {
            // => must be struct
            arrput(referenced_types, ptlang_rc_deref(type).content.name);
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
        return ptlang_rc_add_ref(type2);
    if (type2 == NULL)
        return ptlang_rc_add_ref(type1);

    if (ptlang_context_type_equals(type1, type2, ctx->type_scope))
        return ptlang_rc_add_ref(type1);

    if (ptlang_rc_deref(type1).type == PTLANG_AST_TYPE_REFERENCE &&
        ptlang_rc_deref(type2).type == PTLANG_AST_TYPE_REFERENCE)
    {
        if (ptlang_context_type_equals(ptlang_rc_deref(type1).content.reference.type,
                                       ptlang_rc_deref(type2).content.reference.type, ctx->type_scope))
        {
            return ptlang_ast_type_reference(ptlang_rc_deref(type1).content.reference.type,
                                             ptlang_rc_deref(type1).content.reference.writable &&
                                                 ptlang_rc_deref(type2).content.reference.writable,
                                             NULL);
        }
    }
    else if (ptlang_rc_deref(type1).type == PTLANG_AST_TYPE_FLOAT &&
             ptlang_rc_deref(type2).type == PTLANG_AST_TYPE_FLOAT)
    {
        return ptlang_ast_type_float(ptlang_rc_deref(type1).content.float_size >=
                                             ptlang_rc_deref(type2).content.float_size
                                         ? ptlang_rc_deref(type1).content.float_size
                                         : ptlang_rc_deref(type2).content.float_size,
                                     NULL);
    }
    else if (ptlang_rc_deref(type1).type == PTLANG_AST_TYPE_FLOAT &&
             ptlang_rc_deref(type2).type == PTLANG_AST_TYPE_INTEGER)
    {
        return ptlang_rc_add_ref(type1);
    }
    else if (ptlang_rc_deref(type1).type == PTLANG_AST_TYPE_INTEGER &&
             ptlang_rc_deref(type2).type == PTLANG_AST_TYPE_FLOAT)
    {
        return ptlang_rc_add_ref(type2);
    }
    else if (ptlang_rc_deref(type1).type == PTLANG_AST_TYPE_INTEGER &&
             ptlang_rc_deref(type2).type == PTLANG_AST_TYPE_INTEGER)
    {
        return ptlang_ast_type_integer(ptlang_rc_deref(type1).content.integer.is_signed ||
                                           ptlang_rc_deref(type2).content.integer.is_signed,
                                       ptlang_rc_deref(type1).content.integer.size >
                                               ptlang_rc_deref(type2).content.integer.size
                                           ? ptlang_rc_deref(type1).content.integer.size
                                           : ptlang_rc_deref(type2).content.integer.size,
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
    if ((ptlang_rc_deref(from).type == PTLANG_AST_TYPE_FLOAT &&
         ptlang_rc_deref(to).type == PTLANG_AST_TYPE_FLOAT) ||
        (ptlang_rc_deref(from).type == PTLANG_AST_TYPE_INTEGER &&
         (ptlang_rc_deref(to).type == PTLANG_AST_TYPE_FLOAT ||
          ptlang_rc_deref(to).type == PTLANG_AST_TYPE_INTEGER)))
    {
        if (ptlang_rc_deref(to).type == PTLANG_AST_EXP_INTEGER)
            return (ptlang_rc_deref(to).content.integer.size >= ptlang_rc_deref(from).content.integer.size);
        return true;
    }
    if (ptlang_rc_deref(to).type == PTLANG_AST_TYPE_REFERENCE &&
        ptlang_rc_deref(from).type == PTLANG_AST_TYPE_REFERENCE)
    {
        if (ptlang_context_type_equals(ptlang_rc_deref(to).content.reference.type,
                                       ptlang_rc_deref(from).content.reference.type, ctx->type_scope) &&
            !ptlang_rc_deref(to).content.reference.writable)
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
                            .pos = ptlang_rc_deref(pos),
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
                           ptlang_verify_type_to_string(ptlang_rc_deref(exp).ast_type, type_scope),
                           CONST_STR(after));

    ptlang_error error = (ptlang_error){
        .type = PTLANG_ERROR_TYPE, .pos = ptlang_rc_deref(ptlang_rc_deref(exp).pos), .message = message};
    return error;
}

static void ptlang_verify_exp_check_const(ptlang_ast_exp exp, ptlang_context *ctx, ptlang_error **errors)
{
    switch (ptlang_rc_deref(exp).type)
    {
    case PTLANG_AST_EXP_ASSIGNMENT:
    {
        arrput(*errors, ((ptlang_error){
                            .type = PTLANG_ERROR_NON_CONST_GLOBAL_INITIATOR,
                            .pos = ptlang_rc_deref(ptlang_rc_deref(exp).pos),
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
        ptlang_verify_exp_check_const(ptlang_rc_deref(exp).content.binary_operator.left_value, ctx, errors);
        ptlang_verify_exp_check_const(ptlang_rc_deref(exp).content.binary_operator.right_value, ctx, errors);
        break;
    }
    case PTLANG_AST_EXP_NEGATION:
    case PTLANG_AST_EXP_NOT:
    case PTLANG_AST_EXP_BITWISE_INVERSE:
    case PTLANG_AST_EXP_LENGTH:
    case PTLANG_AST_EXP_DEREFERENCE:
    {
        ptlang_verify_exp_check_const(ptlang_rc_deref(exp).content.unary_operator, ctx, errors);
        break;
    }
    case PTLANG_AST_EXP_FUNCTION_CALL:
    {
        arrput(*errors, ((ptlang_error){
                            .type = PTLANG_ERROR_NON_CONST_GLOBAL_INITIATOR,
                            .pos = ptlang_rc_deref(ptlang_rc_deref(exp).pos),
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
        for (size_t i = 0; i < arrlenu(ptlang_rc_deref(exp).content.struct_.members); i++)
        {
            ptlang_verify_exp_check_const(ptlang_rc_deref(exp).content.struct_.members[i].exp, ctx, errors);
        }
        break;
    }
    case PTLANG_AST_EXP_ARRAY:
    {
        for (size_t i = 0; i < arrlenu(ptlang_rc_deref(exp).content.array.values); i++)
        {
            ptlang_verify_exp_check_const(ptlang_rc_deref(exp).content.array.values[i], ctx, errors);
        }
        break;
    }
    case PTLANG_AST_EXP_TERNARY:
    {

        ptlang_verify_exp_check_const(ptlang_rc_deref(exp).content.ternary_operator.condition, ctx, errors);
        ptlang_verify_exp_check_const(ptlang_rc_deref(exp).content.ternary_operator.if_value, ctx, errors);
        ptlang_verify_exp_check_const(ptlang_rc_deref(exp).content.ternary_operator.else_value, ctx, errors);
        break;
    }
    case PTLANG_AST_EXP_CAST:
    {
        ptlang_verify_exp_check_const(ptlang_rc_deref(exp).content.cast.value, ctx, errors);
        break;
    }
    case PTLANG_AST_EXP_STRUCT_MEMBER:
    {
        ptlang_verify_exp_check_const(ptlang_rc_deref(exp).content.struct_member.struct_, ctx, errors);
        break;
    }
    case PTLANG_AST_EXP_ARRAY_ELEMENT:
    {
        ptlang_verify_exp_check_const(ptlang_rc_deref(exp).content.array_element.array, ctx, errors);
        ptlang_verify_exp_check_const(ptlang_rc_deref(exp).content.array_element.index, ctx, errors);
        break;
    }
    case PTLANG_AST_EXP_REFERENCE:
    {
        ptlang_verify_exp_check_const(ptlang_rc_deref(exp).content.reference.value, ctx, errors);
        break;
    }
    case PTLANG_AST_EXP_BINARY:
    {
        abort();
        break;
    }
    }
}

static ptlang_ast_exp ptlang_verify_eval(ptlang_ast_exp exp, enum ptlang_verify_eval_mode eval_mode,
                                         ptlang_utils_graph_node *node, ptlang_utils_graph_node *nodes,
                                         ptlang_verify_node_table node_table, ptlang_ast_module module,
                                         ptlang_context *ctx, ptlang_error **errors)
{
    ptlang_ast_exp substituted = NULL;
    ptlang_ast_exp evaluated = NULL;

    bool should_write = eval_mode == PTLANG_VERIFY_EVAL_FULLY_AND_WRITE;
    if (should_write)
    {
        eval_mode = PTLANG_VERIFY_EVAL_FULLY;

        ptlang_ast_exp evaled_indices =
            ptlang_verify_eval(exp, PTLANG_VERIFY_EVAL_INDICES, node, nodes, node_table, module, ctx, errors);
        ptlang_utils_graph_node *refers_to = ptlang_verify_get_node(evaled_indices, node_table, ctx);
        ptlang_rc_remove_ref(evaled_indices, ptlang_ast_exp_destroy);

        if (refers_to != NULL && ((ptlang_verify_node_info *)refers_to->data)->evaluated)

            evaluated = ptlang_rc_add_ref(((ptlang_verify_node_info *)refers_to->data)->val);
    }

    if (evaluated == NULL)
        switch (ptlang_rc_deref(exp).type)
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
            ptlang_ast_exp left_value =
                ptlang_verify_eval(ptlang_rc_deref(exp).content.binary_operator.left_value, eval_mode, node,
                                   nodes, node_table, module, ctx, errors);
            ptlang_ast_exp right_value =
                ptlang_verify_eval(ptlang_rc_deref(exp).content.binary_operator.right_value, eval_mode, node,
                                   nodes, node_table, module, ctx, errors);

            ptlang_rc_alloc(substituted);
            ptlang_rc_deref(substituted) = (struct ptlang_ast_exp_s){
                .type = ptlang_rc_deref(exp).type,
                .content.binary_operator =
                    {
                        .left_value = left_value,
                        .right_value = right_value,
                    },
                .pos = ptlang_rc_add_ref(ptlang_rc_deref(exp).pos),
                .ast_type = ptlang_rc_add_ref(ptlang_rc_deref(exp).ast_type),
            };

            break;
        }
        case PTLANG_AST_EXP_LENGTH:
        {
            unsigned pointer_size_in_bytes = LLVMPointerSize(ctx->target_data_layout);
            uint8_t *binary = ptlang_malloc_zero(pointer_size_in_bytes);

            if (ptlang_rc_deref(ptlang_rc_deref(ptlang_rc_deref(exp).content.unary_operator).ast_type).type ==
                PTLANG_AST_TYPE_ARRAY)
            {
                uint8_t bytes =
                    pointer_size_in_bytes <
                            sizeof(ptlang_rc_deref(
                                       ptlang_rc_deref(ptlang_rc_deref(exp).content.unary_operator).ast_type)
                                       .content.array.len)
                        ? pointer_size_in_bytes
                        : sizeof(ptlang_rc_deref(
                                     ptlang_rc_deref(ptlang_rc_deref(exp).content.unary_operator).ast_type)
                                     .content.array.len);
                for (uint8_t i = 0; i < bytes; i++)
                {
                    binary[i] =
                        ptlang_rc_deref(ptlang_rc_deref(ptlang_rc_deref(exp).content.unary_operator).ast_type)
                            .content.array.len >>
                        (i << 3);
                }
            }

            evaluated = ptlang_ast_exp_binary_new(binary, exp);
            break;
        }
        case PTLANG_AST_EXP_NEGATION:
        case PTLANG_AST_EXP_NOT:
        case PTLANG_AST_EXP_BITWISE_INVERSE:
        {
            ptlang_ast_exp operand =
                ptlang_verify_eval(ptlang_rc_deref(exp).content.unary_operator, eval_mode, node, nodes,
                                   node_table, module, ctx, errors);
            ptlang_rc_alloc(substituted);
            ptlang_rc_deref(substituted) = (struct ptlang_ast_exp_s){
                .type = ptlang_rc_deref(exp).type,
                .content.unary_operator = operand,
                .pos = ptlang_rc_add_ref(ptlang_rc_deref(exp).pos),
                .ast_type = ptlang_rc_add_ref(ptlang_rc_deref(exp).ast_type),
            };

            break;
        }
        case PTLANG_AST_EXP_DEREFERENCE:
        {
            ptlang_ast_exp reference =
                ptlang_verify_eval(ptlang_rc_deref(exp).content.unary_operator, PTLANG_VERIFY_EVAL_FULLY,
                                   node, nodes, node_table, module, ctx, errors);

            // recheck cycles
            ptlang_verify_build_graph(node, ptlang_rc_deref(reference).content.reference.value, false,
                                      node_table, ctx);

            ptlang_utils_graph_node *from = node;
            ptlang_utils_graph_node *to = ptlang_verify_get_node(exp, node_table, ctx);
            if (to != NULL)
                ptlang_verify_add_dependency(
                    &from, &to, ptlang_rc_deref(ptlang_rc_deref(reference).content.reference.value).ast_type,
                    node_table, true, ctx);

            ptlang_verify_check_cycles_in_global_defs(nodes, node_table, module, ctx, errors);

            evaluated = ptlang_verify_eval(ptlang_rc_deref(reference).content.reference.value, eval_mode,
                                           node, nodes, node_table, module, ctx, errors);
            break;
        }
        case PTLANG_AST_EXP_VARIABLE:
        {

            // assumes that cycles have already been checked
            ptlang_verify_node_table_value *info =
                shget(node_table, ptlang_rc_deref(exp).content.str_prepresentation);
            ptlang_utils_graph_node *var_node = ptlang_verify_get_node(exp, node_table, ctx);

            ptlang_ast_decl variable =
                ptlang_decl_list_find_last(ctx->scope, ptlang_rc_deref(exp).content.str_prepresentation);

            if (eval_mode == PTLANG_VERIFY_EVAL_INDICES)
                evaluated = ptlang_rc_add_ref(exp);
            else if (ptlang_rc_deref(variable).init == NULL)
                evaluated = ptlang_verify_get_default_value(ptlang_rc_deref(exp).ast_type, ctx);
            else if (((ptlang_verify_node_info *)var_node->data)->evaluated)
                evaluated = ptlang_rc_add_ref(((ptlang_verify_node_info *)var_node->data)->val);
            else if (eval_mode >= PTLANG_VERIFY_EVAL_FULLY)
            {
                ptlang_ast_exp _ =
                    ptlang_verify_eval(ptlang_rc_deref(variable).init, PTLANG_VERIFY_EVAL_FULLY_AND_WRITE,
                                       var_node, nodes, node_table, module, ctx, errors);
                ptlang_rc_remove_ref(_, ptlang_ast_exp_destroy);
                ptlang_rc_remove_ref(ptlang_rc_deref(variable).init, ptlang_ast_exp_destroy);
                ptlang_rc_deref(variable).init =
                    ptlang_rc_add_ref(((ptlang_verify_node_info *)var_node->data)->val);
                info->evaluated = true;
                evaluated = ptlang_rc_add_ref(ptlang_rc_deref(variable).init);
                should_write = false;
            }
            else
                evaluated = ptlang_verify_eval(ptlang_rc_deref(variable).init, eval_mode, node, nodes,
                                               node_table, module, ctx, errors);

            break;
        }
        case PTLANG_AST_EXP_INTEGER:
        case PTLANG_AST_EXP_FLOAT:
        {
            substituted = ptlang_rc_add_ref(exp);
            break;
        }
        case PTLANG_AST_EXP_STRUCT:
        {
            ptlang_ast_struct_def struct_def = ptlang_context_get_struct_def(
                ptlang_rc_deref(exp).content.struct_.type.name, ctx->type_scope);
            ptlang_ast_struct_member_list struct_members = NULL;
            ptlang_utils_graph_node *child_node = node + 1;
            for (size_t i = 0; i < arrlenu(ptlang_rc_deref(struct_def).members); i++)
            {
                size_t j;
                for (j = 0; j < arrlenu(ptlang_rc_deref(exp).content.struct_.members); j++)
                {
                    if (0 == strcmp(ptlang_rc_deref(exp).content.struct_.members[j].str.name,
                                    ptlang_rc_deref(ptlang_rc_deref(struct_def).members[i]).name.name))
                        break;
                }
                struct ptlang_ast_struct_member_s member = (struct ptlang_ast_struct_member_s){
                    .exp =
                        j < arrlenu(ptlang_rc_deref(exp).content.struct_.members)
                            ? (eval_mode >= PTLANG_VERIFY_EVAL_FULLY
                                   ? (should_write ? ptlang_verify_eval(
                                                         ptlang_rc_deref(exp).content.struct_.members[j].exp,
                                                         PTLANG_VERIFY_EVAL_FULLY_AND_WRITE, child_node,
                                                         nodes, node_table, module, ctx, errors)
                                                   : ptlang_verify_eval(
                                                         ptlang_rc_deref(exp).content.struct_.members[j].exp,
                                                         eval_mode, child_node, nodes, node_table, module,
                                                         ctx, errors))
                                   : ptlang_rc_add_ref(ptlang_rc_deref(exp).content.struct_.members[j].exp))
                            : ptlang_verify_get_default_value(
                                  ptlang_rc_deref(ptlang_rc_deref(struct_def).members[i]).type, ctx),
                    .str =
                        ptlang_ast_ident_copy(ptlang_rc_deref(ptlang_rc_deref(struct_def).members[i]).name),
                    .pos = NULL,
                };

                arrpush(struct_members, member);
                child_node += ptlang_verify_calc_node_count(
                    ptlang_rc_deref(ptlang_rc_deref(struct_def).members[i]).type, ctx->type_scope);
            }
            evaluated =
                ptlang_ast_exp_struct_new(ptlang_ast_ident_copy(ptlang_rc_deref(exp).content.struct_.type),
                                          struct_members, ptlang_rc_add_ref(ptlang_rc_deref(exp).pos));

            break;
        }
        case PTLANG_AST_EXP_ARRAY:
        {
            ptlang_ast_exp *array_values = NULL;
            // TODO make local
            ptlang_utils_graph_node *child_node = node + 1;
            size_t element_nodes =
                ptlang_verify_calc_node_count(ptlang_rc_deref(exp).content.array.type, ctx->type_scope);
            for (size_t i = 0; i < ptlang_rc_deref(ptlang_rc_deref(exp).ast_type).content.array.len; i++)
            {
                if (i < arrlenu(ptlang_rc_deref(exp).content.array.values))
                    arrpush(array_values,
                            eval_mode >= PTLANG_VERIFY_EVAL_FULLY
                                ? (should_write
                                       ? ptlang_verify_eval(ptlang_rc_deref(exp).content.array.values[i],
                                                            PTLANG_VERIFY_EVAL_FULLY_AND_WRITE, child_node,
                                                            nodes, node_table, module, ctx, errors)
                                       : ptlang_verify_eval(ptlang_rc_deref(exp).content.array.values[i],
                                                            eval_mode, child_node, nodes, node_table, module,
                                                            ctx, errors))
                                : ptlang_rc_add_ref(ptlang_rc_deref(exp).content.array.values[i]));
                else
                    arrpush(array_values,
                            ptlang_verify_get_default_value(
                                ptlang_rc_deref(ptlang_rc_deref(exp).content.array.type).content.array.type,
                                ctx));
                child_node += element_nodes;
            }
            evaluated =
                ptlang_ast_exp_array_new(ptlang_rc_add_ref(ptlang_context_unname_type(
                                             ptlang_rc_deref(exp).content.array.type, ctx->type_scope)),
                                         array_values, ptlang_rc_add_ref(ptlang_rc_deref(exp).pos));
            break;
        }
        case PTLANG_AST_EXP_TERNARY:
        {
            ptlang_ast_exp condition =
                ptlang_verify_eval(ptlang_rc_deref(exp).content.ternary_operator.condition,
                                   PTLANG_VERIFY_EVAL_FULLY, node, nodes, node_table, module, ctx, errors);
            if (ptlang_rc_deref(condition).content.binary[0] == 0)
            {
                ptlang_verify_build_graph(node, ptlang_rc_deref(exp).content.ternary_operator.else_value,
                                          false, node_table, ctx);
                ptlang_verify_check_cycles_in_global_defs(nodes, node_table, module, ctx, errors);

                ptlang_ast_exp else_value =
                    ptlang_verify_eval(ptlang_rc_deref(exp).content.ternary_operator.else_value, eval_mode,
                                       node, nodes, node_table, module, ctx, errors);
                evaluated = else_value;
            }
            else
            {
                ptlang_verify_build_graph(node, ptlang_rc_deref(exp).content.ternary_operator.if_value, false,
                                          node_table, ctx);
                ptlang_verify_check_cycles_in_global_defs(nodes, node_table, module, ctx, errors);

                ptlang_ast_exp if_value =
                    ptlang_verify_eval(ptlang_rc_deref(exp).content.ternary_operator.if_value, eval_mode,
                                       node, nodes, node_table, module, ctx, errors);
                evaluated = if_value;
            }

            break;
        }
        case PTLANG_AST_EXP_CAST:
        {
            ptlang_ast_exp value = ptlang_verify_eval(ptlang_rc_deref(exp).content.cast.value, eval_mode,
                                                      node, nodes, node_table, module, ctx, errors);
            ptlang_rc_alloc(substituted);
            ptlang_rc_deref(substituted) = (struct ptlang_ast_exp_s){
                .type = ptlang_rc_deref(exp).type,
                .content.cast.type = ptlang_rc_add_ref(ptlang_rc_deref(exp).content.cast.type),
                .content.cast.value = value,
                .pos = ptlang_rc_add_ref(ptlang_rc_deref(exp).pos),
                .ast_type = ptlang_rc_add_ref(ptlang_rc_deref(exp).ast_type),
            };
            break;
        }
        case PTLANG_AST_EXP_STRUCT_MEMBER:
        {
            ptlang_ast_exp struct_ref =
                ptlang_verify_eval(ptlang_rc_deref(exp).content.struct_member.struct_,
                                   PTLANG_VERIFY_EVAL_INDICES, node, nodes, node_table, module, ctx, errors);
            ptlang_ast_exp half_evaluated = ptlang_ast_exp_struct_member_new(
                struct_ref, ptlang_ast_ident_copy(ptlang_rc_deref(exp).content.struct_member.member_name),
                NULL);
            ptlang_rc_deref(half_evaluated).ast_type = ptlang_rc_add_ref(ptlang_rc_deref(exp).ast_type);

            if (eval_mode <= PTLANG_VERIFY_EVAL_INDICES)
            {
                evaluated = half_evaluated;
                break;
            }

            ptlang_ast_exp struct_ = ptlang_verify_eval(ptlang_rc_deref(exp).content.struct_member.struct_,
                                                        PTLANG_VERIFY_EVAL_PARTIALLY, node, nodes, node_table,
                                                        module, ctx, errors);

            for (size_t i = 0; i < arrlenu(ptlang_rc_deref(struct_).content.struct_.members); i++)
            {
                if (0 == strcmp(ptlang_rc_deref(struct_).content.struct_.members[i].str.name,
                                ptlang_rc_deref(exp).content.struct_member.member_name.name))
                {

                    ptlang_verify_build_graph(node, ptlang_rc_deref(struct_).content.struct_.members[i].exp,
                                              false, node_table, ctx);

                    ptlang_utils_graph_node *from = node;
                    ptlang_utils_graph_node *member_node =
                        ptlang_verify_get_node(half_evaluated, node_table, ctx);
                    ptlang_utils_graph_node *to = member_node;
                    if (member_node != NULL)
                        ptlang_verify_add_dependency(
                            &from, &to,
                            ptlang_rc_deref(ptlang_rc_deref(struct_).content.struct_.members[i].exp).ast_type,
                            node_table, false, ctx);
                    else
                        member_node = node;

                    ptlang_verify_check_cycles_in_global_defs(nodes, node_table, module, ctx, errors);

                    evaluated = ptlang_verify_eval(ptlang_rc_deref(struct_).content.struct_.members[i].exp,
                                                   should_write && (eval_mode == PTLANG_VERIFY_EVAL_FULLY)
                                                       ? PTLANG_VERIFY_EVAL_FULLY_AND_WRITE
                                                       : eval_mode,
                                                   member_node, nodes, node_table, module, ctx, errors);

                    should_write = false;

                    break;
                }
            }

            ptlang_rc_remove_ref(struct_, ptlang_ast_exp_destroy);
            ptlang_rc_remove_ref(half_evaluated, ptlang_ast_exp_destroy);

            if (evaluated == NULL)
            {
                evaluated = ptlang_verify_get_default_value(ptlang_rc_deref(exp).ast_type, ctx);
            }
            break;
        }
        // TODO other types
        case PTLANG_AST_EXP_ARRAY_ELEMENT:
        {
            ptlang_ast_exp index =
                ptlang_verify_eval(ptlang_rc_deref(exp).content.array_element.index, PTLANG_VERIFY_EVAL_FULLY,
                                   node, nodes, node_table, module, ctx, errors);

            enum LLVMByteOrdering byteOrdering = LLVMByteOrder(ctx->target_data_layout);

            size_t num = ptlang_verify_binary_to_unsigned(index, ctx);

            if ((ptlang_rc_deref(ptlang_rc_deref(index).ast_type).content.integer.is_signed &&
                 (ptlang_rc_deref(index)
                      .content.binary[byteOrdering == LLVMBigEndian
                                          ? 0
                                          : ptlang_eval_calc_byte_size(ptlang_rc_deref(index).ast_type) - 1] &
                  0x80)) ||
                num >= ptlang_rc_deref(
                           ptlang_rc_deref(ptlang_rc_deref(exp).content.array_element.array).ast_type)
                           .content.array.len)
            {
                // TODO throw error
            }

            ptlang_ast_exp array =
                ptlang_verify_eval(ptlang_rc_deref(exp).content.array_element.array,
                                   PTLANG_VERIFY_EVAL_INDICES, node, nodes, node_table, module, ctx, errors);

            evaluated = ptlang_ast_exp_array_element_new(array, index, NULL);
            ptlang_rc_deref(evaluated).ast_type = ptlang_rc_add_ref(ptlang_rc_deref(exp).ast_type);

            if (eval_mode <= PTLANG_VERIFY_EVAL_INDICES)
                break;

            ptlang_utils_graph_node *from = node;
            ptlang_utils_graph_node *element_node = ptlang_verify_get_node(evaluated, node_table, ctx);
            ptlang_utils_graph_node *to = element_node;
            if (element_node != NULL)
                ptlang_verify_add_dependency(&from, &to, ptlang_rc_deref(exp).ast_type, node_table, false,
                                             ctx);
            else
                element_node = node;

            ptlang_rc_remove_ref(evaluated, ptlang_ast_exp_destroy);
            array = ptlang_verify_eval(ptlang_rc_deref(exp).content.array_element.array,
                                       PTLANG_VERIFY_EVAL_PARTIALLY, node, nodes, node_table, module, ctx,
                                       errors);

            ptlang_verify_build_graph(element_node, ptlang_rc_deref(array).content.array.values[num], false,
                                      node_table, ctx);

            ptlang_verify_check_cycles_in_global_defs(nodes, node_table, module, ctx, errors);

            evaluated = ptlang_verify_eval(ptlang_rc_deref(array).content.array.values[num],
                                           should_write && (eval_mode == PTLANG_VERIFY_EVAL_FULLY)
                                               ? PTLANG_VERIFY_EVAL_FULLY_AND_WRITE
                                               : eval_mode,
                                           element_node, nodes, node_table, module, ctx, errors);

            should_write = false;

            ptlang_rc_remove_ref(array, ptlang_ast_exp_destroy);

            break;
        }
        case PTLANG_AST_EXP_BINARY:
        case PTLANG_AST_EXP_REFERENCE:
        {
            evaluated = ptlang_rc_add_ref(exp);
            break;
        }
        case PTLANG_AST_EXP_ASSIGNMENT:
        case PTLANG_AST_EXP_FUNCTION_CALL:
        {
            abort();
            break;
        }
        }

    if (eval_mode >= PTLANG_VERIFY_EVAL_FULLY)
    {

        if (evaluated == NULL)
            evaluated = ptlang_eval_const_exp(substituted, ctx);

        if (substituted != NULL)
        {
            // ptlang_rc_deref(substituted).pos = NULL;
            // ptlang_rc_deref(substituted).ast_type = NULL;
            ptlang_rc_remove_ref(substituted, ptlang_ast_exp_destroy);
        }

        if (should_write)
        {
            ptlang_verify_node_info *node_info = ((ptlang_verify_node_info *)node->data);
            if (!((ptlang_verify_node_info *)node->data)->evaluated)
            {
                ptlang_verify_set_init(node_info, ptlang_rc_add_ref(evaluated), ctx);
                arrfree(node->edges_to);
                node->edges_to = NULL;
            }
        }
    }
    else
    {
        if (evaluated == NULL)
            evaluated = substituted;
    }

    return evaluated;
}

ptlang_ast_exp ptlang_verify_get_default_value(ptlang_ast_type type, ptlang_context *ctx)
{
    if (type == NULL)
        return NULL;
    switch (ptlang_rc_deref(type).type)
    {
    case PTLANG_AST_TYPE_VOID:
    case PTLANG_AST_TYPE_HEAP_ARRAY:
    case PTLANG_AST_TYPE_FUNCTION:
    case PTLANG_AST_TYPE_REFERENCE:
        return NULL;
    default:
        break;
    }
    type = ptlang_context_unname_type(type, ctx->type_scope);

    ptlang_ast_exp default_value;
    ptlang_rc_alloc(default_value);
    switch (ptlang_rc_deref(type).type)
    {
    case PTLANG_AST_TYPE_INTEGER:
    {
        ptlang_rc_deref(default_value) = (struct ptlang_ast_exp_s){
            .type = PTLANG_AST_EXP_BINARY,
            .content.binary = ptlang_malloc_zero(((ptlang_rc_deref(type).content.integer.size - 1) >> 3) + 1),
        };
        break;
    }
    case PTLANG_AST_TYPE_FLOAT:
    {
        ptlang_rc_deref(default_value) = (struct ptlang_ast_exp_s){
            .type = PTLANG_AST_EXP_BINARY,
            .content.binary = ptlang_malloc_zero(ptlang_rc_deref(type).content.float_size >> 3),
        };
        break;
    }
    case PTLANG_AST_TYPE_ARRAY:
    {
        ptlang_ast_exp *array_values = NULL;
        for (size_t i = 0; i < ptlang_rc_deref(type).content.array.len; i++)
        {
            ptlang_ast_exp element_default =
                ptlang_verify_get_default_value(ptlang_rc_deref(type).content.array.type, ctx);
            arrpush(array_values, element_default);
        }
        ptlang_rc_deref(default_value) = (struct ptlang_ast_exp_s){
            .type = PTLANG_AST_EXP_ARRAY,
            .content.array.type = ptlang_rc_add_ref(
                ptlang_context_unname_type(ptlang_rc_deref(type).content.array.type, ctx->type_scope)),
            .content.array.values = array_values,
        };
        break;
    }
    case PTLANG_AST_TYPE_NAMED:
    {
        // Get struct def
        ptlang_ast_struct_def struct_def =
            ptlang_context_get_struct_def(ptlang_rc_deref(type).content.name, ctx->type_scope);

        // Init members
        ptlang_ast_struct_member_list struct_members = NULL;
        for (size_t i = 0; i < arrlenu(ptlang_rc_deref(struct_def).members); i++)
        {
            struct ptlang_ast_struct_member_s member_default = (struct ptlang_ast_struct_member_s){
                .exp = ptlang_verify_get_default_value(
                    ptlang_rc_deref(ptlang_rc_deref(struct_def).members[i]).type, ctx),
                .str = ptlang_ast_ident_copy(ptlang_rc_deref(ptlang_rc_deref(struct_def).members[i]).name),
                .pos = NULL,
            };

            arrpush(struct_members, member_default);
        }

        ptlang_rc_deref(default_value) = (struct ptlang_ast_exp_s){
            .type = PTLANG_AST_EXP_STRUCT,
            .content.struct_.type.name = NULL,
            .content.struct_.type.pos = NULL,
            .content.struct_.members = struct_members,
        };

        size_t str_size = strlen(ptlang_rc_deref(type).content.name) + 1;
        ptlang_rc_deref(default_value).content.struct_.type.name = ptlang_malloc(str_size);
        memcpy(ptlang_rc_deref(default_value).content.struct_.type.name, ptlang_rc_deref(type).content.name,
               str_size);
        break;
    }

    case PTLANG_AST_TYPE_VOID:
    case PTLANG_AST_TYPE_HEAP_ARRAY:
    case PTLANG_AST_TYPE_FUNCTION:
    case PTLANG_AST_TYPE_REFERENCE:
    {
        abort();
        break;
    }
    }
    ptlang_rc_deref(default_value).ast_type = ptlang_rc_add_ref(type);
    ptlang_rc_deref(default_value).pos = NULL;
    return default_value;
}

static size_t ptlang_verify_calc_node_count(ptlang_ast_type type, ptlang_context_type_scope *type_scope)
{
    type = ptlang_context_unname_type(type, type_scope);
    if (type == NULL)
        return 1;
    // the node of a array / struct itself refers to the the reference to this array / struct
    switch (ptlang_rc_deref(type).type)
    {

    case PTLANG_AST_TYPE_VOID:
        return 0;
    case PTLANG_AST_TYPE_ARRAY:
        return 1 + ptlang_rc_deref(type).content.array.len *
                       ptlang_verify_calc_node_count(ptlang_rc_deref(type).content.array.type, type_scope);
    case PTLANG_AST_TYPE_NAMED:
    {
        size_t nodes = 1;
        ptlang_ast_struct_def struct_def =
            ptlang_context_get_struct_def(ptlang_rc_deref(type).content.name, type_scope);
        for (size_t i = 0; i < arrlenu(ptlang_rc_deref(struct_def).members); i++)
        {
            nodes += ptlang_verify_calc_node_count(
                ptlang_rc_deref(ptlang_rc_deref(struct_def).members[i]).type, type_scope);
        }
        return nodes;
    }
    default:
        return 1;
    }
}

static void ptlang_verify_label_nodes(ptlang_ast_exp path_exp, ptlang_utils_graph_node **node,
                                      ptlang_context_type_scope *type_scope)
{
    ptlang_verify_node_info *node_info = ptlang_malloc(sizeof(ptlang_verify_node_info));
    *node_info = (ptlang_verify_node_info){
        .name = path_exp,
        .evaluated = false,
        .val = NULL,
    };
    (*node)->data = node_info;
    (*node)++;

    if (ptlang_rc_deref(path_exp).ast_type == NULL)
        return;

    switch (ptlang_rc_deref(ptlang_rc_deref(path_exp).ast_type).type)
    {
    case PTLANG_AST_TYPE_ARRAY:
    {
        for (size_t i = 0; i < ptlang_rc_deref(ptlang_rc_deref(path_exp).ast_type).content.array.len; i++)
        {
            char *index_str_repr = ptlang_malloc(21);
            snprintf(index_str_repr, 21, "%" PRIu64, i);
            ptlang_ast_exp index = ptlang_ast_exp_integer_new(index_str_repr, NULL);
            ptlang_ast_exp array_element = ptlang_ast_exp_array_element_new(
                ptlang_rc_add_ref(path_exp), index, ptlang_rc_add_ref(ptlang_rc_deref(path_exp).pos));
            ptlang_rc_deref(array_element).ast_type =
                ptlang_rc_add_ref(ptlang_rc_deref(ptlang_rc_deref(path_exp).ast_type).content.array.type);
            ptlang_verify_label_nodes(array_element, node, type_scope);
        }
        break;
    }

    case PTLANG_AST_TYPE_NAMED:
    {
        ptlang_ast_struct_def struct_def = ptlang_context_get_struct_def(
            ptlang_rc_deref(ptlang_rc_deref(path_exp).ast_type).content.name, type_scope);
        for (size_t i = 0; i < arrlenu(ptlang_rc_deref(struct_def).members); i++)
        {
            ptlang_ast_exp struct_member = ptlang_ast_exp_struct_member_new(
                ptlang_rc_add_ref(path_exp),
                ptlang_ast_ident_copy(ptlang_rc_deref(ptlang_rc_deref(struct_def).members[i]).name),
                ptlang_rc_add_ref(ptlang_rc_deref(path_exp).pos));
            ptlang_rc_deref(struct_member).ast_type =
                ptlang_rc_add_ref(ptlang_rc_deref(ptlang_rc_deref(struct_def).members[i]).type);
            ptlang_verify_label_nodes(struct_member, node, type_scope);
        }
        break;
    }
    default:
        break;
    }
}

static void ptlang_verify_eval_globals(ptlang_ast_module module, ptlang_context *ctx, ptlang_error **errors)
{
    ptlang_verify_node_table node_table = NULL;
    // ptlang_verify_decl_table decl_table = NULL;
    ptlang_utils_graph_node *nodes = NULL;

    size_t index = 0;
    for (size_t i = 0; i < arrlenu(ptlang_rc_deref(module).declarations); i++)
    {
        // shput(decl_table, ptlang_rc_deref(ptlang_rc_deref(module).declarations[i]).name.name,
        // ptlang_rc_deref(module).declarations[i]);
        size_t decl_node_count = ptlang_verify_calc_node_count(
            ptlang_rc_deref(ptlang_rc_deref(module).declarations[i]).type, ctx->type_scope);
        if (decl_node_count > 0)
        {
            ptlang_verify_node_table_value *info = ptlang_malloc(sizeof(ptlang_verify_node_table_value));
            *info = ((ptlang_verify_node_table_value){
                .node = (ptlang_utils_graph_node *)index,
                .evaluated = false,
            });
            shput(node_table, ptlang_rc_deref(ptlang_rc_deref(module).declarations[i]).name.name, info);
        }
        for (size_t j = 0; j < decl_node_count; j++)
        {
            arrpush(nodes, ((struct node_s){.index = -1}));
        }
        index += decl_node_count;
    }

    for (size_t i = 0; i < shlenu(node_table); i++)
    {
        node_table[i].value->node = &nodes[(size_t)node_table[i].value->node];
        // node_table[i].value->node = ((size_t)node_table[i].value->node) + nodes;
    }

    for (size_t i = 0; i < arrlenu(ptlang_rc_deref(module).declarations); i++)
    {
        ptlang_utils_graph_node *node =
            shget(node_table, ptlang_rc_deref(ptlang_rc_deref(module).declarations[i]).name.name)->node;
        size_t var_name_size = strlen(ptlang_rc_deref(ptlang_rc_deref(module).declarations[i]).name.name) + 1;
        char *var_name = ptlang_malloc(var_name_size);
        memcpy(var_name, ptlang_rc_deref(ptlang_rc_deref(module).declarations[i]).name.name, var_name_size);
        ptlang_ast_exp global_var = ptlang_ast_exp_variable_new(
            var_name, ptlang_rc_add_ref(ptlang_rc_deref(ptlang_rc_deref(module).declarations[i]).name.pos));
        ptlang_rc_deref(global_var).ast_type =
            ptlang_rc_add_ref(ptlang_rc_deref(ptlang_rc_deref(module).declarations[i]).type);
        ptlang_verify_label_nodes(global_var, &node, ctx->type_scope);
    }
    for (size_t i = 0; i < arrlenu(ptlang_rc_deref(module).declarations); i++)
    {
        ptlang_utils_graph_node *node =
            shget(node_table, ptlang_rc_deref(ptlang_rc_deref(module).declarations[i]).name.name)->node;
        if (ptlang_rc_deref(ptlang_rc_deref(module).declarations[i]).init != NULL)
            ptlang_verify_build_graph(node, ptlang_rc_deref(ptlang_rc_deref(module).declarations[i]).init,
                                      false, node_table, ctx);
    }

    ptlang_verify_check_cycles_in_global_defs(nodes, node_table, module, ctx, errors);

    for (size_t i = 0; i < arrlenu(ptlang_rc_deref(module).declarations); i++)
    {
        ptlang_verify_node_table_value *info =
            shget(node_table, ptlang_rc_deref(ptlang_rc_deref(module).declarations[i]).name.name);
        ptlang_utils_graph_node *node = info->node;
        ptlang_verify_node_info *node_info = (ptlang_verify_node_info *)node->data;
        if (ptlang_rc_deref(ptlang_rc_deref(module).declarations[i]).init != NULL && !info->evaluated)
        {
            if (!node_info->evaluated)
            {
                ptlang_ast_exp _ = ptlang_verify_eval(
                    ptlang_rc_deref(ptlang_rc_deref(module).declarations[i]).init,
                    PTLANG_VERIFY_EVAL_FULLY_AND_WRITE, node, nodes, node_table, module, ctx, errors);
                ptlang_rc_remove_ref(_, ptlang_ast_exp_destroy);
            }
            ptlang_rc_remove_ref(ptlang_rc_deref(ptlang_rc_deref(module).declarations[i]).init,
                                 ptlang_ast_exp_destroy);
            ptlang_rc_deref(ptlang_rc_deref(module).declarations[i]).init =
                ptlang_rc_add_ref(((ptlang_verify_node_info *)node->data)->val);
            info->evaluated = true;
        }
    }

    for (size_t i = 0; i < shlenu(node_table); i++)
    {
        ptlang_free(node_table[i].value);
    }
    shfree(node_table);

    for (size_t i = 0; i < arrlenu(nodes); i++)
    {
        if (nodes[i].data != NULL)
        {
            ptlang_rc_remove_ref(((ptlang_verify_node_info *)nodes[i].data)->name, ptlang_ast_exp_destroy);
            if (((ptlang_verify_node_info *)nodes[i].data)->val != NULL)
            {

                printf("init: %p, ast_type: %p\n", ((ptlang_verify_node_info *)nodes[i].data)->val,
                       ptlang_rc_deref(((ptlang_verify_node_info *)nodes[i].data)->val).ast_type);
                ptlang_rc_remove_ref(((ptlang_verify_node_info *)nodes[i].data)->val, ptlang_ast_exp_destroy);
            }
            ptlang_free(nodes[i].data);
        }
    }
    ptlang_utils_graph_free(nodes);
}

static void ptlang_verify_check_cycles_in_global_defs(ptlang_utils_graph_node *nodes,
                                                      ptlang_verify_node_table node_table,
                                                      ptlang_ast_module module, ptlang_context *ctx,
                                                      ptlang_error **errors)
{

    for (size_t i = 0; i < arrlenu(nodes); i++)
    {
        nodes[i].index = -1;
    }

    ptlang_utils_graph_node **cycles = ptlang_utils_find_cycles(nodes);

    // for (size_t i = 0; i < arrlenu(ptlang_rc_deref(module).declarations); i++)
    // {
    //     ptlang_verify_node_table_value *info = shget(node_table,
    //     ptlang_rc_deref(ptlang_rc_deref(module).declarations[i]).name.name); ptlang_utils_graph_node *node
    //     = info->node; if (ptlang_rc_deref(ptlang_rc_deref(module).declarations[i]).init == NULL ||
    //     node->in_cycle)
    //     {
    //         if (ptlang_rc_deref(ptlang_rc_deref(module).declarations[i]).init != NULL)
    //             ptlang_ast_exp_destroy(ptlang_rc_deref(ptlang_rc_deref(module).declarations[i]).init);
    //         ptlang_rc_deref(ptlang_rc_deref(module).declarations[i]).init =
    //             ptlang_verify_get_default_value(ptlang_rc_deref(ptlang_rc_deref(module).declarations[i]).type,
    //             ctx);
    //         info->evaluated = true;
    //     }
    // }

    for (size_t i = 0; i < arrlenu(nodes); i++)
    {
        ptlang_verify_node_info *node_info = (ptlang_verify_node_info *)nodes[i].data;
        if (nodes[i].in_cycle)
        {
            if (!node_info->evaluated)
            {
                node_info->evaluated = true;
                node_info->val =
                    ptlang_verify_get_default_value(ptlang_rc_deref(node_info->name).ast_type, ctx);
            }

            arrfree(nodes[i].edges_to);
            nodes[i].edges_to = NULL;
        }
    }

    for (size_t i = 0, lowlink = 0; i < arrlenu(cycles);)
    {
        lowlink = cycles[i]->lowlink;
        ptlang_utils_str *message_components = NULL;
        size_t j;
        for (j = i; j < arrlenu(cycles) && lowlink == cycles[j]->lowlink; j++)
        {
            arrpush(message_components,
                    ptlang_ast_exp_to_string(((ptlang_verify_node_info *)cycles[j]->data)->name));
            arrpush(message_components, CONST_STR(", "));
        }
        arrpop(message_components);

        if (j == i + 1)
        {
            arrins(message_components, 0, CONST_STR("The definition of the global variable "));
            arrpush(message_components, CONST_STR(" refers to itself."));
        }
        else
        {
            arrins(message_components, 0, CONST_STR("The global variables "));
            arrpush(message_components, CONST_STR(" form a circular definition."));
        }

        char *message = ptlang_utils_build_str_from_str_arr(message_components);
        size_t message_size = strlen(message) + 1;
        arrfree(message_components);

        for (; i < j; i++)
        {
            arrput(*errors,
                   ((ptlang_error){
                       .type = PTLANG_ERROR_CYCLE_IN_GLOBAL_INITIATORS,
                       .pos = ptlang_rc_deref(
                           ptlang_rc_deref((ptlang_ast_exp)((ptlang_verify_node_info *)cycles[i]->data)->name)
                               .pos),
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

    arrfree(cycles);
}

/**
 * @returns `false` if cycles have to be rechecked
 */
static bool ptlang_verify_build_graph(ptlang_utils_graph_node *node, ptlang_ast_exp exp, bool depends_on_ref,
                                      ptlang_verify_node_table node_table, ptlang_context *ctx)
{
    switch (ptlang_rc_deref(exp).type)
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
        ptlang_verify_build_graph(node, ptlang_rc_deref(exp).content.binary_operator.left_value,
                                  depends_on_ref, node_table, ctx);
        ptlang_verify_build_graph(node, ptlang_rc_deref(exp).content.binary_operator.right_value,
                                  depends_on_ref, node_table, ctx);
        break;
    case PTLANG_AST_EXP_NOT:
    case PTLANG_AST_EXP_BITWISE_AND:
    case PTLANG_AST_EXP_BITWISE_OR:
    case PTLANG_AST_EXP_BITWISE_XOR:
    case PTLANG_AST_EXP_BITWISE_INVERSE:
        ptlang_verify_build_graph(node, ptlang_rc_deref(exp).content.unary_operator, depends_on_ref,
                                  node_table, ctx);
        break;
    case PTLANG_AST_EXP_LENGTH:
        break;
    case PTLANG_AST_EXP_VARIABLE:
    {
        ptlang_utils_graph_node *to = ptlang_verify_get_node(exp, node_table, ctx);
        ptlang_verify_add_dependency(&node, &to, ptlang_rc_deref(exp).ast_type, node_table, depends_on_ref,
                                     ctx);
        break;
    }
    case PTLANG_AST_EXP_INTEGER:
    case PTLANG_AST_EXP_FLOAT:
        break;
    case PTLANG_AST_EXP_STRUCT:
        for (size_t i = 0; i < arrlenu(ptlang_rc_deref(exp).content.struct_.members); i++)
        {
            ptlang_ast_type struct_type = ptlang_rc_deref(exp).ast_type;
            ptlang_ast_struct_def struct_def =
                ptlang_context_get_struct_def(ptlang_rc_deref(struct_type).content.name, ctx->type_scope);

            size_t offset;
            if (!depends_on_ref)
                for (offset = 0; offset < arrlenu(ptlang_rc_deref(struct_def).members); offset++)
                {
                    if (strcmp(ptlang_rc_deref(ptlang_rc_deref(struct_def).members[offset]).name.name,
                               ptlang_rc_deref(exp).content.struct_.members[i].str.name) == 0)
                    {
                        ptlang_verify_build_graph(node + 1 + offset,
                                                  ptlang_rc_deref(exp).content.struct_.members[i].exp, false,
                                                  node_table, ctx);
                    }
                }
        }
        break;
    case PTLANG_AST_EXP_ARRAY:
        if (!depends_on_ref)
            for (size_t i = 0; i < arrlenu(ptlang_rc_deref(exp).content.array.values); i++)
            {
                ptlang_verify_build_graph(node + 1 + i, ptlang_rc_deref(exp).content.array.values[i], false,
                                          node_table, ctx);
            }
        break;
    case PTLANG_AST_EXP_TERNARY:
        ptlang_verify_build_graph(node, ptlang_rc_deref(exp).content.ternary_operator.condition, false,
                                  node_table, ctx);
        return false;
    case PTLANG_AST_EXP_CAST:
        ptlang_verify_build_graph(node, ptlang_rc_deref(exp).content.cast.value, depends_on_ref, node_table,
                                  ctx);
    case PTLANG_AST_EXP_STRUCT_MEMBER:
    {

        // ptlang_ast_struct_def struct_def =
        //             ptlang_context_get_struct_def(ptlang_rc_deref(ptlang_rc_deref(exp).content.struct_member.struct_).ast_type,
        //             ctx->type_scope);
        //         node++;
        //         for (size_t i = 0;
        //              strcmp(ptlang_rc_deref(struct_def->members[i]).name.name,
        //              ptlang_rc_deref(exp).content.struct_member.member_name.name); i++)
        //             node +=
        //             ptlang_verify_calc_node_count(ptlang_rc_deref(ptlang_rc_deref(struct_def).members[i]).type,
        //             ctx->type_scope);

        ptlang_verify_build_graph(node, ptlang_rc_deref(exp).content.struct_member.struct_, true, node_table,
                                  ctx);
        return false;
    }
    case PTLANG_AST_EXP_ARRAY_ELEMENT:
        ptlang_verify_build_graph(node, ptlang_rc_deref(exp).content.array_element.array, true, node_table,
                                  ctx);
        ptlang_verify_build_graph(node, ptlang_rc_deref(exp).content.array_element.index, false, node_table,
                                  ctx);
        return false;
    case PTLANG_AST_EXP_REFERENCE:
        ptlang_verify_build_graph(node, ptlang_rc_deref(exp).content.reference.value, true, node_table, ctx);
        break;
    case PTLANG_AST_EXP_DEREFERENCE:
        ptlang_verify_build_graph(node, ptlang_rc_deref(exp).content.unary_operator, false, node_table, ctx);
        return false;
    case PTLANG_AST_EXP_BINARY:
        break;
    case PTLANG_AST_EXP_ASSIGNMENT:
    case PTLANG_AST_EXP_FUNCTION_CALL:
    {
        abort();
        break;
    }
    }
    return true;
}

static void ptlang_verify_add_dependency(ptlang_utils_graph_node **from, ptlang_utils_graph_node **to,
                                         ptlang_ast_type type, ptlang_verify_node_table node_table,
                                         bool depends_on_ref, ptlang_context *ctx)
{
    type = ptlang_context_unname_type(type, ctx->type_scope);
    ptlang_ast_type other =
        ptlang_rc_deref((ptlang_ast_exp)(((ptlang_verify_node_info *)(*to)->data)->name)).ast_type;
    other = ptlang_context_unname_type(other, ctx->type_scope);

    arrpush((*from)->edges_to, *to);
    (*from)++;
    (*to)++;

    if (type != NULL && !depends_on_ref)
    {
        bool valid = other != NULL && ptlang_rc_deref(type).type == ptlang_rc_deref(other).type;
        switch (ptlang_rc_deref(type).type)
        {
        case PTLANG_AST_TYPE_ARRAY:
        {
            if (ptlang_rc_deref(type).content.array.len != ptlang_rc_deref(other).content.array.len)
                valid = false;
            if (valid)
            {
                for (size_t i = 0; i < ptlang_rc_deref(type).content.array.len; i++)
                {

                    ptlang_verify_add_dependency(from, to, ptlang_rc_deref(type).content.array.type,
                                                 node_table, false, ctx);
                }
            }
            else
                (*from) += ptlang_rc_deref(type).content.array.len;
            break;
        }
        case PTLANG_AST_TYPE_NAMED:
        {
            if (strcmp(ptlang_rc_deref(type).content.name, ptlang_rc_deref(other).content.name) != 0)
                valid = false;

            ptlang_ast_struct_def struct_def =
                ptlang_context_get_struct_def(ptlang_rc_deref(type).content.name, ctx->type_scope);

            if (valid)
            {
                for (size_t i = 0; i < arrlenu(ptlang_rc_deref(struct_def).members); i++)
                {
                    ptlang_verify_add_dependency(from, to,
                                                 ptlang_rc_deref(ptlang_rc_deref(struct_def).members[i]).type,
                                                 node_table, false, ctx);
                }
            }
            else
                (*from) += arrlenu(ptlang_rc_deref(struct_def).members);

            break;
        }
        default:
            break;
        }
    }

    // for (size_t i = 0; i < node_count; i++)
    // {
    //     arrpush(from[i].edges_to, &to[i]);
    // }
}

static ptlang_utils_graph_node *
ptlang_verify_get_node(ptlang_ast_exp exp, ptlang_verify_node_table node_table, ptlang_context *ctx)
{
    switch (ptlang_rc_deref(exp).type)
    {
    case PTLANG_AST_EXP_VARIABLE:
    {
        return shget(node_table, ptlang_rc_deref(exp).content.str_prepresentation)->node;
    }
    case PTLANG_AST_EXP_STRUCT_MEMBER:
    {
        ptlang_utils_graph_node *base_node =
            ptlang_verify_get_node(ptlang_rc_deref(exp).content.struct_member.struct_, node_table, ctx);
        if (base_node == NULL)
            return NULL;

        ptlang_ast_type struct_type =
            ptlang_rc_deref(ptlang_rc_deref(exp).content.struct_member.struct_).ast_type;
        ptlang_ast_struct_def struct_def =
            ptlang_context_get_struct_def(ptlang_rc_deref(struct_type).content.name, ctx->type_scope);

        size_t offset;
        for (offset = 0; offset < arrlenu(ptlang_rc_deref(struct_def).members); offset++)
        {
            if (strcmp(ptlang_rc_deref(ptlang_rc_deref(struct_def).members[offset]).name.name,
                       ptlang_rc_deref(exp).content.struct_member.member_name.name) == 0)
            {
                break;
            }
        }
        return base_node + offset + 1;
    }
    case PTLANG_AST_EXP_ARRAY_ELEMENT:
    {
        ptlang_utils_graph_node *base_node =
            ptlang_verify_get_node(ptlang_rc_deref(exp).content.array_element.array, node_table, ctx);
        if (base_node == NULL)
            return NULL;
        if (ptlang_rc_deref(ptlang_rc_deref(exp).content.array_element.index).type != PTLANG_AST_EXP_BINARY)
            return NULL;
        return base_node + 1 +
               ptlang_verify_binary_to_unsigned(ptlang_rc_deref(exp).content.array_element.index, ctx);
    }
    case PTLANG_AST_EXP_REFERENCE:
        return ptlang_verify_get_node(ptlang_rc_deref(exp).content.reference.value, node_table, ctx);
    case PTLANG_AST_EXP_DEREFERENCE:
        return ptlang_verify_get_node(ptlang_rc_deref(exp).content.unary_operator, node_table, ctx);
    default:
        return NULL;
    }
}

static size_t ptlang_verify_binary_to_unsigned(ptlang_ast_exp binary, ptlang_context *ctx)
{

    enum LLVMByteOrdering byteOrdering = LLVMByteOrder(ctx->target_data_layout);
    size_t num = 0;

    for (size_t i = sizeof(size_t); i > 0; i--)
    {
        num <<= 8;
        if (i <= ptlang_eval_calc_byte_size(ptlang_rc_deref(binary).ast_type))
            num += ptlang_rc_deref(binary)
                       .content.binary[byteOrdering == LLVMBigEndian
                                           ? ptlang_eval_calc_byte_size(ptlang_rc_deref(binary).ast_type) - i
                                           : i - 1];
    }

    return num;
}

static void ptlang_verify_set_init(ptlang_verify_node_info *node_info, ptlang_ast_exp init,
                                   ptlang_context *ctx)
{
    if (ptlang_rc_deref(ptlang_rc_deref(node_info->name).ast_type).type == PTLANG_AST_TYPE_ARRAY)
    {
        arrsetcap(ptlang_rc_deref(init).content.array.values,
                  ptlang_rc_deref(ptlang_rc_deref(node_info->name).ast_type).content.array.len);
        while (arrlenu(ptlang_rc_deref(init).content.array.values) <
               ptlang_rc_deref(ptlang_rc_deref(node_info->name).ast_type).content.array.len)
        {
            arrpush(ptlang_rc_deref(init).content.array.values,
                    ptlang_verify_get_default_value(
                        ptlang_rc_deref(ptlang_rc_deref(node_info->name).ast_type).content.array.type, ctx));
        }

        if (ptlang_rc_deref(init).ast_type != NULL)
        {
            ptlang_rc_remove_ref(ptlang_rc_deref(init).ast_type, ptlang_ast_type_destroy);
        }
        ptlang_rc_deref(init).ast_type = ptlang_rc_add_ref(ptlang_rc_deref(node_info->name).ast_type);
        printf("initptr: %p\n", init);
    }

    node_info->evaluated = true;
    node_info->val = init;
}
