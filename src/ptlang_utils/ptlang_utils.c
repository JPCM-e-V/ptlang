#include "ptlang_utils_impl.h"

static void tarjan(ptlang_utils_graph_node *v, ptlang_utils_graph_node *graph,
                   ptlang_utils_graph_node ***stack, size_t *index, ptlang_utils_graph_node ***cycles)
{
    v->index = *index;
    v->lowlink = *index;
    (*index)++;
    arrput(*stack, v);
    v->on_stack = true;
    bool self_reference = false;

    for (size_t i = 0; i < arrlenu(v->edges_to); ++i)
    {
        ptlang_utils_graph_node *w = v->edges_to[i];

        if (w->index == -1)
        {
            tarjan(w, graph, stack, index, cycles);
            if (w->lowlink < v->lowlink)
            {
                v->lowlink = w->lowlink;
            }
        }
        else if (w->on_stack)
        {
            if (w->index < v->lowlink)
            {
                v->lowlink = w->index;
            }
            else if (w == v)
            {
                self_reference = true;
            }
        }
    }

    if (v->lowlink == v->index)
    {
        ptlang_utils_graph_node *w = arrpop(*stack);
        w->on_stack = false;
        w->in_cycle = w != v || self_reference;
        if (w != v || self_reference)
            arrpush(*cycles, w);
        while (w != v)
        {
            w = arrpop(*stack);
            arrpush(*cycles, w);
            w->on_stack = false;
            w->in_cycle = true;
        }
    }
}

/**
 * @return ptlang_utils_graph_node** an array of all nodes in cycles grouped by cycle
 */
ptlang_utils_graph_node **ptlang_utils_find_cycles(ptlang_utils_graph_node *graph)
{
    ptlang_utils_graph_node **stack = NULL;
    ptlang_utils_graph_node **cycles = NULL;
    size_t index = 0;

    for (size_t i = 0; i < arrlenu(graph); ++i)
    {
        ptlang_utils_graph_node *current = graph + i;
        if (current->index == -1)
        {
            tarjan(current, graph, &stack, &index, &cycles);
        }
    }

    arrfree(stack);
    return cycles;
}

void ptlang_utils_graph_free(ptlang_utils_graph_node *graph)
{
    for (size_t i = 0; i < arrlenu(graph); i++)
    {
        arrfree(graph[i].edges_to);
    }
    arrfree(graph);
}

static char *ptlang_utils_build_str_from_components(char **components, size_t components_len)
{
    size_t str_len = 0;
    for (size_t i = 0; i < components_len; i++)
    {
        str_len += strlen(components[i]);
    }
    char *str = ptlang_malloc(str_len + 1);
    char *str_ptr = str;
    for (size_t i = 0; i < components_len; i++)
    {
        size_t component_len = strlen(components[i]);
        memcpy(str_ptr, components[i], component_len);
        str_ptr += component_len;
    }
    *str_ptr = '\0';
    return str;
}

char *ptlang_utils_build_str_from_char_arr(char **components)
{
    return ptlang_utils_build_str_from_components(components, arrlenu(components));
}

ptlang_utils_str ptlang_utils_sprintf_alloc(const char *fmt, ...)
{

    /* Determine required size. */

    va_list ap;
    va_start(ap, fmt);
    // clang-tidy false positive: llvm/llvm-project#40656
    int n = vsnprintf(NULL, 0, fmt, ap); // NOLINT(clang-analyzer-valist.Uninitialized)
    va_end(ap);

    if (n < 0)
        return CONST_STR(NULL);

    size_t size = (size_t)n + 1; /* One extra byte for '\0' */
    ptlang_utils_str str = ALLOCATED_STR(ptlang_malloc(size));
    if (str.str == NULL)
        return CONST_STR(NULL);

    va_start(ap, fmt);
    n = vsnprintf(str.str, size, fmt, ap);
    va_end(ap);

    if (n < 0)
    {
        ptlang_free(str.str);
        return CONST_STR(NULL);
    }

    return str;
}

char *ptlang_utils_build_str_from_str_arr(ptlang_utils_str *strings)
{
    size_t string_count = arrlenu(strings);
    size_t str_len = 0;
    for (size_t i = 0; i < string_count; i++)
    {
        str_len += strlen(strings[i].str);
    }
    char *str = ptlang_malloc(str_len + 1);
    char *str_ptr = str;
    for (size_t i = 0; i < string_count; i++)
    {
        size_t component_len = strlen(strings[i].str);
        memcpy(str_ptr, strings[i].str, component_len);
        str_ptr += component_len;
        if (strings[i].to_free)
            ptlang_free(strings[i].str);
    }
    *str_ptr = '\0';
    return str;
}
