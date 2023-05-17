#include "ptlang_ir_builder_impl.h"

typedef struct ptlang_ir_builder_scope_entry_s
{
    char *name;
    LLVMValueRef value;
    ptlang_ast_type type;
} ptlang_ir_builder_scope_entry;

typedef struct ptlang_ir_builder_scope_s ptlang_ir_builder_scope;

struct ptlang_ir_builder_scope_s
{
    ptlang_ir_builder_scope *parent;
    uint64_t entry_count;
    ptlang_ir_builder_scope_entry *entries;
};

static inline void ptlang_ir_builder_new_scope(ptlang_ir_builder_scope *parent, ptlang_ir_builder_scope *new)
{
    *new = (ptlang_ir_builder_scope){
        .parent = parent,
        .entry_count = 0,
    };
}

static inline void ptlang_ir_builder_scope_destroy(ptlang_ir_builder_scope *scope)
{
    free(scope->entries);
}

static inline void ptlang_ir_builder_scope_add(ptlang_ir_builder_scope *scope, char *name, LLVMValueRef value, ptlang_ast_type type)
{
    scope->entry_count++;
    scope->entries = realloc(scope->entries, sizeof(ptlang_ir_builder_scope_entry) * scope->entry_count);
    scope->entries[scope->entry_count - 1] = (ptlang_ir_builder_scope_entry){
        .name = name,
        .value = value,
        .type = type,
    };
}

static inline LLVMValueRef ptlang_ir_builder_scope_get(ptlang_ir_builder_scope *scope, char *name, ptlang_ast_type *type)
{
    while (scope != NULL)
    {
        for (uint64_t i = 0; i < scope->entry_count; i++)
        {
            if (strcmp(scope->entries[i].name, name) == 0)
            {
                if (type != NULL)
                {
                    *type = scope->entries[i].type;
                }
                return scope->entries[i].value;
            }
        }
        scope = scope->parent;
    }
    return NULL;
}

typedef struct ptlang_ir_builder_type_scope_entry_s
{
    char *name;
    LLVMTypeRef type;
} ptlang_ir_builder_type_scope_entry;

typedef struct ptlang_ir_builder_type_scope_s
{
    uint64_t entry_count;
    ptlang_ir_builder_type_scope_entry *entries;
} ptlang_ir_builder_type_scope;

static inline void ptlang_ir_builder_type_scope_destroy(ptlang_ir_builder_type_scope *type_scope)
{
    free(type_scope->entries);
}

static inline void ptlang_ir_builder_type_scope_add(ptlang_ir_builder_type_scope *type_scope, char *name, LLVMTypeRef type)
{
    type_scope->entry_count++;
    type_scope->entries = realloc(type_scope->entries, sizeof(ptlang_ir_builder_type_scope_entry) * type_scope->entry_count);
    type_scope->entries[type_scope->entry_count - 1] = (ptlang_ir_builder_type_scope_entry){
        .name = name,
        .type = type,
    };
}

static inline LLVMTypeRef ptlang_ir_builder_type_scope_get(ptlang_ir_builder_type_scope *type_scope, char *name)
{
    for (uint64_t i = 0; i < type_scope->entry_count; i++)
    {
        if (strcmp(type_scope->entries[i].name, name) == 0)
        {
            return type_scope->entries[i].type;
        }
    }
    return NULL;
}

typedef struct ptlang_ir_builder_build_context_s
{
    LLVMBuilderRef builder;
    LLVMValueRef function;
    ptlang_ir_builder_type_scope *type_scope;
    ptlang_ir_builder_scope *scope;
    uint64_t scope_index;
    ptlang_ir_builder_scope *scopes;
    LLVMValueRef return_ptr;
    LLVMBasicBlockRef return_block;
} ptlang_ir_builder_build_context;

static LLVMValueRef ptlang_ir_builder_exp_ptr(ptlang_ast_exp exp, ptlang_ir_builder_build_context *ctx)
{
    switch (exp->type)
    {
    case PTLANG_AST_EXP_VARIABLE:
        return ptlang_ir_builder_scope_get(ctx->scope, exp->content.str_prepresentation, NULL);
    case PTLANG_AST_EXP_STRUCT_MEMBER:
        return NULL;
    case PTLANG_AST_EXP_ARRAY_ELEMENT:
        return NULL;
    case PTLANG_AST_EXP_REFERENCE:
        return NULL;
    default:
        return NULL;
    }
}

