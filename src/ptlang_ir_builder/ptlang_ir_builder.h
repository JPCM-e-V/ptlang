#pragma once

#ifdef __cplusplus
extern "C"
{
    typedef llvm::DataLayout llvm_data_layout;
#else
typedef struct llvm_data_layout llvm_data_layout;
#endif

#include "ptlang_ast_nodes.h"
#include "ptlang_context.h"
    void ptlang_ir_builder_dump_module(ptlang_ast_module module, ptlang_context *ctx);
    void ptlang_ir_builder_store_data_layout(ptlang_context *ctx);

#ifdef __cplusplus
}
#endif
