#include "ptlang_ir_builder_impl.h"

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

static void ptlang_ir_builder_stmt_allocas(ptlang_ast_stmt stmt, LLVMBuilderRef builder, ptlang_ir_builder_type_scope *type_scope, ptlang_ir_builder_scope *scope, uint64_t *scope_count, ptlang_ir_builder_scope **scopes)
{
    switch (stmt->type)
    {
    case PTLANG_AST_STMT_BLOCK:
        (*scope_count)++;
        *scopes = realloc(*scopes, sizeof(ptlang_ir_builder_scope) * *scope_count);
        ptlang_ir_builder_new_scope(scope, &(*scopes)[*scope_count - 1]);
        for (uint64_t i = 0; i < stmt->content.block.stmt_count; i++)
        {
            ptlang_ir_builder_stmt_allocas(stmt->content.block.stmts[i], builder, type_scope, &(*scopes)[*scope_count - 1], scope_count, scopes);
        }
        break;
    case PTLANG_AST_STMT_DECL:
        ptlang_ir_builder_scope_add(scope, stmt->content.decl->name, LLVMBuildAlloca(builder, ptlang_ir_builder_type(stmt->content.decl->type, type_scope), stmt->content.decl->name));
        break;
    case PTLANG_AST_STMT_IF:
    case PTLANG_AST_STMT_WHILE:
        ptlang_ir_builder_stmt_allocas(stmt->content.control_flow.stmt, builder, type_scope, scope, scope_count, scopes);
        break;
    case PTLANG_AST_STMT_IF_ELSE:
        ptlang_ir_builder_stmt_allocas(stmt->content.control_flow2.stmt[0], builder, type_scope, scope, scope_count, scopes);
        ptlang_ir_builder_stmt_allocas(stmt->content.control_flow2.stmt[1], builder, type_scope, scope, scope_count, scopes);
        break;
    default:
        break;
    }
}

