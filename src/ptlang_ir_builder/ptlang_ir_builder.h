#pragma once

#ifdef __cplusplus
#    include <llvm/Target/TargetMachine.h>

extern "C"
{
    typedef llvm::TargetMachine llvm_target_machine;
#else
typedef struct llvm_target_machine llvm_target_machine;
#endif

#include "ptlang_ast_nodes.h"
#include "ptlang_context.h"
    void ptlang_ir_builder_dump_module(ptlang_ast_module module, ptlang_context *ctx);
    void ptlang_ir_builder_store_data_layout(ptlang_context *ctx);

#ifdef __cplusplus
}
#endif
