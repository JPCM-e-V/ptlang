#include "ptlang_ast_impl.h"

ptlang_ast_struct_def ptlang_ast_struct_def_new(ptlang_ast_ident name, ptlang_ast_decl *members,
                                                ptlang_ast_code_position pos)
{
    ptlang_ast_struct_def struct_def = ptlang_malloc(sizeof(struct ptlang_ast_struct_def_s));
    *struct_def = (struct ptlang_ast_struct_def_s){
        .name = name,
        .members = members,
        .pos = pos,
    };
    return struct_def;
}

ptlang_ast_module ptlang_ast_module_new(void)
{
    ptlang_ast_module module = ptlang_malloc(sizeof(struct ptlang_ast_module_s));
    *module = (struct ptlang_ast_module_s){
        .functions = NULL,
        .declarations = NULL,
        .struct_defs = NULL,
        .type_aliases = NULL,
    };
    return module;
}

void ptlang_ast_module_add_function(ptlang_ast_module module, ptlang_ast_func function)
{
    arrput(module->functions, function);
    // module->function_count++;
    // module->functions = ptlang_realloc(module->functions, sizeof(ptlang_ast_func) *
    // module->function_count); module->functions[module->function_count - 1] = function;
}

void ptlang_ast_module_add_declaration(ptlang_ast_module module, ptlang_ast_decl declaration)
{
    arrput(module->declarations, declaration);
    // module->declaration_count++;
    // module->declarations =
    //     ptlang_realloc(module->declarations, sizeof(ptlang_ast_decl) * module->declaration_count);
    // module->declarations[module->declaration_count - 1] = declaration;
}

void ptlang_ast_module_add_struct_def(ptlang_ast_module module, ptlang_ast_struct_def struct_def)
{
    arrput(module->struct_defs, struct_def);
    // module->struct_def_count++;
    // module->struct_defs =
    //     ptlang_realloc(module->struct_defs, sizeof(ptlang_ast_struct_def) * module->struct_def_count);
    // module->struct_defs[module->struct_def_count - 1] = struct_def;
}

void ptlang_ast_module_add_type_alias(ptlang_ast_module module, ptlang_ast_ident name, ptlang_ast_type type,
                                      ptlang_ast_code_position pos)
{
    arrput(module->type_aliases, ((struct ptlang_ast_module_type_alias_s){
                                     .name = name,
                                     .type = type,
                                     .pos = pos,
                                 }));
    // module->type_alias_count++;
    // module->type_aliases = ptlang_realloc(
    //     module->type_aliases, sizeof(struct ptlang_ast_module_type_alias_s) * module->type_alias_count);

    // module->type_aliases[module->type_alias_count - 1] = (struct ptlang_ast_module_type_alias_s){
    //     .name = name,
    //     .type = type,
    // };
}

ptlang_ast_func ptlang_ast_func_new(ptlang_ast_ident name, ptlang_ast_type return_type,
                                    ptlang_ast_decl *parameters, ptlang_ast_stmt stmt, bool export)
{
    ptlang_ast_func function = ptlang_malloc(sizeof(struct ptlang_ast_func_s));
    *function = (struct ptlang_ast_func_s){
        .name = name,
        .return_type = return_type,
        .parameters = parameters,
        .stmt = stmt,
        .export = export,
    };
    return function;
}

ptlang_ast_decl ptlang_ast_decl_new(ptlang_ast_type type, ptlang_ast_ident name, bool writable,
                                    ptlang_ast_code_position pos)
{
    ptlang_ast_decl declaration = ptlang_malloc(sizeof(struct ptlang_ast_decl_s));
    *declaration = (struct ptlang_ast_decl_s){
        .type = type,
        .name = name,
        .writable = writable,
        .export = false,
        .init = 0,
        .pos = pos,
    };
    return declaration;
}

void ptlang_ast_decl_set_init(ptlang_ast_decl decl, ptlang_ast_exp init) { decl->init = init; }

void ptlang_ast_decl_set_export(ptlang_ast_decl decl, bool export) { decl->export = export; }

void ptlang_ast_func_set_export(ptlang_ast_func func, bool export) { func->export = export; }

// ptlang_ast_decl_list ptlang_ast_decl_list_new(void)
// {
//     ptlang_ast_decl_list decl_list = ptlang_malloc(sizeof(struct ptlang_ast_decl_list_s));
//     *decl_list = (struct ptlang_ast_decl_list_s){
//         .count = 0,
//     };
//     return decl_list;
// }