static ptlang_ast_type ptlang_ir_builder_combine_types(ptlang_ast_type a, ptlang_ast_type b)
{
    if ((a->type != PTLANG_AST_TYPE_INTEGER && a->type != PTLANG_AST_TYPE_FLOAT) || (b->type != PTLANG_AST_TYPE_INTEGER && b->type != PTLANG_AST_TYPE_FLOAT))
    {
        return NULL;
    }
    else if (a->type == PTLANG_AST_TYPE_INTEGER && b->type == PTLANG_AST_TYPE_INTEGER)
    {
        return ptlang_ast_type_integer(a->content.integer.is_signed || a->content.integer.is_signed, a->content.integer.size > b->content.integer.size ? a->content.integer.size : b->content.integer.size);
    }
    else if (a->type == PTLANG_AST_TYPE_INTEGER)
    {
        return ptlang_ast_type_float(b->content.float_size);
    }
    else if (b->type == PTLANG_AST_TYPE_INTEGER)
    {
        return ptlang_ast_type_float(a->content.float_size);
    }
    else
    {
        return ptlang_ast_type_float(a->content.float_size > b->content.float_size ? a->content.float_size : b->content.float_size);
    }
}

// static ptlang_ast_type ptlang_ir_builder_type_for_bitwise(ptlang_ast_type type)
// {
//     switch (type->type)
//     {
//     case PTLANG_AST_TYPE_INTEGER:
//         return ptlang_ast_type_integer(type->content.integer.is_signed, type->content.integer.size);
//     case PTLANG_AST_TYPE_FLOAT:
//         return ptlang_ast_type_integer(false, type->content.float_size);
//     default:
//         return NULL;
//     }
// }
static ptlang_ast_type ptlang_ir_builder_exp_type(ptlang_ast_exp exp, ptlang_ir_builder_build_context *ctx)
{
    switch (exp->type)
    {
    case PTLANG_AST_EXP_ASSIGNMENT:
        return ptlang_ir_builder_exp_type(exp->content.binary_operator.left_value, ctx);
    case PTLANG_AST_EXP_ADDITION:
    case PTLANG_AST_EXP_SUBTRACTION:
    case PTLANG_AST_EXP_MULTIPLICATION:
    case PTLANG_AST_EXP_DIVISION:
    case PTLANG_AST_EXP_MODULO:
    {
        ptlang_ast_type left = ptlang_ir_builder_exp_type(exp->content.binary_operator.left_value, ctx);
        ptlang_ast_type right = ptlang_ir_builder_exp_type(exp->content.binary_operator.right_value, ctx);
        ptlang_ast_type type = ptlang_ir_builder_combine_types(left, right);
        ptlang_ast_type_destroy(left);
        ptlang_ast_type_destroy(right);
        return type;
    }
    case PTLANG_AST_EXP_NEGATION:
        return ptlang_ir_builder_exp_type(exp->content.unary_operator, ctx);
    case PTLANG_AST_EXP_EQUAL:
    case PTLANG_AST_EXP_NOT_EQUAL:
    case PTLANG_AST_EXP_GREATER:
    case PTLANG_AST_EXP_GREATER_EQUAL:
    case PTLANG_AST_EXP_LESS:
    case PTLANG_AST_EXP_LESS_EQUAL:
    case PTLANG_AST_EXP_AND:
    case PTLANG_AST_EXP_OR:
    case PTLANG_AST_EXP_NOT:
        return ptlang_ast_type_integer(false, 1);
    case PTLANG_AST_EXP_LEFT_SHIFT:
    case PTLANG_AST_EXP_RIGHT_SHIFT:
    {
        ptlang_ast_type orig = ptlang_ir_builder_exp_type(exp->content.binary_operator.left_value, ctx);
        // ptlang_ast_type bitwise = ptlang_ir_builder_type_for_bitwise(orig);
        // ptlang_ast_type_destroy(orig);
        return orig;
    }
    case PTLANG_AST_EXP_BITWISE_AND:
    case PTLANG_AST_EXP_BITWISE_OR:
    case PTLANG_AST_EXP_BITWISE_XOR:
    {
        ptlang_ast_type left = ptlang_ir_builder_exp_type(exp->content.binary_operator.left_value, ctx);
        // ptlang_ast_type left_bitwise = ptlang_ir_builder_type_for_bitwise(left);
        ptlang_ast_type right = ptlang_ir_builder_exp_type(exp->content.binary_operator.right_value, ctx);
        // ptlang_ast_type right_bitwise = ptlang_ir_builder_type_for_bitwise(right);
        ptlang_ast_type both_bitwise = ptlang_ir_builder_combine_types(left, right);
        ptlang_ast_type_destroy(left);
        ptlang_ast_type_destroy(right);
        // ptlang_ast_type_destroy(left_bitwise);
        // ptlang_ast_type_destroy(right_bitwise);

        return both_bitwise;
    }
    case PTLANG_AST_EXP_BITWISE_INVERSE:
    {
        ptlang_ast_type orig = ptlang_ir_builder_exp_type(exp->content.unary_operator, ctx);
        // ptlang_ast_type bitwise = ptlang_ir_builder_type_for_bitwise(orig);
        // ptlang_ast_type_destroy(orig);
        return orig;
    }
    case PTLANG_AST_EXP_FUNCTION_CALL:
    { // ptlang_ir_builder_scope_get()
        ptlang_ast_type function = ptlang_ir_builder_exp_type(exp->content.function_call.function, ctx);
        ptlang_ast_type return_type = ptlang_ast_type_copy(function->content.function.return_type);
        ptlang_ast_type_destroy(function);
        return return_type;
    }
    case PTLANG_AST_EXP_VARIABLE:
    {
        ptlang_ast_type var_type;
        ptlang_ir_builder_scope_get(ctx->scope, exp->content.str_prepresentation, &var_type);
        return ptlang_ast_type_copy(var_type);
    }
    case PTLANG_AST_EXP_INTEGER:
    {
        bool is_signed = false;
        uint32_t size = 64;
        char *size_str = strchr(exp->content.str_prepresentation, 'u');
        if (size_str == NULL)
            size_str = strchr(exp->content.str_prepresentation, 'U');
        if (size_str == NULL)
        {
            is_signed = true;
            size_str = strchr(exp->content.str_prepresentation, 's');
            if (size_str == NULL)
                size_str = strchr(exp->content.str_prepresentation, 'S');
        }
        if (size_str != NULL)
        {
            size = strtoul(size_str + 1, NULL, 10);
        }
        return ptlang_ast_type_integer(is_signed, size);
    }
    case PTLANG_AST_EXP_FLOAT:
    {
        char *f = strchr(exp->content.str_prepresentation, 'f');
        if (f == NULL)
        {
            f = strchr(exp->content.str_prepresentation, 'F');
        }
        enum ptlang_ast_type_float_size float_size = PTLANG_AST_TYPE_FLOAT_64;
        if (f != NULL)
        {
            if (strcmp(f + 1, "16"))
            {
                float_size = PTLANG_AST_TYPE_FLOAT_16;
            }
            else if (strcmp(f + 1, "32"))
            {
                float_size = PTLANG_AST_TYPE_FLOAT_32;
            }
            else if (strcmp(f + 1, "64"))
            {
                float_size = PTLANG_AST_TYPE_FLOAT_64;
            }
            else if (strcmp(f + 1, "128"))
            {
                float_size = PTLANG_AST_TYPE_FLOAT_128;
            }
        }

        return ptlang_ast_type_float(float_size);
    }
    case PTLANG_AST_EXP_STRUCT:
        return ptlang_ast_type_named(exp->content.struct_.type);
    case PTLANG_AST_EXP_ARRAY:
        return ptlang_ast_type_copy(exp->content.array.type);
    case PTLANG_AST_EXP_HEAP_ARRAY_FROM_LENGTH:
        return ptlang_ast_type_copy(exp->content.heap_array.type);
    case PTLANG_AST_EXP_TERNARY:
    {
        ptlang_ast_type if_value = ptlang_ir_builder_exp_type(exp->content.ternary_operator.if_value, ctx);
        ptlang_ast_type else_value = ptlang_ir_builder_exp_type(exp->content.ternary_operator.else_value, ctx);
        ptlang_ast_type type = ptlang_ir_builder_combine_types(if_value, else_value);
        ptlang_ast_type_destroy(if_value);
        ptlang_ast_type_destroy(else_value);
        return type;
    }
    case PTLANG_AST_EXP_CAST:
        return ptlang_ast_type_copy(exp->content.cast.type);
    case PTLANG_AST_EXP_STRUCT_MEMBER:
        return NULL;
    case PTLANG_AST_EXP_ARRAY_ELEMENT:
    {
        ptlang_ast_type arr_type = ptlang_ir_builder_exp_type(exp->content.array_element.array, ctx);
        ptlang_ast_type type;
        if (arr_type->type == PTLANG_AST_TYPE_HEAP_ARRAY)
        {
            type = ptlang_ast_type_copy(arr_type->content.heap_array.type);
        }
        else
        {
            type = ptlang_ast_type_copy(arr_type->content.array.type);
        }
        ptlang_ast_type_destroy(arr_type);
        return type;
    }
    case PTLANG_AST_EXP_REFERENCE:
        return ptlang_ast_type_reference(exp->content.reference.value, exp->content.reference.writable);
    case PTLANG_AST_EXP_DEREFERENCE:
        return ptlang_ir_builder_exp_type(exp->content.unary_operator, ctx);
    }

    // LLVMSizeOf():
}

