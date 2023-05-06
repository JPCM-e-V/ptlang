#include "ptlang_ir_builder.h"

typedef struct ptlang_ir_builder_scope_entry_s
{
    char *name;
    LLVMValueRef value;
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

static inline void ptlang_ir_builder_scope_add(ptlang_ir_builder_scope *scope, char *name, LLVMValueRef value)
{
    scope->entry_count++;
    scope->entries = realloc(scope->entries, sizeof(ptlang_ir_builder_scope_entry) * scope->entry_count);
    scope->entries[scope->entry_count - 1] = (ptlang_ir_builder_scope_entry){
        .name = name,
        .value = value,
    };
}

static inline LLVMValueRef ptlang_ir_builder_scope_get(ptlang_ir_builder_scope *scope, char *name)
{
    while (scope != NULL)
    {
        for (uint64_t i = 0; i < scope->entry_count; i++)
        {
            if (strcmp(scope->entries[i].name, name) == 0)
            {
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

static LLVMValueRef ptlang_ir_builder_exp(ptlang_ast_exp exp);
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
        ptlang_ir_builder_scope_add(&global_scope, ast_module->declarations[i]->name, LLVMAddGlobal(llvm_module, ptlang_ir_builder_type(ast_module->declarations[i]->type, &type_scope), ast_module->declarations[i]->name));
    }

    ptlang_ir_builder_scope_destroy(&global_scope);
    ptlang_ir_builder_type_scope_destroy(&type_scope);

    // LLVMValueRef glob = LLVMAddGlobal(llvm_module, LLVMInt1Type(), "glob");
    // LLVMSetInitializer(glob, LLVMConstInt(LLVMInt1Type(), 0, false));

    return llvm_module;
}
