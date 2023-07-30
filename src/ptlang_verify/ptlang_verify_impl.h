#ifndef PTLANG_VERIFY_IMPL_H
#define PTLANG_VERIFY_IMPL_H

#include "ptlang_ast_impl.h"
#include "ptlang_verify.h"
#include <stdio.h>

#include "stb_ds.h"

typedef struct pltang_verify_type_alias_s pltang_verify_type_alias;
struct pltang_verify_type_alias_s
{
    ptlang_ast_type type;
    char *name;
    ptlang_ast_code_position pos;
    ptlang_ast_ident *referenced_types;
    pltang_verify_type_alias **referencing_types;
    bool resolved;
};

typedef struct
{
    char *key;
    pltang_verify_type_alias value;
} pltang_verify_type_alias_table;

typedef struct pltang_verify_struct_s pltang_verify_struct;
struct pltang_verify_struct_s
{
    char *name;
    ptlang_ast_code_position pos;
    char **referenced_types;
    pltang_verify_struct **referencing_types;
    bool resolved;
};

typedef struct
{
    char *key;
    pltang_verify_struct value;
} pltang_verify_struct_table;

static void ptlang_verify_type_resolvability(ptlang_ast_module ast_module, ptlang_context *ctx,
                                             ptlang_error **errors);

static pltang_verify_type_alias
pltang_verify_type_alias_create(struct ptlang_ast_module_type_alias_s ast_type_alias, ptlang_context *ctx);

static void ptlang_verify_struct_defs(ptlang_ast_struct_def *struct_defs, ptlang_context *ctx,
                                      ptlang_error **errors);
static bool ptlang_verify_type(ptlang_ast_type type, ptlang_context *ctx, ptlang_error **errors);

static pltang_verify_struct pltang_verify_struct_create(ptlang_ast_struct_def ast_struct_def,
                                                        ptlang_context *ctx, ptlang_error **errors);

#endif