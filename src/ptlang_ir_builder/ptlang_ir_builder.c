#include "ptlang_ir_builder_impl.h"

#define HEAP_ARRAY_TYPE(element_type, ctx)                                                                   \
    LLVMStructType(                                                                                          \
        (LLVMTypeRef[]){                                                                                     \
            LLVMPointerType((element_type), 0),                                                              \
            LLVMIntPtrType((ctx)->target_info),                                                              \
        },                                                                                                   \
        2, false);

#define INIT_FUNCTION(name, return_type, param_types)                                                        \
    static inline void ptlang_ir_builder_init_##name##_func(ptlang_ir_builder_build_context *ctx)            \
    {                                                                                                        \
        if (ctx->name##_func == NULL)                                                                        \
        {                                                                                                    \
            ctx->name##_func =                                                                               \
                LLVMAddFunction(ctx->module, #name,                                                          \
                                LLVMFunctionType((return_type), (param_types),                               \
                                                 sizeof(param_types) / sizeof(LLVMTypeRef), false));         \
        }                                                                                                    \
    }

typedef struct ptlang_ir_builder_scope_entry_value_s
{
    LLVMValueRef value;
    ptlang_ast_type type;
    bool direct; // whether the value is directly stored (for functions)
} ptlang_ir_builder_scope_entry_value;

typedef struct ptlang_ir_builder_scope_entry_s
{
    char *key;
    ptlang_ir_builder_scope_entry_value value;
} ptlang_ir_builder_scope_entry;

struct ptlang_ir_builder_scope_s
{
    ptlang_ir_builder_scope *parent;
    // uint64_t entry_count;
    ptlang_ir_builder_scope_entry *entries;
};

static inline void ptlang_ir_builder_new_scope(ptlang_ir_builder_scope *parent, ptlang_ir_builder_scope *new)
{
    *new = (ptlang_ir_builder_scope){
        .parent = parent,
        .entries = NULL,
    };
}

static inline void ptlang_ir_builder_scope_destroy(ptlang_ir_builder_scope *scope) { shfree(scope->entries); }

static inline void ptlang_ir_builder_scope_add(ptlang_ir_builder_scope *scope, char *name, LLVMValueRef value,
                                               ptlang_ast_type type, bool direct)
{
    shput(scope->entries, name,
          ((ptlang_ir_builder_scope_entry_value){
              .value = value,
              .type = type,
              .direct = direct,
          }));
    // scope->entry_count++;
    // scope->entries =
    //     ptlang_realloc(scope->entries, sizeof(ptlang_ir_builder_scope_entry) * scope->entry_count);
    // scope->entries[scope->entry_count - 1] = (ptlang_ir_builder_scope_entry){
    //     .name = name,
    //     .value = value,
    //     .type = type,
    //     .direct = direct,
    // };
}

static inline LLVMValueRef ptlang_ir_builder_scope_get(ptlang_ir_builder_scope *scope, char *name,
                                                       ptlang_ast_type *type, bool *direct)
{
    while (scope != NULL)
    {
        // for (size_t i = 0; i < arrlenu(scope->entries); i++)
        // {
        //     if (strcmp(scope->entries[i].name, name) == 0)
        //     {
        ptrdiff_t i = shgeti(scope->entries, name);
        if (i != -1)
        {
            if (type != NULL)
            {
                *type = scope->entries[i].value.type;
            }
            if (direct != NULL)
            {
                *direct = scope->entries[i].value.direct;
            }
            return scope->entries[i].value.value;
            //     }
            // }
        }
        scope = scope->parent;
    }
    // noTODO (should be fixed by verify)
    abort();
    fprintf(stderr, "var not found\n");
    exit(EXIT_FAILURE);
}

struct ptlang_ir_builder_struct_def_s
{
    char *key;
    ptlang_ast_decl *value;
};

typedef struct ptlang_ir_builder_type_scope_entry_s
{
    LLVMTypeRef type;
    ptlang_ast_type ptlang_type;
} ptlang_ir_builder_type_scope_entry;

struct ptlang_ir_builder_type_scope_s
{
    char *key;
    ptlang_ir_builder_type_scope_entry value;
};

struct break_and_continue_block
{
    LLVMBasicBlockRef break_;
    LLVMBasicBlockRef continue_;
};

static inline ptlang_ast_type ptlang_ir_builder_unname_type(ptlang_ast_type type,
                                                            ptlang_ir_builder_build_context *ctx)
{
    ptlang_ast_type new_type;
    while (type != NULL && type->type == PTLANG_AST_TYPE_NAMED)
    {
        new_type = shget(ctx->type_scope, type->content.name).ptlang_type;
        if (new_type == NULL)
        {
            break;
        }
        type = new_type;
    }
    return type;
}

INIT_FUNCTION(malloc, LLVMPointerType(LLVMInt8Type(), 0), (LLVMTypeRef[]){LLVMIntPtrType(ctx->target_info)})
INIT_FUNCTION(realloc, LLVMPointerType(LLVMInt8Type(), 0),
              ((LLVMTypeRef[]){LLVMPointerType(LLVMInt8Type(), 0), LLVMIntPtrType(ctx->target_info)}))
INIT_FUNCTION(free, LLVMVoidType(), (LLVMTypeRef[]){LLVMPointerType(LLVMInt8Type(), 0)})

static ptlang_ast_type ptlang_ir_builder_exp_type(ptlang_ast_exp exp, ptlang_ir_builder_build_context *ctx);

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
            LLVMTypeRef struct_type_llvm = ptlang_ir_builder_type(struct_type, ctx, LLVMGetGlobalContext());
            ptlang_ast_decl *members = shget(ctx->struct_defs, struct_type->content.name);
            ptlang_ast_type_destroy(struct_type);

            for (size_t i = 0; i < arrlenu(members); i++)
            {
                if (strcmp(members[i]->name.name, exp->content.struct_member.member_name.name) == 0)
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
                LLVMTypeRef llvm_array_type = ptlang_ir_builder_type(array_type, ctx, LLVMGetGlobalContext());
                element_ptr = LLVMBuildInBoundsGEP2(
                    ctx->builder, llvm_array_type, array_ptr,
                    (LLVMValueRef[]){LLVMConstInt(LLVMInt1Type(), 0, false), index}, 2, "arrayelement");
            }
        }
        else if (array_type->type == PTLANG_AST_TYPE_HEAP_ARRAY)
        {
            LLVMValueRef struct_ptr = ptlang_ir_builder_exp_ptr(exp->content.array_element.array, ctx);
            LLVMValueRef heap_ptr;
            LLVMTypeRef llvm_element_type =
                ptlang_ir_builder_type(array_type->content.heap_array.type, ctx, LLVMGetGlobalContext());
            if (struct_ptr != NULL)
            {
                LLVMValueRef heap_ptr_ptr = LLVMBuildStructGEP2(
                    ctx->builder, ptlang_ir_builder_type(array_type, ctx, LLVMGetGlobalContext()), struct_ptr,
                    0, "heapptrptr");
                heap_ptr = LLVMBuildLoad2(ctx->builder, LLVMPointerType(llvm_element_type, 0), heap_ptr_ptr,
                                          "heapptr");
            }
            else
            {
                LLVMValueRef struct_ = ptlang_ir_builder_exp(exp->content.array_element.array, ctx);
                heap_ptr = LLVMBuildExtractValue(ctx->builder, struct_, 0, "heapptr");
            }
            element_ptr = LLVMBuildInBoundsGEP2(ctx->builder, llvm_element_type, heap_ptr,
                                                (LLVMValueRef[]){index}, 1, "heaparrayelement");
        }
        ptlang_ast_type_destroy(array_type);
        return element_ptr;
    }
    case PTLANG_AST_EXP_DEREFERENCE:
        return ptlang_ir_builder_exp(exp->content.unary_operator, ctx);
    case PTLANG_AST_EXP_LENGTH:
    {
        LLVMValueRef heap_arr_ptr = ptlang_ir_builder_exp_ptr(exp->content.unary_operator, ctx);
        if (heap_arr_ptr != NULL)
        {
            ptlang_ast_type array_type = ptlang_ir_builder_exp_type(exp->content.unary_operator, ctx);
            LLVMTypeRef array_llvm_type = ptlang_ir_builder_type(array_type, ctx, LLVMGetGlobalContext());
            ptlang_ast_type_destroy(array_type);
            return LLVMBuildStructGEP2(ctx->builder, array_llvm_type, heap_arr_ptr, 1, "heaparraylengthptr");
        }
        return NULL;
    }
    default:
        return NULL;
    }
}