// void ptlang_ast_decl_list_add(ptlang_ast_decl_list list, ptlang_ast_decl decl)
// {
//     list->count++;
//     list->decls = ptlang_realloc(list->decls, sizeof(ptlang_ast_decl) * list->count);
//     list->decls[list->count - 1] = decl;
// }

// ptlang_ast_type_list ptlang_ast_type_list_new(void)
// {
//     ptlang_ast_type_list type_list = ptlang_malloc(sizeof(struct ptlang_ast_type_list_s));
//     *type_list = (struct ptlang_ast_type_list_s){
//         .count = 0,
//     };
//     return type_list;
// }

// void ptlang_ast_type_list_add(ptlang_ast_type_list list, ptlang_ast_type type)
// {
//     list->count++;
//     list->types = ptlang_realloc(list->types, sizeof(ptlang_ast_type) * list->count);
//     list->types[list->count - 1] = type;
// }

// ptlang_ast_exp_list ptlang_ast_exp_list_new(void)
// {
//     ptlang_ast_exp_list exp_list = ptlang_malloc(sizeof(struct ptlang_ast_exp_list_s));
//     *exp_list = (struct ptlang_ast_exp_list_s){
//         .count = 0,
//     };
//     return exp_list;
// }

// void ptlang_ast_exp_list_add(ptlang_ast_exp_list list, ptlang_ast_exp exp)
// {
//     list->count++;
//     list->exps = ptlang_realloc(list->exps, sizeof(ptlang_ast_exp) * list->count);
//     list->exps[list->count - 1] = exp;
// }

// ptlang_ast_str_exp_list ptlang_ast_str_exp_list_new(void)
// {
//     ptlang_ast_str_exp_list str_exp_list = ptlang_malloc(sizeof(struct ptlang_ast_str_exp_list_s));
//     *str_exp_list = (struct ptlang_ast_str_exp_list_s){
//         .count = 0,
//     };
//     return str_exp_list;
// }

// void ptlang_ast_str_exp_list_add(ptlang_ast_str_exp_list list, char *str, ptlang_ast_exp exp)
// {
//     list->count++;
//     list->str_exps = ptlang_realloc(list->str_exps, sizeof(struct ptlang_ast_str_exp_s) * list->count);
//     list->str_exps[list->count - 1] = (struct ptlang_ast_str_exp_s){
//         .str = str,
//         .exp = exp,
//     };
// }

void ptlang_ast_struct_member_list_add(ptlang_ast_struct_member_list *list, ptlang_ast_ident str,
                                       ptlang_ast_exp exp, ptlang_ast_code_position pos)
{
    arrput(*list, ((struct ptlang_ast_struct_member_s){
                      .str = str,
                      .exp = exp,
                      .pos = pos,
                  }));
}

ptlang_ast_type ptlang_ast_type_void(ptlang_ast_code_position pos)
{
    ptlang_ast_type type = ptlang_malloc(sizeof(struct ptlang_ast_type_s));
    *type = (struct ptlang_ast_type_s){
        .type = PTLANG_AST_TYPE_VOID,
        .pos = pos,
    };
    return type;
}

ptlang_ast_type ptlang_ast_type_integer(bool is_signed, uint32_t size, ptlang_ast_code_position pos)
{
    ptlang_ast_type type = ptlang_malloc(sizeof(struct ptlang_ast_type_s));
    *type = (struct ptlang_ast_type_s){
        .type = PTLANG_AST_TYPE_INTEGER,
        .content.integer =
            {
                .is_signed = is_signed,
                .size = size,
            },
        .pos = pos,
    };
    return type;
}

ptlang_ast_type ptlang_ast_type_float(enum ptlang_ast_type_float_size size, ptlang_ast_code_position pos)
{
    ptlang_ast_type type = ptlang_malloc(sizeof(struct ptlang_ast_type_s));
    *type = (struct ptlang_ast_type_s){
        .type = PTLANG_AST_TYPE_FLOAT,
        .content.float_size = size,
        .pos = pos,
    };
    return type;
}