static LLVMValueRef ptlang_ir_builder_build_cast(LLVMValueRef input, ptlang_ast_type from, ptlang_ast_type to, ptlang_ir_builder_build_context *ctx)
{
}

static LLVMValueRef ptlang_ir_builder_exp(ptlang_ast_exp exp, ptlang_ir_builder_build_context *ctx)
{
    switch (exp->type)
    {
    case PTLANG_AST_EXP_ASSIGNMENT:
    {
        LLVMValueRef ptr = ptlang_ir_builder_exp_ptr(exp->content.binary_operator.left_value, ctx);
        LLVMValueRef value = ptlang_ir_builder_exp(exp->content.binary_operator.right_value, ctx);
        LLVMBuildStore(ctx->builder, value, ptr);
        return value;
    }
    case PTLANG_AST_EXP_ADDITION:
    {
        return NULL;
    }
    case PTLANG_AST_EXP_SUBTRACTION:
        return NULL;
    case PTLANG_AST_EXP_NEGATION:
        return NULL;
    case PTLANG_AST_EXP_MULTIPLICATION:
        return NULL;
    case PTLANG_AST_EXP_DIVISION:
        return NULL;
    case PTLANG_AST_EXP_MODULO:
        return NULL;
    case PTLANG_AST_EXP_EQUAL:
        return NULL;
    case PTLANG_AST_EXP_NOT_EQUAL:
        return NULL;
    case PTLANG_AST_EXP_GREATER:
        return NULL;
    case PTLANG_AST_EXP_GREATER_EQUAL:
        return NULL;
    case PTLANG_AST_EXP_LESS:
        return NULL;
    case PTLANG_AST_EXP_LESS_EQUAL:
        return NULL;
    case PTLANG_AST_EXP_LEFT_SHIFT:
        return NULL;
    case PTLANG_AST_EXP_RIGHT_SHIFT:
        return NULL;
    case PTLANG_AST_EXP_AND:
        return NULL;
    case PTLANG_AST_EXP_OR:
        return NULL;
    case PTLANG_AST_EXP_NOT:
        return NULL;
    case PTLANG_AST_EXP_BITWISE_AND:
        return NULL;
    case PTLANG_AST_EXP_BITWISE_OR:
        return NULL;
    case PTLANG_AST_EXP_BITWISE_XOR:
        return NULL;
    case PTLANG_AST_EXP_BITWISE_INVERSE:
        return NULL;
    case PTLANG_AST_EXP_FUNCTION_CALL:
        return NULL;
    case PTLANG_AST_EXP_VARIABLE:
        // return LLVMBuildLoad2(builder, );
    case PTLANG_AST_EXP_INTEGER:
        return NULL;
    case PTLANG_AST_EXP_FLOAT:
        return NULL;
    case PTLANG_AST_EXP_STRUCT:
        return NULL;
    case PTLANG_AST_EXP_ARRAY:
        return NULL;
    case PTLANG_AST_EXP_HEAP_ARRAY_FROM_LENGTH:
        return NULL;
    case PTLANG_AST_EXP_TERNARY:
        return NULL;
    case PTLANG_AST_EXP_CAST:
        return NULL;
    case PTLANG_AST_EXP_STRUCT_MEMBER:
        return NULL;
    case PTLANG_AST_EXP_ARRAY_ELEMENT:
        return NULL;
    case PTLANG_AST_EXP_REFERENCE:
        return NULL;
    case PTLANG_AST_EXP_DEREFERENCE:
        return NULL;
    }
}

