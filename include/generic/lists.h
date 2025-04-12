
#if !defined(ELEM)
#error need: #define ELEM ...
#endif

#if !defined(TYPE)
#error need: #define TYPE(v) ...
#endif

#include <stdlib.h>
#include <stddef.h>

typedef ELEM TYPE(elem_t);

#undef ELEM

struct TYPE(t);
typedef struct TYPE(t) TYPE(t);

struct TYPE(t) {
    size_t len;
    TYPE(elem_t) *data;
    size_t alloc;
};

static inline TYPE(t) TYPE(new) (void) {
    return (TYPE(t)) {
        .len = 0,
    };
}

static inline void TYPE(delete) (TYPE(t) vec) {
    free(vec.data);
}

static inline void TYPE(push) (TYPE(t) *vec, TYPE(elem_t) elem) {
    size_t pos = vec->len++;
    if (pos >= vec->alloc) {
        vec->alloc = vec->len * 2;
        vec->data = realloc(vec->data, vec->alloc * sizeof(TYPE(elem_t) ));
    }
    vec->data[pos] = elem;
}

static inline TYPE(elem_t) *TYPE(index) (TYPE(t) *vec, size_t index) {
    #if defined(SAFE)
        if (index >= vec->len) {
            SAFE("OUT OF BOUNDS %zu", index);
        }
    #endif
    return &vec->data[index];
}

static inline TYPE(elem_t) TYPE(pop) (TYPE(t) *vec) {
    #if defined(SAFE)
        if (vec->len == 0) {
            SAFE("POP FROM EMPTY LIST");
        }
    #endif
    return vec->data[--vec->len];
}

static inline size_t TYPE(len) (TYPE(t) *vec) {
    return vec->len;
}

static inline _Bool TYPE(is_empty) (TYPE(t) *vec) {
    return TYPE(len)(vec) == 0;
}

#undef LIST