ptlang_ast_type ptlang_ast_type_function(ptlang_ast_type return_type, ptlang_ast_type *parameters,
                                         ptlang_ast_code_position pos)
{
    ptlang_ast_type type = ptlang_malloc(sizeof(struct ptlang_ast_type_s));
    *type = (struct ptlang_ast_type_s){
        .type = PTLANG_AST_TYPE_FUNCTION,
        .content.function =
            {
                .return_type = return_type,
                .parameters = parameters,
            },
        .pos = pos,
    };
    return type;
}

ptlang_ast_type ptlang_ast_type_heap_array(ptlang_ast_type element_type, ptlang_ast_code_position pos)
{
    ptlang_ast_type type = ptlang_malloc(sizeof(struct ptlang_ast_type_s));
    *type = (struct ptlang_ast_type_s){
        .type = PTLANG_AST_TYPE_HEAP_ARRAY,
        .content.heap_array.type = element_type,
        .pos = pos,
    };
    return type;
}
ptlang_ast_type ptlang_ast_type_array(ptlang_ast_type element_type, uint64_t len,
                                      ptlang_ast_code_position pos)
{
    ptlang_ast_type type = ptlang_malloc(sizeof(struct ptlang_ast_type_s));
    *type = (struct ptlang_ast_type_s){
        .type = PTLANG_AST_TYPE_ARRAY,
        .content.array =
            {
                .type = element_type,
                .len = len,
            },
        .pos = pos,
    };
    return type;
}
ptlang_ast_type ptlang_ast_type_reference(ptlang_ast_type element_type, bool writable,
                                          ptlang_ast_code_position pos)
{
    ptlang_ast_type type = ptlang_malloc(sizeof(struct ptlang_ast_type_s));
    *type = (struct ptlang_ast_type_s){
        .type = PTLANG_AST_TYPE_REFERENCE,
        .content.reference =
            {
                .type = element_type,
                .writable = writable,
            },
        .pos = pos,
    };
    return type;
}

ptlang_ast_type ptlang_ast_type_named(char *name, ptlang_ast_code_position pos)
{
    ptlang_ast_type type = ptlang_malloc(sizeof(struct ptlang_ast_type_s));
    *type = (struct ptlang_ast_type_s){
        .type = PTLANG_AST_TYPE_NAMED,
        .content.name = name,
        .pos = pos,
    };
    return type;
}

#define BINARY_OP(lower, upper)                                                                              \
    ptlang_ast_exp ptlang_ast_exp_##lower##_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value,       \
                                                ptlang_ast_code_position pos)                                \
    {                                                                                                        \
        ptlang_ast_exp exp = ptlang_malloc(sizeof(struct ptlang_ast_exp_s));                                 \
        *exp = (struct ptlang_ast_exp_s){                                                                    \
            .type = PTLANG_AST_EXP_##upper,                                                                  \
            .content.binary_operator =                                                                       \
                {                                                                                            \
                    .left_value = left_value,                                                                \
                    .right_value = right_value,                                                              \
                },                                                                                           \
            .pos = pos,                                                                                      \
        };                                                                                                   \
        return exp;                                                                                          \
    }

#define UNARY_OP(lower, upper)                                                                               \
    ptlang_ast_exp ptlang_ast_exp_##lower##_new(ptlang_ast_exp value, ptlang_ast_code_position pos)          \
    {                                                                                                        \
        ptlang_ast_exp exp = ptlang_malloc(sizeof(struct ptlang_ast_exp_s));                                 \
        *exp = (struct ptlang_ast_exp_s){                                                                    \
            .type = PTLANG_AST_EXP_##upper,                                                                  \
            .content.unary_operator = value,                                                                 \
            .pos = pos,                                                                                      \
        };                                                                                                   \
        return exp;                                                                                          \
    }

BINARY_OP(assignment, ASSIGNMENT)
BINARY_OP(addition, ADDITION)
BINARY_OP(subtraction, SUBTRACTION)
UNARY_OP(negation, NEGATION)
BINARY_OP(multiplication, MULTIPLICATION)
BINARY_OP(division, DIVISION)
BINARY_OP(modulo, MODULO)
BINARY_OP(remainder, REMAINDER)
BINARY_OP(equal, EQUAL)
BINARY_OP(not_equal, NOT_EQUAL)
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
UNARY_OP(length, LENGTH)

