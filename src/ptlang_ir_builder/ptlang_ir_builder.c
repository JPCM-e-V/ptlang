#include "ptlang_ir_builder_impl.h"

#define HEAP_ARRAY_TYPE(element_type, ctx) LLVMStructType((LLVMTypeRef[]){                        \
                                                              LLVMPointerType((element_type), 0), \
                                                              LLVMIntPtrType((ctx)->target_info), \
                                                          },                                      \
                                                          2, false);

#define INIT_FUNCTION(name, return_type, param_types)                                                                                                                 \
    static inline void ptlang_ir_builder_init_##name##_func(ptlang_ir_builder_build_context *ctx)                                                                     \
    {                                                                                                                                                                 \
        if (ctx->name##_func == NULL)                                                                                                                                 \
        {                                                                                                                                                             \
            ctx->name##_func = LLVMAddFunction(ctx->module, #name, LLVMFunctionType((return_type), (param_types), sizeof(param_types) / sizeof(LLVMTypeRef), false)); \
        }                                                                                                                                                             \
    }

typedef struct ptlang_ir_builder_scope_entry_s
{
    char *name;
    LLVMValueRef value;
    ptlang_ast_type type;
    bool direct; // whether the value is directly stored (for functions)
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

static inline void ptlang_ir_builder_scope_add(ptlang_ir_builder_scope *scope, char *name, LLVMValueRef value, ptlang_ast_type type, bool direct)
{
    scope->entry_count++;
    scope->entries = realloc(scope->entries, sizeof(ptlang_ir_builder_scope_entry) * scope->entry_count);
    scope->entries[scope->entry_count - 1] = (ptlang_ir_builder_scope_entry){
        .name = name,
        .value = value,
        .type = type,
        .direct = direct,
    };
}

static inline LLVMValueRef ptlang_ir_builder_scope_get(ptlang_ir_builder_scope *scope, char *name, ptlang_ast_type *type, bool *direct)
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
                if (direct != NULL)
                {
                    *direct = scope->entries[i].direct;
                }
                return scope->entries[i].value;
            }
        }
        scope = scope->parent;
    }
    return NULL;
}

typedef struct ptlang_ir_builder_struct_def_s
{
    char *key;
    ptlang_ast_decl_list value;
} ptlang_ir_builder_struct_def;

typedef struct ptlang_ir_builder_type_scope_entry_s
{
    LLVMTypeRef type;
    ptlang_ast_type ptlang_type;
} ptlang_ir_builder_type_scope_entry;

typedef struct ptlang_ir_builder_type_scope_s
{
    char *key;
    ptlang_ir_builder_type_scope_entry value;
} ptlang_ir_builder_type_scope;

static inline ptlang_ast_type ptlang_ir_builder_unname_type(ptlang_ast_type type, ptlang_ir_builder_type_scope *type_scope)
{
    ptlang_ast_type new_type;
    while (type->type == PTLANG_AST_TYPE_NAMED)
    {
        new_type = shget(type_scope, type->content.name).ptlang_type;
        if (new_type == NULL)
        {
            break;
        }
        type = new_type;
    }
    return type;
}

typedef struct ptlang_ir_builder_build_context_s
{
    LLVMBuilderRef builder;
    LLVMModuleRef module;
    LLVMValueRef function;
    ptlang_ir_builder_type_scope *type_scope;
    ptlang_ir_builder_scope *scope;
    uint64_t scope_number;
    ptlang_ir_builder_scope **scopes;
    LLVMValueRef return_ptr;
    LLVMBasicBlockRef return_block;
    ptlang_ast_type return_type;
    ptlang_ir_builder_struct_def *struct_defs;
    LLVMTargetDataRef target_info;
    LLVMValueRef malloc_func;
    LLVMValueRef realloc_func;
    LLVMValueRef free_func;
} ptlang_ir_builder_build_context;

INIT_FUNCTION(malloc, LLVMPointerType(LLVMInt8Type(), 0), (LLVMTypeRef[]){LLVMIntPtrType(ctx->target_info)})
INIT_FUNCTION(realloc, LLVMPointerType(LLVMInt8Type(), 0), ((LLVMTypeRef[]){LLVMPointerType(LLVMInt8Type(), 0), LLVMIntPtrType(ctx->target_info)}))
INIT_FUNCTION(free, LLVMVoidType(), (LLVMTypeRef[]){LLVMPointerType(LLVMInt8Type(), 0)})

static LLVMTypeRef ptlang_ir_builder_type(ptlang_ast_type type, ptlang_ir_builder_build_context *ctx);

static ptlang_ast_type ptlang_ir_builder_exp_type(ptlang_ast_exp exp, ptlang_ir_builder_build_context *ctx);
static LLVMValueRef ptlang_ir_builder_exp(ptlang_ast_exp exp, ptlang_ir_builder_build_context *ctx);

static LLVMValueRef ptlang_ir_builder_exp_ptr(ptlang_ast_exp exp, ptlang_ir_builder_build_context *ctx)
{
    switch (exp->type)
    {
    case PTLANG_AST_EXP_VARIABLE:
        return ptlang_ir_builder_scope_get(ctx->scope, exp->content.str_prepresentation, NULL, NULL);
    case PTLANG_AST_EXP_STRUCT_MEMBER:
    {
        LLVMValueRef struct_ptr = ptlang_ir_builder_exp_ptr(exp->content.struct_member.struct_, ctx);
        if (struct_ptr == NULL)
        {
            return NULL;
        }
        else
        {
            ptlang_ast_type struct_type = ptlang_ir_builder_exp_type(exp->content.struct_member.struct_, ctx);
            LLVMTypeRef struct_type_llvm = ptlang_ir_builder_type(struct_type, ctx);
            ptlang_ast_decl_list members = shget(ctx->struct_defs, struct_type->content.name);
            ptlang_ast_type_destroy(struct_type);

            for (uint64_t i = 0; i < members->count; i++)
            {
                if (strcmp(members->decls[i]->name, exp->content.struct_member.member_name) == 0)
                {
                    return LLVMBuildStructGEP2(ctx->builder, struct_type_llvm, struct_ptr, i, "structmember");
                }
            }
            return NULL;
        }
    }
    case PTLANG_AST_EXP_ARRAY_ELEMENT:
    {
        ptlang_ast_type array_type = ptlang_ir_builder_exp_type(exp->content.array_element.array, ctx);
        LLVMValueRef index = ptlang_ir_builder_exp(exp->content.array_element.index, ctx);
        LLVMValueRef element_ptr = NULL;
        if (array_type->type == PTLANG_AST_TYPE_ARRAY)
        {
            LLVMValueRef array_ptr = ptlang_ir_builder_exp_ptr(exp->content.array_element.array, ctx);
            if (array_ptr != NULL)
            {
                LLVMTypeRef llvm_array_type = ptlang_ir_builder_type(array_type, ctx);
                element_ptr = LLVMBuildInBoundsGEP2(ctx->builder, llvm_array_type, array_ptr, (LLVMValueRef[]){LLVMConstInt(LLVMInt1Type(), 0, false), index}, 2, "arrayelement");
            }
        }
        else if (array_type->type == PTLANG_AST_TYPE_HEAP_ARRAY)
        {
            LLVMValueRef struct_ptr = ptlang_ir_builder_exp_ptr(exp->content.array_element.array, ctx);
            LLVMValueRef heap_ptr;
            LLVMTypeRef llvm_element_type = ptlang_ir_builder_type(array_type->content.heap_array.type, ctx);
            if (struct_ptr != NULL)
            {
                LLVMValueRef heap_ptr_ptr = LLVMBuildStructGEP2(ctx->builder, ptlang_ir_builder_type(array_type, ctx), struct_ptr, 0, "heapptrptr");
                heap_ptr = LLVMBuildLoad2(ctx->builder, LLVMPointerType(llvm_element_type, 0), heap_ptr_ptr, "heapptr");
            }
            else
            {
                LLVMValueRef struct_ = ptlang_ir_builder_exp(exp->content.array_element.array, ctx);
                heap_ptr = LLVMBuildExtractValue(ctx->builder, struct_, 0, "heapptr");
            }
            element_ptr = LLVMBuildInBoundsGEP2(ctx->builder, llvm_element_type, heap_ptr, (LLVMValueRef[]){index}, 1, "heaparrayelement");
        }
        ptlang_ast_type_destroy(array_type);
        return element_ptr;
    }
    case PTLANG_AST_EXP_REFERENCE:
        return NULL;
    case PTLANG_AST_EXP_LENGTH:
    {
        LLVMValueRef heap_arr_ptr = ptlang_ir_builder_exp_ptr(exp->content.unary_operator, ctx);
        if (heap_arr_ptr != NULL)
        {
            ptlang_ast_type array_type = ptlang_ir_builder_exp_type(exp->content.unary_operator, ctx);
            LLVMTypeRef array_llvm_type = ptlang_ir_builder_type(array_type, ctx);
            ptlang_ast_type_destroy(array_type);
            return LLVMBuildStructGEP2(ctx->builder, array_llvm_type, heap_arr_ptr, 1, "heaparraylengthptr");
        }
        return NULL;
    }
    default:
        return NULL;
    }
}

static ptlang_ast_type ptlang_ir_builder_combine_types(ptlang_ast_type a, ptlang_ast_type b, ptlang_ir_builder_type_scope *type_scope)
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

static LLVMTypeRef ptlang_ir_builder_type(ptlang_ast_type type, ptlang_ir_builder_build_context *ctx)
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
            param_types[i] = ptlang_ir_builder_type(type->content.function.parameters->types[i], ctx);
        }
        function_type = LLVMFunctionType(ptlang_ir_builder_type(type->content.function.return_type, ctx), param_types, type->content.function.parameters->count, false);
        free(param_types);
        function_type = LLVMPointerType(function_type, 0);
        return function_type;
    case PTLANG_AST_TYPE_HEAP_ARRAY:
        return HEAP_ARRAY_TYPE(ptlang_ir_builder_type(type->content.heap_array.type, ctx), ctx);
    case PTLANG_AST_TYPE_ARRAY:
        return LLVMArrayType(ptlang_ir_builder_type(type->content.array.type, ctx), type->content.array.len);
    case PTLANG_AST_TYPE_REFERENCE:
        return LLVMPointerType(ptlang_ir_builder_type(type->content.reference.type, ctx), 0);
    case PTLANG_AST_TYPE_NAMED:
        return shget(ctx->type_scope, type->content.name).type;
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

