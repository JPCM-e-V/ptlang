#include "ptlang_ast_impl.h"

ptlang_ast_module ptlang_ast_module_new()
{
    ptlang_ast_module module = malloc(sizeof(struct ptlang_ast_module_s));
    *module = (struct ptlang_ast_module_s){0};
    return module;
}

void ptlang_ast_module_add_function(ptlang_ast_module module, ptlang_ast_func function)
{
    module->function_count++;
    module->functions = realloc(module->functions, sizeof(ptlang_ast_func) * module->function_count);
    module->functions[module->function_count - 1] = function;
}

void ptlang_ast_module_add_declaration(ptlang_ast_module module, ptlang_ast_decl declaration)
{
    module->declaration_count++;
    module->declarations = realloc(module->declarations, sizeof(ptlang_ast_decl) * module->declaration_count);
    module->declarations[module->declaration_count - 1] = declaration;
}

void ptlang_ast_module_add_struct_def(ptlang_ast_module module, char *name, uint64_t member_count, char **member_names, ptlang_ast_type *member_types)
{
    // TODO
}

void ptlang_ast_module_add_type_alias(ptlang_ast_module module, char *name, ptlang_ast_type type)
{
    module->type_alias_count++;
    module->type_aliases = realloc(module->type_aliases, sizeof(struct ptlang_ast_module_type_alias_s) * module->type_alias_count);

    size_t name_size = strlen(name) + 1;
    module->type_aliases[module->type_alias_count - 1] = (struct ptlang_ast_module_type_alias_s){
        .name = malloc(name_size),
        .type = type,
    };
    memcpy(module->type_aliases[module->type_alias_count - 1].name, name, name_size);
}

ptlang_ast_func ptlang_ast_func_new(char *name, ptlang_ast_type return_type)
{
    ptlang_ast_func function = malloc(sizeof(struct ptlang_ast_func_s));

    size_t name_size = strlen(name) + 1;
    *function = (struct ptlang_ast_func_s){
        .name = malloc(name_size),
        .type.return_type = return_type,
    };
    memcpy(function->name, name, name_size);

    return function;
}

void ptlang_ast_func_add_parameter(ptlang_ast_func function, char *name, ptlang_ast_type type)
{
    function->type.parameter_count++;

    function->parameter_names = realloc(function->parameter_names, sizeof(char *) * function->type.parameter_count);
    size_t name_size = strlen(name) + 1;
    function->parameter_names[function->type.parameter_count - 1] = malloc(name_size);
    memcpy(function->parameter_names[function->type.parameter_count - 1], name, name_size);

    function->type.parameters = realloc(function->type.parameters, sizeof(ptlang_ast_type) * function->type.parameter_count);
    function->type.parameters[function->type.parameter_count - 1] = type;
}

ptlang_ast_decl ptlang_ast_decl_new(ptlang_ast_type type, char *name, bool writable)
{
    ptlang_ast_decl declaration = malloc(sizeof(struct ptlang_ast_decl_s));
    size_t name_size = strlen(name) + 1;
    *declaration = (struct ptlang_ast_decl_s){
        .type = type,
        .name = malloc(name_size),
        .writable = writable,
    };
    memcpy(declaration->name, name, name_size);
    return declaration;
}

ptlang_ast_type ptlang_ast_type_integer(bool is_signed, uint32_t size)
{
    ptlang_ast_type type = malloc(sizeof(struct ptlang_ast_type_s));
    *type = (struct ptlang_ast_type_s){
        .type = PTLANG_AST_TYPE_INTEGER,
        .content.integer = {
            .is_signed = is_signed,
            .size = size,
        },
    };
    return type;
}

ptlang_ast_type ptlang_ast_type_float(enum ptlang_ast_type_float_size size)
{
    ptlang_ast_type type = malloc(sizeof(struct ptlang_ast_type_s));
    *type = (struct ptlang_ast_type_s){
        .type = PTLANG_AST_TYPE_FLOAT,
        .content.float_size = size,
    };
    return type;
}

ptlang_ast_type ptlang_ast_type_function(ptlang_ast_type return_type, uint64_t parameter_count, ptlang_ast_type *parameters)
{
    ptlang_ast_type type = malloc(sizeof(struct ptlang_ast_type_s));
    *type = (struct ptlang_ast_type_s){
        .type = PTLANG_AST_TYPE_FUNCTION,
        .content.function = {
            .return_type = return_type,
            .parameter_count = parameter_count,
            .parameters = parameters,
        },
    };
    return type;
}

ptlang_ast_type ptlang_ast_type_function_new(ptlang_ast_type return_type)
{
    ptlang_ast_type type = malloc(sizeof(struct ptlang_ast_type_s));
    *type = (struct ptlang_ast_type_s){
        .type = PTLANG_AST_TYPE_FUNCTION,
        .content.function = {
            .return_type = return_type,
        },
    };
    return type;
}

