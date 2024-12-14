#pragma once

#include "ptlang_eval.h"
#include <llvm/IR/NoFolder.h>
#define FOLDER llvm::NoFolder
#include "ptlang_ir_builder_llvm.h"
#include "ptlang_utils.h"

#include <llvm/Support/Casting.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/Interpreter.h>
#include <llvm/Transforms/Utils/Cloning.h>

// #include <llvm-c/Core.h>
// #include <llvm-c/ExecutionEngine.h>

// typedef struct ptlang_eval_value_s
// {
//     uint8_t *data;
//     ptlang_ast_type type;
// } ptlang_eval_value;