#if 0
static void ptlang_ir_builder_free(LLVMValueRef heap_arr, LLVMTypeRef element_type, bool isptr, ptlang_ir_builder_build_context *ctx)
{
    ptlang_ir_builder_init_free_func(ctx);

    LLVMTypeRef array_type = HEAP_ARRAY_TYPE(element_type, ctx);

    LLVMValueRef len;
    if (isptr)
    {
        LLVMValueRef len_ptr = LLVMBuildStructGEP2(ctx->builder, array_type, heap_arr, 1, "freegetlenptr");
        len = LLVMBuildLoad2(ctx->builder, LLVMIntPtrType(ctx->target_info), len_ptr, "freeloadlen");
    }
    else
    {
        len = LLVMBuildExtractValue(ctx->builder, heap_arr, 1, "freeextractlen");
    }

    LLVMValueRef len_cmp = LLVMBuildICmp(ctx->builder, LLVMIntNE, len, LLVMConstInt(LLVMIntPtrType(ctx->target_info), 0, false), "freelencmp");

    LLVMBasicBlockRef free_block = LLVMAppendBasicBlock(ctx->function, "freefree");
    LLVMBasicBlockRef end_block = LLVMAppendBasicBlock(ctx->function, "freeend");

    LLVMBuildCondBr(ctx->builder, len_cmp, free_block, end_block);

    LLVMPositionBuilderAtEnd(ctx->builder, free_block);

    LLVMValueRef heap_ptr;
    if (isptr)
    {
        LLVMValueRef heap_ptr_ptr = LLVMBuildStructGEP2(ctx->builder, array_type, heap_arr, 0, "freegetheapptrptr");
        heap_ptr = LLVMBuildLoad2(ctx->builder, LLVMPointerType(element_type, 0), heap_ptr_ptr, "freeloadheapptr");
    }
    else
    {
        heap_ptr = LLVMBuildExtractValue(ctx->builder, heap_arr, 0, "freeextractheapptr");
    }

    heap_ptr = LLVMBuildPointerCast(ctx->builder, heap_ptr, LLVMPointerType(LLVMInt8Type(), 0), "freechangeptrtype");
    LLVMBuildCall2(ctx->builder, LLVMFunctionType(LLVMVoidType(), (LLVMTypeRef[]){LLVMPointerType(LLVMInt8Type(), 0)}, 1, false), ctx->free_func, (LLVMValueRef[]){heap_ptr}, 1, "");

    LLVMBuildBr(ctx->builder, end_block);
    LLVMPositionBuilderAtEnd(ctx->builder, end_block);
}
#endif

static ptlang_ast_type ptlang_ir_builder_type_copy_and_unname(ptlang_ast_type type, ptlang_ir_builder_build_context *ctx)
{
    return ptlang_ast_type_copy(ptlang_ir_builder_unname_type(type, ctx->type_scope));
}

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
    case PTLANG_AST_EXP_REMAINDER:
    {
        ptlang_ast_type left = ptlang_ir_builder_exp_type(exp->content.binary_operator.left_value, ctx);
        ptlang_ast_type right = ptlang_ir_builder_exp_type(exp->content.binary_operator.right_value, ctx);
        ptlang_ast_type type = ptlang_ir_builder_combine_types(left, right, ctx->type_scope);
        ptlang_ast_type_destroy(left);
        ptlang_ast_type_destroy(right);
        return type;
    }
    case PTLANG_AST_EXP_NEGATION:
    {
        ptlang_ast_type type = ptlang_ir_builder_exp_type(exp->content.unary_operator, ctx);
        if (type->type == PTLANG_AST_TYPE_INTEGER)
        {
            uint32_t size = type->content.integer.size;
            ptlang_ast_type_destroy(type);
            return ptlang_ast_type_integer(true, size);
        }
        return type;
    }
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
        ptlang_ast_type both_bitwise = ptlang_ir_builder_combine_types(left, right, ctx->type_scope);
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
    case PTLANG_AST_EXP_LENGTH:
        return ptlang_ast_type_integer(false, LLVMPointerSize(ctx->target_info) * 8);
    case PTLANG_AST_EXP_FUNCTION_CALL:
    { // ptlang_ir_builder_scope_get()
        ptlang_ast_type function = ptlang_ir_builder_exp_type(exp->content.function_call.function, ctx);
        ptlang_ast_type return_type = ptlang_ir_builder_type_copy_and_unname(function->content.function.return_type, ctx);
        ptlang_ast_type_destroy(function);
        return return_type;
    }
    case PTLANG_AST_EXP_VARIABLE:
    {
        ptlang_ast_type var_type;
        ptlang_ir_builder_scope_get(ctx->scope, exp->content.str_prepresentation, &var_type, NULL);
        return ptlang_ir_builder_type_copy_and_unname(var_type, ctx);
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
    {
        size_t name_len = strlen(exp->content.struct_.type) + 1;
        char *name = malloc(name_len);
        memcpy(name, exp->content.struct_.type, name_len);
        return ptlang_ast_type_named(name);
    }
    case PTLANG_AST_EXP_ARRAY:
        return ptlang_ir_builder_type_copy_and_unname(exp->content.array.type, ctx);
    case PTLANG_AST_EXP_TERNARY:
    {
        ptlang_ast_type if_value = ptlang_ir_builder_exp_type(exp->content.ternary_operator.if_value, ctx);
        ptlang_ast_type else_value = ptlang_ir_builder_exp_type(exp->content.ternary_operator.else_value, ctx);
        ptlang_ast_type type = ptlang_ir_builder_combine_types(if_value, else_value, ctx->type_scope);
        ptlang_ast_type_destroy(if_value);
        ptlang_ast_type_destroy(else_value);
        return type;
    }
    case PTLANG_AST_EXP_CAST:
        return ptlang_ir_builder_type_copy_and_unname(exp->content.cast.type, ctx);
    case PTLANG_AST_EXP_STRUCT_MEMBER:
    {
        ptlang_ast_type struct_type = ptlang_ir_builder_exp_type(exp->content.struct_member.struct_, ctx);
        ptlang_ast_decl_list members = shget(ctx->struct_defs, struct_type->content.name);
        ptlang_ast_type_destroy(struct_type);

        for (uint64_t i = 0; i < members->count; i++)
        {
            if (strcmp(members->decls[i]->name, exp->content.struct_member.member_name) == 0)
            {
                return ptlang_ir_builder_type_copy_and_unname(members->decls[i]->type, ctx);
            }
        }

        return NULL;
    }
    case PTLANG_AST_EXP_ARRAY_ELEMENT:
    {
        ptlang_ast_type arr_type = ptlang_ir_builder_exp_type(exp->content.array_element.array, ctx);
        ptlang_ast_type type;
        if (arr_type->type == PTLANG_AST_TYPE_HEAP_ARRAY)
        {
            type = ptlang_ir_builder_type_copy_and_unname(arr_type->content.heap_array.type, ctx);
        }
        else
        {
            type = ptlang_ir_builder_type_copy_and_unname(arr_type->content.array.type, ctx);
        }
        ptlang_ast_type_destroy(arr_type);
        return type;
    }
    case PTLANG_AST_EXP_REFERENCE:
        return ptlang_ast_type_reference(ptlang_ir_builder_exp_type(exp->content.reference.value, ctx), exp->content.reference.writable);
    case PTLANG_AST_EXP_DEREFERENCE:
        return ptlang_ir_builder_exp_type(exp->content.unary_operator, ctx);
    }

    // LLVMSizeOf():
}

static LLVMValueRef ptlang_ir_builder_type_default_value(ptlang_ast_type type, ptlang_ir_builder_build_context *ctx)
{
    type = ptlang_ir_builder_unname_type(type, ctx->type_scope);
    LLVMTypeRef llvm_type = ptlang_ir_builder_type(type, ctx);
    switch (type->type)
    {
    case PTLANG_AST_TYPE_INTEGER:
        return LLVMConstInt(llvm_type, 0, false);
    case PTLANG_AST_TYPE_FLOAT:
        return LLVMConstReal(llvm_type, 0.0);
    case PTLANG_AST_TYPE_FUNCTION:
        return NULL;
    case PTLANG_AST_TYPE_HEAP_ARRAY:
        return LLVMConstStruct((LLVMValueRef[]){
                                   LLVMGetPoison(LLVMPointerType(ptlang_ir_builder_type(type->content.heap_array.type, ctx), 0)),
                                   LLVMConstInt(LLVMIntPtrType(ctx->target_info), 0, false),
                               },
                               2, false);
    case PTLANG_AST_TYPE_ARRAY:
    {
        LLVMValueRef element_value = ptlang_ir_builder_type_default_value(type->content.array.type, ctx);
        LLVMValueRef *element_values = malloc(sizeof(LLVMValueRef) * type->content.array.len);
        for (uint64_t i = 0; i < type->content.array.len; i++)
        {
            element_values[i] = element_value;
        }
        LLVMValueRef array = LLVMConstArray(ptlang_ir_builder_type(type->content.array.type, ctx), element_values, type->content.array.len);
        free(element_values);
        return array;
    }
    case PTLANG_AST_TYPE_REFERENCE:
        return NULL;
    case PTLANG_AST_TYPE_NAMED:
    {
        ptlang_ast_decl_list members = shget(ctx->struct_defs, type->content.name);
        LLVMValueRef *member_values = malloc(sizeof(LLVMValueRef) * members->count);
        for (uint64_t i = 0; i < members->count; i++)
        {
            member_values[i] = ptlang_ir_builder_type_default_value(members->decls[i]->type, ctx);
        }
        LLVMValueRef struct_ = LLVMConstNamedStruct(llvm_type, member_values, members->count);
        free(member_values);
        return struct_;
    }
    }
}