static LLVMTypeRef ptlang_ir_builder_type(ptlang_ast_type type, ptlang_ir_builder_type_scope *type_scope)
{
    if (type == NULL)
    {
        return LLVMVoidType();
    }

    LLVMTypeRef *param_types;
    LLVMTypeRef function_type;

    switch (type->type)
    {
    case PTLANG_AST_TYPE_INTEGER:
        return LLVMIntType(type->content.integer.size);
    case PTLANG_AST_TYPE_FLOAT:
        switch (type->content.float_size)
        {
        case PTLANG_AST_TYPE_FLOAT_16:
            return LLVMHalfType();
        case PTLANG_AST_TYPE_FLOAT_32:
            return LLVMFloatType();
        case PTLANG_AST_TYPE_FLOAT_64:
            return LLVMDoubleType();
        case PTLANG_AST_TYPE_FLOAT_128:
            return LLVMFP128Type();
        }
    case PTLANG_AST_TYPE_FUNCTION:
        param_types = malloc(sizeof(LLVMTypeRef) * type->content.function.parameters->count);
        for (uint64_t i = 0; i < type->content.function.parameters->count; i++)
        {
            param_types[i] = ptlang_ir_builder_type(type->content.function.parameters->types[i], type_scope);
        }
        function_type = LLVMFunctionType(ptlang_ir_builder_type(type->content.function.return_type, type_scope), param_types, type->content.function.parameters->count, false);
        free(param_types);
        return function_type;
    case PTLANG_AST_TYPE_HEAP_ARRAY:
        return LLVMPointerType(ptlang_ir_builder_type(type->content.heap_array.type, type_scope), 0);
    case PTLANG_AST_TYPE_ARRAY:
        return LLVMArrayType(ptlang_ir_builder_type(type->content.array.type, type_scope), type->content.array.len);
    case PTLANG_AST_TYPE_REFERENCE:
        return LLVMPointerType(ptlang_ir_builder_type(type->content.reference.type, type_scope), 0);
    case PTLANG_AST_TYPE_NAMED:
        return ptlang_ir_builder_type_scope_get(type_scope, type->content.name);
    }
}

