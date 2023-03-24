#ifndef PTLANG_AST_H
#define PTLANG_AST_H

#include <stdint.h>
#include <stdbool.h>

typedef struct ptlang_ast_type_s *ptlang_ast_type;
typedef struct ptlang_ast_stmt_s *ptlang_ast_stmt;
typedef struct ptlang_ast_module_s *ptlang_ast_module;
typedef struct ptlang_ast_func_s *ptlang_ast_func;
typedef struct ptlang_ast_exp_s *ptlang_ast_exp;
typedef struct ptlang_ast_decl_s *ptlang_ast_decl;
typedef struct ptlang_ast_struct_def_s *ptlang_ast_struct_def;

enum ptlang_ast_type_float_size
{
    PTLANG_AST_TYPE_FLOAT_16,
    PTLANG_AST_TYPE_FLOAT_32,
    PTLANG_AST_TYPE_FLOAT_64,
    PTLANG_AST_TYPE_FLOAT_128,
};

ptlang_ast_struct_def ptlang_ast_struct_def_new(char *name);
void ptlang_ast_struct_def_add_member(ptlang_ast_struct_def struct_def, char *name, ptlang_ast_type type);

ptlang_ast_module ptlang_ast_module_new();

void ptlang_ast_module_add_function(ptlang_ast_module module, ptlang_ast_func function);
void ptlang_ast_module_add_declaration(ptlang_ast_module module, ptlang_ast_decl declaration);
void ptlang_ast_module_add_struct_def(ptlang_ast_module module, ptlang_ast_struct_def struct_def);
void ptlang_ast_module_add_type_alias(ptlang_ast_module module, char *name, ptlang_ast_type type);

ptlang_ast_func ptlang_ast_func_new(char *name, ptlang_ast_type return_type);
void ptlang_ast_func_add_parameter(ptlang_ast_func function, char *name, ptlang_ast_type type);

ptlang_ast_decl ptlang_ast_decl_new(ptlang_ast_type type, char *name, bool writable);

ptlang_ast_type ptlang_ast_type_integer(bool is_signed, uint32_t size);
ptlang_ast_type ptlang_ast_type_float(enum ptlang_ast_type_float_size size);
ptlang_ast_type ptlang_ast_type_function(ptlang_ast_type return_type, uint64_t parameter_count, ptlang_ast_type *parameter);
ptlang_ast_type ptlang_ast_type_function_new(ptlang_ast_type return_type);
void ptlang_ast_type_function_add_parameter(ptlang_ast_type function_type, ptlang_ast_type patameter);
ptlang_ast_type ptlang_ast_type_heap_array(ptlang_ast_type element_type);
ptlang_ast_type ptlang_ast_type_array(ptlang_ast_type element_type, uint64_t len);
ptlang_ast_type ptlang_ast_type_reference(ptlang_ast_type type, bool writable);

