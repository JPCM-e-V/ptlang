#ifndef PTLANG_UTILS_H
#define PTLANG_UTILS_H

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#define ptlang_malloc(size) (size) == 0 ? NULL : malloc(size)
#define ptlang_malloc_zero(size) (size) == 0 ? NULL : memset(ptlang_malloc(size), 0, (size))
#define ptlang_free(ptr) free(ptr)
#define ptlang_realloc(ptr, size) (size) == 0 ? (free(ptr), NULL) : realloc(ptr, (size))

#ifdef NDEBUG
#    define ptlang_assert(val) ((void)val)
#else
#    define ptlang_assert(val) assert(val)
#endif

typedef struct node_s ptlang_utils_graph_node;

struct node_s
{
    ptlang_utils_graph_node **edges_to;
    bool in_cycle;
    size_t index;
    size_t lowlink;
    bool on_stack;
};

typedef struct ptlang_utils_str
{
    char *str;
    bool to_free;
} ptlang_utils_str;

// #define ptlang_utils_sprintf_alloc(fmt, ...)                                                                 \
//     (char *a = ptlang_malloc(snprintf(NULL, 0, fmt, __VA_ARGS__) + 1), sprintf(a, fmt, __VA_ARGS__), a)

#define CONST_STR(str_const) ((ptlang_utils_str){.str = str_const, .to_free = false})
#define ALLOCATED_STR(str_to_free) ((ptlang_utils_str){.str = str_to_free, .to_free = true})

#define ptlang_utils_stb_array(stb_array, c_array)                                                           \
    {                                                                                                        \
        stb_array = NULL;                                                                                    \
        memcpy(arraddnptr(stb_array, sizeof(c_array) / sizeof(c_array[0])), c_array, sizeof(c_array));       \
    }
// #define ptlang_utils_stb_array(c_array) {char *stb_array = NULL; for(size_t i = 0; i <
// sizeof(c_array)/sizeof(char*); i++){arrpush(stb_array, )}}
#define ptlang_utils_build_str(char_ptr, ...)                                                                \
    {                                                                                                        \
        ptlang_utils_str strings[] = {__VA_ARGS__};                                                          \
        size_t string_count = sizeof(strings) / sizeof(strings[0]);                                          \
        size_t string_lengths[sizeof(strings) / sizeof(strings[0])];                                         \
        size_t str_len = 0;                                                                                  \
        for (size_t i = 0; i < string_count; i++)                                                            \
        {                                                                                                    \
            string_lengths[i] = strlen(strings[i].str);                                                      \
            str_len += string_lengths[i];                                                                    \
        }                                                                                                    \
        char_ptr = ptlang_malloc(str_len + 1);                                                               \
        char *str_ptr = char_ptr;                                                                            \
        for (size_t i = 0; i < string_count; i++)                                                            \
        {                                                                                                    \
            memcpy(str_ptr, strings[i].str, string_lengths[i]);                                              \
            str_ptr += string_lengths[i];                                                                    \
            if (strings[i].to_free)                                                                          \
                ptlang_free(strings[i].str);                                                                 \
        }                                                                                                    \
        *str_ptr = '\0';                                                                                     \
    }

ptlang_utils_graph_node **ptlang_utils_find_cycles(ptlang_utils_graph_node *graph);

char *ptlang_utils_build_str_from_stb_arr(char **components);

void ptlang_utils_graph_free(ptlang_utils_graph_node *graph);

ptlang_utils_str ptlang_utils_sprintf_alloc(const char *fmt, ...);

#endif