ptlang_ast_exp ptlang_ast_exp_function_call_new(ptlang_ast_exp function, ptlang_ast_exp *parameters,
                                                ptlang_ast_code_position pos)
{
    ptlang_ast_exp exp = ptlang_malloc(sizeof(struct ptlang_ast_exp_s));
    *exp = (struct ptlang_ast_exp_s){
        .type = PTLANG_AST_EXP_FUNCTION_CALL,
        .content.function_call =
            {
                .function = function,
                .parameters = parameters,
            },
        .pos = pos,
    };
    return exp;
}

#define STR_REPR(lower, upper)                                                                               \
    ptlang_ast_exp ptlang_ast_exp_##lower##_new(char *str_representation, ptlang_ast_code_position pos)      \
    {                                                                                                        \
        ptlang_ast_exp exp = ptlang_malloc(sizeof(struct ptlang_ast_exp_s));                                 \
        *exp = (struct ptlang_ast_exp_s){                                                                    \
            .type = PTLANG_AST_EXP_##upper,                                                                  \
            .content.str_prepresentation = str_representation,                                               \
            .pos = pos,                                                                                      \
        };                                                                                                   \
        return exp;                                                                                          \
    }

STR_REPR(variable, VARIABLE)
STR_REPR(integer, INTEGER)
STR_REPR(float, FLOAT)

ptlang_ast_exp ptlang_ast_exp_struct_new(ptlang_ast_ident type, ptlang_ast_struct_member_list members,
                                         ptlang_ast_code_position pos)
{
    ptlang_ast_exp exp = ptlang_malloc(sizeof(struct ptlang_ast_exp_s));
    *exp = (struct ptlang_ast_exp_s){
        .type = PTLANG_AST_EXP_STRUCT,
        .content.struct_ =
            {
                .type = type,
                .members = members,
            },
        .pos = pos,
    };
    return exp;
}

ptlang_ast_exp ptlang_ast_exp_array_new(ptlang_ast_type type, ptlang_ast_exp *values,
                                        ptlang_ast_code_position pos)
{
    ptlang_ast_exp exp = ptlang_malloc(sizeof(struct ptlang_ast_exp_s));
    *exp = (struct ptlang_ast_exp_s){
        .type = PTLANG_AST_EXP_ARRAY,
        .content.array =
            {
                .type = type,
                .values = values,
            },
        .pos = pos,
    };
    return exp;
}

ptlang_ast_exp ptlang_ast_exp_ternary_operator_new(ptlang_ast_exp condition, ptlang_ast_exp if_value,
                                                   ptlang_ast_exp else_value, ptlang_ast_code_position pos)
{
    ptlang_ast_exp exp = ptlang_malloc(sizeof(struct ptlang_ast_exp_s));
    *exp = (struct ptlang_ast_exp_s){
        .type = PTLANG_AST_EXP_TERNARY,
        .content.ternary_operator =
            {
                .condition = condition,
                .if_value = if_value,
                .else_value = else_value,
            },
        .pos = pos,
    };
    return exp;
}
ptlang_ast_exp ptlang_ast_exp_cast_new(ptlang_ast_type type, ptlang_ast_exp value,
                                       ptlang_ast_code_position pos)
{
    ptlang_ast_exp exp = ptlang_malloc(sizeof(struct ptlang_ast_exp_s));
    *exp = (struct ptlang_ast_exp_s){
        .type = PTLANG_AST_EXP_CAST,
        .content.cast =
            {
                .type = type,
                .value = value,
            },
        .pos = pos,
    };
    return exp;
}

ptlang_ast_exp ptlang_ast_exp_struct_member_new(ptlang_ast_exp struct_, ptlang_ast_ident member_name,
                                                ptlang_ast_code_position pos)
{
    ptlang_ast_exp exp = ptlang_malloc(sizeof(struct ptlang_ast_exp_s));
    *exp = (struct ptlang_ast_exp_s){
        .type = PTLANG_AST_EXP_STRUCT_MEMBER,
        .content.struct_member =
            {
                .struct_ = struct_,
                .member_name = member_name,
            },
        .pos = pos,
    };
    return exp;
}
ptlang_ast_exp ptlang_ast_exp_array_element_new(ptlang_ast_exp array, ptlang_ast_exp index,
                                                ptlang_ast_code_position pos)
{
    ptlang_ast_exp exp = ptlang_malloc(sizeof(struct ptlang_ast_exp_s));
    *exp = (struct ptlang_ast_exp_s){
        .type = PTLANG_AST_EXP_ARRAY_ELEMENT,
        .content.array_element =
            {
                .array = array,
                .index = index,
            },
        .pos = pos,
    };
    return exp;
}

