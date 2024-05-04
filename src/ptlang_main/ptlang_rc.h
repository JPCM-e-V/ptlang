#include "ptlang_utils.h"
#include <string.h>

#define PTLANG_RC_DEFINE_REF_STRUCT(type, name)                                                              \
    struct name##_ref_struct                                                                                 \
    {                                                                                                        \
        size_t ref_count;                                                                                    \
        type content;                                                                                        \
    }

// #define PTLANG_RC_DEFINE_REF_TYPE(type, name)                                                                \
//     PTLANG_RC_DEFINE_REF_STRUCT(type, name) PTLANG_RC_DEFINE_REF_TYPE_ONLY(name)

#ifdef NDEBUG

#    define PTLANG_RC_DEFINE_REF_TYPE_ONLY(name) typedef struct name##_ref_struct *name

#    define ptlang_rc_alloc(ref) (void)((ref) = ptlang_malloc(sizeof(*(ref))), (ref)->ref_count = 0)

#    define ptlang_rc_add_ref(ref) ((ref)->ref_count += 1, (ref))

#    define ptlang_rc_remove_ref_with_fn(ref, destroy_fn, ...)                                               \
        (((ref)->ref_count == 0) ? (destroy_fn(&(ref)->content), ptlang_free(ref))                           \
                                 : (void)((ref)->ref_count -= 1))

#    define ptlang_rc_deref(ref) ((ref)->content)

#else
#    include <stdint.h>

void *ptlang_rc_add_ref_func(const char *function, char *file, uint64_t line, void **ref);

#    define PTLANG_RC_DEFINE_REF_TYPE_ONLY(name) typedef struct name##_ref_struct **name

#    ifdef DEBUG_PTR
#        define ptlang_rc_alloc(ref)                                                                         \
            (void)((ref) = ptlang_malloc(sizeof(*(ref))), *(ref) = ptlang_malloc(sizeof(**(ref))),           \
                   (*(ref))->ref_count = 0,                                                                  \
                   printf("allo ar inner: %p outer: %p method: %s file: " __FILE__ ":%d\n", (void *)*ref,    \
                          (void *)ref, &__FUNCTION__[0], __LINE__))
#    else
#        define ptlang_rc_alloc(ref)                                                                         \
            (void)((ref) = ptlang_malloc(sizeof(*(ref))), *(ref) = ptlang_malloc(sizeof(**(ref))),           \
                   (*(ref))->ref_count = 0)
#    endif

// #    define ptlang_rc_add_ref(ref)                                                                           \
//         (printf("ar inner: %p outer: %p method: %s file: " __FILE__ ":%d\n", *ref, ref, __FUNCTION__,        \
//                 __LINE__),                                                                                   \
//          (*(ref))->ref_count += 1, memcpy(ptlang_malloc(sizeof(*(ref))), (ref), sizeof(*(ref))))

#    define ptlang_rc_add_ref(ref)                                                                           \
        ptlang_rc_add_ref_func(&__FUNCTION__[0], __FILE__, __LINE__, (void **)(ref))

#    ifdef DEBUG_PTR
#        define ptlang_rc_remove_ref_with_fn(ref, destroy_fn, ...)                                           \
            (printf("%srr inner: %p outer: %p method: %s file: " __FILE__ ":%d\n",                           \
                    ((*(ref))->ref_count == 0) ? "del " : "", (void *)*ref, (void *)ref, &__FUNCTION__[0],   \
                    __LINE__),                                                                               \
             ((*(ref))->ref_count == 0) ? (destroy_fn(&(*(ref))->content), ptlang_free(*(ref)))              \
                                        : (void)((*(ref))->ref_count -= 1),                                  \
             ptlang_free(ref))
#    else
#        define ptlang_rc_remove_ref_with_fn(ref, destroy_fn, ...)                                           \
            (((*(ref))->ref_count == 0) ? (destroy_fn(&(*(ref))->content), ptlang_free(*(ref)))              \
                                        : (void)((*(ref))->ref_count -= 1),                                  \
             ptlang_free(ref))
#    endif

#    define ptlang_rc_deref(ref) ((*(ref))->content)

#    ifdef asdf
#        include <inttypes.h>
#        include <stdio.h>
void *ptlang_rc_add_ref_func(const char *function, char *file, uint64_t line, void **ref)
{
    void **new_ref = memcpy(ptlang_malloc(sizeof(*(ref))), (ref), sizeof(*(ref)));
    *((size_t *)(*(ref))) += 1;
    ;
#        ifdef DEBUG_PTR
    printf("ar inner: %p outer: %p method: %s file: %s:%" PRIu64 "\n", (void *)*ref, (void *)new_ref,
           function, file, line);
#        endif
    return (void *)new_ref;
}
#    endif

#endif

#define ptlang_rc_remove_ref(...) ptlang_rc_remove_ref_with_fn(__VA_ARGS__, (void), i r g e n d e t w a s)

// #include <stdio.h>
// PTLANG_RC_DEFINE_REF_TYPE(int, intref)

// void use(void *anything);
// void use(void *anything) { printf("%p\n", anything); }

// void remove_int(int *a);
// void remove_int(int *a) { printf("remove int %d\n", *a); }

// int main(void)
// {
//     intref ref, otherref;
//     ptlang_rc_alloc(ref);

//     ptlang_rc_deref(ref) = 1;

//     use(&ptlang_rc_deref(ref));

//     otherref = ptlang_rc_add_ref(ref);

//     // otherref = ((*ref)->ref_count += 1, *(uint8_t *)malloc(sizeof(*ref)) = *ref);

//     use(ref);

//     ptlang_rc_remove_ref(ref, remove_int);

//     ptlang_rc_deref(otherref) = 1;

//     ptlang_rc_remove_ref(otherref, remove_int);

//     return 0;
// }