static void ptlang_ir_builder_stmt_allocas(ptlang_ast_stmt stmt, ptlang_ir_builder_build_context *ctx)
// static void ptlang_ir_builder_stmt_allocas(ptlang_ast_stmt stmt, LLVMBuilderRef builder, ptlang_ir_builder_type_scope *type_scope, ptlang_ir_builder_scope *scope, uint64_t *scope_count, ptlang_ir_builder_scope **scopes)
{
    switch (stmt->type)
    {
    case PTLANG_AST_STMT_BLOCK:
    {
        ptlang_ir_builder_scope *old_scope = ctx->scope;
        ctx->scope_index++;
        ctx->scopes = realloc(ctx->scopes, sizeof(ptlang_ir_builder_scope) * ctx->scope_index);
        ctx->scope = &ctx->scopes[ctx->scope_index - 1];
        ptlang_ir_builder_new_scope(old_scope, ctx->scope);
        for (uint64_t i = 0; i < stmt->content.block.stmt_count; i++)
        {
            ptlang_ir_builder_stmt_allocas(stmt->content.block.stmts[i], ctx);
        }
        ctx->scope = old_scope;
        break;
    }
    case PTLANG_AST_STMT_DECL:
        ptlang_ir_builder_scope_add(ctx->scope, stmt->content.decl->name, LLVMBuildAlloca(ctx->builder, ptlang_ir_builder_type(stmt->content.decl->type, ctx->type_scope), stmt->content.decl->name), stmt->content.decl->type);
        break;
    case PTLANG_AST_STMT_IF:
    case PTLANG_AST_STMT_WHILE:
        ptlang_ir_builder_stmt_allocas(stmt->content.control_flow.stmt, ctx);
        break;
    case PTLANG_AST_STMT_IF_ELSE:
        ptlang_ir_builder_stmt_allocas(stmt->content.control_flow2.stmt[0], ctx);
        ptlang_ir_builder_stmt_allocas(stmt->content.control_flow2.stmt[1], ctx);
        break;
    default:
        break;
    }
}

#if 1
static void ptlang_ir_builder_stmt(ptlang_ast_stmt stmt, ptlang_ir_builder_build_context *ctx)
// static void ptlang_ir_builder_stmt(ptlang_ast_stmt stmt, LLVMBuilderRef builder, LLVMValueRef function, ptlang_ir_builder_type_scope *type_scope, ptlang_ir_builder_scope *scope, uint64_t *scope_index, ptlang_ir_builder_scope *scopes, LLVMValueRef return_ptr, LLVMBasicBlockRef return_block)
{

    switch (stmt->type)
    {
    case PTLANG_AST_STMT_BLOCK:
    {
        ptlang_ir_builder_scope *old_scope = ctx->scope;
        ctx->scope = &ctx->scopes[ctx->scope_index];
        ctx->scope_index++;
        for (uint64_t i = 0; i < stmt->content.block.stmt_count; i++)
        {
            ptlang_ir_builder_stmt(stmt->content.block.stmts[i], ctx);
        }
        ctx->scope = old_scope;
        break;
    }
    case PTLANG_AST_STMT_EXP:
        ptlang_ir_builder_exp(stmt->content.exp, ctx);
        break;
    case PTLANG_AST_STMT_IF:
    {
        LLVMBasicBlockRef if_block = LLVMAppendBasicBlock(ctx->function, "if");
        LLVMBasicBlockRef endif_block = LLVMAppendBasicBlock(ctx->function, "endif");

        LLVMBuildCondBr(ctx->builder, ptlang_ir_builder_exp(stmt->content.control_flow.condition, ctx), if_block, endif_block);

        LLVMPositionBuilderAtEnd(ctx->builder, if_block);
        ptlang_ir_builder_stmt(stmt->content.control_flow.stmt, ctx);
        LLVMBuildBr(ctx->builder, endif_block);

        LLVMPositionBuilderAtEnd(ctx->builder, endif_block);
        break;
    }
    case PTLANG_AST_STMT_IF_ELSE:
    {
        LLVMBasicBlockRef if_block = LLVMAppendBasicBlock(ctx->function, "if");
        LLVMBasicBlockRef else_block = LLVMAppendBasicBlock(ctx->function, "else");
        LLVMBasicBlockRef endif_block = LLVMAppendBasicBlock(ctx->function, "endif");

        LLVMBuildCondBr(ctx->builder, ptlang_ir_builder_exp(stmt->content.control_flow2.condition, ctx), if_block, else_block);

        LLVMPositionBuilderAtEnd(ctx->builder, if_block);
        ptlang_ir_builder_stmt(stmt->content.control_flow2.stmt[0], ctx);
        LLVMBuildBr(ctx->builder, endif_block);

        LLVMPositionBuilderAtEnd(ctx->builder, else_block);
        ptlang_ir_builder_stmt(stmt->content.control_flow2.stmt[1], ctx);
        LLVMBuildBr(ctx->builder, endif_block);

        LLVMPositionBuilderAtEnd(ctx->builder, endif_block);
        break;
    }
    case PTLANG_AST_STMT_WHILE:
    {
        LLVMBasicBlockRef condition_block = LLVMAppendBasicBlock(ctx->function, "while_condition");
        LLVMBasicBlockRef stmt_block = LLVMAppendBasicBlock(ctx->function, "while_block");
        LLVMBasicBlockRef endwhile_block = LLVMAppendBasicBlock(ctx->function, "endwhile");

        LLVMBuildBr(ctx->builder, condition_block);

        LLVMPositionBuilderAtEnd(ctx->builder, condition_block);
        LLVMBuildCondBr(ctx->builder, ptlang_ir_builder_exp(stmt->content.control_flow.condition, ctx), stmt_block, endwhile_block);

        LLVMPositionBuilderAtEnd(ctx->builder, stmt_block);
        ptlang_ir_builder_stmt(stmt->content.control_flow.stmt, ctx);
        LLVMBuildBr(ctx->builder, condition_block);

        LLVMPositionBuilderAtEnd(ctx->builder, endwhile_block);
        break;
    }
    case PTLANG_AST_STMT_RETURN:
        if (stmt->content.exp != NULL)
        {
            LLVMBuildStore(ctx->builder, ptlang_ir_builder_exp(stmt->content.exp, ctx), ctx->return_ptr);
        }
        LLVMBuildBr(ctx->builder, ctx->return_block);
        break;
    case PTLANG_AST_STMT_RET_VAL:
        LLVMBuildStore(ctx->builder, ptlang_ir_builder_exp(stmt->content.exp, ctx), ctx->return_ptr);
        break;
    case PTLANG_AST_STMT_BREAK:
    case PTLANG_AST_STMT_CONTINUE:
        break;
    default:
        break;
    }
}
#endif