ptlang_ast_exp ptlang_ast_exp_reference_new(bool writable, ptlang_ast_exp value, ptlang_ast_code_position pos)
{
    ptlang_ast_exp exp = ptlang_malloc(sizeof(struct ptlang_ast_exp_s));
    *exp = (struct ptlang_ast_exp_s){
        .type = PTLANG_AST_EXP_REFERENCE,
        .content.reference =
            {
                .writable = writable,
                .value = value,
            },
        .pos = pos,
    };
    return exp;
}

UNARY_OP(dereference, DEREFERENCE)

ptlang_ast_stmt ptlang_ast_stmt_block_new(ptlang_ast_code_position pos)
{
    ptlang_ast_stmt stmt = ptlang_malloc(sizeof(struct ptlang_ast_stmt_s));
    *stmt = (struct ptlang_ast_stmt_s){
        .type = PTLANG_AST_STMT_BLOCK,
        .content.block =
            {
                .stmts = NULL,
            },
        .pos = pos,
    };
    return stmt;
}

void ptlang_ast_stmt_block_add_stmt(ptlang_ast_stmt block_stmt, ptlang_ast_stmt stmt)
{
    assert(block_stmt->type == PTLANG_AST_STMT_BLOCK);

    arrput(block_stmt->content.block.stmts, stmt);

    // block_stmt->content.block.stmt_count++;
    // block_stmt->content.block.stmts = ptlang_realloc(
    //     block_stmt->content.block.stmts, sizeof(ptlang_ast_stmt) * block_stmt->content.block.stmt_count);
    // block_stmt->content.block.stmts[block_stmt->content.block.stmt_count - 1] = stmt;
}
ptlang_ast_stmt ptlang_ast_stmt_expr_new(ptlang_ast_exp exp, ptlang_ast_code_position pos)
{
    ptlang_ast_stmt stmt = ptlang_malloc(sizeof(struct ptlang_ast_stmt_s));
    *stmt = (struct ptlang_ast_stmt_s){
        .type = PTLANG_AST_STMT_EXP,
        .content.exp = exp,
        .pos = pos,
    };
    return stmt;
}
ptlang_ast_stmt ptlang_ast_stmt_decl_new(ptlang_ast_decl decl, ptlang_ast_code_position pos)
{
    ptlang_ast_stmt stmt = ptlang_malloc(sizeof(struct ptlang_ast_stmt_s));
    *stmt = (struct ptlang_ast_stmt_s){
        .type = PTLANG_AST_STMT_DECL,
        .content.decl = decl,
        .pos = pos,
    };
    return stmt;
}
ptlang_ast_stmt ptlang_ast_stmt_if_new(ptlang_ast_exp condition, ptlang_ast_stmt if_stmt,
                                       ptlang_ast_code_position pos)
{
    ptlang_ast_stmt stmt = ptlang_malloc(sizeof(struct ptlang_ast_stmt_s));
    *stmt = (struct ptlang_ast_stmt_s){
        .type = PTLANG_AST_STMT_IF,
        .content.control_flow =
            {
                .condition = condition,
                .stmt = if_stmt,
            },
        .pos = pos,
    };
    return stmt;
}
ptlang_ast_stmt ptlang_ast_stmt_if_else_new(ptlang_ast_exp condition, ptlang_ast_stmt if_stmt,
                                            ptlang_ast_stmt else_stmt, ptlang_ast_code_position pos)
{
    ptlang_ast_stmt stmt = ptlang_malloc(sizeof(struct ptlang_ast_stmt_s));
    *stmt = (struct ptlang_ast_stmt_s){
        .type = PTLANG_AST_STMT_IF_ELSE,
        .content.control_flow2 =
            {
                .condition = condition,
                .stmt = {if_stmt, else_stmt},
            },
        .pos = pos,
    };
    return stmt;
}
ptlang_ast_stmt ptlang_ast_stmt_while_new(ptlang_ast_exp condition, ptlang_ast_stmt loop_stmt,
                                          ptlang_ast_code_position pos)
{
    ptlang_ast_stmt stmt = ptlang_malloc(sizeof(struct ptlang_ast_stmt_s));
    *stmt = (struct ptlang_ast_stmt_s){
        .type = PTLANG_AST_STMT_WHILE,
        .content.control_flow =
            {
                .condition = condition,
                .stmt = loop_stmt,
            },
        .pos = pos,
    };
    return stmt;
}
ptlang_ast_stmt ptlang_ast_stmt_return_new(ptlang_ast_exp return_value, ptlang_ast_code_position pos)
{
    ptlang_ast_stmt stmt = ptlang_malloc(sizeof(struct ptlang_ast_stmt_s));
    *stmt = (struct ptlang_ast_stmt_s){
        .type = PTLANG_AST_STMT_RETURN,
        .content.exp = return_value,
        .pos = pos,
    };
    return stmt;
}
ptlang_ast_stmt ptlang_ast_stmt_ret_val_new(ptlang_ast_exp return_value, ptlang_ast_code_position pos)
{
    ptlang_ast_stmt stmt = ptlang_malloc(sizeof(struct ptlang_ast_stmt_s));
    *stmt = (struct ptlang_ast_stmt_s){
        .type = PTLANG_AST_STMT_RET_VAL,
        .content.exp = return_value,
        .pos = pos,
    };
    return stmt;
}
ptlang_ast_stmt ptlang_ast_stmt_break_new(uint64_t nesting_level, ptlang_ast_code_position pos)
{
    ptlang_ast_stmt stmt = ptlang_malloc(sizeof(struct ptlang_ast_stmt_s));
    *stmt = (struct ptlang_ast_stmt_s){
        .type = PTLANG_AST_STMT_BREAK,
        .content.nesting_level = nesting_level,
        .pos = pos,
    };
    return stmt;
}
ptlang_ast_stmt ptlang_ast_stmt_continue_new(uint64_t nesting_level, ptlang_ast_code_position pos)
{
    ptlang_ast_stmt stmt = ptlang_malloc(sizeof(struct ptlang_ast_stmt_s));
    *stmt = (struct ptlang_ast_stmt_s){
        .type = PTLANG_AST_STMT_CONTINUE,
        .content.nesting_level = nesting_level,
        .pos = pos,
    };
    return stmt;
}