void ptlang_ast_type_function_add_parameter(ptlang_ast_type function_type, ptlang_ast_type patameter)
{
    assert(function_type->type == PTLANG_AST_TYPE_FUNCTION);

    function_type->content.function.parameter_count++;
    function_type->content.function.parameters = realloc(function_type->content.function.parameters, sizeof(ptlang_ast_type) * function_type->content.function.parameter_count);
    function_type->content.function.parameters[function_type->content.function.parameter_count - 1] = patameter;
}

ptlang_ast_type ptlang_ast_type_heap_array(ptlang_ast_type element_type)
{
    ptlang_ast_type type = malloc(sizeof(struct ptlang_ast_type_s));
    *type = (struct ptlang_ast_type_s){
        .type = PTLANG_AST_TYPE_HEAP_ARRAY,
        .content.heap_array.type = element_type,
    };
    return type;
}
ptlang_ast_type ptlang_ast_type_array(ptlang_ast_type element_type, uint64_t len)
{
    ptlang_ast_type type = malloc(sizeof(struct ptlang_ast_type_s));
    *type = (struct ptlang_ast_type_s){
        .type = PTLANG_AST_TYPE_ARRAY,
        .content.array = {
            .type = element_type,
            .len = len,
        },
    };
    return type;
}
ptlang_ast_type ptlang_ast_type_reference(ptlang_ast_type element_type, bool writable)
{
    ptlang_ast_type type = malloc(sizeof(struct ptlang_ast_type_s));
    *type = (struct ptlang_ast_type_s){
        .type = PTLANG_AST_TYPE_REFERENCE,
        .content.reference = {
            .type = element_type,
            .writable = writable,
        },
    };
    return type;
}

ptlang_ast_exp ptlang_ast_exp_assignment_new(char *variable_name, ptlang_ast_exp exp)
{
    // TODO
}

#define BINARY_OP(lower, upper)                                                                        \
    ptlang_ast_exp ptlang_ast_exp_##lower##_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value) \
    {                                                                                                  \
        ptlang_ast_exp exp = malloc(sizeof(struct ptlang_ast_exp_s));                                  \
        *exp = (struct ptlang_ast_exp_s){                                                              \
            .type = PTLANG_AST_EXP_##upper,                                                            \
            .content.binary_operator = {                                                               \
                .left_value = left_value,                                                              \
                .right_value = right_value,                                                            \
            },                                                                                         \
        };                                                                                             \
        return exp;                                                                                    \
    }

#define UNARY_OP(lower, upper)                                        \
    ptlang_ast_exp ptlang_ast_exp_##lower##_new(ptlang_ast_exp value) \
    {                                                                 \
        ptlang_ast_exp exp = malloc(sizeof(struct ptlang_ast_exp_s)); \
        *exp = (struct ptlang_ast_exp_s){                             \
            .type = PTLANG_AST_EXP_##upper,                           \
            .content.unary_operator = value,                          \
        };                                                            \
        return exp;                                                   \
    }

BINARY_OP(addition, ADDITION)
BINARY_OP(subtraction, SUBTRACTION)
UNARY_OP(negation, NEGATION)
BINARY_OP(multiplication, MULTIPLICATION)
BINARY_OP(division, DIVISION)
BINARY_OP(modulo, MODULO)
BINARY_OP(equal, EQUAL)
BINARY_OP(greater, GREATER)
BINARY_OP(greater_equal, GREATER_EQUAL)
BINARY_OP(less, LESS)
BINARY_OP(less_equal, LESS_EQUAL)
BINARY_OP(left_shift, LEFT_SHIFT)
BINARY_OP(right_shift, RIGHT_SHIFT)
BINARY_OP(and, AND)
BINARY_OP(or, OR)
UNARY_OP(not, NOT)
BINARY_OP(bitwise_and, BITWISE_AND)
BINARY_OP(bitwise_or, BITWISE_OR)
BINARY_OP(bitwise_xor, BITWISE_XOR)
UNARY_OP(bitwise_inverse, BITWISE_INVERSE)

ptlang_ast_exp ptlang_ast_exp_function_call_new(char *function_name)
{
    ptlang_ast_exp exp = malloc(sizeof(ptlang_ast_exp));
    size_t name_size = strlen(function_name) + 1;
    *exp = (struct ptlang_ast_exp_s){
        .type = PTLANG_AST_EXP_FUNCTION_CALL,
        .content.function_call = {
            .parameter_count = 0,
            .function_name = malloc(name_size)}};
    memcpy(exp->content.function_call.function_name, function_name, name_size);
    return exp;
}
void ptlang_ast_exp_function_call_add_parameter(ptlang_ast_exp exp_function_call, ptlang_ast_exp parameter)
{
    assert(exp_function_call->type == PTLANG_AST_EXP_FUNCTION_CALL);

    exp_function_call->content.function_call.parameter_count++;
    exp_function_call->content.function_call.parameters = realloc(exp_function_call->content.function_call.parameters, sizeof(ptlang_ast_exp) * exp_function_call->content.function_call.parameter_count);
    exp_function_call->content.function_call.parameters[exp_function_call->content.function_call.parameter_count - 1] = parameter;
}

