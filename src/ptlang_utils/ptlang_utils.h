#ifndef PTLANG_UTILS_H
#define PTLANG_UTILS_H

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#define ptlang_malloc(size) size == 0 ? NULL : malloc(size)
#define ptlang_free(ptr) free(ptr)
#define ptlang_realloc(ptr, size) size == 0 ? (free(ptr), NULL) : realloc(ptr, size)

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

ptlang_utils_graph_node **ptlang_utils_find_cycles(ptlang_utils_graph_node *graph);

char *ptlang_utils_build_str_from_stb_arr(char **components);

void ptlang_utils_graph_free(ptlang_utils_graph_node *graph);

#endif
