#include "ptlang_ast_impl.h"

ptlang_ast_struct_def ptlang_ast_struct_def_new(char *name)
{
    ptlang_ast_struct_def struct_def = malloc(sizeof(struct ptlang_ast_struct_def_s));
    *struct_def = (struct ptlang_ast_struct_def_s){
        .name = name,
    };
    return struct_def;
}

void ptlang_ast_struct_def_add_member(ptlang_ast_struct_def struct_def, char *name, ptlang_ast_type type)
{
    struct_def->member_count++;

    struct_def->member_names = realloc(struct_def->member_names, sizeof(char *) * struct_def->member_count);
    struct_def->member_names[struct_def->member_count - 1] = name;

    struct_def->member_types = realloc(struct_def->member_types, sizeof(ptlang_ast_type) * struct_def->member_count);
    struct_def->member_types[struct_def->member_count - 1] = type;
}

ptlang_ast_module ptlang_ast_module_new()
{
    ptlang_ast_module module = malloc(sizeof(struct ptlang_ast_module_s));
    *module = (struct ptlang_ast_module_s){
        .function_count = 0,
        .declaration_count = 0,
        .struct_def_count = 0,
        .type_alias_count = 0,
    };
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

void ptlang_ast_module_add_struct_def(ptlang_ast_module module, ptlang_ast_struct_def struct_def)
{
    module->struct_def_count++;
    module->struct_defs = realloc(module->struct_defs, sizeof(ptlang_ast_struct_def) * module->struct_def_count);
    module->struct_defs[module->struct_def_count - 1] = struct_def;
}

void ptlang_ast_module_add_type_alias(ptlang_ast_module module, char *name, ptlang_ast_type type)
{
    module->type_alias_count++;
    module->type_aliases = realloc(module->type_aliases, sizeof(struct ptlang_ast_module_type_alias_s) * module->type_alias_count);

    module->type_aliases[module->type_alias_count - 1] = (struct ptlang_ast_module_type_alias_s){
        .name = name,
        .type = type,
    };
}

ptlang_ast_func ptlang_ast_func_new(char *name, ptlang_ast_type return_type, ptlang_ast_func_parameter_list parameters, ptlang_ast_stmt stmt)
{
    ptlang_ast_func function = malloc(sizeof(struct ptlang_ast_func_s));
    *function = (struct ptlang_ast_func_s){
        .name = name,
        .return_type = return_type,
        .parameters = parameters,
        .stmt = stmt,
    };
    return function;
}

ptlang_ast_decl ptlang_ast_decl_new(ptlang_ast_type type, char *name, bool writable)
{
    ptlang_ast_decl declaration = malloc(sizeof(struct ptlang_ast_decl_s));
    *declaration = (struct ptlang_ast_decl_s){
        .type = type,
        .name = name,
        .writable = writable,
    };
    return declaration;
}

ptlang_ast_func_parameter_list ptlang_ast_func_parameter_list_new()
{
    ptlang_ast_func_parameter_list func_parameter_list = malloc(sizeof(struct ptlang_ast_func_parameter_list_s));
    *func_parameter_list = (struct ptlang_ast_func_parameter_list_s){
        .count = 0,
    };
    return func_parameter_list;
}

void ptlang_ast_func_parameter_list_add(ptlang_ast_func_parameter_list list, ptlang_ast_decl decl)
{
    list->count++;
    list->decls = realloc(list->decls, sizeof(ptlang_ast_decl) * list->count);
    list->decls[list->count - 1] = decl;
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

BINARY_OP(assignment, ASSIGNMENT)
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
    *exp = (struct ptlang_ast_exp_s){
        .type = PTLANG_AST_EXP_FUNCTION_CALL,
        .content.function_call = {
            .parameter_count = 0,
            .function_name = function_name}};
    return exp;
}
void ptlang_ast_exp_function_call_add_parameter(ptlang_ast_exp exp_function_call, ptlang_ast_exp parameter)
{
    assert(exp_function_call->type == PTLANG_AST_EXP_FUNCTION_CALL);
    exp_function_call->content.function_call.parameter_count++;
    exp_function_call->content.function_call.parameters = realloc(exp_function_call->content.function_call.parameters, sizeof(ptlang_ast_exp) * exp_function_call->content.function_call.parameter_count);
    exp_function_call->content.function_call.parameters[exp_function_call->content.function_call.parameter_count - 1] = parameter;
}

#define STR_REPR(lower, upper)                                            \
    ptlang_ast_exp ptlang_ast_exp_##lower##_new(char *str_representation) \
    {                                                                     \
        ptlang_ast_exp exp = malloc(sizeof(ptlang_ast_exp));              \
        *exp = (struct ptlang_ast_exp_s){                                 \
            .type = PTLANG_AST_EXP_##upper##,                             \
            .content.str_prepresentation = str_representation};           \
        return exp;                                                       \
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

    exp_struct->content.struct_.names = realloc(exp_struct->content.struct_.names, sizeof(char *) * exp_struct->content.struct_.length);
    exp_struct->content.struct_.names[exp_struct->content.struct_.length - 1] = name;
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

ptlang_ast_exp ptlang_ast_exp_struct_member_new(ptlang_ast_exp struct_, char *member_name)
{
    ptlang_ast_exp exp = malloc(sizeof(struct ptlang_ast_exp_s));
    *exp = (struct ptlang_ast_exp_s){
        .type = PTLANG_AST_EXP_STRUCT_MEMBER,
        .content.struct_member = {
            .struct_ = struct_,
            .member_name = member_name,
        },
    };
    return exp;
}
ptlang_ast_exp ptlang_ast_exp_array_element_new(ptlang_ast_exp array, ptlang_ast_exp index)
{
    ptlang_ast_exp exp = malloc(sizeof(struct ptlang_ast_exp_s));
    *exp = (struct ptlang_ast_exp_s){
        .type = PTLANG_AST_EXP_ARRAY_ELEMENT,
        .content.array_element = {
            .array = array,
            .index = index,
        },
    };
    return exp;
}

ptlang_ast_exp ptlang_ast_exp_reference_new(bool writable, ptlang_ast_exp value)
{
    ptlang_ast_exp exp = malloc(sizeof(struct ptlang_ast_exp_s));
    *exp = (struct ptlang_ast_exp_s){
        .type = PTLANG_AST_EXP_REFERENCE,
        .content.reference = {
            .writable = writable,
            .value = value,
        },
    };
    return exp;
}

UNARY_OP(dereference, DEREFERENCE)

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
ptlang_ast_stmt ptlang_ast_stmt_ret_val_new(ptlang_ast_exp return_value)
{
    ptlang_ast_stmt stmt = malloc(sizeof(ptlang_ast_stmt));
    *stmt = (struct ptlang_ast_stmt_s){
        .type = PTLANG_AST_STMT_RET_VAL,
        .content.exp = return_value};
    return stmt;
}
ptlang_ast_stmt ptlang_ast_stmt_break_new(uint64_t nesting_level)
{
    ptlang_ast_stmt stmt = malloc(sizeof(ptlang_ast_stmt));
    *stmt = (struct ptlang_ast_stmt_s){
        .type = PTLANG_AST_STMT_BREAK,
        .content.nesting_level = nesting_level};
    return stmt;
}
ptlang_ast_stmt ptlang_ast_stmt_continue_new(uint64_t nesting_level)
{
    ptlang_ast_stmt stmt = malloc(sizeof(ptlang_ast_stmt));
    *stmt = (struct ptlang_ast_stmt_s){
        .type = PTLANG_AST_STMT_CONTINUE,
        .content.nesting_level = nesting_level};
    return stmt;
}

void ptlang_ast_type_destroy(ptlang_ast_type type)
{
    switch (type->type)
    {
    case PTLANG_AST_TYPE_FUNCTION:
        ptlang_ast_type_destroy(type->content.function.return_type);
        for (uint64_t i = 0; i < type->content.function.parameter_count; i++)
        {
            ptlang_ast_type_destroy(type->content.function.parameters[i]);
        }
        free(type->content.function.parameters);
        break;
    case PTLANG_AST_TYPE_HEAP_ARRAY:
        ptlang_ast_type_destroy(type->content.heap_array.type);
        break;
    case PTLANG_AST_TYPE_ARRAY:
        ptlang_ast_type_destroy(type->content.array.type);
        break;
    case PTLANG_AST_TYPE_REFERENCE:
        ptlang_ast_type_destroy(type->content.reference.type);
        break;
    case PTLANG_AST_TYPE_STRUCT:
        free(type->content.structure);
        break;
    default:
        break;
    }
    free(type);
}

void ptlang_ast_stmt_destroy(ptlang_ast_stmt stmt)
{
    switch (stmt->type)
    {
    case PTLANG_AST_STMT_BLOCK:
        for (uint64_t i = 0; i < stmt->content.block.stmt_count; i++)
        {
            ptlang_ast_stmt_destroy(stmt->content.block.stmts[i]);
        }
        free(stmt->content.block.stmts);
        break;
    case PTLANG_AST_STMT_EXP:
    case PTLANG_AST_STMT_RETURN:
    case PTLANG_AST_STMT_RET_VAL:
        ptlang_ast_exp_destroy(stmt->content.exp);
        break;
    case PTLANG_AST_STMT_DECL:
        ptlang_ast_decl_destroy(stmt->content.decl);
        break;
    case PTLANG_AST_STMT_IF:
    case PTLANG_AST_STMT_WHILE:
        ptlang_ast_exp_destroy(stmt->content.control_flow.condition);
        ptlang_ast_stmt_destroy(stmt->content.control_flow.stmt);
        break;
    case PTLANG_AST_STMT_IF_ELSE:
        ptlang_ast_exp_destroy(stmt->content.control_flow2.condition);
        ptlang_ast_stmt_destroy(stmt->content.control_flow2.stmt[0]);
        ptlang_ast_stmt_destroy(stmt->content.control_flow2.stmt[1]);
        break;
    default:
        break;
    }
    free(stmt);
}

void ptlang_ast_module_destroy(ptlang_ast_module module)
{
    for (uint64_t i = 0; i < module->function_count; i++)
    {
        ptlang_ast_func_destroy(module->functions[i]);
    }
    free(module->functions);
    for (uint64_t i = 0; i < module->declaration_count; i++)
    {
        ptlang_ast_decl_destroy(module->declarations[i]);
    }
    free(module->declarations);
    for (uint64_t i = 0; i < module->struct_def_count; i++)
    {
        ptlang_ast_struct_def_destroy(module->struct_defs[i]);
    }
    free(module->struct_defs);
    for (uint64_t i = 0; i < module->type_alias_count; i++)
    {
        free(module->type_aliases[i].name);
        ptlang_ast_type_destroy(module->type_aliases[i].type);
    }
    free(module->type_aliases);

    free(module);
}

void ptlang_ast_func_destroy(ptlang_ast_func func)
{
    free(func->name);
    ptlang_ast_type_destroy(func->return_type);
    ptlang_ast_func_parameter_list_destroy(func->parameters);
    ptlang_ast_stmt_destroy(func->stmt);

    free(func);
}

void ptlang_ast_exp_destroy(ptlang_ast_exp exp)
{
    switch (exp->type)
    {
    case PTLANG_AST_EXP_ASSIGNMENT:
    case PTLANG_AST_EXP_ADDITION:
    case PTLANG_AST_EXP_SUBTRACTION:
    case PTLANG_AST_EXP_MULTIPLICATION:
    case PTLANG_AST_EXP_DIVISION:
    case PTLANG_AST_EXP_MODULO:
    case PTLANG_AST_EXP_EQUAL:
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
        ptlang_ast_exp_destroy(exp->content.binary_operator.left_value);
        ptlang_ast_exp_destroy(exp->content.binary_operator.right_value);
        break;
    case PTLANG_AST_EXP_NEGATION:
    case PTLANG_AST_EXP_NOT:
    case PTLANG_AST_EXP_BITWISE_INVERSE:
    case PTLANG_AST_EXP_DEREFERENCE:
        ptlang_ast_exp_destroy(exp->content.unary_operator);
        break;
    case PTLANG_AST_EXP_FUNCTION_CALL:
        free(exp->content.function_call.function_name);
        for (uint64_t i = 0; i < exp->content.function_call.parameter_count; i++)
        {
            ptlang_ast_exp_destroy(exp->content.function_call.parameters[i]);
        }
        free(exp->content.function_call.parameters);
        break;
    case PTLANG_AST_EXP_VARIABLE:
    case PTLANG_AST_EXP_INTEGER:
    case PTLANG_AST_EXP_FLOAT:
        free(exp->content.str_prepresentation);
        break;
    case PTLANG_AST_EXP_STRUCT:
        ptlang_ast_type_destroy(exp->content.struct_.type);
        for (uint64_t i = 0; i < exp->content.struct_.length; i++)
        {
            ptlang_ast_exp_destroy(exp->content.struct_.values[i]);
            free(exp->content.struct_.names[i]);
        }
        free(exp->content.struct_.values);
        free(exp->content.struct_.names);
        break;
    case PTLANG_AST_EXP_ARRAY:
        ptlang_ast_type_destroy(exp->content.array.type);
        for (uint64_t i = 0; i < exp->content.array.length; i++)
        {
            ptlang_ast_exp_destroy(exp->content.array.values[i]);
        }
        free(exp->content.array.values);
        break;
    case PTLANG_AST_EXP_HEAP_ARRAY_FROM_LENGTH:
        ptlang_ast_type_destroy(exp->content.heap_array.type);
        ptlang_ast_exp_destroy(exp->content.heap_array.length);
        break;
    case PTLANG_AST_EXP_TERNARY:
        ptlang_ast_exp_destroy(exp->content.ternary_operator.condition);
        ptlang_ast_exp_destroy(exp->content.ternary_operator.if_value);
        ptlang_ast_exp_destroy(exp->content.ternary_operator.else_value);
        break;
    case PTLANG_AST_EXP_CAST:
        ptlang_ast_type_destroy(exp->content.cast.type);
        ptlang_ast_exp_destroy(exp->content.cast.value);
        break;
    case PTLANG_AST_EXP_STRUCT_MEMBER:
        ptlang_ast_exp_destroy(exp->content.struct_member.struct_);
        free(exp->content.struct_member.member_name);
        break;
    case PTLANG_AST_EXP_ARRAY_ELEMENT:
        ptlang_ast_exp_destroy(exp->content.array_element.array);
        ptlang_ast_exp_destroy(exp->content.array_element.index);
        break;
    case PTLANG_AST_EXP_REFERENCE:
        ptlang_ast_exp_destroy(exp->content.reference.value);
        break;
    }

    free(exp);
}

void ptlang_ast_decl_destroy(ptlang_ast_decl decl)
{
    ptlang_ast_type_destroy(decl->type);
    free(decl->name);

    free(decl);
}

void ptlang_ast_struct_def_destroy(ptlang_ast_struct_def struct_def)
{
    free(struct_def->name);
    for (uint64_t i = 0; i < struct_def->member_count; i++)
    {
        free(struct_def->member_names[i]);
        ptlang_ast_type_destroy(struct_def->member_types[i]);
    }
    free(struct_def->member_names);
    free(struct_def->member_types);

    free(struct_def);
}

void ptlang_ast_func_parameter_list_destroy(ptlang_ast_func_parameter_list func_parameter_list)
{
    for (uint64_t i = 0; i < func_parameter_list->count; i++)
    {
        ptlang_ast_decl_destroy(func_parameter_list->decls[i]);
    }
    free(func_parameter_list->decls);
    free(func_parameter_list);
}