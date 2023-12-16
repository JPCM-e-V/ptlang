#ifndef PTLANG_AST_H
#define PTLANG_AST_H

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct ptlang_ast_code_position_s
{
    uint64_t from_line;
    uint64_t from_column;
    uint64_t to_line;
    uint64_t to_column;
} ptlang_ast_code_position_s;

typedef struct ptlang_ast_code_position_s *ptlang_ast_code_position;

typedef struct ptlang_ast_ident_s
{
    char *name;
    ptlang_ast_code_position pos;
} ptlang_ast_ident;

typedef struct ptlang_ast_type_s *ptlang_ast_type;
typedef struct ptlang_ast_stmt_s *ptlang_ast_stmt;
typedef struct ptlang_ast_module_s *ptlang_ast_module;
typedef struct ptlang_ast_func_s *ptlang_ast_func;
typedef struct ptlang_ast_exp_s *ptlang_ast_exp;
typedef struct ptlang_ast_decl_s *ptlang_ast_decl;
typedef struct ptlang_ast_struct_def_s *ptlang_ast_struct_def;
typedef struct ptlang_ast_struct_member_s *ptlang_ast_struct_member_list;
// typedef struct ptlang_ast_decl_list_s *ptlang_ast_decl_list;
// typedef struct ptlang_ast_type_list_s *ptlang_ast_type_list;
// typedef struct ptlang_ast_exp_list_s *ptlang_ast_exp_list;
// typedef struct ptlang_ast_str_exp_list_s *ptlang_ast_str_exp_list;

enum ptlang_ast_type_float_size
{
    PTLANG_AST_TYPE_FLOAT_16 = 16,
    PTLANG_AST_TYPE_FLOAT_32 = 32,
    PTLANG_AST_TYPE_FLOAT_64 = 64,
    PTLANG_AST_TYPE_FLOAT_128 = 128,
};

ptlang_ast_struct_def ptlang_ast_struct_def_new(ptlang_ast_ident name, ptlang_ast_decl *members,
                                                ptlang_ast_code_position pos);

ptlang_ast_module ptlang_ast_module_new(void);

void ptlang_ast_module_add_function(ptlang_ast_module module, ptlang_ast_func function);
void ptlang_ast_module_add_declaration(ptlang_ast_module module, ptlang_ast_decl declaration);
void ptlang_ast_module_add_struct_def(ptlang_ast_module module, ptlang_ast_struct_def struct_def);
void ptlang_ast_module_add_type_alias(ptlang_ast_module module, ptlang_ast_ident name, ptlang_ast_type type,
                                      ptlang_ast_code_position pos);

ptlang_ast_func ptlang_ast_func_new(ptlang_ast_ident name, ptlang_ast_type return_type,
                                    ptlang_ast_decl *parameters, ptlang_ast_stmt stmt, bool export);

ptlang_ast_decl ptlang_ast_decl_new(ptlang_ast_type type, ptlang_ast_ident name, bool writable,
                                    ptlang_ast_code_position pos);
void ptlang_ast_decl_set_init(ptlang_ast_decl decl, ptlang_ast_exp init);
void ptlang_ast_decl_set_export(ptlang_ast_decl decl, bool export);
void ptlang_ast_func_set_export(ptlang_ast_func func, bool export);

// ptlang_ast_decl_list ptlang_ast_decl_list_new(void);
// void ptlang_ast_decl_list_add(ptlang_ast_decl_list list, ptlang_ast_decl decl);

// ptlang_ast_type_list ptlang_ast_type_list_new(void);
// void ptlang_ast_type_list_add(ptlang_ast_type_list list, ptlang_ast_type type);

// ptlang_ast_exp_list ptlang_ast_exp_list_new(void);
// void ptlang_ast_exp_list_add(ptlang_ast_exp_list list, ptlang_ast_exp exp);

// ptlang_ast_str_exp_list ptlang_ast_str_exp_list_new(void);
// void ptlang_ast_str_exp_list_add(ptlang_ast_str_exp_list list, char *str, ptlang_ast_exp exp);

void ptlang_ast_struct_member_list_add(ptlang_ast_struct_member_list *list, ptlang_ast_ident str,
                                       ptlang_ast_exp exp, ptlang_ast_code_position pos);

ptlang_ast_type ptlang_ast_type_void(ptlang_ast_code_position pos);
ptlang_ast_type ptlang_ast_type_integer(bool is_signed, uint32_t size, ptlang_ast_code_position pos);
ptlang_ast_type ptlang_ast_type_float(enum ptlang_ast_type_float_size size, ptlang_ast_code_position pos);
ptlang_ast_type ptlang_ast_type_function(ptlang_ast_type return_type, ptlang_ast_type *parameters,
                                         ptlang_ast_code_position pos);
ptlang_ast_type ptlang_ast_type_heap_array(ptlang_ast_type element_type, ptlang_ast_code_position pos);
ptlang_ast_type ptlang_ast_type_array(ptlang_ast_type element_type, uint64_t len,
                                      ptlang_ast_code_position pos);
ptlang_ast_type ptlang_ast_type_reference(ptlang_ast_type type, bool writable, ptlang_ast_code_position pos);
ptlang_ast_type ptlang_ast_type_named(char *name, ptlang_ast_code_position pos);