static ptlang_ast_type ptlang_ir_builder_combine_types(ptlang_ast_type a, ptlang_ast_type b,
                                                       ptlang_ir_builder_type_scope *type_scope)
{
    if ((a->type != PTLANG_AST_TYPE_INTEGER && a->type != PTLANG_AST_TYPE_FLOAT) ||
        (b->type != PTLANG_AST_TYPE_INTEGER && b->type != PTLANG_AST_TYPE_FLOAT))
    {
        return NULL;
    }
    else if (a->type == PTLANG_AST_TYPE_INTEGER && b->type == PTLANG_AST_TYPE_INTEGER)
    {
        return ptlang_ast_type_integer(a->content.integer.is_signed || a->content.integer.is_signed,
                                       a->content.integer.size > b->content.integer.size
                                           ? a->content.integer.size
                                           : b->content.integer.size,
                                       NULL);
    }
    else if (a->type == PTLANG_AST_TYPE_INTEGER)
    {
        return ptlang_ast_type_float(b->content.float_size, NULL);
    }
    else if (b->type == PTLANG_AST_TYPE_INTEGER)
    {
        return ptlang_ast_type_float(a->content.float_size, NULL);
    }
    else
    {
        return ptlang_ast_type_float(a->content.float_size > b->content.float_size ? a->content.float_size
                                                                                   : b->content.float_size,
                                     NULL);
    }
}