#if 0
static void ptlang_ir_builder_stmt(ptlang_ast_stmt stmt, LLVMBuilderRef builder, LLVMValueRef function, ptlang_ir_builder_type_scope *type_scope, ptlang_ir_builder_scope *scope, uint64_t *scope_index, ptlang_ir_builder_scope *scopes)
{

    switch (stmt->type)
    {
    case PTLANG_AST_STMT_BLOCK:
    {
        ptlang_ir_builder_scope *block_scope = &scopes[*scope_index];
        (*scope_index)++;
        for (uint64_t i = 0; i < stmt->content.block.stmt_count; i++)
        {
            ptlang_ir_builder_stmt(stmt->content.block.stmts[i], builder, function, type_scope, block_scope, scope_index, scopes);
        }
        break;
    }
    case PTLANG_AST_STMT_EXP:
        ptlang_ir_builder_exp(stmt->content.exp);
        break;
    case PTLANG_AST_STMT_IF:
    {
        LLVMBasicBlockRef if_block = LLVMAppendBasicBlock(function, "if");
        LLVMBasicBlockRef endif_block = LLVMAppendBasicBlock(function, "endif");

        LLVMBuildCondBr(builder, ptlang_ir_builder_exp(stmt->content.control_flow.condition), if_block, endif_block);

        LLVMPositionBuilderAtEnd(builder, if_block);
        ptlang_ir_builder_stmt(stmt->content.control_flow.stmt, builder, function, type_scope, scope, scope_index, scopes);
        LLVMBuildBr(builder, endif_block);

        LLVMPositionBuilderAtEnd(builder, endif_block);
        break;
    }
    case PTLANG_AST_STMT_IF_ELSE:
    {
        LLVMBasicBlockRef if_block = LLVMAppendBasicBlock(function, "if");
        LLVMBasicBlockRef else_block = LLVMAppendBasicBlock(function, "else");
        LLVMBasicBlockRef endif_block = LLVMAppendBasicBlock(function, "endif");

        LLVMBuildCondBr(builder, ptlang_ir_builder_exp(stmt->content.control_flow2.condition), if_block, else_block);

        LLVMPositionBuilderAtEnd(builder, if_block);
        ptlang_ir_builder_stmt(stmt->content.control_flow2.stmt[0], builder, function, type_scope, scope, scope_index, scopes);
        LLVMBuildBr(builder, endif_block);

        LLVMPositionBuilderAtEnd(builder, else_block);
        ptlang_ir_builder_stmt(stmt->content.control_flow2.stmt[1], builder, function, type_scope, scope, scope_index, scopes);
        LLVMBuildBr(builder, endif_block);

        LLVMPositionBuilderAtEnd(builder, endif_block);
        break;
    }
    case PTLANG_AST_STMT_WHILE:
    {
        LLVMBasicBlockRef condition_block = LLVMAppendBasicBlock(function, "while_condition");
        LLVMBasicBlockRef stmt_block = LLVMAppendBasicBlock(function, "while_block");
        LLVMBasicBlockRef endwhile_block = LLVMAppendBasicBlock(function, "endwhile");

        LLVMBuildBr(builder, condition_block);

        LLVMPositionBuilderAtEnd(builder, condition_block);
        LLVMBuildCondBr(builder, ptlang_ir_builder_exp(stmt->content.control_flow.condition), stmt_block, endwhile_block);

        LLVMPositionBuilderAtEnd(builder, stmt_block);
        ptlang_ir_builder_stmt(stmt->content.control_flow.stmt, builder, function, type_scope, scope, scope_index, scopes);
        LLVMBuildBr(builder, condition_block);

        LLVMPositionBuilderAtEnd(builder, endwhile_block);
        break;
    }
    case PTLANG_AST_STMT_RETURN:
        if(stmt->content.exp!=NULL){
            
        }
        break;
    case PTLANG_AST_STMT_RET_VAL:
    case PTLANG_AST_STMT_BREAK:
    case PTLANG_AST_STMT_CONTINUE:
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
        ptlang_ir_builder_scope_add(&global_scope, ast_module->declarations[i]->name, LLVMAddGlobal(llvm_module, ptlang_ir_builder_type(ast_module->declarations[i]->type, &type_scope), ast_module->declarations[i]->name));
    }

    LLVMValueRef *functions = malloc(sizeof(LLVMValueRef) * ast_module->function_count);
    for (uint64_t i = 0; i < ast_module->function_count; i++)
    {
        LLVMTypeRef *param_types = malloc(sizeof(LLVMTypeRef) * ast_module->functions[i]->parameters->count);
        for (uint64_t j = 0; j < ast_module->functions[i]->parameters->count; j++)
        {
            param_types[j] = ptlang_ir_builder_type(ast_module->functions[i]->parameters->decls[j]->type, &type_scope);
        }
        LLVMTypeRef function_type = LLVMFunctionType(ptlang_ir_builder_type(ast_module->functions[i]->return_type, &type_scope), param_types, ast_module->functions[i]->parameters->count, false);
        free(param_types);
        functions[i] = LLVMAddFunction(llvm_module, ast_module->functions[i]->name, function_type);
        ptlang_ir_builder_scope_add(&global_scope, ast_module->functions[i]->name, functions[i]);
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
            ptlang_ir_builder_scope_add(&function_scope, ast_module->functions[i]->parameters->decls[j]->name, param_ptrs[j]);
        }

        ptlang_ir_builder_scope *scopes = NULL;
        uint64_t scope_count = 0;

        ptlang_ir_builder_stmt_allocas(ast_module->functions[i]->stmt, builder, &type_scope, &function_scope, &scope_count, &scopes);

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

    ptlang_ir_builder_scope_destroy(&global_scope);
    ptlang_ir_builder_type_scope_destroy(&type_scope);

    // LLVMValueRef glob = LLVMAddGlobal(llvm_module, LLVMInt1Type(), "glob");
    // LLVMSetInitializer(glob, LLVMConstInt(LLVMInt1Type(), 0, false));

    return llvm_module;
}
