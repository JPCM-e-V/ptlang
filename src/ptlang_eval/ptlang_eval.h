#pragma once

#include "ptlang_ast_nodes.h"
#include "ptlang_context.h"
#include <stdint.h>

// typedef ptlang_eval_value;

// typedef struct ptlang_eval_var_s
// {
//     char *key;
//     ptlang_eval_value value;
// } ptlang_eval_var;

// ptlang_eval_value ptlang_eval_const_exp(ptlang_ast_exp exp, ptlang_ast_type type, ptlang_eval_var *vars);

// LLVMValueRef ptlang_eval_byte_array_to_llvm(ptlang_eval_value val);

ptlang_ast_exp ptlang_eval_const_exp(ptlang_ast_exp exp, ptlang_context *ctx);
uint32_t ptlang_eval_calc_byte_size(ptlang_ast_type type);
