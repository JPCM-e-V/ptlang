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

static char *ptlang_utils_build_str(char **components, size_t components_len)
{
    size_t str_len = 0;
    for (size_t i = 0; i < components_len; i++)
    {
        str_len += strlen(components[i]);
    }
    char *str = malloc(str_len + 1);
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

char *ptlang_utils_build_str_from_stb_arr(char **components)
{
    return ptlang_utils_build_str(components, arrlenu(components));
}

char *ptlang_utils_build_str_from_arr(char *components[])
{
    return ptlang_utils_build_str(components, sizeof(components) / sizeof(char *));
}