#define STR_REPR(lower, upper)                                                  \
    ptlang_ast_exp ptlang_ast_exp_##lower##_new(char *str_representation)       \
    {                                                                           \
        ptlang_ast_exp exp = malloc(sizeof(ptlang_ast_exp));                    \
        size_t str_size = strlen(str_representation) + 1;                       \
        *exp = (struct ptlang_ast_exp_s){                                       \
            .type = PTLANG_AST_EXP_##upper##,                                   \
            .content.str_prepresentation = malloc(str_size)};                   \
        memcpy(exp->content.str_prepresentation, str_representation, str_size); \
        return exp;                                                             \
    }

STR_REPR(variable, VARIABLE)
STR_REPR(integer, INTEGER)
STR_REPR(float, FLOAT)

ptlang_ast_exp ptlang_ast_exp_struct_new(ptlang_ast_type type)
{
    ptlang_ast_exp exp = malloc(sizeof(ptlang_ast_exp));
    *exp = (struct ptlang_ast_exp_s){
        .type = PTLANG_AST_EXP_STRUCT,
        .content.struct_ = {
            .type = type,
            .length = 0}};
    return exp;
}

void ptlang_ast_exp_struct_add_value(ptlang_ast_exp exp_struct, char *name, ptlang_ast_exp value)
{
    assert(exp_struct->type == PTLANG_AST_EXP_STRUCT);

    exp_struct->content.struct_.length++;
    exp_struct->content.struct_.values = realloc(exp_struct->content.struct_.values, sizeof(ptlang_ast_exp) * exp_struct->content.struct_.length);
    exp_struct->content.struct_.values[exp_struct->content.struct_.length - 1] = value;

    size_t name_size = strlen(name) + 1;
    exp_struct->content.struct_.names = realloc(exp_struct->content.struct_.names, sizeof(char *) * exp_struct->content.struct_.length);
    exp_struct->content.struct_.names[exp_struct->content.struct_.length - 1] = malloc(name_size);
    memcpy(exp_struct->content.struct_.names[exp_struct->content.struct_.length - 1], name, name_size);
}

ptlang_ast_exp ptlang_ast_exp_array_new(ptlang_ast_type type)
{
    ptlang_ast_exp exp = malloc(sizeof(ptlang_ast_exp));
    *exp = (struct ptlang_ast_exp_s){
        .type = PTLANG_AST_EXP_ARRAY,
        .content.array = {
            .type = type,
            .length = 0}};
    return exp;
}

void ptlang_ast_exp_array_add_value(ptlang_ast_exp exp_array, ptlang_ast_exp value)
{
    assert(exp_array->type == PTLANG_AST_EXP_ARRAY);

    exp_array->content.array.length++;
    exp_array->content.array.values = realloc(exp_array->content.array.values, sizeof(ptlang_ast_exp) * exp_array->content.array.length);
    exp_array->content.array.values[exp_array->content.array.length - 1] = value;
}

ptlang_ast_exp ptlang_ast_exp_heap_array_from_length_new(ptlang_ast_type type, ptlang_ast_exp length)
{
    ptlang_ast_exp exp = malloc(sizeof(ptlang_ast_exp));
    *exp = (struct ptlang_ast_exp_s){
        .type = PTLANG_AST_EXP_HEAP_ARRAY_FROM_LENGTH,
        .content.heap_array = {
            .type = type,
            .length = length}};
    return exp;
}

ptlang_ast_exp ptlang_ast_exp_ternary_operator_new(ptlang_ast_exp condition, ptlang_ast_exp if_value, ptlang_ast_exp else_value)
{
    ptlang_ast_exp exp = malloc(sizeof(ptlang_ast_exp));
    *exp = (struct ptlang_ast_exp_s){
        .type = PTLANG_AST_EXP_TERNARY,
        .content.ternary_operator = {
            .condition = condition,
            .if_value = if_value,
            .else_value = else_value}};
    return exp;
}
ptlang_ast_exp ptlang_ast_exp_cast_new(ptlang_ast_type type, ptlang_ast_exp value)
{
    ptlang_ast_exp exp = malloc(sizeof(ptlang_ast_exp));
    *exp = (struct ptlang_ast_exp_s){
        .type = PTLANG_AST_EXP_CAST,
        .content.cast = {
            .type = type,
            .value = value}};
    return exp;
}