ptlang_ast_type *ptlang_ast_type_list_copy(ptlang_ast_type *type_list)
{
    ptlang_ast_type *copy = NULL;
    size_t type_list_len = arrlenu(type_list);
    arrsetlen(copy, type_list_len);
    for (size_t i = 0; i < type_list_len; i++)
    {
        copy[i] = ptlang_ast_type_copy(type_list[i]);
    }
    return copy;
}

ptlang_ast_type ptlang_ast_type_copy(ptlang_ast_type type)
{
    if (type == NULL)
    {
        return NULL;
    }
    switch (type->type)
    {
    case PTLANG_AST_TYPE_VOID:
        return ptlang_ast_type_void(ptlang_ast_code_position_copy(type->pos));
    case PTLANG_AST_TYPE_INTEGER:
        return ptlang_ast_type_integer(type->content.integer.is_signed, type->content.integer.size,
                                       ptlang_ast_code_position_copy(type->pos));
    case PTLANG_AST_TYPE_FLOAT:
        return ptlang_ast_type_float(type->content.float_size, ptlang_ast_code_position_copy(type->pos));
    case PTLANG_AST_TYPE_FUNCTION:
        return ptlang_ast_type_function(ptlang_ast_type_copy(type->content.function.return_type),
                                        ptlang_ast_type_list_copy(type->content.function.parameters),
                                        ptlang_ast_code_position_copy(type->pos));
    case PTLANG_AST_TYPE_HEAP_ARRAY:
        return ptlang_ast_type_heap_array(ptlang_ast_type_copy(type->content.heap_array.type),
                                          ptlang_ast_code_position_copy(type->pos));
    case PTLANG_AST_TYPE_ARRAY:
        return ptlang_ast_type_array(ptlang_ast_type_copy(type->content.array.type), type->content.array.len,
                                     ptlang_ast_code_position_copy(type->pos));
    case PTLANG_AST_TYPE_REFERENCE:
        return ptlang_ast_type_reference(ptlang_ast_type_copy(type->content.reference.type),
                                         type->content.reference.writable,
                                         ptlang_ast_code_position_copy(type->pos));
    case PTLANG_AST_TYPE_NAMED:
    {
        size_t name_len = strlen(type->content.name) + 1;
        char *name = ptlang_malloc(name_len);
        memcpy(name, type->content.name, name_len);
        return ptlang_ast_type_named(name, ptlang_ast_code_position_copy(type->pos));
    }
    }
    abort();
}