typedef struct ptlang_ir_builder_type_alias_s *ptlang_ir_builder_type_alias;
struct ptlang_ir_builder_type_alias_s
{
    ptlang_ast_type type;
    char *name;
    char **referenced_types;
    ptlang_ir_builder_type_alias *referencing_types;
    bool resolved;
};

typedef struct
{
    char *key;
    ptlang_ir_builder_type_alias value;
} *ptlang_ir_type_alias_table;

static char **ptlang_ir_builder_type_alias_get_referenced_types_from_ast_type(ptlang_ast_type ast_type, ptlang_ir_builder_type_scope *type_scope)
{
    char **referenced_types = NULL;
    switch (ast_type->type)
    {
    case PTLANG_AST_TYPE_INTEGER:
    case PTLANG_AST_TYPE_FLOAT:
        arrput(referenced_types, "");
        break;
    case PTLANG_AST_TYPE_ARRAY:
        return ptlang_ir_builder_type_alias_get_referenced_types_from_ast_type(ast_type->content.array.type, type_scope);
    case PTLANG_AST_TYPE_HEAP_ARRAY:
        return ptlang_ir_builder_type_alias_get_referenced_types_from_ast_type(ast_type->content.heap_array.type, type_scope);
    case PTLANG_AST_TYPE_REFERENCE:
        return ptlang_ir_builder_type_alias_get_referenced_types_from_ast_type(ast_type->content.reference.type, type_scope);
    case PTLANG_AST_TYPE_NAMED:
        if (ptlang_ir_builder_type_scope_get(type_scope, ast_type->content.name) == NULL)
            arrput(referenced_types, ast_type->content.name);
        else
            arrput(referenced_types, "");
        break;
    case PTLANG_AST_TYPE_FUNCTION:
        referenced_types = ptlang_ir_builder_type_alias_get_referenced_types_from_ast_type(ast_type->content.function.return_type, type_scope);
        for (uint64_t i = 0; i < ast_type->content.function.parameters->count; i++)
        {
            char **param_referenced_types = ptlang_ir_builder_type_alias_get_referenced_types_from_ast_type(ast_type->content.function.parameters->types[i], type_scope);
            for (size_t j = 0; j < arrlenu(param_referenced_types); j++)
            {
                arrput(referenced_types, param_referenced_types[j]);
            }
            arrfree(param_referenced_types);
        }
        break;
    }
    return referenced_types;
}
static ptlang_ir_builder_type_alias ptlang_ir_builder_type_alias_create(struct ptlang_ast_module_type_alias_s ast_type_alias, ptlang_ir_builder_type_scope *type_scope)
{
    ptlang_ir_builder_type_alias type_alias = malloc(sizeof(struct ptlang_ir_builder_type_alias_s));

    *type_alias = (struct ptlang_ir_builder_type_alias_s){
        .name = ast_type_alias.name,
        .type = ast_type_alias.type,
        .referenced_types = ptlang_ir_builder_type_alias_get_referenced_types_from_ast_type(ast_type_alias.type, type_scope),
        .referencing_types = NULL,
        .resolved = false,
    };
    return type_alias;
}