static LLVMValueRef ptlang_ir_builder_build_cast(LLVMValueRef input, ptlang_ast_type from, ptlang_ast_type to, ptlang_ir_builder_build_context *ctx)
{
    // from = ptlang_ir_builder_unname_type(from, ctx->type_scope);
    to = ptlang_ir_builder_unname_type(to, ctx->type_scope);

    if (from->type == PTLANG_AST_TYPE_INTEGER && to->type == PTLANG_AST_TYPE_INTEGER)
    {
        if (from->content.integer.size < to->content.integer.size)
        {
            if (from->content.integer.is_signed)
            {
                return LLVMBuildSExt(ctx->builder, input, ptlang_ir_builder_type(to, ctx), "cast");
            }
            else
            {
                return LLVMBuildZExt(ctx->builder, input, ptlang_ir_builder_type(to, ctx), "cast");
            }
        }
        else if (from->content.integer.size > to->content.integer.size)
        {
            return LLVMBuildTrunc(ctx->builder, input, ptlang_ir_builder_type(to, ctx), "cast");
        }
        else
        {
            return input;
        }
    }
    else if (from->type == PTLANG_AST_TYPE_FLOAT && to->type == PTLANG_AST_TYPE_FLOAT)
    {
        if (from->content.float_size < to->content.float_size)
        {
            return LLVMBuildFPExt(ctx->builder, input, ptlang_ir_builder_type(to, ctx), "cast");
        }
        else if (from->content.float_size > to->content.float_size)
        {
            return LLVMBuildFPTrunc(ctx->builder, input, ptlang_ir_builder_type(to, ctx), "cast");
        }
        else
        {
            return input;
        }
    }
    else if (from->type == PTLANG_AST_TYPE_INTEGER && to->type == PTLANG_AST_TYPE_FLOAT)
    {
        if (from->content.integer.is_signed)
        {
            return LLVMBuildSIToFP(ctx->builder, input, ptlang_ir_builder_type(to, ctx), "cast");
        }
        else
        {
            return LLVMBuildUIToFP(ctx->builder, input, ptlang_ir_builder_type(to, ctx), "cast");
        }
    }
    else if (from->type == PTLANG_AST_TYPE_FLOAT && to->type == PTLANG_AST_TYPE_INTEGER)
    {
        if (to->content.integer.is_signed)
        {
            return LLVMBuildFPToSI(ctx->builder, input, ptlang_ir_builder_type(to, ctx), "cast");
        }
        else
        {
            return LLVMBuildFPToUI(ctx->builder, input, ptlang_ir_builder_type(to, ctx), "cast");
        }
    }
    else
    {
        return input;
    }
}

static LLVMValueRef ptlang_ir_builder_exp_and_cast(ptlang_ast_exp exp, ptlang_ast_type type, ptlang_ir_builder_build_context *ctx)
{
    ptlang_ast_type from = ptlang_ir_builder_exp_type(exp, ctx);
    LLVMValueRef uncasted = ptlang_ir_builder_exp(exp, ctx);
    LLVMValueRef casted = ptlang_ir_builder_build_cast(uncasted, from, type, ctx);
    ptlang_ast_type_destroy(from);
    return casted;
}

static void ptlang_ir_builder_prepare_binary_op(ptlang_ast_exp exp, LLVMValueRef *left_value, LLVMValueRef *right_value, ptlang_ast_type *ret_type, ptlang_ir_builder_build_context *ctx)
{
    ptlang_ast_type left_type = ptlang_ir_builder_exp_type(exp->content.binary_operator.left_value, ctx);
    ptlang_ast_type right_type = ptlang_ir_builder_exp_type(exp->content.binary_operator.right_value, ctx);
    *ret_type = ptlang_ir_builder_combine_types(left_type, right_type, ctx->type_scope);
    LLVMValueRef left_value_uncasted = ptlang_ir_builder_exp(exp->content.binary_operator.left_value, ctx);
    *left_value = ptlang_ir_builder_build_cast(left_value_uncasted, left_type, *ret_type, ctx);
    LLVMValueRef right_value_uncasted = ptlang_ir_builder_exp(exp->content.binary_operator.right_value, ctx);
    *right_value = ptlang_ir_builder_build_cast(right_value_uncasted, right_type, *ret_type, ctx);
    ptlang_ast_type_destroy(left_type);
    ptlang_ast_type_destroy(right_type);
}

static LLVMValueRef ptlang_ir_builder_exp_as_bool(ptlang_ast_exp exp, ptlang_ir_builder_build_context *ctx)
{
    ptlang_ast_type type = ptlang_ir_builder_exp_type(exp, ctx);

    LLVMValueRef value = ptlang_ir_builder_exp(exp, ctx);

    LLVMTypeRef llvm_type = ptlang_ir_builder_type(type, ctx);

    if (type->type == PTLANG_AST_TYPE_INTEGER && type->content.integer.size != 1)
    {
        value = LLVMBuildICmp(ctx->builder, LLVMIntNE, value, LLVMConstInt(llvm_type, 0, false), "tobool");
    }
    else if (type->type == PTLANG_AST_TYPE_FLOAT)
    {
        value = LLVMBuildFCmp(ctx->builder, LLVMRealUNE, value, LLVMConstReal(llvm_type, 0.0), "tobool");
    }
    ptlang_ast_type_destroy(type);
    return value;
}

