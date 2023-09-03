#include "ptlang_utils_impl.h"

char *ptlang_utils_build_str(char **components)
{
    size_t str_len = 0;
    for (size_t i = 0; i < arrlenu(components); i++)
    {
        str_len += strlen(components[i]);
    }
    char *str = malloc(str_len + 1);
    char *str_ptr = str;
    for (size_t i = 0; i < arrlenu(components); i++)
    {
        size_t component_len = strlen(components[i]);
        memcpy(str_ptr, components[i], component_len);
        str_ptr += component_len;
    }
    *str_ptr = '\0';
    return str;
}