// static void ptlang_ir_builder_resolve_type(ptlang_ast_type ast_type, ptlang_ir_type_alias_table alias_table)
// {
//     switch (ast_type->type)
//     {
//     case PTLANG_AST_TYPE_INTEGER:
//     case PTLANG_AST_TYPE_FLOAT:
//         return;
//     case PTLANG_AST_TYPE_ARRAY:
//         ptlang_ir_builder_resolve_type(ast_type->content.array.type, alias_table);
//         return;
//     case PTLANG_AST_TYPE_HEAP_ARRAY:
//         ptlang_ir_builder_resolve_type(ast_type->content.heap_array.type, alias_table);
//         return;
//     case PTLANG_AST_TYPE_REFERENCE:
//         ptlang_ir_builder_resolve_type(ast_type->content.reference.type, alias_table);
//     case PTLANG_AST_TYPE_FUNCTION:
//         for (uint64_t i = 0; i < ast_type->content.function.parameters->count; i++)
//         {
//             ptlang_ir_builder_resolve_type(ast_type->content.function.parameters->types[i], alias_table);
//         }
//         ptlang_ir_builder_resolve_type(ast_type->content.function.return_type, alias_table);
//         return;
//     case PTLANG_AST_TYPE_NAMED:
//         ptlang_ast_type_destroy_content(ast_type);
//         ast_type = ptlang_ast_type_cop return shget(alias_table, ast_type->content.name)->resolved_type;
//     }
// }

