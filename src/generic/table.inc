
#if !defined(KEY)
#error need: #define KEY ...
#endif

#if !defined(VALUE)
#error need: #define VALUE ...
#endif

#if !defined(TYPE)
#error need: #define TYPE ...
#endif

struct TYPE(t);
typedef struct TYPE(t) TYPE(t);

struct TYPE(t) {

};

#undef KEY
#undef VALUE
#undef TYPE
