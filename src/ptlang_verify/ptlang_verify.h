#ifndef PTLANG_VERIFY_H
#define PTLANG_VERIFY_H

#include "ptlang_ast.h"
#include "ptlang_context.h"
#include "ptlang_error.h"

ptlang_error *ptlang_verify_module(ptlang_ast_module module, ptlang_context *ctx);

#endif