LLVMModuleRef ptlang_ir_builder_module(ptlang_ast_module ast_module)
{
    LLVMModuleRef llvm_module = LLVMModuleCreateWithName("t");

    LLVMBuilderRef builder = LLVMCreateBuilder();

    ptlang_ir_builder_scope global_scope = {};
    ptlang_ir_builder_type_scope type_scope = {};

    LLVMTypeRef *structs = malloc(sizeof(LLVMTypeRef) * ast_module->struct_def_count);
    for (uint64_t i = 0; i < ast_module->struct_def_count; i++)
    {
        structs[i] = LLVMStructCreateNamed(LLVMGetGlobalContext(), ast_module->struct_defs[i]->name);
        ptlang_ir_builder_type_scope_add(&type_scope, ast_module->struct_defs[i]->name, structs[i]);
    }

    // Type aliases

    ptlang_ir_type_alias_table alias_table = NULL;

    ptlang_ir_builder_type_alias primitive_alias = malloc(sizeof(struct ptlang_ir_builder_type_alias_s));
    *primitive_alias = (struct ptlang_ir_builder_type_alias_s){
        .name = "",
        .type = NULL,
        .referenced_types = NULL,
        .referencing_types = NULL,
        .resolved = false};
    shput(alias_table, "", primitive_alias);

    for (int64_t i = 0; i < ast_module->type_alias_count; i++)
    {
        ptlang_ir_builder_type_alias type_alias = ptlang_ir_builder_type_alias_create(ast_module->type_aliases[i], &type_scope);
        shput(alias_table, ast_module->type_aliases[i].name, type_alias);
    }

    for (int64_t i = 0; i < ast_module->type_alias_count; i++)
    {
        ptlang_ir_builder_type_alias type_alias = shget(alias_table, ast_module->type_aliases[i].name);
        for (size_t j = 0; j < arrlenu(type_alias->referenced_types); j++)
        {
            ptlang_ir_builder_type_alias **referencing_types = &(shget(alias_table, type_alias->referenced_types[j])->referencing_types);
            arrput(*referencing_types, type_alias);
        }
    }

    ptlang_ir_builder_type_alias *alias_candidates = NULL;
    arrput(alias_candidates, primitive_alias);
    while (arrlenu(alias_candidates) != 0)
    {
        ptlang_ir_builder_type_alias type_alias = arrpop(alias_candidates);

        // check if resolved
        if (type_alias->resolved)
            continue;
        type_alias->resolved = true;
        for (size_t i = 0; i < arrlenu(type_alias->referenced_types); i++)
        {
            if (!shget(alias_table, type_alias->referenced_types[i])->resolved)
            {
                type_alias->resolved = false;
                break;
            }
        }
        if (!type_alias->resolved)
            continue;

        // add type to type scope
        ptlang_ir_builder_type_scope_add(&type_scope, type_alias->name, ptlang_ir_builder_type(type_alias->type, &type_scope));

        // add referencing types to candidates
        for (size_t i = 0; i < arrlenu(type_alias->referencing_types); i++)
        {
            arrput(alias_candidates, type_alias->referencing_types[i]);
        }
    }

    for (uint64_t i = 0; i < ast_module->struct_def_count; i++)
    {
        LLVMTypeRef *elements = malloc(sizeof(LLVMTypeRef) * ast_module->struct_defs[i]->members->count);
        for (uint64_t j = 0; j < ast_module->struct_defs[i]->members->count; j++)
        {
            elements[j] = ptlang_ir_builder_type(ast_module->struct_defs[i]->members->decls[j]->type, &type_scope);
        }
        LLVMStructSetBody(structs[i], elements, ast_module->struct_defs[i]->members->count, false);
        free(elements);
    }
    free(structs);

    for (uint64_t i = 0; i < ast_module->declaration_count; i++)
    {
        ptlang_ir_builder_scope_add(&global_scope, ast_module->declarations[i]->name, LLVMAddGlobal(llvm_module, ptlang_ir_builder_type(ast_module->declarations[i]->type, &type_scope), ast_module->declarations[i]->name), ast_module->declarations[i]->type);
    }

    LLVMValueRef *functions = malloc(sizeof(LLVMValueRef) * ast_module->function_count);
    ptlang_ast_type *function_types = malloc(sizeof(ptlang_ast_type) * ast_module->function_count);
    for (uint64_t i = 0; i < ast_module->function_count; i++)
    {
        ptlang_ast_type_list param_type_list = ptlang_ast_type_list_new();

        LLVMTypeRef *param_types = malloc(sizeof(LLVMTypeRef) * ast_module->functions[i]->parameters->count);
        for (uint64_t j = 0; j < ast_module->functions[i]->parameters->count; j++)
        {
            ptlang_ast_type_list_add(param_type_list, ptlang_ast_type_copy(ast_module->functions[i]->parameters->decls[j]->type));

            param_types[j] = ptlang_ir_builder_type(ast_module->functions[i]->parameters->decls[j]->type, &type_scope);
        }
        LLVMTypeRef function_type = LLVMFunctionType(ptlang_ir_builder_type(ast_module->functions[i]->return_type, &type_scope), param_types, ast_module->functions[i]->parameters->count, false);
        free(param_types);
        functions[i] = LLVMAddFunction(llvm_module, ast_module->functions[i]->name, function_type);

        function_types[i] = ptlang_ast_type_function(ptlang_ast_type_copy(ast_module->functions[i]->return_type), param_type_list);

        ptlang_ir_builder_scope_add(&global_scope, ast_module->functions[i]->name, functions[i], function_types[i]);
    }

    for (uint64_t i = 0; i < ast_module->function_count; i++)
    {
        LLVMBasicBlockRef entry_block = LLVMAppendBasicBlock(functions[i], "entry");
        LLVMBasicBlockRef return_block = LLVMAppendBasicBlock(functions[i], "return");

        LLVMPositionBuilderAtEnd(builder, entry_block);

        ptlang_ir_builder_scope function_scope;
        ptlang_ir_builder_new_scope(&global_scope, &function_scope);

        LLVMValueRef return_ptr;
        if (ast_module->functions[i]->return_type != NULL)
        {
            return_ptr = LLVMBuildAlloca(builder, ptlang_ir_builder_type(ast_module->functions[i]->return_type, &type_scope), "return");
        }

        LLVMValueRef *param_ptrs = malloc(sizeof(LLVMValueRef) * ast_module->functions[i]->parameters->count);
        for (uint64_t j = 0; j < ast_module->functions[i]->parameters->count; j++)
        {
            param_ptrs[j] = LLVMBuildAlloca(builder, ptlang_ir_builder_type(ast_module->functions[i]->parameters->decls[j]->type, &type_scope), ast_module->functions[i]->parameters->decls[j]->name);
            ptlang_ir_builder_scope_add(&function_scope, ast_module->functions[i]->parameters->decls[j]->name, param_ptrs[j], ast_module->functions[i]->parameters->decls[j]->type);
        }

        ptlang_ir_builder_build_context ctx = {
            .builder = builder,
            .function = functions[i],
            .type_scope = &type_scope,
            .scope = &function_scope,
            .scope_index = 0,
            .scopes = NULL,
            .return_ptr = return_ptr,
            .return_block = return_block,
        };

        ptlang_ir_builder_stmt_allocas(ast_module->functions[i]->stmt, &ctx);

        ctx.scope_index = 0;

        for (uint64_t j = 0; j < ast_module->functions[i]->parameters->count; j++)
        {
            LLVMBuildStore(builder, LLVMGetParam(functions[i], j), param_ptrs[j]);
        }
        free(param_ptrs);

        LLVMBuildBr(builder, return_block);

        LLVMPositionBuilderAtEnd(builder, return_block);

        if (ast_module->functions[i]->return_type != NULL)
        {
            LLVMValueRef return_value = LLVMBuildLoad2(builder, ptlang_ir_builder_type(ast_module->functions[i]->return_type, &type_scope), return_ptr, "return_val");
            LLVMBuildRet(builder, return_value);
        }
        else
        {
            LLVMBuildRetVoid(builder);
        }

        // llvmpara
        ptlang_ir_builder_scope_destroy(&function_scope);
    }
    free(functions);

    for (uint64_t i = 0; i < ast_module->function_count; i++)
    {
        ptlang_ast_type_destroy(function_types[i]);
    }
    free(function_types);

    ptlang_ir_builder_scope_destroy(&global_scope);
    ptlang_ir_builder_type_scope_destroy(&type_scope);

    // LLVMValueRef glob = LLVMAddGlobal(llvm_module, LLVMInt1Type(), "glob");
    // LLVMSetInitializer(glob, LLVMConstInt(LLVMInt1Type(), 0, false));

    return llvm_module;
}