ptlang_ast_stmt ptlang_ast_stmt_block_new()
{
    ptlang_ast_stmt stmt = malloc(sizeof(ptlang_ast_stmt));
    *stmt = (struct ptlang_ast_stmt_s){
        .type = PTLANG_AST_STMT_BLOCK,
        .content.block = {
            .stmt_count = 0}};
    return stmt;
}

void ptlang_ast_stmt_block_add_stmt(ptlang_ast_stmt block_stmt, ptlang_ast_stmt stmt)
{
    assert(block_stmt->type == PTLANG_AST_STMT_BLOCK);

    block_stmt->content.block.stmt_count++;
    block_stmt->content.block.stmts = realloc(block_stmt->content.block.stmts, sizeof(ptlang_ast_stmt) * block_stmt->content.block.stmt_count);
    block_stmt->content.block.stmts[block_stmt->content.block.stmt_count - 1] = stmt;
}
ptlang_ast_stmt ptlang_ast_stmt_expr_new(ptlang_ast_exp exp)
{
    ptlang_ast_stmt stmt = malloc(sizeof(ptlang_ast_stmt));
    *stmt = (struct ptlang_ast_stmt_s){
        .type = PTLANG_AST_STMT_EXP,
        .content.exp = exp};
    return stmt;
}
ptlang_ast_stmt ptlang_ast_stmt_decl_new(ptlang_ast_decl decl)
{
    ptlang_ast_stmt stmt = malloc(sizeof(ptlang_ast_stmt));
    *stmt = (struct ptlang_ast_stmt_s){
        .type = PTLANG_AST_STMT_DECL,
        .content.decl = decl};
    return stmt;
}
ptlang_ast_stmt ptlang_ast_stmt_if_new(ptlang_ast_exp condition, ptlang_ast_stmt if_stmt)
{
    ptlang_ast_stmt stmt = malloc(sizeof(ptlang_ast_stmt));
    *stmt = (struct ptlang_ast_stmt_s){
        .type = PTLANG_AST_STMT_IF,
        .content.control_flow = {
            .condition = condition,
            .stmt = if_stmt}};
    return stmt;
}
ptlang_ast_stmt ptlang_ast_stmt_if_else_new(ptlang_ast_exp condition, ptlang_ast_stmt if_stmt, ptlang_ast_stmt else_stmt)
{
    ptlang_ast_stmt stmt = malloc(sizeof(ptlang_ast_stmt));
    *stmt = (struct ptlang_ast_stmt_s){
        .type = PTLANG_AST_STMT_IF_ELSE,
        .content.control_flow2 = {
            .condition = condition,
            .stmt = {if_stmt, else_stmt}}};
    return stmt;
}
ptlang_ast_stmt ptlang_ast_stmt_while_new(ptlang_ast_exp condition, ptlang_ast_stmt loop_stmt)
{
    ptlang_ast_stmt stmt = malloc(sizeof(ptlang_ast_stmt));
    *stmt = (struct ptlang_ast_stmt_s){
        .type = PTLANG_AST_STMT_WHILE,
        .content.control_flow = {
            .condition = condition,
            .stmt = loop_stmt}};
    return stmt;
}
ptlang_ast_stmt ptlang_ast_stmt_return_new(ptlang_ast_exp return_value)
{
    ptlang_ast_stmt stmt = malloc(sizeof(ptlang_ast_stmt));
    *stmt = (struct ptlang_ast_stmt_s){
        .type = PTLANG_AST_STMT_RETURN,
        .content.exp = return_value};
    return stmt;
}
ptlang_ast_stmt ptlang_ast_stmt_ret_val_new(ptlang_ast_exp return_value) {
    ptlang_ast_stmt stmt = malloc(sizeof(ptlang_ast_stmt));
    *stmt = (struct ptlang_ast_stmt_s){
        .type = PTLANG_AST_STMT_RET_VAL,
        .content.exp = return_value};
    return stmt;
}
ptlang_ast_stmt ptlang_ast_stmt_break_new(uint64_t nesting_level) {
    ptlang_ast_stmt stmt = malloc(sizeof(ptlang_ast_stmt));
    *stmt = (struct ptlang_ast_stmt_s){
        .type = PTLANG_AST_STMT_BREAK,
        .content.nesting_level = nesting_level};
    return stmt;
}
ptlang_ast_stmt ptlang_ast_stmt_continue_new(uint64_t nesting_level) {
    ptlang_ast_stmt stmt = malloc(sizeof(ptlang_ast_stmt));
    *stmt = (struct ptlang_ast_stmt_s){
        .type = PTLANG_AST_STMT_CONTINUE,
        .content.nesting_level = nesting_level};
    return stmt;
}