ptlang_ast_exp ptlang_ast_exp_assignment_new(ptlang_ast_exp assignable, ptlang_ast_exp exp);
ptlang_ast_exp ptlang_ast_exp_addition_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value);
ptlang_ast_exp ptlang_ast_exp_subtraction_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value);
ptlang_ast_exp ptlang_ast_exp_negation_new(ptlang_ast_exp value);
ptlang_ast_exp ptlang_ast_exp_multiplication_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value);
ptlang_ast_exp ptlang_ast_exp_division_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value);
ptlang_ast_exp ptlang_ast_exp_modulo_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value);
ptlang_ast_exp ptlang_ast_exp_equal_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value);
ptlang_ast_exp ptlang_ast_exp_greater_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value);
ptlang_ast_exp ptlang_ast_exp_greater_equal_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value);
ptlang_ast_exp ptlang_ast_exp_less_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value);
ptlang_ast_exp ptlang_ast_exp_less_equal_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value);
ptlang_ast_exp ptlang_ast_exp_left_shift_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value);
ptlang_ast_exp ptlang_ast_exp_right_shift_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value);
ptlang_ast_exp ptlang_ast_exp_and_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value);
ptlang_ast_exp ptlang_ast_exp_or_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value);
ptlang_ast_exp ptlang_ast_exp_not_new(ptlang_ast_exp value);
ptlang_ast_exp ptlang_ast_exp_bitwise_and_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value);
ptlang_ast_exp ptlang_ast_exp_bitwise_or_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value);
ptlang_ast_exp ptlang_ast_exp_bitwise_xor_new(ptlang_ast_exp left_value, ptlang_ast_exp right_value);
ptlang_ast_exp ptlang_ast_exp_bitwise_inverse_new(ptlang_ast_exp value);
ptlang_ast_exp ptlang_ast_exp_function_call_new(char *function_name);
void ptlang_ast_exp_function_call_add_parameter(ptlang_ast_exp exp_function_call, ptlang_ast_exp parameter);
ptlang_ast_exp ptlang_ast_exp_variable_new(char *str_prepresentation);
ptlang_ast_exp ptlang_ast_exp_integer_new(char *str_prepresentation);
ptlang_ast_exp ptlang_ast_exp_float_new(char *str_prepresentation);
ptlang_ast_exp ptlang_ast_exp_struct_new(ptlang_ast_type type);
void ptlang_ast_exp_struct_add_value(ptlang_ast_exp exp_struct, char *name, ptlang_ast_exp value);
ptlang_ast_exp ptlang_ast_exp_array_new(ptlang_ast_type type);
void ptlang_ast_exp_array_add_value(ptlang_ast_exp exp_array, ptlang_ast_exp value);
ptlang_ast_exp ptlang_ast_exp_heap_array_from_length_new(ptlang_ast_type type, ptlang_ast_exp length);
ptlang_ast_exp ptlang_ast_exp_ternary_operator_new(ptlang_ast_exp condition, ptlang_ast_exp if_value, ptlang_ast_exp else_value);
ptlang_ast_exp ptlang_ast_exp_cast_new(ptlang_ast_type type, ptlang_ast_exp value);
ptlang_ast_exp ptlang_ast_exp_struct_member_new(ptlang_ast_exp struct_, char *member_name);
ptlang_ast_exp ptlang_ast_exp_array_element_new(ptlang_ast_exp array, ptlang_ast_exp index);
ptlang_ast_exp ptlang_ast_exp_reference_new(bool writable, ptlang_ast_exp value);
ptlang_ast_exp ptlang_ast_exp_dereference_new(ptlang_ast_exp value);

ptlang_ast_stmt ptlang_ast_stmt_block_new();
void ptlang_ast_stmt_block_add_stmt(ptlang_ast_stmt block_stmt, ptlang_ast_stmt stmt);
ptlang_ast_stmt ptlang_ast_stmt_expr_new(ptlang_ast_exp exp);
ptlang_ast_stmt ptlang_ast_stmt_decl_new(ptlang_ast_decl decl);
ptlang_ast_stmt ptlang_ast_stmt_if_new(ptlang_ast_exp condition, ptlang_ast_stmt stmt);
ptlang_ast_stmt ptlang_ast_stmt_if_else_new(ptlang_ast_exp condition, ptlang_ast_stmt if_stmt, ptlang_ast_stmt else_stmt);
ptlang_ast_stmt ptlang_ast_stmt_while_new(ptlang_ast_exp condition, ptlang_ast_stmt stmt);
ptlang_ast_stmt ptlang_ast_stmt_return_new(ptlang_ast_exp return_value);
ptlang_ast_stmt ptlang_ast_stmt_ret_val_new(ptlang_ast_exp return_value);
ptlang_ast_stmt ptlang_ast_stmt_break_new(uint64_t nesting_level);
ptlang_ast_stmt ptlang_ast_stmt_continue_new(uint64_t nesting_level);

#endif
