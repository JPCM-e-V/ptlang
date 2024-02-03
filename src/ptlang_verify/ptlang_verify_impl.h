#ifndef PTLANG_VERIFY_IMPL_H
#define PTLANG_VERIFY_IMPL_H

#include "ptlang_ast_nodes.h"
#include "ptlang_eval.h"
#include "ptlang_utils.h"
#include "ptlang_verify.h"
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#include "stb_ds.h"

typedef struct ptlang_verify_type_alias_s ptlang_verify_type_alias;
struct ptlang_verify_type_alias_s
{
    ptlang_ast_type type;
    char *name;
    ptlang_ast_code_position pos;
    ptlang_ast_ident *referenced_types;
    ptlang_verify_type_alias **referencing_types;
    bool resolved;
};

typedef struct
{
    char *key;
    ptlang_verify_type_alias value;
} ptlang_verify_type_alias_table;

typedef struct ptlang_verify_node_table_s
{
    char *key;
    ptlang_utils_graph_node *value;
} *ptlang_verify_node_table;

typedef struct ptlang_verify_decl_table_s
{
    char *key;
    ptlang_ast_decl *value;
} *ptlang_verify_decl_table;

// typedef struct ptlang_verify_struct_s ptlang_verify_struct;
// struct ptlang_verify_struct_s
// {
//     char *name;
//     ptlang_ast_code_position pos;
//     char **referenced_types;
//     ptlang_verify_struct **referencing_types;
//     bool resolved;
// };

// typedef struct
// {
//     char *key;
//     ptlang_verify_struct value;
// } ptlang_verify_struct_table;

static void ptlang_verify_type_resolvability(ptlang_ast_module ast_module, ptlang_context *ctx,
                                             ptlang_error **errors);

// static ptlang_verify_type_alias
// ptlang_verify_type_alias_create(struct ptlang_ast_module_type_alias_s ast_type_alias, ptlang_context *ctx);

static void ptlang_verify_struct_defs(ptlang_ast_struct_def *struct_defs, ptlang_context *ctx,
                                      ptlang_error **errors);
static bool ptlang_verify_type(ptlang_ast_type type, ptlang_context *ctx, ptlang_error **errors);

// static ptlang_verify_struct ptlang_verify_struct_create(ptlang_ast_struct_def ast_struct_def,
//                                                         ptlang_context *ctx, ptlang_error **errors);

static void ptlang_verify_decl(ptlang_ast_decl decl, size_t scope_offset, ptlang_context *ctx,
                               ptlang_error **errors);

static void ptlang_verify_function(ptlang_ast_func function, ptlang_context *ctx, ptlang_error **errors);

static void ptlang_verify_statement(ptlang_ast_stmt statement, uint64_t nesting_level,
                                    bool validate_return_type, ptlang_ast_type wanted_return_type,
                                    size_t scope_offset, bool *has_return_value, bool *is_unreachable,
                                    ptlang_context *ctx, ptlang_error **errors);

static void ptlang_verify_exp(ptlang_ast_exp expression, ptlang_context *ctx, ptlang_error **errors);

static bool ptlang_verify_implicit_cast(ptlang_ast_type from, ptlang_ast_type to, ptlang_context *ctx);

static void ptlang_verify_check_implicit_cast(ptlang_ast_type from, ptlang_ast_type to,
                                              ptlang_ast_code_position pos, ptlang_context *ctx,
                                              ptlang_error **errors);

static void ptlang_verify_functions(ptlang_ast_func *functions, ptlang_context *ctx, ptlang_error **errors);

static size_t *ptlang_verify_type_alias_get_referenced_types_from_ast_type(ptlang_ast_type ast_type,
                                                                           ptlang_context *ctx,
                                                                           ptlang_error **errors,
                                                                           bool *has_error);

static char **ptlang_verify_struct_get_referenced_types_from_struct_def(ptlang_ast_struct_def struct_def,
                                                                        ptlang_context *ctx,
                                                                        ptlang_error **errors);

static ptlang_utils_str ptlang_verify_type_to_string(ptlang_ast_type type,
                                                     ptlang_context_type_scope *type_scope);

static ptlang_error ptlang_verify_generate_type_error(char *before, ptlang_ast_exp exp, char *after,
                                                      ptlang_context_type_scope *type_scope);

static ptlang_ast_type ptlang_verify_unify_types(ptlang_ast_type type1, ptlang_ast_type type2,
                                                 ptlang_context *ctx);

static void ptlang_verify_exp_check_const(ptlang_ast_exp exp, ptlang_context *ctx, ptlang_error **errors);

static ptlang_ast_exp ptlang_verify_eval(ptlang_ast_exp exp, bool eval_fully, ptlang_utils_graph_node *node,
                                         ptlang_utils_graph_node *nodes, ptlang_verify_node_table node_table,
                                         ptlang_ast_module module, ptlang_context *ctx,
                                         ptlang_error **errors);

static void ptlang_verify_decl_header(ptlang_ast_decl decl, size_t scope_offset, ptlang_context *ctx,
                                      ptlang_error **errors);

ptlang_ast_exp ptlang_verify_get_default_value(ptlang_ast_type type, ptlang_context *ctx);

static size_t ptlang_verify_calc_node_count(ptlang_ast_type type, ptlang_context_type_scope *type_scope);

static size_t ptlang_verify_binary_to_unsigned(ptlang_ast_exp binary, ptlang_context *ctx);

static ptlang_utils_graph_node *
ptlang_verify_get_node(ptlang_ast_exp exp, ptlang_verify_node_table node_table, ptlang_context *ctx);

static void ptlang_verify_add_dependency(ptlang_utils_graph_node **from, ptlang_utils_graph_node **to,
                                         ptlang_ast_type type, ptlang_verify_node_table node_table,
                                         bool depends_on_ref, ptlang_context *ctx);

static bool ptlang_verify_build_graph(ptlang_utils_graph_node *node, ptlang_ast_exp exp, bool depends_on_ref,
                                      ptlang_verify_node_table node_table, ptlang_context *ctx);

static void ptlang_verify_label_nodes(ptlang_ast_exp path_exp, ptlang_utils_graph_node **node,
                                      ptlang_context_type_scope *type_scope);

static void ptlang_verify_check_cycles_in_global_defs(ptlang_utils_graph_node *nodes,
                                                      ptlang_verify_node_table node_table,
                                                      ptlang_ast_module module, ptlang_context *ctx,
                                                      ptlang_error **errors);

static void ptlang_verify_eval_globals(ptlang_ast_module module, ptlang_context *ctx, ptlang_error **errors);

static void ptlang_verify_decl_init(ptlang_ast_decl decl, size_t scope_offset, ptlang_context *ctx,
                                    ptlang_error **errors);

static void ptlang_verify_global_decls(ptlang_ast_decl *declarations, ptlang_context *ctx,
                                       ptlang_error **errors);

#endif
