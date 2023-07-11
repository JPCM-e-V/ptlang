#include <stdlib.h>

#define ptlang_malloc(size) size == 0 ? NULL : malloc(size)
#define ptlang_free(ptr) free(ptr)
#define ptlang_realloc(ptr, size) size == 0 ? (free(ptr), NULL) : realloc(ptr, size)