static LLVMValueRef ptlang_ir_builder_exp(ptlang_ast_exp exp, ptlang_ir_builder_build_context *ctx)
{
    switch (exp->type)
    {
    case PTLANG_AST_EXP_ASSIGNMENT:
    {
        ptlang_ast_type type = ptlang_ir_builder_exp_type(exp->content.binary_operator.left_value, ctx);
        LLVMValueRef value = ptlang_ir_builder_exp_and_cast(exp->content.binary_operator.right_value, type, ctx);
        ptlang_ast_type_destroy(type);

        if (exp->content.binary_operator.left_value->type != PTLANG_AST_EXP_LENGTH)
        {
            LLVMValueRef ptr = ptlang_ir_builder_exp_ptr(exp->content.binary_operator.left_value, ctx);
            LLVMBuildStore(ctx->builder, value, ptr);
        }
        else
        {
            ptlang_ir_builder_init_malloc_func(ctx);
            ptlang_ir_builder_init_realloc_func(ctx);
            ptlang_ir_builder_init_free_func(ctx);

            LLVMBasicBlockRef reallocstart_block = LLVMAppendBasicBlock(ctx->function, "reallocstart");
            LLVMBasicBlockRef reallocnewalloc_block = LLVMAppendBasicBlock(ctx->function, "reallocnewalloc");
            LLVMBasicBlockRef reallocorignotzero_block = LLVMAppendBasicBlock(ctx->function, "reallocorignotzero");
            LLVMBasicBlockRef reallocrealloc_block = LLVMAppendBasicBlock(ctx->function, "reallocrealloc");
            LLVMBasicBlockRef reallocstorenew_block = LLVMAppendBasicBlock(ctx->function, "reallocstorenew");
            LLVMBasicBlockRef reallocfree_block = LLVMAppendBasicBlock(ctx->function, "reallocfree");
            LLVMBasicBlockRef reallocend_block = LLVMAppendBasicBlock(ctx->function, "reallocend");

            LLVMValueRef heap_arr_ptr = ptlang_ir_builder_exp_ptr(exp->content.binary_operator.left_value->content.unary_operator, ctx);
            ptlang_ast_type heap_arr_type = ptlang_ir_builder_exp_type(exp->content.binary_operator.left_value->content.unary_operator, ctx);
            LLVMTypeRef heap_arr_llvm_type = ptlang_ir_builder_type(heap_arr_type, ctx);
            LLVMTypeRef heap_arr_element_type = ptlang_ir_builder_type(heap_arr_type->content.heap_array.type, ctx);
            ptlang_ast_type_destroy(heap_arr_type);
            LLVMValueRef heap_len_ptr = LLVMBuildStructGEP2(ctx->builder, heap_arr_llvm_type, heap_arr_ptr, 1, "reallocheaplenptr");

            LLVMValueRef orig_len = LLVMBuildLoad2(ctx->builder, LLVMIntPtrType(ctx->target_info), heap_len_ptr, "loadlen");

            LLVMValueRef cmplens = LLVMBuildICmp(ctx->builder, LLVMIntEQ, value, orig_len, "realloccmplens");
            LLVMBuildCondBr(ctx->builder, cmplens, reallocend_block, reallocstart_block);

            LLVMPositionBuilderAtEnd(ctx->builder, reallocstart_block);
            LLVMValueRef heap_ptr_ptr = LLVMBuildStructGEP2(ctx->builder, heap_arr_llvm_type, heap_arr_ptr, 0, "reallocheapptrptr");
            LLVMValueRef size = LLVMBuildNUWMul(ctx->builder, value, LLVMSizeOf(heap_arr_element_type), "realloccalcsize");

            LLVMValueRef cmporiglenzero = LLVMBuildICmp(ctx->builder, LLVMIntEQ, orig_len, LLVMConstInt(LLVMIntPtrType(ctx->target_info), 0, false), "realloccmporiglenzero");
            LLVMBuildCondBr(ctx->builder, cmporiglenzero, reallocnewalloc_block, reallocorignotzero_block);

            LLVMPositionBuilderAtEnd(ctx->builder, reallocnewalloc_block);

            LLVMValueRef mallocated = LLVMBuildCall2(ctx->builder, LLVMFunctionType(LLVMPointerType(LLVMInt8Type(), 0), (LLVMTypeRef[]){LLVMIntPtrType(ctx->target_info)}, 1, false), ctx->malloc_func, (LLVMValueRef[]){size}, 1, "malloc");
            // LLVMBuildStore(ctx->builder, mallocated, heap_ptr_ptr);
            LLVMBuildBr(ctx->builder, reallocstorenew_block);

            LLVMPositionBuilderAtEnd(ctx->builder, reallocorignotzero_block);

            LLVMValueRef heap_ptr = LLVMBuildLoad2(ctx->builder, LLVMPointerType(heap_arr_element_type, 0), heap_ptr_ptr, "reallocoldmemory");
            LLVMValueRef heap_byte_ptr = LLVMBuildPointerCast(ctx->builder, heap_ptr, LLVMPointerType(LLVMInt8Type(), 0), "realloctobyteptr");

            LLVMValueRef cmpnewlenzero = LLVMBuildICmp(ctx->builder, LLVMIntEQ, value, LLVMConstInt(LLVMIntPtrType(ctx->target_info), 0, false), "realloccmpnewlenzero");
            LLVMBuildCondBr(ctx->builder, cmpnewlenzero, reallocfree_block, reallocrealloc_block);

            LLVMPositionBuilderAtEnd(ctx->builder, reallocrealloc_block);

            LLVMValueRef reallocated = LLVMBuildCall2(ctx->builder, LLVMFunctionType(LLVMPointerType(LLVMInt8Type(), 0), (LLVMTypeRef[]){LLVMPointerType(LLVMInt8Type(), 0), LLVMIntPtrType(ctx->target_info)}, 2, false), ctx->realloc_func, (LLVMValueRef[]){heap_byte_ptr, size}, 2, "realloc");
            LLVMBuildBr(ctx->builder, reallocstorenew_block);

            LLVMPositionBuilderAtEnd(ctx->builder, reallocstorenew_block);
            LLVMValueRef phi = LLVMBuildPhi(ctx->builder, LLVMPointerType(LLVMInt8Type(), 0), "reallocnewmemory");
            LLVMAddIncoming(phi, (LLVMValueRef[]){mallocated, reallocated}, (LLVMBasicBlockRef[]){reallocnewalloc_block, reallocrealloc_block}, 2);

            LLVMValueRef new_heap_ptr = LLVMBuildPointerCast(ctx->builder, phi, LLVMPointerType(heap_arr_element_type, 0), "reallocfrombyteptr");

            LLVMBuildStore(ctx->builder, new_heap_ptr, heap_ptr_ptr);
            LLVMBuildStore(ctx->builder, value, heap_len_ptr);
            LLVMBuildBr(ctx->builder, reallocend_block);

            LLVMPositionBuilderAtEnd(ctx->builder, reallocfree_block);

            LLVMBuildCall2(ctx->builder, LLVMFunctionType(LLVMVoidType(), (LLVMTypeRef[]){LLVMPointerType(LLVMInt8Type(), 0)}, 1, false), ctx->free_func, (LLVMValueRef[]){heap_byte_ptr}, 1, "");

            LLVMBuildStore(ctx->builder, LLVMConstStruct((LLVMValueRef[]){
                                                             LLVMGetPoison(LLVMPointerType(heap_arr_element_type, 0)),
                                                             LLVMConstInt(LLVMIntPtrType(ctx->target_info), 0, false),
                                                         },
                                                         2, false),
                           heap_arr_ptr);

            LLVMBuildBr(ctx->builder, reallocend_block);

            LLVMPositionBuilderAtEnd(ctx->builder, reallocend_block);
        }

        return value;
    }
    case PTLANG_AST_EXP_ADDITION:
    {
        // ptlang_ast_type left_type = ptlang_ir_builder_exp_type(exp->content.binary_operator.left_value, ctx);
        // ptlang_ast_type right_type = ptlang_ir_builder_exp_type(exp->content.binary_operator.right_value, ctx);
        ptlang_ast_type ret_type; // = ptlang_ir_builder_combine_types(left_type, right_type, ctx->type_scope);
        LLVMValueRef left_value;  // = ptlang_ir_builder_exp(exp->content.binary_operator.left_value, ctx);
        // left_value = ptlang_ir_builder_build_cast(left_value, left_type, ret_type, ctx);
        LLVMValueRef right_value; // = ptlang_ir_builder_exp(exp->content.binary_operator.right_value, ctx);
        // right_value = ptlang_ir_builder_build_cast(right_value, right_type, ret_type, ctx);

        ptlang_ir_builder_prepare_binary_op(exp, &left_value, &right_value, &ret_type, ctx);

        LLVMValueRef ret_val;

        if (ret_type->type == PTLANG_AST_TYPE_INTEGER)
        {
            if (ret_type->content.integer.is_signed)
            {
                ret_val = LLVMBuildNSWAdd(ctx->builder, left_value, right_value, "add");
            }
            else
            {
                ret_val = LLVMBuildNUWAdd(ctx->builder, left_value, right_value, "add");
            }
        }
        else
        {
            ret_val = LLVMBuildFAdd(ctx->builder, left_value, right_value, "add");
        }
        ptlang_ast_type_destroy(ret_type);
        return ret_val;
    }
    case PTLANG_AST_EXP_SUBTRACTION:
    {
        ptlang_ast_type ret_type;
        LLVMValueRef left_value;
        LLVMValueRef right_value;

        ptlang_ir_builder_prepare_binary_op(exp, &left_value, &right_value, &ret_type, ctx);

        LLVMValueRef ret_val;

        if (ret_type->type == PTLANG_AST_TYPE_INTEGER)
        {
            if (ret_type->content.integer.is_signed)
            {
                ret_val = LLVMBuildNSWSub(ctx->builder, left_value, right_value, "sub");
            }
            else
            {
                ret_val = LLVMBuildNUWSub(ctx->builder, left_value, right_value, "sub");
            }
        }
        else
        {
            ret_val = LLVMBuildFSub(ctx->builder, left_value, right_value, "sub");
        }
        ptlang_ast_type_destroy(ret_type);
        return ret_val;
    }
    case PTLANG_AST_EXP_NEGATION:
    {
        ptlang_ast_type type = ptlang_ir_builder_exp_type(exp->content.unary_operator, ctx);
        LLVMValueRef input = ptlang_ir_builder_exp(exp->content.unary_operator, ctx);

        LLVMValueRef ret_val;

        if (type->type == PTLANG_AST_TYPE_INTEGER)
        {
            if (type->content.integer.is_signed)
            {
                ret_val = LLVMBuildNSWSub(ctx->builder, LLVMConstInt(LLVMIntType(type->content.integer.size), 0, false), input, "neg");
            }
            else
            {
                ret_val = LLVMBuildSub(ctx->builder, LLVMConstInt(LLVMIntType(type->content.integer.size), 0, false), input, "neg");
            }
        }
        else
        {
            ret_val = LLVMBuildFNeg(ctx->builder, input, "neg");
        }
        ptlang_ast_type_destroy(type);
        return ret_val;
    }
    case PTLANG_AST_EXP_MULTIPLICATION:
    {
        ptlang_ast_type ret_type;
        LLVMValueRef left_value;
        LLVMValueRef right_value;

        ptlang_ir_builder_prepare_binary_op(exp, &left_value, &right_value, &ret_type, ctx);

        LLVMValueRef ret_val;

        if (ret_type->type == PTLANG_AST_TYPE_INTEGER)
        {
            if (ret_type->content.integer.is_signed)
            {
                ret_val = LLVMBuildNSWMul(ctx->builder, left_value, right_value, "mul");
            }
            else
            {
                ret_val = LLVMBuildNUWMul(ctx->builder, left_value, right_value, "mul");
            }
        }
        else
        {
            ret_val = LLVMBuildFMul(ctx->builder, left_value, right_value, "mul");
        }
        ptlang_ast_type_destroy(ret_type);
        return ret_val;
    }
    case PTLANG_AST_EXP_DIVISION:
    {
        ptlang_ast_type ret_type;
        LLVMValueRef left_value;
        LLVMValueRef right_value;

        ptlang_ir_builder_prepare_binary_op(exp, &left_value, &right_value, &ret_type, ctx);

        LLVMValueRef ret_val;

        if (ret_type->type == PTLANG_AST_TYPE_INTEGER)
        {
            if (ret_type->content.integer.is_signed)
            {
                ret_val = LLVMBuildSDiv(ctx->builder, left_value, right_value, "div");
            }
            else
            {
                ret_val = LLVMBuildUDiv(ctx->builder, left_value, right_value, "div");
            }
        }
        else
        {
            ret_val = LLVMBuildFDiv(ctx->builder, left_value, right_value, "div");
        }
        ptlang_ast_type_destroy(ret_type);
        return ret_val;
    }
    case PTLANG_AST_EXP_MODULO:
    {
        ptlang_ast_type ret_type;
        LLVMValueRef left_value;
        LLVMValueRef right_value;

        ptlang_ir_builder_prepare_binary_op(exp, &left_value, &right_value, &ret_type, ctx);

        LLVMValueRef ret_val;

        if (ret_type->type == PTLANG_AST_TYPE_INTEGER)
        {
            if (ret_type->content.integer.is_signed)
            {
                ret_val = LLVMBuildSRem(ctx->builder, left_value, right_value, "mod");
                ret_val = LLVMBuildAdd(ctx->builder, ret_val, right_value, "mod");
                ret_val = LLVMBuildSRem(ctx->builder, ret_val, right_value, "mod");
            }
            else
            {
                ret_val = LLVMBuildURem(ctx->builder, left_value, right_value, "mod");
            }
        }
        else
        {
            ret_val = LLVMBuildFRem(ctx->builder, left_value, right_value, "mod");
            ret_val = LLVMBuildFAdd(ctx->builder, ret_val, right_value, "mod");
            ret_val = LLVMBuildFRem(ctx->builder, ret_val, right_value, "mod");
        }
        ptlang_ast_type_destroy(ret_type);
        return ret_val;
    }
    case PTLANG_AST_EXP_REMAINDER:
    {
        ptlang_ast_type ret_type;
        LLVMValueRef left_value;
        LLVMValueRef right_value;

        ptlang_ir_builder_prepare_binary_op(exp, &left_value, &right_value, &ret_type, ctx);

        LLVMValueRef ret_val;

        if (ret_type->type == PTLANG_AST_TYPE_INTEGER)
        {
            if (ret_type->content.integer.is_signed)
            {
                ret_val = LLVMBuildSRem(ctx->builder, left_value, right_value, "remainder");
            }
            else
            {
                ret_val = LLVMBuildURem(ctx->builder, left_value, right_value, "remainder");
            }
        }
        else
        {
            ret_val = LLVMBuildFRem(ctx->builder, left_value, right_value, "remainder");
        }
        ptlang_ast_type_destroy(ret_type);
        return ret_val;
    }
    case PTLANG_AST_EXP_EQUAL:
    {
        ptlang_ast_type ret_type;
        LLVMValueRef left_value;
        LLVMValueRef right_value;

        ptlang_ir_builder_prepare_binary_op(exp, &left_value, &right_value, &ret_type, ctx);

        LLVMValueRef ret_val;

        if (ret_type->type == PTLANG_AST_TYPE_INTEGER)
        {
            ret_val = LLVMBuildICmp(ctx->builder, LLVMIntEQ, left_value, right_value, "eq");
        }
        else
        {
            ret_val = LLVMBuildFCmp(ctx->builder, LLVMRealOEQ, left_value, right_value, "eq");
        }
        ptlang_ast_type_destroy(ret_type);
        return ret_val;
    }
    case PTLANG_AST_EXP_NOT_EQUAL:
    {
        ptlang_ast_type ret_type;
        LLVMValueRef left_value;
        LLVMValueRef right_value;

        ptlang_ir_builder_prepare_binary_op(exp, &left_value, &right_value, &ret_type, ctx);

        LLVMValueRef ret_val;

        if (ret_type->type == PTLANG_AST_TYPE_INTEGER)
        {
            ret_val = LLVMBuildICmp(ctx->builder, LLVMIntNE, left_value, right_value, "neq");
        }
        else
        {
            ret_val = LLVMBuildFCmp(ctx->builder, LLVMRealUNE, left_value, right_value, "neq");
        }
        ptlang_ast_type_destroy(ret_type);
        return ret_val;
    }
    case PTLANG_AST_EXP_GREATER:
    {
        ptlang_ast_type ret_type;
        LLVMValueRef left_value;
        LLVMValueRef right_value;

        ptlang_ir_builder_prepare_binary_op(exp, &left_value, &right_value, &ret_type, ctx);

        LLVMValueRef ret_val;

        if (ret_type->type == PTLANG_AST_TYPE_INTEGER)
        {

            if (ret_type->content.integer.is_signed)
            {
                ret_val = LLVMBuildICmp(ctx->builder, LLVMIntSGT, left_value, right_value, "gt");
            }
            else
            {
                ret_val = LLVMBuildICmp(ctx->builder, LLVMIntUGT, left_value, right_value, "gt");
            }
        }
        else
        {
            ret_val = LLVMBuildFCmp(ctx->builder, LLVMRealOGT, left_value, right_value, "gt");
        }
        ptlang_ast_type_destroy(ret_type);
        return ret_val;
    }
    case PTLANG_AST_EXP_GREATER_EQUAL:
    {
        ptlang_ast_type ret_type;
        LLVMValueRef left_value;
        LLVMValueRef right_value;

        ptlang_ir_builder_prepare_binary_op(exp, &left_value, &right_value, &ret_type, ctx);

        LLVMValueRef ret_val;

        if (ret_type->type == PTLANG_AST_TYPE_INTEGER)
        {

            if (ret_type->content.integer.is_signed)
            {
                ret_val = LLVMBuildICmp(ctx->builder, LLVMIntSGE, left_value, right_value, "gte");
            }
            else
            {
                ret_val = LLVMBuildICmp(ctx->builder, LLVMIntUGE, left_value, right_value, "gte");
            }
        }
        else
        {
            ret_val = LLVMBuildFCmp(ctx->builder, LLVMRealOGE, left_value, right_value, "gte");
        }
        ptlang_ast_type_destroy(ret_type);
        return ret_val;
    }
    case PTLANG_AST_EXP_LESS:
    {
        ptlang_ast_type ret_type;
        LLVMValueRef left_value;
        LLVMValueRef right_value;

        ptlang_ir_builder_prepare_binary_op(exp, &left_value, &right_value, &ret_type, ctx);

        LLVMValueRef ret_val;

        if (ret_type->type == PTLANG_AST_TYPE_INTEGER)
        {

            if (ret_type->content.integer.is_signed)
            {
                ret_val = LLVMBuildICmp(ctx->builder, LLVMIntSLT, left_value, right_value, "lt");
            }
            else
            {
                ret_val = LLVMBuildICmp(ctx->builder, LLVMIntULT, left_value, right_value, "lt");
            }
        }
        else
        {
            ret_val = LLVMBuildFCmp(ctx->builder, LLVMRealOLT, left_value, right_value, "lt");
        }
        ptlang_ast_type_destroy(ret_type);
        return ret_val;
    }
    case PTLANG_AST_EXP_LESS_EQUAL:
    {
        ptlang_ast_type ret_type;
        LLVMValueRef left_value;
        LLVMValueRef right_value;

        ptlang_ir_builder_prepare_binary_op(exp, &left_value, &right_value, &ret_type, ctx);

        LLVMValueRef ret_val;

        if (ret_type->type == PTLANG_AST_TYPE_INTEGER)
        {

            if (ret_type->content.integer.is_signed)
            {
                ret_val = LLVMBuildICmp(ctx->builder, LLVMIntSLE, left_value, right_value, "lte");
            }
            else
            {
                ret_val = LLVMBuildICmp(ctx->builder, LLVMIntULE, left_value, right_value, "lte");
            }
        }
        else
        {
            ret_val = LLVMBuildFCmp(ctx->builder, LLVMRealOLE, left_value, right_value, "lte");
        }
        ptlang_ast_type_destroy(ret_type);
        return ret_val;
    }
    case PTLANG_AST_EXP_LEFT_SHIFT:
    {
        ptlang_ast_type ret_type;
        LLVMValueRef left_value;
        LLVMValueRef right_value;

        ptlang_ir_builder_prepare_binary_op(exp, &left_value, &right_value, &ret_type, ctx);

        ptlang_ast_type type = ptlang_ir_builder_exp_type(exp->content.binary_operator.left_value, ctx);

        LLVMValueRef ret_val = LLVMBuildShl(ctx->builder, left_value, right_value, "leftshift");

        ret_val = ptlang_ir_builder_build_cast(ret_val, ret_type, type, ctx);
        ptlang_ast_type_destroy(ret_type);

        ptlang_ast_type_destroy(type);
        return ret_val;
    }
    case PTLANG_AST_EXP_RIGHT_SHIFT:
    {
        ptlang_ast_type ret_type;
        LLVMValueRef left_value;
        LLVMValueRef right_value;

        ptlang_ir_builder_prepare_binary_op(exp, &left_value, &right_value, &ret_type, ctx);

        ptlang_ast_type type = ptlang_ir_builder_exp_type(exp->content.binary_operator.left_value, ctx);

        LLVMValueRef ret_val;
        if (type->content.integer.is_signed)
        {
            ret_val = LLVMBuildAShr(ctx->builder, left_value, right_value, "rightshift");
        }
        else
        {
            ret_val = LLVMBuildLShr(ctx->builder, left_value, right_value, "rightshift");
        }
        ret_val = ptlang_ir_builder_build_cast(ret_val, ret_type, type, ctx);
        ptlang_ast_type_destroy(ret_type);

        ptlang_ast_type_destroy(type);
        return ret_val;
    }
    case PTLANG_AST_EXP_AND:
    {
        LLVMValueRef left = ptlang_ir_builder_exp_as_bool(exp->content.binary_operator.left_value, ctx);

        LLVMBasicBlockRef second_block = LLVMAppendBasicBlock(ctx->function, "andright");
        LLVMBasicBlockRef and_end = LLVMAppendBasicBlock(ctx->function, "andend");

        LLVMValueRef first_br = LLVMBuildCondBr(ctx->builder, left, second_block, and_end);

        LLVMPositionBuilderAtEnd(ctx->builder, second_block);
        LLVMValueRef right = ptlang_ir_builder_exp_as_bool(exp->content.binary_operator.right_value, ctx);
        LLVMValueRef second_br = LLVMBuildBr(ctx->builder, and_end);

        LLVMPositionBuilderAtEnd(ctx->builder, and_end);
        LLVMValueRef phi = LLVMBuildPhi(ctx->builder, LLVMInt1Type(), "and");
        LLVMAddIncoming(phi, (LLVMValueRef[]){LLVMConstInt(LLVMInt1Type(), 0, false), right}, (LLVMBasicBlockRef[]){LLVMGetInstructionParent(first_br), LLVMGetInstructionParent(second_br)}, 2);

        return phi;
    }
    case PTLANG_AST_EXP_OR:
    {
        LLVMValueRef left = ptlang_ir_builder_exp_as_bool(exp->content.binary_operator.left_value, ctx);

        LLVMBasicBlockRef second_block = LLVMAppendBasicBlock(ctx->function, "orright");
        LLVMBasicBlockRef or_end = LLVMAppendBasicBlock(ctx->function, "orend");

        LLVMValueRef first_br = LLVMBuildCondBr(ctx->builder, left, or_end, second_block);

        LLVMPositionBuilderAtEnd(ctx->builder, second_block);
        LLVMValueRef right = ptlang_ir_builder_exp_as_bool(exp->content.binary_operator.right_value, ctx);
        LLVMValueRef second_br = LLVMBuildBr(ctx->builder, or_end);

        LLVMPositionBuilderAtEnd(ctx->builder, or_end);
        LLVMValueRef phi = LLVMBuildPhi(ctx->builder, LLVMInt1Type(), "or");
        LLVMAddIncoming(phi, (LLVMValueRef[]){LLVMConstInt(LLVMInt1Type(), 1, false), right}, (LLVMBasicBlockRef[]){LLVMGetInstructionParent(first_br), LLVMGetInstructionParent(second_br)}, 2);

        return phi;
    }
    case PTLANG_AST_EXP_NOT:
        return LLVMBuildSelect(ctx->builder, ptlang_ir_builder_exp_as_bool(exp->content.unary_operator, ctx), LLVMConstInt(LLVMInt1Type(), 0, false), LLVMConstInt(LLVMInt1Type(), 1, false), "not");
    case PTLANG_AST_EXP_BITWISE_AND:
    {
        ptlang_ast_type ret_type;
        LLVMValueRef left_value;
        LLVMValueRef right_value;

        ptlang_ir_builder_prepare_binary_op(exp, &left_value, &right_value, &ret_type, ctx);

        LLVMValueRef ret_val = LLVMBuildAnd(ctx->builder, left_value, right_value, "bitwiseand");

        ptlang_ast_type_destroy(ret_type);
        return ret_val;
    }
    case PTLANG_AST_EXP_BITWISE_OR:
    {
        ptlang_ast_type ret_type;
        LLVMValueRef left_value;
        LLVMValueRef right_value;

        ptlang_ir_builder_prepare_binary_op(exp, &left_value, &right_value, &ret_type, ctx);

        LLVMValueRef ret_val = LLVMBuildOr(ctx->builder, left_value, right_value, "bitwiseor");

        ptlang_ast_type_destroy(ret_type);
        return ret_val;
    }
    case PTLANG_AST_EXP_BITWISE_XOR:
    {
        ptlang_ast_type ret_type;
        LLVMValueRef left_value;
        LLVMValueRef right_value;

        ptlang_ir_builder_prepare_binary_op(exp, &left_value, &right_value, &ret_type, ctx);

        LLVMValueRef ret_val = LLVMBuildXor(ctx->builder, left_value, right_value, "bitwisexor");

        ptlang_ast_type_destroy(ret_type);
        return ret_val;
    }
    case PTLANG_AST_EXP_BITWISE_INVERSE:
    {
        ptlang_ast_type type = ptlang_ir_builder_exp_type(exp->content.unary_operator, ctx);
        LLVMValueRef input = ptlang_ir_builder_exp(exp->content.unary_operator, ctx);

        LLVMValueRef ret_val = LLVMBuildXor(ctx->builder, input, LLVMConstInt(LLVMIntType(type->content.integer.size), -1, true), "bitwiseinverse");

        ptlang_ast_type_destroy(type);
        return ret_val;
    }
    case PTLANG_AST_EXP_LENGTH:
    {
        LLVMValueRef ptr = ptlang_ir_builder_exp_ptr(exp, ctx);
        if (ptr != NULL)
        {
            return LLVMBuildLoad2(ctx->builder, LLVMIntPtrType(ctx->target_info), ptr, "loadlen");
        }
        else
        {
            return LLVMBuildExtractValue(ctx->builder, ptlang_ir_builder_exp(exp->content.unary_operator, ctx), 1, "extractlen");
        }
        return NULL;
    }
    case PTLANG_AST_EXP_FUNCTION_CALL:
    {
        ptlang_ast_type function_type = ptlang_ir_builder_exp_type(exp->content.function_call.function, ctx);

        LLVMValueRef *args = malloc(sizeof(LLVMValueRef) * exp->content.function_call.parameters->count);
        LLVMTypeRef *param_types = malloc(sizeof(LLVMTypeRef) * exp->content.function_call.parameters->count);
        for (uint64_t i = 0; i < exp->content.function_call.parameters->count; i++)
        {
            args[i] = ptlang_ir_builder_exp_and_cast(exp->content.function_call.parameters->exps[i], function_type->content.function.parameters->types[i], ctx);

            param_types[i] = ptlang_ir_builder_type(function_type->content.function.parameters->types[i], ctx);
        }

        LLVMTypeRef type = LLVMFunctionType(ptlang_ir_builder_type(function_type->content.function.return_type, ctx), param_types, exp->content.function_call.parameters->count, false);
        free(param_types);

        // LLVMTypeRef type = ptlang_ir_builder_type(function_type_unnamed, ctx);

        LLVMValueRef function_value = ptlang_ir_builder_exp(exp->content.function_call.function, ctx);
        LLVMValueRef ret_val = LLVMBuildCall2(ctx->builder, type, function_value, args, exp->content.function_call.parameters->count, "funccall");
        free(args);
        ptlang_ast_type_destroy(function_type);
        return ret_val;
    }
    case PTLANG_AST_EXP_VARIABLE:
    {
        // return LLVMBuildLoad2(builder, );
        ptlang_ast_type type;
        bool direct;
        LLVMValueRef ptr = ptlang_ir_builder_scope_get(ctx->scope, exp->content.str_prepresentation, &type, &direct);
        if (direct)
        {
            return ptr;
        }
        LLVMTypeRef t = ptlang_ir_builder_type(type, ctx);
        return LLVMBuildLoad2(ctx->builder, t, ptr, "loadvar");
    }
    case PTLANG_AST_EXP_INTEGER:
    {
        size_t str_len;

        char *suffix_begin = strchr(exp->content.str_prepresentation, 'u');
        if (suffix_begin == NULL)
            suffix_begin = strchr(exp->content.str_prepresentation, 'U');
        if (suffix_begin == NULL)
            suffix_begin = strchr(exp->content.str_prepresentation, 's');
        if (suffix_begin == NULL)
            suffix_begin = strchr(exp->content.str_prepresentation, 'S');

        if (suffix_begin == NULL)
        {
            str_len = strlen(exp->content.str_prepresentation);
        }
        else
        {
            str_len = suffix_begin - exp->content.str_prepresentation;
        }

        ptlang_ast_type type = ptlang_ir_builder_exp_type(exp, ctx);

        LLVMValueRef value = LLVMConstIntOfStringAndSize(ptlang_ir_builder_type(type, ctx), exp->content.str_prepresentation, str_len, 10);
        ptlang_ast_type_destroy(type);
        return value;
    }
    case PTLANG_AST_EXP_FLOAT:
    {
        size_t str_len;

        char *suffix_begin = strchr(exp->content.str_prepresentation, 'f');
        if (suffix_begin == NULL)
            suffix_begin = strchr(exp->content.str_prepresentation, 'F');

        if (suffix_begin == NULL)
        {
            str_len = strlen(exp->content.str_prepresentation);
        }
        else
        {
            str_len = suffix_begin - exp->content.str_prepresentation;
        }

        ptlang_ast_type type = ptlang_ir_builder_exp_type(exp, ctx);

        LLVMValueRef value = LLVMConstRealOfStringAndSize(ptlang_ir_builder_type(type, ctx), exp->content.str_prepresentation, str_len);
        ptlang_ast_type_destroy(type);
        return value;
    }
    case PTLANG_AST_EXP_STRUCT:
    {
        ptlang_ast_type type = ptlang_ir_builder_exp_type(exp, ctx);
        ptlang_ast_decl_list members = shget(ctx->struct_defs, type->content.name);
        LLVMTypeRef struct_type = ptlang_ir_builder_type(type, ctx);
        ptlang_ast_type_destroy(type);

        LLVMValueRef *struct_members = malloc(sizeof(LLVMValueRef) * members->count);
        for (uint64_t i = 0; i < members->count; i++)
        {
            LLVMValueRef member_val = NULL;
            for (uint64_t j = 0; j < exp->content.struct_.members->count; j++)
            {
                if (strcmp(members->decls[i]->name, exp->content.struct_.members->str_exps[j].str) == 0)
                {
                    member_val = ptlang_ir_builder_exp_and_cast(exp->content.struct_.members->str_exps[j].exp, members->decls[i]->type, ctx);
                    break;
                }
            }

            if (member_val == NULL)
            {
                member_val = ptlang_ir_builder_type_default_value(members->decls[i]->type, ctx);
                // member_val = LLVMGetUndef(ptlang_ir_builder_type(members->decls[i]->type, ctx));
            }

            struct_members[i] = member_val;
        }

        LLVMValueRef struct_ = LLVMConstNamedStruct(struct_type, struct_members, members->count);
        free(struct_members);
        return struct_;
    }
    case PTLANG_AST_EXP_ARRAY:
    {
        ptlang_ast_type unnamed_type = ptlang_ir_builder_unname_type(exp->content.array.type, ctx->type_scope);

        LLVMTypeRef element_type = ptlang_ir_builder_type(unnamed_type->content.array.type, ctx);
        LLVMValueRef *array_elements = malloc(sizeof(LLVMValueRef) * unnamed_type->content.array.len);

        uint64_t i = 0;
        for (; i < exp->content.array.values->count; i++)
        {
            array_elements[i] = ptlang_ir_builder_exp_and_cast(exp->content.array.values->exps[i], unnamed_type->content.array.type, ctx);
        }
        LLVMValueRef default_element_value = ptlang_ir_builder_type_default_value(unnamed_type->content.array.type, ctx);
        for (; i < unnamed_type->content.array.len; i++)
        {
            array_elements[i] = default_element_value;
        }

        LLVMValueRef array = LLVMConstArray(element_type, array_elements, unnamed_type->content.array.len);
        return array;
    }
    case PTLANG_AST_EXP_TERNARY:
    {
        ptlang_ast_type type = ptlang_ir_builder_exp_type(exp, ctx);
        LLVMValueRef condition = ptlang_ir_builder_exp_as_bool(exp->content.ternary_operator.condition, ctx);

        LLVMBasicBlockRef if_block = LLVMAppendBasicBlock(ctx->function, "ternaryif");
        LLVMBasicBlockRef else_block = LLVMAppendBasicBlock(ctx->function, "ternaryelse");
        LLVMBasicBlockRef end_block = LLVMAppendBasicBlock(ctx->function, "ternaryend");

        LLVMBuildCondBr(ctx->builder, condition, if_block, else_block);

        LLVMPositionBuilderAtEnd(ctx->builder, if_block);
        LLVMValueRef if_value = ptlang_ir_builder_exp_and_cast(exp->content.ternary_operator.if_value, type, ctx);
        LLVMValueRef if_br = LLVMBuildBr(ctx->builder, end_block);

        LLVMPositionBuilderAtEnd(ctx->builder, else_block);
        LLVMValueRef else_value = ptlang_ir_builder_exp_and_cast(exp->content.ternary_operator.else_value, type, ctx);
        LLVMValueRef else_br = LLVMBuildBr(ctx->builder, end_block);

        LLVMPositionBuilderAtEnd(ctx->builder, end_block);

        LLVMValueRef phi = LLVMBuildPhi(ctx->builder, ptlang_ir_builder_type(type, ctx), "ternary");

        ptlang_ast_type_destroy(type);

        LLVMAddIncoming(phi, (LLVMValueRef[]){if_value, else_value}, (LLVMBasicBlockRef[]){LLVMGetInstructionParent(if_br), LLVMGetInstructionParent(else_br)}, 2);

        return phi;
    }
    case PTLANG_AST_EXP_CAST:
        return ptlang_ir_builder_exp_and_cast(exp->content.cast.value, exp->content.cast.type, ctx);
    case PTLANG_AST_EXP_STRUCT_MEMBER:
    {
        ptlang_ast_type type = ptlang_ir_builder_exp_type(exp, ctx);
        LLVMTypeRef ret_type = ptlang_ir_builder_type(type, ctx);
        ptlang_ast_type_destroy(type);

        LLVMValueRef as_ptr = ptlang_ir_builder_exp_ptr(exp, ctx);
        if (as_ptr != NULL)
        {
            return LLVMBuildLoad2(ctx->builder, ret_type, as_ptr, "structmember");
        }
        else
        {
            ptlang_ast_type struct_type = ptlang_ir_builder_exp_type(exp->content.struct_member.struct_, ctx);
            ptlang_ast_decl_list members = shget(ctx->struct_defs, struct_type->content.name);
            ptlang_ast_type_destroy(struct_type);

            for (uint64_t i = 0; i < members->count; i++)
            {
                if (strcmp(members->decls[i]->name, exp->content.struct_member.member_name) == 0)
                {
                    return LLVMBuildExtractValue(ctx->builder, ptlang_ir_builder_exp(exp->content.struct_member.struct_, ctx), i, "structmember");
                }
            }
            return NULL;
        }
    }
    case PTLANG_AST_EXP_ARRAY_ELEMENT:
    {
        ptlang_ast_type type = ptlang_ir_builder_exp_type(exp, ctx);
        LLVMTypeRef ret_type = ptlang_ir_builder_type(type, ctx);
        ptlang_ast_type_destroy(type);

        LLVMValueRef as_ptr = ptlang_ir_builder_exp_ptr(exp, ctx);
        if (as_ptr != NULL)
        {
            return LLVMBuildLoad2(ctx->builder, ret_type, as_ptr, "arrayelement");
        }
        // else
        // {
        //     LLVMGEP
        //     return LLVMBuildExtractValue(ctx->builder, ptlang_ir_builder_exp(exp->content.array_element.array, ctx), ptlang_ir_builder_exp(exp->content.array_element.index), "arrayelement");
        // }
    }
    case PTLANG_AST_EXP_REFERENCE:
        return NULL;
    case PTLANG_AST_EXP_DEREFERENCE:
        return NULL;
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
        ctx->scope_number++;
        ctx->scopes = realloc(ctx->scopes, sizeof(ptlang_ir_builder_scope *) * ctx->scope_number);
        ctx->scope = malloc(sizeof(ptlang_ir_builder_scope));
        ctx->scopes[ctx->scope_number - 1] = ctx->scope;
        ptlang_ir_builder_new_scope(old_scope, ctx->scope);
        for (uint64_t i = 0; i < stmt->content.block.stmt_count; i++)
        {
            ptlang_ir_builder_stmt_allocas(stmt->content.block.stmts[i], ctx);
        }
        ctx->scope = old_scope;
        break;
    }
    case PTLANG_AST_STMT_DECL:
        ptlang_ir_builder_scope_add(ctx->scope, stmt->content.decl->name, LLVMBuildAlloca(ctx->builder, ptlang_ir_builder_type(stmt->content.decl->type, ctx), stmt->content.decl->name), stmt->content.decl->type, false);
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
// static void ptlang_ir_builder_stmt(ptlang_ast_stmt stmt, LLVMBuilderRef builder, LLVMValueRef function, ptlang_ir_builder_type_scope *type_scope, ptlang_ir_builder_scope *scope, uint64_t *scope_number, ptlang_ir_builder_scope *scopes, LLVMValueRef return_ptr, LLVMBasicBlockRef return_block)
{

    switch (stmt->type)
    {
    case PTLANG_AST_STMT_BLOCK:
    {
        ptlang_ir_builder_scope *old_scope = ctx->scope;
        ctx->scope = ctx->scopes[ctx->scope_number];
        ctx->scope_number++;
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
    case PTLANG_AST_STMT_DECL:
    {
        ptlang_ast_type type;
        LLVMValueRef ptr = ptlang_ir_builder_scope_get(ctx->scope, stmt->content.decl->name, &type, NULL);
        LLVMBuildStore(ctx->builder, ptlang_ir_builder_type_default_value(type, ctx), ptr);
        break;
    }
    case PTLANG_AST_STMT_IF:
    {
        LLVMBasicBlockRef if_block = LLVMAppendBasicBlock(ctx->function, "if");
        LLVMBasicBlockRef endif_block = LLVMAppendBasicBlock(ctx->function, "endif");

        LLVMBuildCondBr(ctx->builder, ptlang_ir_builder_exp_as_bool(stmt->content.control_flow.condition, ctx), if_block, endif_block);

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

        LLVMBuildCondBr(ctx->builder, ptlang_ir_builder_exp_as_bool(stmt->content.control_flow2.condition, ctx), if_block, else_block);

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
        LLVMBuildCondBr(ctx->builder, ptlang_ir_builder_exp_as_bool(stmt->content.control_flow.condition, ctx), stmt_block, endwhile_block);

        LLVMPositionBuilderAtEnd(ctx->builder, stmt_block);
        ptlang_ir_builder_stmt(stmt->content.control_flow.stmt, ctx);
        LLVMBuildBr(ctx->builder, condition_block);

        LLVMPositionBuilderAtEnd(ctx->builder, endwhile_block);
        break;
    }
    case PTLANG_AST_STMT_RETURN:
        if (stmt->content.exp != NULL)
        {
            // ptlang_ast_type type = ptlang_ir_builder_exp_type(stmt->content.exp, ctx);
            // LLVMValueRef uncasted = ptlang_ir_builder_exp(stmt->content.exp, ctx);
            // LLVMValueRef casted = ptlang_ir_builder_build_cast(uncasted, type, ctx->return_type, ctx);
            // ptlang_ast_type_destroy(type);
            LLVMBuildStore(ctx->builder, ptlang_ir_builder_exp_and_cast(stmt->content.exp, ctx->return_type, ctx), ctx->return_ptr);
        }
        LLVMBuildBr(ctx->builder, ctx->return_block);
        LLVMPositionBuilderAtEnd(ctx->builder, LLVMAppendBasicBlock(ctx->function, "impossible"));
        break;
    case PTLANG_AST_STMT_RET_VAL:
        // ptlang_ast_type type = ptlang_ir_builder_exp_type(stmt->content.exp, ctx);
        // LLVMValueRef uncasted = ptlang_ir_builder_exp(stmt->content.exp, ctx);
        // LLVMValueRef casted = ptlang_ir_builder_build_cast(uncasted, type, ctx->return_type, ctx);
        // ptlang_ast_type_destroy(type);
        LLVMBuildStore(ctx->builder, ptlang_ir_builder_exp_and_cast(stmt->content.exp, ctx->return_type, ctx), ctx->return_ptr);
        break;
    case PTLANG_AST_STMT_BREAK:
    case PTLANG_AST_STMT_CONTINUE:
        break;
    default:
        break;
    }
}
#endif

typedef struct ptlang_ir_builder_type_alias_s ptlang_ir_builder_type_alias;
struct ptlang_ir_builder_type_alias_s
{
    ptlang_ast_type type;
    char *name;
    char **referenced_types;
    ptlang_ir_builder_type_alias **referencing_types;
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
        if (shgeti(type_scope, ast_type->content.name) == -1)
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
    // ptlang_ir_builder_type_alias type_alias = malloc(sizeof(struct ptlang_ir_builder_type_alias_s));

    return (struct ptlang_ir_builder_type_alias_s){
        .name = ast_type_alias.name,
        .type = ast_type_alias.type,
        .referenced_types = ptlang_ir_builder_type_alias_get_referenced_types_from_ast_type(ast_type_alias.type, type_scope),
        .referencing_types = NULL,
        .resolved = false,
    };
    // return type_alias;
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

static void ptlang_ir_builder_type_add_attributes(ptlang_ast_type type, LLVMValueRef function, LLVMAttributeIndex index, ptlang_ir_builder_build_context *ctx)
{
    type = ptlang_ir_builder_unname_type(type, ctx->type_scope);
    switch (type->type)
    {
    case PTLANG_AST_TYPE_INTEGER:
        if (type->content.integer.is_signed)
        {
            LLVMAddAttributeAtIndex(function, index, LLVMCreateEnumAttribute(LLVMGetGlobalContext(), LLVMGetEnumAttributeKindForName("signext", sizeof("signext") - 1), 0));
        }
        else
        {
            LLVMAddAttributeAtIndex(function, index, LLVMCreateEnumAttribute(LLVMGetGlobalContext(), LLVMGetEnumAttributeKindForName("zeroext", sizeof("zeroext") - 1), 0));
        }
        break;
    default:
        break;
    }
}

LLVMModuleRef ptlang_ir_builder_module(ptlang_ast_module ast_module, LLVMTargetDataRef target_info)
{

    LLVMContextSetOpaquePointers(LLVMGetGlobalContext(), false);
    LLVMModuleRef llvm_module = LLVMModuleCreateWithName("t");
    LLVMSetModuleDataLayout(llvm_module, target_info);

    ptlang_ir_builder_build_context ctx = {
        .builder = LLVMCreateBuilder(),
        .module = llvm_module,
        .target_info = target_info,
    };

    ptlang_ir_builder_scope global_scope = {.entry_count = 0};
    // ptlang_ir_builder_type_scope *type_scope = NULL;

    // ptlang_ir_builder_struct_def *struct_defs = NULL;

    LLVMTypeRef *structs = malloc(sizeof(LLVMTypeRef) * ast_module->struct_def_count);
    for (uint64_t i = 0; i < ast_module->struct_def_count; i++)
    {
        structs[i] = LLVMStructCreateNamed(LLVMGetGlobalContext(), ast_module->struct_defs[i]->name);
        shput(ctx.type_scope, ast_module->struct_defs[i]->name, ((ptlang_ir_builder_type_scope_entry){
                                                                    .type = structs[i],
                                                                }));

        shput(ctx.struct_defs, ast_module->struct_defs[i]->name, ast_module->struct_defs[i]->members);
    }

    // Type aliases

    ptlang_ir_type_alias_table alias_table = NULL;

    // ptlang_ir_builder_type_alias primitive_alias = ;

    shput(alias_table, "", ((ptlang_ir_builder_type_alias){
                               .name = "",
                               .type = NULL,
                               .referenced_types = NULL,
                               .referencing_types = NULL,
                               .resolved = false,
                           }));

    for (uint64_t i = 0; i < ast_module->type_alias_count; i++)
    {
        ptlang_ir_builder_type_alias type_alias = ptlang_ir_builder_type_alias_create(ast_module->type_aliases[i], ctx.type_scope);
        shput(alias_table, ast_module->type_aliases[i].name, type_alias);
    }

    for (uint64_t i = 0; i < ast_module->type_alias_count; i++)
    {
        ptlang_ir_builder_type_alias *type_alias = &shget(alias_table, ast_module->type_aliases[i].name);
        for (size_t j = 0; j < arrlenu(type_alias->referenced_types); j++)
        {
            size_t index = shgeti(alias_table, type_alias->referenced_types[j]);
            arrput(alias_table[index].value.referencing_types, type_alias);
        }
    }

    ptlang_ir_builder_type_alias **alias_candidates = NULL;
    arrput(alias_candidates, &alias_table[0].value);
    while (arrlenu(alias_candidates) != 0)
    {
        ptlang_ir_builder_type_alias *type_alias = arrpop(alias_candidates);

        // check if resolved
        if (type_alias->resolved)
            continue;
        type_alias->resolved = true;
        for (size_t i = 0; i < arrlenu(type_alias->referenced_types); i++)
        {
            if (!shget(alias_table, type_alias->referenced_types[i]).resolved)
            {
                type_alias->resolved = false;
                break;
            }
        }
        if (!type_alias->resolved)
            continue;

        // add type to type scope
        if (*type_alias->name != '\0')
        {
            shput(ctx.type_scope, type_alias->name, ((ptlang_ir_builder_type_scope_entry){
                                                        .type = ptlang_ir_builder_type(type_alias->type, &ctx),
                                                        .ptlang_type = ptlang_ir_builder_unname_type(type_alias->type, ctx.type_scope),
                                                    }));
        }

        // add referencing types to candidates
        for (size_t i = 0; i < arrlenu(type_alias->referencing_types); i++)
        {
            arrput(alias_candidates, type_alias->referencing_types[i]);
        }
    }

    arrfree(alias_candidates);

    for (uint64_t i = 0; i < ast_module->type_alias_count + 1; i++)
    {
        ptlang_ir_builder_type_alias type_alias = alias_table[i].value;
        arrfree(type_alias.referenced_types);
        arrfree(type_alias.referencing_types);
    }

    shfree(alias_table);

    for (uint64_t i = 0; i < ast_module->struct_def_count; i++)
    {
        LLVMTypeRef *elements = malloc(sizeof(LLVMTypeRef) * ast_module->struct_defs[i]->members->count);
        for (uint64_t j = 0; j < ast_module->struct_defs[i]->members->count; j++)
        {
            elements[j] = ptlang_ir_builder_type(ast_module->struct_defs[i]->members->decls[j]->type, &ctx);
        }
        LLVMStructSetBody(structs[i], elements, ast_module->struct_defs[i]->members->count, false);
        free(elements);
    }
    free(structs);

    LLVMValueRef *glob_decl_values = malloc(sizeof(LLVMValueRef) * ast_module->declaration_count);
    for (uint64_t i = 0; i < ast_module->declaration_count; i++)
    {
        LLVMTypeRef t = ptlang_ir_builder_type(ast_module->declarations[i]->type, &ctx);
        glob_decl_values[i] = LLVMAddGlobal(llvm_module, t, ast_module->declarations[i]->name);
        LLVMSetGlobalConstant(glob_decl_values[i], !ast_module->declarations[i]->writable);
        LLVMSetLinkage(glob_decl_values[i], ast_module->declarations[i]->export ? LLVMExternalLinkage : LLVMInternalLinkage);
        ptlang_ir_builder_scope_add(&global_scope, ast_module->declarations[i]->name, glob_decl_values[i], ast_module->declarations[i]->type, false);
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

            param_types[j] = ptlang_ir_builder_type(ast_module->functions[i]->parameters->decls[j]->type, &ctx);
        }
        LLVMTypeRef function_type = LLVMFunctionType(ptlang_ir_builder_type(ast_module->functions[i]->return_type, &ctx), param_types, ast_module->functions[i]->parameters->count, false);
        free(param_types);
        functions[i] = LLVMAddFunction(llvm_module, ast_module->functions[i]->name, function_type);
        LLVMSetLinkage(functions[i], ast_module->functions[i]->export ? LLVMExternalLinkage : LLVMInternalLinkage);

        if (ast_module->functions[i]->return_type != NULL)
        {
            ptlang_ir_builder_type_add_attributes(ast_module->functions[i]->return_type, functions[i], LLVMAttributeReturnIndex, &ctx);
        }

        for (uint64_t j = 0; j < ast_module->functions[i]->parameters->count; j++)
        {
            ptlang_ir_builder_type_add_attributes(ast_module->functions[i]->parameters->decls[j]->type, functions[i], j + 1, &ctx);
        }

        function_types[i] = ptlang_ast_type_function(ptlang_ast_type_copy(ast_module->functions[i]->return_type), param_type_list);

        ptlang_ir_builder_scope_add(&global_scope, ast_module->functions[i]->name, functions[i], function_types[i], true);
    }

    for (uint64_t i = 0; i < ast_module->declaration_count; i++)
    {
        LLVMSetInitializer(glob_decl_values[i], ptlang_ir_builder_type_default_value(ast_module->declarations[i]->type, &ctx));
    }
    free(glob_decl_values);

    for (uint64_t i = 0; i < ast_module->function_count; i++)
    {
        ctx.function = functions[i];
        ctx.return_type = ast_module->functions[i]->return_type;
        ctx.scope_number = 0;
        ctx.scopes = NULL;

        LLVMBasicBlockRef entry_block = LLVMAppendBasicBlock(functions[i], "entry");
        ctx.return_block = LLVMAppendBasicBlock(functions[i], "return");

        LLVMPositionBuilderAtEnd(ctx.builder, entry_block);

        ptlang_ir_builder_scope function_scope;
        ptlang_ir_builder_new_scope(&global_scope, &function_scope);
        ctx.scope = &function_scope;

        if (ast_module->functions[i]->return_type != NULL)
        {
            ctx.return_ptr = LLVMBuildAlloca(ctx.builder, ptlang_ir_builder_type(ast_module->functions[i]->return_type, &ctx), "return");
        }

        LLVMValueRef *param_ptrs = malloc(sizeof(LLVMValueRef) * ast_module->functions[i]->parameters->count);
        for (uint64_t j = 0; j < ast_module->functions[i]->parameters->count; j++)
        {
            param_ptrs[j] = LLVMBuildAlloca(ctx.builder, ptlang_ir_builder_type(ast_module->functions[i]->parameters->decls[j]->type, &ctx), ast_module->functions[i]->parameters->decls[j]->name);
            ptlang_ir_builder_scope_add(&function_scope, ast_module->functions[i]->parameters->decls[j]->name, param_ptrs[j], ast_module->functions[i]->parameters->decls[j]->type, false);
        }

        // ptlang_ir_builder_build_context ctx = {
        //     .builder = builder,
        //     .function = functions[i],
        //     .type_scope = type_scope,
        //     .scope = &function_scope,
        //     .scope_number = 0,
        //     .scopes = NULL,
        //     .return_ptr = return_ptr,
        //     .return_block = return_block,
        //     .return_type = ast_module->functions[i]->return_type,
        //     .struct_defs = struct_defs,
        //     .target_info = target_info,
        // };

        ptlang_ir_builder_stmt_allocas(ast_module->functions[i]->stmt, &ctx);

        ctx.scope_number = 0;

        if (ast_module->functions[i]->return_type != NULL)
        {
            LLVMBuildStore(ctx.builder, ptlang_ir_builder_type_default_value(ast_module->functions[i]->return_type, &ctx), ctx.return_ptr);
        }

        for (uint64_t j = 0; j < ast_module->functions[i]->parameters->count; j++)
        {
            LLVMBuildStore(ctx.builder, LLVMGetParam(functions[i], j), param_ptrs[j]);
        }
        free(param_ptrs);

        ptlang_ir_builder_stmt(ast_module->functions[i]->stmt, &ctx);

        for (uint64_t j = 0; j < ctx.scope_number; j++)
        {
            ptlang_ir_builder_scope_destroy(ctx.scopes[j]);
            free(ctx.scopes[j]);
        }

        free(ctx.scopes);

        LLVMBuildBr(ctx.builder, ctx.return_block);

        LLVMPositionBuilderAtEnd(ctx.builder, ctx.return_block);

        if (ast_module->functions[i]->return_type != NULL)
        {
            LLVMTypeRef t = ptlang_ir_builder_type(ast_module->functions[i]->return_type, &ctx);
            LLVMValueRef return_value = LLVMBuildLoad2(ctx.builder, t, ctx.return_ptr, "");

            LLVMBuildRet(ctx.builder, return_value);
        }
        else
        {
            LLVMBuildRetVoid(ctx.builder);
        }

        // llvmpara
        ptlang_ir_builder_scope_destroy(&function_scope);
    }
    free(functions);

    shfree(ctx.struct_defs);

    LLVMDisposeBuilder(ctx.builder);

    for (uint64_t i = 0; i < ast_module->function_count; i++)
    {
        ptlang_ast_type_destroy(function_types[i]);
    }
    free(function_types);

    ptlang_ir_builder_scope_destroy(&global_scope);
    shfree(ctx.type_scope);

    // LLVMValueRef glob = LLVMAddGlobal(llvm_module, LLVMInt1Type(), "glob");
    // LLVMSetInitializer(glob, LLVMConstInt(LLVMInt1Type(), 0, false));

    return llvm_module;
}
