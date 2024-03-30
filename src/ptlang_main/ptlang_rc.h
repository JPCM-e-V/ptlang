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

#    define PTLANG_RC_DEFINE_REF_TYPE_ONLY(name) typedef struct name##_ref_struct **name

#    define ptlang_rc_alloc(ref)                                                                             \
        (void)((ref) = ptlang_malloc(sizeof(*(ref))), *(ref) = ptlang_malloc(sizeof(**(ref))),               \
               (*(ref))->ref_count = 0)

#    define ptlang_rc_add_ref(ref)                                                                           \
        ((*(ref))->ref_count += 1, memcpy(ptlang_malloc(sizeof(*(ref))), (ref), sizeof(*(ref))))

#    define ptlang_rc_remove_ref_with_fn(ref, destroy_fn, ...)                                               \
        (((*(ref))->ref_count == 0) ? (destroy_fn(&(*(ref))->content), ptlang_free(*(ref)))                  \
                                    : (void)((*(ref))->ref_count -= 1),                                      \
         ptlang_free(ref))

#    define ptlang_rc_deref(ref) ((*(ref))->content)

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