// ptlang_ast_func *ptlang_ast_module_get_funcs(ptlang_ast_module module, uint64_t *count)
// {
//     *count = module->function_count;
//     return module->functions;
// }

// ptlang_ast_decl *ptlang_ast_module_get_decls(ptlang_ast_module module, uint64_t *count)
// {
//     *count = module->declaration_count;
//     return module->declarations;
// }

// ptlang_ast_struct_def *ptlang_ast_module_get_struct_defs(ptlang_ast_module module, uint64_t *count)
// {
//     *count = module->struct_def_count;
//     return module->struct_defs;
// }

// uint64_t ptlang_ast_module_get_type_alias_count(ptlang_ast_module module)
// {
//     return module->type_alias_count;
// }

// void ptlang_ast_module_get_type_aliases(ptlang_ast_module module, char **names, ptlang_ast_type *types)
// {
//     for (uint64_t i = 0; i < module->type_alias_count; i++)
//     {
//         names[i] = module->type_aliases[i].name;
//         types[i] = module->type_aliases[i].type;
//     }
// }

void ptlang_ast_ident_destroy(ptlang_ast_ident ident)
{
    ptlang_free(ident.name);
    ptlang_free(ident.pos);
}

void ptlang_ast_type_destroy(ptlang_ast_type type)
{
    if (type != NULL)
    {
        ptlang_free(type->pos);
        switch (type->type)
        {
        case PTLANG_AST_TYPE_FUNCTION:
            ptlang_ast_type_destroy(type->content.function.return_type);
            ptlang_ast_type_list_destroy(type->content.function.parameters);
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
        case PTLANG_AST_TYPE_NAMED:
            ptlang_free(type->content.name);
            break;
        default:
            break;
        }
        ptlang_free(type);
    }
}