LLVMTypeRef ptlang_ir_builder_type(ptlang_ast_type type, ptlang_ir_builder_build_context *ctx,
                                   LLVMContextRef C)
{
    LLVMTypeRef *param_types;
    LLVMTypeRef function_type;

    switch (type->type)
    {
    case PTLANG_AST_TYPE_VOID:
        return LLVMVoidTypeInContext(C);
    case PTLANG_AST_TYPE_INTEGER:
        return LLVMIntTypeInContext(C, type->content.integer.size);
    case PTLANG_AST_TYPE_FLOAT:
        switch (type->content.float_size)
        {
        case PTLANG_AST_TYPE_FLOAT_16:
            return LLVMHalfTypeInContext(C);
        case PTLANG_AST_TYPE_FLOAT_32:
            return LLVMFloatTypeInContext(C);
        case PTLANG_AST_TYPE_FLOAT_64:
            return LLVMDoubleTypeInContext(C);
        case PTLANG_AST_TYPE_FLOAT_128:
            return LLVMFP128TypeInContext(C);
        }
    case PTLANG_AST_TYPE_FUNCTION:
        param_types = ptlang_malloc(sizeof(LLVMTypeRef) * arrlenu(type->content.function.parameters));
        for (size_t i = 0; i < arrlenu(type->content.function.parameters); i++)
        {
            param_types[i] = ptlang_ir_builder_type(type->content.function.parameters[i], ctx, C);
        }
        // function_type = LLVMFunctionType(ptlang_ir_builder_type(type->content.function.return_type, ctx,
        // C),
        //                                  param_types, arrlenu(type->content.function.parameters), false);
        ptlang_free(param_types);
        function_type = LLVMPointerTypeInContext(C, 0);
        return function_type;
    case PTLANG_AST_TYPE_HEAP_ARRAY:
        return HEAP_ARRAY_TYPE(ptlang_ir_builder_type(type->content.heap_array.type, ctx, C), ctx);
    case PTLANG_AST_TYPE_ARRAY:
        return LLVMArrayType(ptlang_ir_builder_type(type->content.array.type, ctx, C),
                             type->content.array.len);
    case PTLANG_AST_TYPE_REFERENCE:
        return LLVMPointerType(ptlang_ir_builder_type(type->content.reference.type, ctx, C), 0);
    case PTLANG_AST_TYPE_NAMED:
        return shget(ctx->type_scope, type->content.name).type;
    }
    abort();
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

static ptlang_ast_type ptlang_ir_builder_type_copy_and_unname(ptlang_ast_type type,
                                                              ptlang_ir_builder_build_context *ctx)
{
    return ptlang_ast_type_copy(ptlang_ir_builder_unname_type(type, ctx));
}

static ptlang_ast_type ptlang_ir_builder_exp_type(ptlang_ast_exp exp, ptlang_ir_builder_build_context *ctx)
{
    return ptlang_ast_type_copy(exp->ast_type);
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
            return ptlang_ast_type_integer(type->content.integer.is_signed, size, NULL);
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
        return ptlang_ast_type_integer(false, 1, NULL);
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
        return ptlang_ast_type_integer(false, LLVMPointerSize(ctx->target_info) * 8, NULL);
    case PTLANG_AST_EXP_FUNCTION_CALL:
    { // ptlang_ir_builder_scope_get()
        ptlang_ast_type function = ptlang_ir_builder_exp_type(exp->content.function_call.function, ctx);
        ptlang_ast_type return_type =
            ptlang_ir_builder_type_copy_and_unname(function->content.function.return_type, ctx);
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
        return ptlang_ast_type_integer(is_signed, size, NULL);
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

        return ptlang_ast_type_float(float_size, NULL);
    }
    case PTLANG_AST_EXP_STRUCT:
    {
        size_t name_len = strlen(exp->content.struct_.type.name) + 1;
        char *name = ptlang_malloc(name_len);
        memcpy(name, exp->content.struct_.type.name, name_len);
        return ptlang_ast_type_named(name, NULL);
    }
    case PTLANG_AST_EXP_ARRAY:
        return ptlang_ir_builder_type_copy_and_unname(exp->content.array.type, ctx);
    case PTLANG_AST_EXP_TERNARY:
    {
        ptlang_ast_type if_value = ptlang_ir_builder_exp_type(exp->content.ternary_operator.if_value, ctx);
        ptlang_ast_type else_value =
            ptlang_ir_builder_exp_type(exp->content.ternary_operator.else_value, ctx);
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
        ptlang_ast_decl *members = shget(ctx->struct_defs, struct_type->content.name);
        ptlang_ast_type_destroy(struct_type);

        for (size_t i = 0; i < arrlenu(members); i++)
        {
            if (strcmp(members[i]->name.name, exp->content.struct_member.member_name.name) == 0)
            {
                return ptlang_ir_builder_type_copy_and_unname(members[i]->type, ctx);
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
        return ptlang_ast_type_reference(ptlang_ir_builder_exp_type(exp->content.reference.value, ctx),
                                         exp->content.reference.writable, NULL);
    case PTLANG_AST_EXP_DEREFERENCE:
    {
        ptlang_ast_type exp_type = ptlang_ir_builder_exp_type(exp->content.unary_operator, ctx);
        return ptlang_ir_builder_type_copy_and_unname(exp_type->content.reference.type, ctx);
        ptlang_ast_type_destroy(exp_type);
    }
    }

    // LLVMSizeOf():
    abort();
}

static LLVMValueRef ptlang_ir_builder_type_default_value(ptlang_ast_type type,
                                                         ptlang_ir_builder_build_context *ctx)
{
    type = ptlang_ir_builder_unname_type(type, ctx);
    LLVMTypeRef llvm_type = ptlang_ir_builder_type(type, ctx, LLVMGetGlobalContext());
    switch (type->type)
    {
    case PTLANG_AST_TYPE_VOID:
        abort();
    case PTLANG_AST_TYPE_INTEGER:
        return LLVMConstInt(llvm_type, 0, false);
    case PTLANG_AST_TYPE_FLOAT:
        return LLVMConstReal(llvm_type, 0.0);
    case PTLANG_AST_TYPE_FUNCTION:
        return NULL;
    case PTLANG_AST_TYPE_HEAP_ARRAY:
        return LLVMConstStruct(
            (LLVMValueRef[]){
                LLVMGetPoison(LLVMPointerType(
                    ptlang_ir_builder_type(type->content.heap_array.type, ctx, LLVMGetGlobalContext()), 0)),
                LLVMConstInt(LLVMIntPtrType(ctx->target_info), 0, false),
            },
            2, false);
    case PTLANG_AST_TYPE_ARRAY:
    {
        LLVMValueRef element_value = ptlang_ir_builder_type_default_value(type->content.array.type, ctx);
        LLVMValueRef *element_values = ptlang_malloc(sizeof(LLVMValueRef) * type->content.array.len);
        for (uint64_t i = 0; i < type->content.array.len; i++)
        {
            element_values[i] = element_value;
        }
        LLVMValueRef array =
            LLVMConstArray(ptlang_ir_builder_type(type->content.array.type, ctx, LLVMGetGlobalContext()),
                           element_values, type->content.array.len);
        ptlang_free(element_values);
        return array;
    }
    case PTLANG_AST_TYPE_REFERENCE:
        return NULL;
    case PTLANG_AST_TYPE_NAMED:
    {
        ptlang_ast_decl *members = shget(ctx->struct_defs, type->content.name);
        LLVMValueRef *member_values = ptlang_malloc(sizeof(LLVMValueRef) * arrlenu(members));
        for (size_t i = 0; i < arrlenu(members); i++)
        {
            member_values[i] = ptlang_ir_builder_type_default_value(members[i]->type, ctx);
        }
        LLVMValueRef struct_ = LLVMConstNamedStruct(llvm_type, member_values, arrlenu(members));
        ptlang_free(member_values);
        return struct_;
    }
    }
    abort();
}

static LLVMValueRef ptlang_ir_builder_build_cast(LLVMValueRef input, ptlang_ast_type from, ptlang_ast_type to,
                                                 ptlang_ir_builder_build_context *ctx)
{
    // from = ptlang_ir_builder_unname_type(from, ctx);
    to = ptlang_ir_builder_unname_type(to, ctx);

    if (from->type == PTLANG_AST_TYPE_INTEGER && to->type == PTLANG_AST_TYPE_INTEGER)
    {
        if (from->content.integer.size < to->content.integer.size)
        {
            if (from->content.integer.is_signed)
            {
                return LLVMBuildSExt(ctx->builder, input,
                                     ptlang_ir_builder_type(to, ctx, LLVMGetGlobalContext()), "cast");
            }
            else
            {
                return LLVMBuildZExt(ctx->builder, input,
                                     ptlang_ir_builder_type(to, ctx, LLVMGetGlobalContext()), "cast");
            }
        }
        else if (from->content.integer.size > to->content.integer.size)
        {
            return LLVMBuildTrunc(ctx->builder, input,
                                  ptlang_ir_builder_type(to, ctx, LLVMGetGlobalContext()), "cast");
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
            return LLVMBuildFPExt(ctx->builder, input,
                                  ptlang_ir_builder_type(to, ctx, LLVMGetGlobalContext()), "cast");
        }
        else if (from->content.float_size > to->content.float_size)
        {
            return LLVMBuildFPTrunc(ctx->builder, input,
                                    ptlang_ir_builder_type(to, ctx, LLVMGetGlobalContext()), "cast");
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
            return LLVMBuildSIToFP(ctx->builder, input,
                                   ptlang_ir_builder_type(to, ctx, LLVMGetGlobalContext()), "cast");
        }
        else
        {
            return LLVMBuildUIToFP(ctx->builder, input,
                                   ptlang_ir_builder_type(to, ctx, LLVMGetGlobalContext()), "cast");
        }
    }
    else if (from->type == PTLANG_AST_TYPE_FLOAT && to->type == PTLANG_AST_TYPE_INTEGER)
    {
        if (to->content.integer.is_signed)
        {
            return LLVMBuildFPToSI(ctx->builder, input,
                                   ptlang_ir_builder_type(to, ctx, LLVMGetGlobalContext()), "cast");
        }
        else
        {
            return LLVMBuildFPToUI(ctx->builder, input,
                                   ptlang_ir_builder_type(to, ctx, LLVMGetGlobalContext()), "cast");
        }
    }
    else
    {
        return input;
    }
}

static LLVMValueRef ptlang_ir_builder_exp_and_cast(ptlang_ast_exp exp, ptlang_ast_type type,
                                                   ptlang_ir_builder_build_context *ctx)
{
    ptlang_ast_type from = ptlang_ir_builder_exp_type(exp, ctx);
    LLVMValueRef uncasted = ptlang_ir_builder_exp(exp, ctx);
    LLVMValueRef casted = ptlang_ir_builder_build_cast(uncasted, from, type, ctx);
    ptlang_ast_type_destroy(from);
    return casted;
}

static void ptlang_ir_builder_prepare_binary_op(ptlang_ast_exp exp, LLVMValueRef *left_value,
                                                LLVMValueRef *right_value, ptlang_ast_type *ret_type,
                                                ptlang_ir_builder_build_context *ctx)
{
    // ptlang_ast_type left_type = ptlang_ir_builder_exp_type(exp->content.binary_operator.left_value, ctx);
    ptlang_ast_type left_type = exp->content.binary_operator.left_value->ast_type;
    // ptlang_ast_type right_type = ptlang_ir_builder_exp_type(exp->content.binary_operator.right_value, ctx);
    ptlang_ast_type right_type = exp->content.binary_operator.right_value->ast_type;
    // *ret_type = ptlang_ir_builder_combine_types(left_type, right_type, ctx->type_scope);
    *ret_type = ptlang_ast_type_copy(exp->ast_type);
    LLVMValueRef left_value_uncasted = ptlang_ir_builder_exp(exp->content.binary_operator.left_value, ctx);
    *left_value = ptlang_ir_builder_build_cast(left_value_uncasted, left_type, *ret_type, ctx);
    LLVMValueRef right_value_uncasted = ptlang_ir_builder_exp(exp->content.binary_operator.right_value, ctx);
    *right_value = ptlang_ir_builder_build_cast(right_value_uncasted, right_type, *ret_type, ctx);
    // ptlang_ast_type_destroy(left_type);
    // ptlang_ast_type_destroy(right_type);
}

static LLVMValueRef ptlang_ir_builder_exp_as_bool(ptlang_ast_exp exp, ptlang_ir_builder_build_context *ctx)
{
    ptlang_ast_type type = ptlang_ir_builder_exp_type(exp, ctx);

    LLVMValueRef value = ptlang_ir_builder_exp(exp, ctx);

    LLVMTypeRef llvm_type = ptlang_ir_builder_type(type, ctx, LLVMGetGlobalContext());

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

LLVMValueRef ptlang_ir_builder_exp(ptlang_ast_exp exp, ptlang_ir_builder_build_context *ctx)
{
    switch (exp->type)
    {
    case PTLANG_AST_EXP_ASSIGNMENT:
    {
        ptlang_ast_type type = ptlang_ir_builder_exp_type(exp->content.binary_operator.left_value, ctx);
        LLVMValueRef value =
            ptlang_ir_builder_exp_and_cast(exp->content.binary_operator.right_value, type, ctx);
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
            LLVMBasicBlockRef reallocorignotzero_block =
                LLVMAppendBasicBlock(ctx->function, "reallocorignotzero");
            LLVMBasicBlockRef reallocrealloc_block = LLVMAppendBasicBlock(ctx->function, "reallocrealloc");
            LLVMBasicBlockRef reallocstorenew_block = LLVMAppendBasicBlock(ctx->function, "reallocstorenew");
            LLVMBasicBlockRef reallocfree_block = LLVMAppendBasicBlock(ctx->function, "reallocfree");
            LLVMBasicBlockRef reallocend_block = LLVMAppendBasicBlock(ctx->function, "reallocend");

            LLVMValueRef heap_arr_ptr = ptlang_ir_builder_exp_ptr(
                exp->content.binary_operator.left_value->content.unary_operator, ctx);
            ptlang_ast_type heap_arr_type = ptlang_ir_builder_exp_type(
                exp->content.binary_operator.left_value->content.unary_operator, ctx);
            LLVMTypeRef heap_arr_llvm_type =
                ptlang_ir_builder_type(heap_arr_type, ctx, LLVMGetGlobalContext());
            LLVMTypeRef heap_arr_element_type =
                ptlang_ir_builder_type(heap_arr_type->content.heap_array.type, ctx, LLVMGetGlobalContext());
            ptlang_ast_type_destroy(heap_arr_type);
            LLVMValueRef heap_len_ptr =
                LLVMBuildStructGEP2(ctx->builder, heap_arr_llvm_type, heap_arr_ptr, 1, "reallocheaplenptr");

            LLVMValueRef orig_len =
                LLVMBuildLoad2(ctx->builder, LLVMIntPtrType(ctx->target_info), heap_len_ptr, "loadlen");

            LLVMValueRef cmplens = LLVMBuildICmp(ctx->builder, LLVMIntEQ, value, orig_len, "realloccmplens");
            LLVMBuildCondBr(ctx->builder, cmplens, reallocend_block, reallocstart_block);

            LLVMPositionBuilderAtEnd(ctx->builder, reallocstart_block);
            LLVMValueRef heap_ptr_ptr =
                LLVMBuildStructGEP2(ctx->builder, heap_arr_llvm_type, heap_arr_ptr, 0, "reallocheapptrptr");
            LLVMValueRef size =
                LLVMBuildNUWMul(ctx->builder, value, LLVMSizeOf(heap_arr_element_type), "realloccalcsize");

            LLVMValueRef cmporiglenzero = LLVMBuildICmp(
                ctx->builder, LLVMIntEQ, orig_len, LLVMConstInt(LLVMIntPtrType(ctx->target_info), 0, false),
                "realloccmporiglenzero");
            LLVMBuildCondBr(ctx->builder, cmporiglenzero, reallocnewalloc_block, reallocorignotzero_block);

            LLVMPositionBuilderAtEnd(ctx->builder, reallocnewalloc_block);

            LLVMValueRef mallocated =
                LLVMBuildCall2(ctx->builder,
                               LLVMFunctionType(LLVMPointerType(LLVMInt8Type(), 0),
                                                (LLVMTypeRef[]){LLVMIntPtrType(ctx->target_info)}, 1, false),
                               ctx->malloc_func, (LLVMValueRef[]){size}, 1, "malloc");
            // LLVMBuildStore(ctx->builder, mallocated, heap_ptr_ptr);
            LLVMBuildBr(ctx->builder, reallocstorenew_block);

            LLVMPositionBuilderAtEnd(ctx->builder, reallocorignotzero_block);

            LLVMValueRef heap_ptr = LLVMBuildLoad2(ctx->builder, LLVMPointerType(heap_arr_element_type, 0),
                                                   heap_ptr_ptr, "reallocoldmemory");
            LLVMValueRef heap_byte_ptr = LLVMBuildPointerCast(
                ctx->builder, heap_ptr, LLVMPointerType(LLVMInt8Type(), 0), "realloctobyteptr");

            LLVMValueRef cmpnewlenzero = LLVMBuildICmp(
                ctx->builder, LLVMIntEQ, value, LLVMConstInt(LLVMIntPtrType(ctx->target_info), 0, false),
                "realloccmpnewlenzero");
            LLVMBuildCondBr(ctx->builder, cmpnewlenzero, reallocfree_block, reallocrealloc_block);

            LLVMPositionBuilderAtEnd(ctx->builder, reallocrealloc_block);

            LLVMValueRef reallocated =
                LLVMBuildCall2(ctx->builder,
                               LLVMFunctionType(LLVMPointerType(LLVMInt8Type(), 0),
                                                (LLVMTypeRef[]){LLVMPointerType(LLVMInt8Type(), 0),
                                                                LLVMIntPtrType(ctx->target_info)},
                                                2, false),
                               ctx->realloc_func, (LLVMValueRef[]){heap_byte_ptr, size}, 2, "realloc");
            LLVMBuildBr(ctx->builder, reallocstorenew_block);

            LLVMPositionBuilderAtEnd(ctx->builder, reallocstorenew_block);
            LLVMValueRef phi =
                LLVMBuildPhi(ctx->builder, LLVMPointerType(LLVMInt8Type(), 0), "reallocnewmemory");
            LLVMAddIncoming(phi, (LLVMValueRef[]){mallocated, reallocated},
                            (LLVMBasicBlockRef[]){reallocnewalloc_block, reallocrealloc_block}, 2);

            LLVMValueRef new_heap_ptr = LLVMBuildPointerCast(
                ctx->builder, phi, LLVMPointerType(heap_arr_element_type, 0), "reallocfrombyteptr");

            LLVMBuildStore(ctx->builder, new_heap_ptr, heap_ptr_ptr);
            LLVMBuildStore(ctx->builder, value, heap_len_ptr);
            LLVMBuildBr(ctx->builder, reallocend_block);

            LLVMPositionBuilderAtEnd(ctx->builder, reallocfree_block);

            LLVMBuildCall2(ctx->builder,
                           LLVMFunctionType(LLVMVoidType(),
                                            (LLVMTypeRef[]){LLVMPointerType(LLVMInt8Type(), 0)}, 1, false),
                           ctx->free_func, (LLVMValueRef[]){heap_byte_ptr}, 1, "");

            LLVMBuildStore(ctx->builder,
                           LLVMConstStruct(
                               (LLVMValueRef[]){
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
        // ptlang_ast_type left_type = ptlang_ir_builder_exp_type(exp->content.binary_operator.left_value,
        // ctx); ptlang_ast_type right_type =
        // ptlang_ir_builder_exp_type(exp->content.binary_operator.right_value, ctx);
        ptlang_ast_type
            ret_type;            // = ptlang_ir_builder_combine_types(left_type, right_type, ctx->type_scope);
        LLVMValueRef left_value; // = ptlang_ir_builder_exp(exp->content.binary_operator.left_value, ctx);
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
                ret_val = LLVMBuildNSWSub(ctx->builder,
                                          LLVMConstInt(LLVMIntType(type->content.integer.size), 0, false),
                                          input, "neg");
            }
            else
            {
                ret_val = LLVMBuildSub(ctx->builder,
                                       LLVMConstInt(LLVMIntType(type->content.integer.size), 0, false), input,
                                       "neg");
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
        LLVMAddIncoming(
            phi, (LLVMValueRef[]){LLVMConstInt(LLVMInt1Type(), 0, false), right},
            (LLVMBasicBlockRef[]){LLVMGetInstructionParent(first_br), LLVMGetInstructionParent(second_br)},
            2);

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
        LLVMAddIncoming(
            phi, (LLVMValueRef[]){LLVMConstInt(LLVMInt1Type(), 1, false), right},
            (LLVMBasicBlockRef[]){LLVMGetInstructionParent(first_br), LLVMGetInstructionParent(second_br)},
            2);

        return phi;
    }
    case PTLANG_AST_EXP_NOT:
        return LLVMBuildSelect(ctx->builder, ptlang_ir_builder_exp_as_bool(exp->content.unary_operator, ctx),
                               LLVMConstInt(LLVMInt1Type(), 0, false), LLVMConstInt(LLVMInt1Type(), 1, false),
                               "not");
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

        LLVMValueRef ret_val =
            LLVMBuildXor(ctx->builder, input, LLVMConstInt(LLVMIntType(type->content.integer.size), -1, true),
                         "bitwiseinverse");

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
            return LLVMBuildExtractValue(
                ctx->builder, ptlang_ir_builder_exp(exp->content.unary_operator, ctx), 1, "extractlen");
        }
        return NULL;
    }
    case PTLANG_AST_EXP_FUNCTION_CALL:
    {
        ptlang_ast_type function_type = ptlang_ir_builder_exp_type(exp->content.function_call.function, ctx);

        LLVMValueRef *args =
            ptlang_malloc(sizeof(LLVMValueRef) * arrlenu(exp->content.function_call.parameters));
        LLVMTypeRef *param_types =
            ptlang_malloc(sizeof(LLVMTypeRef) * arrlenu(exp->content.function_call.parameters));
        for (size_t i = 0; i < arrlenu(exp->content.function_call.parameters); i++)
        {
            args[i] = ptlang_ir_builder_exp_and_cast(exp->content.function_call.parameters[i],
                                                     function_type->content.function.parameters[i], ctx);

            param_types[i] = ptlang_ir_builder_type(function_type->content.function.parameters[i], ctx,
                                                    LLVMGetGlobalContext());
        }

        LLVMTypeRef type = LLVMFunctionType(
            ptlang_ir_builder_type(function_type->content.function.return_type, ctx, LLVMGetGlobalContext()),
            param_types, arrlenu(exp->content.function_call.parameters), false);
        ptlang_free(param_types);

        // LLVMTypeRef type = ptlang_ir_builder_type(function_type_unnamed, ctx);

        LLVMValueRef function_value = ptlang_ir_builder_exp(exp->content.function_call.function, ctx);
        LLVMValueRef ret_val = LLVMBuildCall2(ctx->builder, type, function_value, args,
                                              arrlenu(exp->content.function_call.parameters), "funccall");
        ptlang_free(args);
        ptlang_ast_type_destroy(function_type);
        return ret_val;
    }
    case PTLANG_AST_EXP_VARIABLE:
    {
        // return LLVMBuildLoad2(builder, );
        ptlang_ast_type type;
        bool direct;
        LLVMValueRef ptr =
            ptlang_ir_builder_scope_get(ctx->scope, exp->content.str_prepresentation, &type, &direct);
        if (direct)
        {
            return ptr;
        }
        LLVMTypeRef t = ptlang_ir_builder_type(type, ctx, LLVMGetGlobalContext());
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

        LLVMValueRef value =
            LLVMConstIntOfStringAndSize(ptlang_ir_builder_type(type, ctx, LLVMGetGlobalContext()),
                                        exp->content.str_prepresentation, str_len, 10);
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

        LLVMValueRef value =
            LLVMConstRealOfStringAndSize(ptlang_ir_builder_type(type, ctx, LLVMGetGlobalContext()),
                                         exp->content.str_prepresentation, str_len);
        ptlang_ast_type_destroy(type);
        return value;
    }
    case PTLANG_AST_EXP_STRUCT:
    {
        ptlang_ast_type type = ptlang_ir_builder_exp_type(exp, ctx);
        ptlang_ast_decl *members = shget(ctx->struct_defs, type->content.name);
        LLVMTypeRef struct_type = ptlang_ir_builder_type(type, ctx, LLVMGetGlobalContext());
        ptlang_ast_type_destroy(type);

        LLVMValueRef *struct_members = ptlang_malloc(sizeof(LLVMValueRef) * arrlenu(members));
        for (size_t i = 0; i < arrlenu(members); i++)
        {
            LLVMValueRef member_val = NULL;
            for (size_t j = 0; j < arrlenu(exp->content.struct_.members); j++)
            {
                if (strcmp(members[i]->name.name, exp->content.struct_.members[j].str.name) == 0)
                {
                    member_val = ptlang_ir_builder_exp_and_cast(exp->content.struct_.members[j].exp,
                                                                members[i]->type, ctx);
                    break;
                }
            }

            if (member_val == NULL)
            {
                member_val = ptlang_ir_builder_type_default_value(members[i]->type, ctx);
                // member_val = LLVMGetUndef(ptlang_ir_builder_type(members->decls[i]->type, ctx));
            }

            struct_members[i] = member_val;
        }

        LLVMValueRef struct_ = LLVMConstNamedStruct(struct_type, struct_members, arrlenu(members));
        ptlang_free(struct_members);
        return struct_;
    }
    case PTLANG_AST_EXP_ARRAY:
    {
        ptlang_ast_type unnamed_type = ptlang_ir_builder_unname_type(exp->content.array.type, ctx);

        LLVMTypeRef element_type =
            ptlang_ir_builder_type(unnamed_type->content.array.type, ctx, LLVMGetGlobalContext());
        LLVMValueRef *array_elements = ptlang_malloc(sizeof(LLVMValueRef) * unnamed_type->content.array.len);

        size_t i = 0;
        for (; i < arrlenu(exp->content.array.values); i++)
        {
            array_elements[i] = ptlang_ir_builder_exp_and_cast(exp->content.array.values[i],
                                                               unnamed_type->content.array.type, ctx);
        }
        LLVMValueRef default_element_value =
            ptlang_ir_builder_type_default_value(unnamed_type->content.array.type, ctx);
        for (; i < unnamed_type->content.array.len; i++)
        {
            array_elements[i] = default_element_value;
        }

        LLVMValueRef array = LLVMConstArray(element_type, array_elements, unnamed_type->content.array.len);
        ptlang_free(array_elements);
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
        LLVMValueRef if_value =
            ptlang_ir_builder_exp_and_cast(exp->content.ternary_operator.if_value, type, ctx);
        LLVMValueRef if_br = LLVMBuildBr(ctx->builder, end_block);

        LLVMPositionBuilderAtEnd(ctx->builder, else_block);
        LLVMValueRef else_value =
            ptlang_ir_builder_exp_and_cast(exp->content.ternary_operator.else_value, type, ctx);
        LLVMValueRef else_br = LLVMBuildBr(ctx->builder, end_block);

        LLVMPositionBuilderAtEnd(ctx->builder, end_block);

        LLVMValueRef phi =
            LLVMBuildPhi(ctx->builder, ptlang_ir_builder_type(type, ctx, LLVMGetGlobalContext()), "ternary");

        ptlang_ast_type_destroy(type);

        LLVMAddIncoming(
            phi, (LLVMValueRef[]){if_value, else_value},
            (LLVMBasicBlockRef[]){LLVMGetInstructionParent(if_br), LLVMGetInstructionParent(else_br)}, 2);

        return phi;
    }
    case PTLANG_AST_EXP_CAST:
        return ptlang_ir_builder_exp_and_cast(exp->content.cast.value, exp->content.cast.type, ctx);
    case PTLANG_AST_EXP_STRUCT_MEMBER:
    {
        ptlang_ast_type type = ptlang_ir_builder_exp_type(exp, ctx);
        LLVMTypeRef ret_type = ptlang_ir_builder_type(type, ctx, LLVMGetGlobalContext());
        ptlang_ast_type_destroy(type);

        LLVMValueRef as_ptr = ptlang_ir_builder_exp_ptr(exp, ctx);
        if (as_ptr != NULL)
        {
            return LLVMBuildLoad2(ctx->builder, ret_type, as_ptr, "structmember");
        }
        else
        {
            ptlang_ast_type struct_type = ptlang_ir_builder_exp_type(exp->content.struct_member.struct_, ctx);
            ptlang_ast_decl *members = shget(ctx->struct_defs, struct_type->content.name);
            ptlang_ast_type_destroy(struct_type);

            for (size_t i = 0; i < arrlenu(members); i++)
            {
                if (strcmp(members[i]->name.name, exp->content.struct_member.member_name.name) == 0)
                {
                    return LLVMBuildExtractValue(
                        ctx->builder, ptlang_ir_builder_exp(exp->content.struct_member.struct_, ctx), i,
                        "structmember");
                }
            }
            return NULL;
        }
    }
    case PTLANG_AST_EXP_ARRAY_ELEMENT:
    {
        ptlang_ast_type type = ptlang_ir_builder_exp_type(exp, ctx);
        LLVMTypeRef ret_type = ptlang_ir_builder_type(type, ctx, LLVMGetGlobalContext());
        ptlang_ast_type_destroy(type);

        LLVMValueRef as_ptr = ptlang_ir_builder_exp_ptr(exp, ctx);
        if (as_ptr != NULL)
        {
            return LLVMBuildLoad2(ctx->builder, ret_type, as_ptr, "arrayelement");
        }
        // else
        // {
        //     LLVMGEP
        //     return LLVMBuildExtractValue(ctx->builder,
        //     ptlang_ir_builder_exp(exp->content.array_element.array, ctx),
        //     ptlang_ir_builder_exp(exp->content.array_element.index), "arrayelement");
        // }
    }
    case PTLANG_AST_EXP_REFERENCE:
        return ptlang_ir_builder_exp_ptr(exp->content.reference.value, ctx);
    case PTLANG_AST_EXP_DEREFERENCE:
    {
        ptlang_ast_type type = ptlang_ir_builder_exp_type(exp->content.unary_operator, ctx);
        LLVMTypeRef ret_type =
            ptlang_ir_builder_type(type->content.reference.type, ctx, LLVMGetGlobalContext());
        ptlang_ast_type_destroy(type);
        return LLVMBuildLoad2(ctx->builder, ret_type, ptlang_ir_builder_exp(exp->content.unary_operator, ctx),
                              "dereference");
    }
    case PTLANG_AST_EXP_BINARY:
    {

        uint32_t bit_size = exp->ast_type->type == PTLANG_AST_TYPE_INTEGER
                                ? exp->ast_type->content.integer.size
                                : exp->ast_type->content.float_size;
        uint32_t byte_size = (bit_size - 1) / 8 + 1;

        // LLVMValueRef *bytes = ptlang_malloc(sizeof(LLVMValueRef) * byte_size);

        // LLVMTypeRef byte = LLVMInt8Type();

        LLVMTypeRef type = LLVMIntType(bit_size);

        LLVMValueRef value = LLVMConstInt(type, 0, false);

        enum LLVMByteOrdering byteOrdering = LLVMByteOrder(ctx->target_info);

        for (uint32_t i = 0; i < byte_size; i++)
        {
            value = LLVMConstShl(value, LLVMConstInt(type, 1, false));
            value = LLVMConstOr(
                value,
                LLVMConstInt(type, exp->content.binary[byteOrdering == LLVMBigEndian ? i : byte_size - i - 1],
                             false));
            // LLVMConstString()
            // bytes[i] = LLVMConstInt(type, exp->content.binary[i], false);
        }

        // LLVMValueRef as_array = LLVMConstArray(LLVMArrayType2(byte, byte_size), bytes, byte_size);

        // // // // // // // // LLVMValueRef *bytes = ptlang_malloc(sizeof(LLVMValueRef) * byte_size);

        // // // // // // // // LLVMTypeRef byte = LLVMInt8Type();

        // // // // // // // // for (uint32_t i = 0; i < byte_size; i++)
        // // // // // // // // {
        // // // // // // // //     bytes[i] = LLVMConstInt(byte, exp->content.binary[i], false);
        // // // // // // // // }

        // // // // // // // LLVMValueRef as_array = LLVMConstString("fsafd", 5, true);
        // // // // // // // // LLVMConstArray(LLVMArrayType(byte, byte_size), bytes, byte_size);

        // // // // // // // // ptlang_free(bytes);

        // LLVMValueRef as_int = LLVMConstBitCast(as_array, LLVMIntType(byte_size * 8));
        if (bit_size != byte_size * 8)
        {
            value = LLVMConstTrunc(value, LLVMIntType(bit_size));
        }
        return value;
    }
    }
    abort();
}

static void ptlang_ir_builder_stmt_allocas(ptlang_ast_stmt stmt, ptlang_ir_builder_build_context *ctx)
// static void ptlang_ir_builder_stmt_allocas(ptlang_ast_stmt stmt, LLVMBuilderRef builder,
// ptlang_ir_builder_type_scope *type_scope, ptlang_ir_builder_scope *scope, uint64_t *scope_count,
// ptlang_ir_builder_scope **scopes)
{
    switch (stmt->type)
    {
    case PTLANG_AST_STMT_BLOCK:
    {
        ptlang_ir_builder_scope *old_scope = ctx->scope;
        ctx->scope_number++;
        ctx->scopes = ptlang_realloc(ctx->scopes, sizeof(ptlang_ir_builder_scope *) * ctx->scope_number);
        ctx->scope = ptlang_malloc(sizeof(ptlang_ir_builder_scope));
        ctx->scopes[ctx->scope_number - 1] = ctx->scope;
        ptlang_ir_builder_new_scope(old_scope, ctx->scope);
        for (size_t i = 0; i < arrlenu(stmt->content.block.stmts); i++)
        {
            ptlang_ir_builder_stmt_allocas(stmt->content.block.stmts[i], ctx);
        }
        ctx->scope = old_scope;
        break;
    }
    case PTLANG_AST_STMT_DECL:
        ptlang_ir_builder_scope_add(
            ctx->scope, stmt->content.decl->name.name,
            LLVMBuildAlloca(ctx->builder,
                            ptlang_ir_builder_type(stmt->content.decl->type, ctx, LLVMGetGlobalContext()),
                            stmt->content.decl->name.name),
            stmt->content.decl->type, false);
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
// static void ptlang_ir_builder_stmt(ptlang_ast_stmt stmt, LLVMBuilderRef builder, LLVMValueRef function,
// ptlang_ir_builder_type_scope *type_scope, ptlang_ir_builder_scope *scope, uint64_t *scope_number,
// ptlang_ir_builder_scope *scopes, LLVMValueRef return_ptr, LLVMBasicBlockRef return_block)
{

    switch (stmt->type)
    {
    case PTLANG_AST_STMT_BLOCK:
    {
        ptlang_ir_builder_scope *old_scope = ctx->scope;
        ctx->scope = ctx->scopes[ctx->scope_number];
        ctx->scope_number++;
        for (size_t i = 0; i < arrlenu(stmt->content.block.stmts); i++)
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
        LLVMValueRef ptr =
            ptlang_ir_builder_scope_get(ctx->scope, stmt->content.decl->name.name, &type, NULL);
        LLVMBuildStore(ctx->builder,
                       stmt->content.decl->init != NULL
                           ? ptlang_ir_builder_exp_and_cast(stmt->content.decl->init, type, ctx)
                           : ptlang_ir_builder_type_default_value(type, ctx),
                       ptr);
        break;
    }
    case PTLANG_AST_STMT_IF:
    {
        LLVMBasicBlockRef if_block = LLVMAppendBasicBlock(ctx->function, "if");
        LLVMBasicBlockRef endif_block = LLVMAppendBasicBlock(ctx->function, "endif");

        LLVMBuildCondBr(ctx->builder,
                        ptlang_ir_builder_exp_as_bool(stmt->content.control_flow.condition, ctx), if_block,
                        endif_block);

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

        LLVMBuildCondBr(ctx->builder,
                        ptlang_ir_builder_exp_as_bool(stmt->content.control_flow2.condition, ctx), if_block,
                        else_block);

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

        arrpush(ctx->break_and_continue_blocks, ((struct break_and_continue_block){
                                                    .break_ = endwhile_block,
                                                    .continue_ = condition_block,
                                                }));

        LLVMBuildBr(ctx->builder, condition_block);

        LLVMPositionBuilderAtEnd(ctx->builder, condition_block);
        LLVMBuildCondBr(ctx->builder,
                        ptlang_ir_builder_exp_as_bool(stmt->content.control_flow.condition, ctx), stmt_block,
                        endwhile_block);

        LLVMPositionBuilderAtEnd(ctx->builder, stmt_block);
        ptlang_ir_builder_stmt(stmt->content.control_flow.stmt, ctx);
        LLVMBuildBr(ctx->builder, condition_block);

        (void)arrpop(ctx->break_and_continue_blocks);

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
            LLVMBuildStore(ctx->builder,
                           ptlang_ir_builder_exp_and_cast(stmt->content.exp, ctx->return_type, ctx),
                           ctx->return_ptr);
        }
        LLVMBuildBr(ctx->builder, ctx->return_block);
        LLVMPositionBuilderAtEnd(ctx->builder, LLVMAppendBasicBlock(ctx->function, "impossible"));
        break;
    case PTLANG_AST_STMT_RET_VAL:
        // ptlang_ast_type type = ptlang_ir_builder_exp_type(stmt->content.exp, ctx);
        // LLVMValueRef uncasted = ptlang_ir_builder_exp(stmt->content.exp, ctx);
        // LLVMValueRef casted = ptlang_ir_builder_build_cast(uncasted, type, ctx->return_type, ctx);
        // ptlang_ast_type_destroy(type);
        LLVMBuildStore(ctx->builder, ptlang_ir_builder_exp_and_cast(stmt->content.exp, ctx->return_type, ctx),
                       ctx->return_ptr);
        break;
    case PTLANG_AST_STMT_BREAK:
        LLVMBuildBr(ctx->builder, ctx->break_and_continue_blocks[arrlenu(ctx->break_and_continue_blocks) - 1 -
                                                                 stmt->content.nesting_level]
                                      .break_);
        LLVMPositionBuilderAtEnd(ctx->builder, LLVMAppendBasicBlock(ctx->function, "impossible"));
        break;
    case PTLANG_AST_STMT_CONTINUE:
        LLVMBuildBr(ctx->builder, ctx->break_and_continue_blocks[arrlenu(ctx->break_and_continue_blocks) - 1 -
                                                                 stmt->content.nesting_level]
                                      .continue_);
        LLVMPositionBuilderAtEnd(ctx->builder, LLVMAppendBasicBlock(ctx->function, "impossible"));
        break;
    default:
        break;
    }
}
#endif

// static void ptlang_ir_builder_resolve_type(ptlang_ast_type ast_type, ptlang_ir_type_alias_table
// alias_table)
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

static void ptlang_ir_builder_type_add_attributes(ptlang_ast_type type, LLVMValueRef function,
                                                  LLVMAttributeIndex index,
                                                  ptlang_ir_builder_build_context *ctx)
{
    type = ptlang_ir_builder_unname_type(type, ctx);
    switch (type->type)
    {
    case PTLANG_AST_TYPE_INTEGER:
        if (type->content.integer.is_signed)
        {
            LLVMAddAttributeAtIndex(
                function, index,
                LLVMCreateEnumAttribute(LLVMGetGlobalContext(),
                                        LLVMGetEnumAttributeKindForName("signext", sizeof("signext") - 1),
                                        0));
        }
        else
        {
            LLVMAddAttributeAtIndex(
                function, index,
                LLVMCreateEnumAttribute(LLVMGetGlobalContext(),
                                        LLVMGetEnumAttributeKindForName("zeroext", sizeof("zeroext") - 1),
                                        0));
        }
        break;
    default:
        break;
    }
}

LLVMModuleRef ptlang_ir_builder_module(ptlang_ast_module ast_module, LLVMTargetDataRef target_info)
{
    LLVMModuleRef llvm_module = LLVMModuleCreateWithName("t");
    LLVMSetModuleDataLayout(llvm_module, target_info);

    ptlang_ir_builder_build_context ctx = {
        .builder = LLVMCreateBuilder(),
        .module = llvm_module,
        .target_info = target_info,
    };

    ptlang_ir_builder_scope global_scope = {.entries = NULL};
    // ptlang_ir_builder_type_scope *type_scope = NULL;

    // ptlang_ir_builder_struct_def *struct_defs = NULL;

    size_t f = sizeof(LLVMTypeRef) * arrlenu(ast_module->struct_defs);
    LLVMTypeRef *structs = ptlang_malloc(f);
    for (size_t i = 0; i < arrlenu(ast_module->struct_defs); i++)
    {
        structs[i] = LLVMStructCreateNamed(LLVMGetGlobalContext(), ast_module->struct_defs[i]->name.name);
        shput(ctx.type_scope, ast_module->struct_defs[i]->name.name,
              ((ptlang_ir_builder_type_scope_entry){
                  .type = structs[i],
              }));

        shput(ctx.struct_defs, ast_module->struct_defs[i]->name.name, ast_module->struct_defs[i]->members);
    }

    for (size_t i = 0; i < arrlenu(ast_module->struct_defs); i++)
    {
        LLVMTypeRef *elements =
            ptlang_malloc(sizeof(LLVMTypeRef) * arrlenu(ast_module->struct_defs[i]->members));
        for (size_t j = 0; j < arrlenu(ast_module->struct_defs[i]->members); j++)
        {
            elements[j] = ptlang_ir_builder_type(ast_module->struct_defs[i]->members[j]->type, &ctx,
                                                 LLVMGetGlobalContext());
        }
        LLVMStructSetBody(structs[i], elements, arrlenu(ast_module->struct_defs[i]->members), false);
        ptlang_free(elements);
    }
    ptlang_free(structs);

    LLVMValueRef *glob_decl_values = ptlang_malloc(sizeof(LLVMValueRef) * arrlenu(ast_module->declarations));
    for (size_t i = 0; i < arrlenu(ast_module->declarations); i++)
    {
        LLVMTypeRef t =
            ptlang_ir_builder_type(ast_module->declarations[i]->type, &ctx, LLVMGetGlobalContext());
        glob_decl_values[i] = LLVMAddGlobal(llvm_module, t, ast_module->declarations[i]->name.name);
        LLVMSetGlobalConstant(glob_decl_values[i], !ast_module->declarations[i]->writable);
        LLVMSetLinkage(glob_decl_values[i],
                       ast_module->declarations[i]->export ? LLVMExternalLinkage : LLVMInternalLinkage);
        ptlang_ir_builder_scope_add(&global_scope, ast_module->declarations[i]->name.name,
                                    glob_decl_values[i], ast_module->declarations[i]->type, false);
    }

    LLVMValueRef *functions = ptlang_malloc(sizeof(LLVMValueRef) * arrlenu(ast_module->functions));
    ptlang_ast_type *function_types = ptlang_malloc(sizeof(ptlang_ast_type) * arrlenu(ast_module->functions));
    for (size_t i = 0; i < arrlenu(ast_module->functions); i++)
    {
        ptlang_ast_type *param_type_list = NULL;

        LLVMTypeRef *param_types =
            ptlang_malloc(sizeof(LLVMTypeRef) * arrlenu(ast_module->functions[i]->parameters));
        for (size_t j = 0; j < arrlenu(ast_module->functions[i]->parameters); j++)
        {
            arrput(param_type_list, ptlang_ast_type_copy(ast_module->functions[i]->parameters[j]->type));

            param_types[j] = ptlang_ir_builder_type(ast_module->functions[i]->parameters[j]->type, &ctx,
                                                    LLVMGetGlobalContext());
        }
        LLVMTypeRef function_type = LLVMFunctionType(
            ptlang_ir_builder_type(ast_module->functions[i]->return_type, &ctx, LLVMGetGlobalContext()),
            param_types, arrlenu(ast_module->functions[i]->parameters), false);
        ptlang_free(param_types);
        functions[i] = LLVMAddFunction(llvm_module, ast_module->functions[i]->name.name, function_type);
        LLVMSetLinkage(functions[i],
                       ast_module->functions[i]->export ? LLVMExternalLinkage : LLVMInternalLinkage);

        if (ast_module->functions[i]->return_type->type != PTLANG_AST_TYPE_VOID)
        {
            ptlang_ir_builder_type_add_attributes(ast_module->functions[i]->return_type, functions[i],
                                                  LLVMAttributeReturnIndex, &ctx);
        }

        for (size_t j = 0; j < arrlenu(ast_module->functions[i]->parameters); j++)
        {
            ptlang_ir_builder_type_add_attributes(ast_module->functions[i]->parameters[j]->type, functions[i],
                                                  j + 1, &ctx);
        }

        function_types[i] = ptlang_ast_type_function(
            ptlang_ast_type_copy(ast_module->functions[i]->return_type), param_type_list, NULL);

        ptlang_ir_builder_scope_add(&global_scope, ast_module->functions[i]->name.name, functions[i],
                                    function_types[i], true);
    }

    for (size_t i = 0; i < arrlenu(ast_module->declarations); i++)
    {
        LLVMSetInitializer(
            glob_decl_values[i],
            ast_module->declarations[i]->init != NULL
                ? ptlang_ir_builder_exp_and_cast(ast_module->declarations[i]->init,
                                                 ast_module->declarations[i]->type, &ctx)
                : ptlang_ir_builder_type_default_value(ast_module->declarations[i]->type, &ctx));
    }
    ptlang_free(glob_decl_values);

    for (size_t i = 0; i < arrlenu(ast_module->functions); i++)
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

        if (ast_module->functions[i]->return_type->type != PTLANG_AST_TYPE_VOID)
        {
            ctx.return_ptr = LLVMBuildAlloca(
                ctx.builder,
                ptlang_ir_builder_type(ast_module->functions[i]->return_type, &ctx, LLVMGetGlobalContext()),
                "return");
        }

        LLVMValueRef *param_ptrs =
            ptlang_malloc(sizeof(LLVMValueRef) * arrlenu(ast_module->functions[i]->parameters));
        for (size_t j = 0; j < arrlenu(ast_module->functions[i]->parameters); j++)
        {
            param_ptrs[j] =
                LLVMBuildAlloca(ctx.builder,
                                ptlang_ir_builder_type(ast_module->functions[i]->parameters[j]->type, &ctx,
                                                       LLVMGetGlobalContext()),
                                ast_module->functions[i]->parameters[j]->name.name);
            ptlang_ir_builder_scope_add(&function_scope, ast_module->functions[i]->parameters[j]->name.name,
                                        param_ptrs[j], ast_module->functions[i]->parameters[j]->type, false);
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

        if (ast_module->functions[i]->return_type->type != PTLANG_AST_TYPE_VOID)
        {
            LLVMBuildStore(ctx.builder,
                           ptlang_ir_builder_type_default_value(ast_module->functions[i]->return_type, &ctx),
                           ctx.return_ptr);
        }

        for (size_t j = 0; j < arrlenu(ast_module->functions[i]->parameters); j++)
        {
            LLVMBuildStore(ctx.builder, LLVMGetParam(functions[i], j), param_ptrs[j]);
        }
        ptlang_free(param_ptrs);

        ptlang_ir_builder_stmt(ast_module->functions[i]->stmt, &ctx);

        for (uint64_t j = 0; j < ctx.scope_number; j++)
        {
            ptlang_ir_builder_scope_destroy(ctx.scopes[j]);
            ptlang_free(ctx.scopes[j]);
        }

        ptlang_free(ctx.scopes);

        LLVMBuildBr(ctx.builder, ctx.return_block);

        LLVMPositionBuilderAtEnd(ctx.builder, ctx.return_block);

        if (ast_module->functions[i]->return_type->type != PTLANG_AST_TYPE_VOID)
        {
            LLVMTypeRef t =
                ptlang_ir_builder_type(ast_module->functions[i]->return_type, &ctx, LLVMGetGlobalContext());
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
    ptlang_free(functions);

    shfree(ctx.struct_defs);

    LLVMDisposeBuilder(ctx.builder);

    for (size_t i = 0; i < arrlenu(ast_module->functions); i++)
    {
        ptlang_ast_type_destroy(function_types[i]);
    }
    ptlang_free(function_types);

    ptlang_ir_builder_scope_destroy(&global_scope);
    shfree(ctx.type_scope);
    arrfree(ctx.break_and_continue_blocks);

    // LLVMValueRef glob = LLVMAddGlobal(llvm_module, LLVMInt1Type(), "glob");
    // LLVMSetInitializer(glob, LLVMConstInt(LLVMInt1Type(), 0, false));

    return llvm_module;
}
