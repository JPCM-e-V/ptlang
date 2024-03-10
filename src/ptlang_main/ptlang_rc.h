#include "ptlang_utils.h"
#include <stddef.h>

#ifdef NDEBUG

#    define PTLANG_RC_DEFINE_REF_TYPE(type, name)                                                            \
        typedef struct                                                                                       \
        {                                                                                                    \
            size_t ref_count;                                                                                \
            type content;                                                                                    \
        } *name;

#    define ptlang_rc_alloc(ref)                                                                             \
        {                                                                                                    \
            ref = ptlang_malloc(sizeof(*ref));                                                               \
            ref->ref_count = 1;                                                                              \
        }

#    define ptlang_rc_add_ref(ref, new_ref)                                                                  \
        {                                                                                                    \
            ref->ref_count += 1;                                                                             \
            new_ref = ref;                                                                                   \
        }

#    define ptlang_rc_remove_ref(ref, destroy_fn)                                                            \
        {                                                                                                    \
            ref->ref_count -= 1;                                                                             \
            if (ref->ref_count == 0)                                                                         \
            {                                                                                                \
                destroy_fn(&ref->content);                                                                   \
                ptlang_free(ref);                                                                            \
            }                                                                                                \
        }

#    define ptlang_rc_deref(ref) (ref->content)

#else

#    define PTLANG_RC_DEFINE_REF_TYPE(type, name)                                                            \
        typedef struct                                                                                       \
        {                                                                                                    \
            size_t ref_count;                                                                                \
            type content;                                                                                    \
        } **name;

#    define ptlang_rc_alloc(ref)                                                                             \
        {                                                                                                    \
            ref = ptlang_malloc(sizeof(*ref));                                                               \
            *ref = ptlang_malloc(sizeof(**ref));                                                             \
            (*ref)->ref_count = 1;                                                                           \
        }

#    define ptlang_rc_add_ref(ref, new_ref)                                                                  \
        {                                                                                                    \
            (*ref)->ref_count += 1;                                                                          \
            new_ref = ptlang_malloc(sizeof(*new_ref));                                                       \
            *new_ref = *ref;                                                                                 \
        }

#    define ptlang_rc_remove_ref(ref, destroy_fn)                                                            \
        {                                                                                                    \
            (*ref)->ref_count -= 1;                                                                          \
            if ((*ref)->ref_count == 0)                                                                      \
            {                                                                                                \
                destroy_fn(&(*ref)->content);                                                                \
                ptlang_free(*ref);                                                                           \
            }                                                                                                \
            ptlang_free(ref);                                                                                \
        }

#    define ptlang_rc_deref(ref) ((*ref)->content)

#endif

// PTLANG_RC_DEFINE_REF_TYPE(int, intref);

// #include <stdio.h>

// void use(void *anything) { printf("%p\n", anything); }

// void remove_int(int *a) { printf("remove int %d\n", *a); }

// int main()
// {
//     intref ref, otherref;
//     ptlang_rc_alloc(ref);

//     ptlang_rc_deref(ref) = 1;

//     use(&ptlang_rc_deref(ref));

//     ptlang_rc_add_ref(ref, otherref);

//     use(ref);

//     ptlang_rc_remove_ref(ref, remove_int);

//     ptlang_rc_deref(ref) = 1;

//     ptlang_rc_remove_ref(otherref, remove_int);

//     return 0;
// }