void ptlang_ast_stmt_destroy(ptlang_ast_stmt stmt)
{
    ptlang_free(stmt->pos);
    switch (stmt->type)
    {
    case PTLANG_AST_STMT_BLOCK:
        for (size_t i = 0; i < arrlenu(stmt->content.block.stmts); i++)
        {
            ptlang_ast_stmt_destroy(stmt->content.block.stmts[i]);
        }
        arrfree(stmt->content.block.stmts);
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
    ptlang_free(stmt);
}

void ptlang_ast_module_destroy(ptlang_ast_module module)
{
    for (size_t i = 0; i < arrlenu(module->functions); i++)
    {
        ptlang_ast_func_destroy(module->functions[i]);
    }
    arrfree(module->functions);
    for (size_t i = 0; i < arrlenu(module->declarations); i++)
    {
        ptlang_ast_decl_destroy(module->declarations[i]);
    }
    arrfree(module->declarations);
    for (size_t i = 0; i < arrlenu(module->struct_defs); i++)
    {
        ptlang_ast_struct_def_destroy(module->struct_defs[i]);
    }
    arrfree(module->struct_defs);
    for (size_t i = 0; i < arrlenu(module->type_aliases); i++)
    {
        ptlang_ast_ident_destroy(module->type_aliases[i].name);
        ptlang_ast_type_destroy(module->type_aliases[i].type);
    }
    arrfree(module->type_aliases);

    ptlang_free(module);
}

void ptlang_ast_func_destroy(ptlang_ast_func func)
{
    ptlang_ast_ident_destroy(func->name);
    ptlang_free(func->pos);
    ptlang_ast_type_destroy(func->return_type);
    ptlang_ast_decl_list_destroy(func->parameters);
    ptlang_ast_stmt_destroy(func->stmt);

    ptlang_free(func);
}

void ptlang_ast_exp_destroy(ptlang_ast_exp exp)
{
    ptlang_free(exp->pos);
    ptlang_ast_type_destroy(exp->ast_type);
    switch (exp->type)
    {
    case PTLANG_AST_EXP_ASSIGNMENT:
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
        ptlang_ast_exp_destroy(exp->content.binary_operator.left_value);
        ptlang_ast_exp_destroy(exp->content.binary_operator.right_value);
        break;
    case PTLANG_AST_EXP_NEGATION:
    case PTLANG_AST_EXP_NOT:
    case PTLANG_AST_EXP_BITWISE_INVERSE:
    case PTLANG_AST_EXP_LENGTH:
    case PTLANG_AST_EXP_DEREFERENCE:
        ptlang_ast_exp_destroy(exp->content.unary_operator);
        break;
    case PTLANG_AST_EXP_FUNCTION_CALL:
        ptlang_ast_exp_destroy(exp->content.function_call.function);
        ptlang_ast_exp_list_destroy(exp->content.function_call.parameters);
        break;
    case PTLANG_AST_EXP_VARIABLE:
    case PTLANG_AST_EXP_INTEGER:
    case PTLANG_AST_EXP_FLOAT:
        ptlang_free(exp->content.str_prepresentation);
        break;
    case PTLANG_AST_EXP_STRUCT:
        ptlang_ast_ident_destroy(exp->content.struct_.type);
        ptlang_ast_struct_member_list_destroy(exp->content.struct_.members);
        break;
    case PTLANG_AST_EXP_ARRAY:
        ptlang_ast_type_destroy(exp->content.array.type);
        ptlang_ast_exp_list_destroy(exp->content.array.values);
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
        ptlang_ast_ident_destroy(exp->content.struct_member.member_name);
        break;
    case PTLANG_AST_EXP_ARRAY_ELEMENT:
        ptlang_ast_exp_destroy(exp->content.array_element.array);
        ptlang_ast_exp_destroy(exp->content.array_element.index);
        break;
    case PTLANG_AST_EXP_REFERENCE:
        ptlang_ast_exp_destroy(exp->content.reference.value);
        break;
    }

    ptlang_free(exp);
}

void ptlang_ast_decl_destroy(ptlang_ast_decl decl)
{
    ptlang_free(decl->pos);
    ptlang_ast_type_destroy(decl->type);
    if (decl->init != NULL)
    {
        ptlang_ast_exp_destroy(decl->init);
    }
    ptlang_ast_ident_destroy(decl->name);

    ptlang_free(decl);
}

void ptlang_ast_struct_def_destroy(ptlang_ast_struct_def struct_def)
{
    ptlang_free(struct_def->pos);
    ptlang_ast_ident_destroy(struct_def->name);
    ptlang_ast_decl_list_destroy(struct_def->members);

    ptlang_free(struct_def);
}

void ptlang_ast_decl_list_destroy(ptlang_ast_decl *decl_list)
{
    for (size_t i = 0; i < arrlenu(decl_list); i++)
    {
        ptlang_ast_decl_destroy(decl_list[i]);
    }
    arrfree(decl_list);
}

void ptlang_ast_type_list_destroy(ptlang_ast_type *type_list)
{

    for (size_t i = 0; i < arrlenu(type_list); i++)
    {
        ptlang_ast_type_destroy(type_list[i]);
    }
    arrfree(type_list);
}

void ptlang_ast_exp_list_destroy(ptlang_ast_exp *exp_list)
{
    for (size_t i = 0; i < arrlenu(exp_list); i++)
    {
        ptlang_ast_exp_destroy(exp_list[i]);
    }
    arrfree(exp_list);
}

void ptlang_ast_struct_member_list_destroy(ptlang_ast_struct_member_list member_list)
{
    for (size_t i = 0; i < arrlenu(member_list); i++)
    {
        ptlang_ast_ident_destroy(member_list[i].str);
        ptlang_ast_exp_destroy(member_list[i].exp);
    }
    arrfree(member_list);
}

ptlang_ast_decl ptlang_decl_list_find_last(ptlang_ast_decl *decl_list, char *name)
{
    for (size_t i = arrlenu(decl_list); i > 0; i--)
    {
        if (0 == strcmp(decl_list[i - 1]->name.name, name))
        {
            return decl_list[i - 1];
        }
    }
    return NULL;
}

ptlang_ast_code_position ptlang_ast_code_position_copy(ptlang_ast_code_position pos)
{
    if (pos == NULL)
    {
        return NULL;
    }
    ptlang_ast_code_position new_pos = ptlang_malloc(sizeof(*new_pos));
    *new_pos = ((ptlang_ast_code_position_s){
        .from_line = pos->from_line,
        .from_column = pos->from_column,
        .to_line = pos->to_line,
        .to_column = pos->to_column,
    });
    return new_pos;
}