ptlang_ast_exp ptlang_ast_exp_assignment_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value,
                                             ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_addition_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value,
                                           ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_subtraction_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value,
                                              ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_negation_new(ptlang_ast_exp value, ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_multiplication_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value,
                                                 ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_division_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value,
                                           ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_modulo_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value,
                                         ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_remainder_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value,
                                            ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_equal_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value,
                                        ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_not_equal_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value,
                                            ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_greater_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value,
                                          ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_greater_equal_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value,
                                                ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_less_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value,
                                       ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_less_equal_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value,
                                             ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_left_shift_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value,
                                             ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_right_shift_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value,
                                              ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_and_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value,
                                      ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_or_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value,
                                     ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_not_new(ptlang_ast_exp value, ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_bitwise_and_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value,
                                              ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_bitwise_or_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value,
                                             ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_bitwise_xor_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value,
                                              ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_bitwise_inverse_new(ptlang_ast_exp value, ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_length_new(ptlang_ast_exp value, ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_function_call_new(ptlang_ast_exp function, ptlang_ast_exp *parameters,
                                                ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_variable_new(char *str_prepresentation, ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_integer_new(char *str_prepresentation, ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_float_new(char *str_prepresentation, ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_struct_new(ptlang_ast_ident type, ptlang_ast_struct_member_list members,
                                         ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_array_new(ptlang_ast_type type, ptlang_ast_exp *values,
                                        ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_ternary_operator_new(ptlang_ast_exp condition, ptlang_ast_exp if_value,
                                                   ptlang_ast_exp else_value, ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_cast_new(ptlang_ast_type type, ptlang_ast_exp value,
                                       ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_struct_member_new(ptlang_ast_exp struct_, ptlang_ast_ident member_name,
                                                ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_array_element_new(ptlang_ast_exp array, ptlang_ast_exp index,
                                                ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_reference_new(bool writable, ptlang_ast_exp value,
                                            ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_dereference_new(ptlang_ast_exp value, ptlang_ast_code_position pos);
ptlang_ast_exp ptlang_ast_exp_binary_new(uint8_t* binary, ptlang_ast_exp prev);

ptlang_ast_stmt ptlang_ast_stmt_block_new(ptlang_ast_code_position pos);
void ptlang_ast_stmt_block_add_stmt(ptlang_ast_stmt block_stmt, ptlang_ast_stmt stmt);
ptlang_ast_stmt ptlang_ast_stmt_expr_new(ptlang_ast_exp exp, ptlang_ast_code_position pos);
ptlang_ast_stmt ptlang_ast_stmt_decl_new(ptlang_ast_decl decl, ptlang_ast_code_position pos);
ptlang_ast_stmt ptlang_ast_stmt_if_new(ptlang_ast_exp condition, ptlang_ast_stmt stmt,
                                       ptlang_ast_code_position pos);
ptlang_ast_stmt ptlang_ast_stmt_if_else_new(ptlang_ast_exp condition, ptlang_ast_stmt if_stmt,
                                            ptlang_ast_stmt else_stmt, ptlang_ast_code_position pos);
ptlang_ast_stmt ptlang_ast_stmt_while_new(ptlang_ast_exp condition, ptlang_ast_stmt stmt,
                                          ptlang_ast_code_position pos);
ptlang_ast_stmt ptlang_ast_stmt_return_new(ptlang_ast_exp return_value, ptlang_ast_code_position pos);
ptlang_ast_stmt ptlang_ast_stmt_ret_val_new(ptlang_ast_exp return_value, ptlang_ast_code_position pos);
ptlang_ast_stmt ptlang_ast_stmt_break_new(uint64_t nesting_level, ptlang_ast_code_position pos);
ptlang_ast_stmt ptlang_ast_stmt_continue_new(uint64_t nesting_level, ptlang_ast_code_position pos);

// ptlang_ast_func *ptlang_ast_module_get_funcs(ptlang_ast_module module, uint64_t *count);
// ptlang_ast_decl *ptlang_ast_module_get_decls(ptlang_ast_module module, uint64_t *count);
// ptlang_ast_struct_def *ptlang_ast_module_get_struct_defs(ptlang_ast_module module, uint64_t *count);
// uint64_t ptlang_ast_module_get_type_alias_count(ptlang_ast_module module);
// void ptlang_ast_module_get_type_aliases(ptlang_ast_module module, char **names, ptlang_ast_type *types);

ptlang_ast_type ptlang_ast_type_copy(ptlang_ast_type type);
ptlang_ast_type *ptlang_ast_type_list_copy(ptlang_ast_type *type_list);

void ptlang_ast_type_destroy(ptlang_ast_type type);
void ptlang_ast_stmt_destroy(ptlang_ast_stmt stmt);
void ptlang_ast_module_destroy(ptlang_ast_module module);
void ptlang_ast_func_destroy(ptlang_ast_func func);
void ptlang_ast_exp_destroy(ptlang_ast_exp exp);
void ptlang_ast_decl_destroy(ptlang_ast_decl decl);
void ptlang_ast_struct_def_destroy(ptlang_ast_struct_def struct_def);
void ptlang_ast_decl_list_destroy(ptlang_ast_decl *decl_list);
void ptlang_ast_type_list_destroy(ptlang_ast_type *type_list);
void ptlang_ast_exp_list_destroy(ptlang_ast_exp *exp_list);
void ptlang_ast_struct_member_list_destroy(ptlang_ast_struct_member_list member_list);

ptlang_ast_decl ptlang_decl_list_find_last(ptlang_ast_decl *decl_list, char *name);

ptlang_ast_code_position ptlang_ast_code_position_copy(ptlang_ast_code_position pos);

ptlang_ast_exp ptlang_ast_exp_copy(ptlang_ast_exp exp);

ptlang_ast_ident ptlang_ast_ident_copy(ptlang_ast_ident ident);


#endif
