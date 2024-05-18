#pragma once

#ifdef __cplusplus
extern "C" {
#endif


#include "ptlang_ast_nodes.h"
#include "ptlang_context.h"

void ptlang_ir_builder_dump_module(ptlang_ast_module module, ptlang_context* ctx);


#ifdef __cplusplus
}
#endif
