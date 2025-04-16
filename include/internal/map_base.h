////////////////////////////////
// map_t - Non-blocking hashmap
////////////////////////////////
// You wanna intern lots of things on lots of cores? this is for you. It's
// inspired by Cliff's non-blocking hashmap.
//
// To use it, you'll need to define MAP_FN and then include the header:
//
//   #define MAP_FN(n) XXX_hm_ ## n
//   #include <nbhm.h>
//
// This will compile implementations of the hashset using
//
//   bool MAP_FN(cmp)(const void *a, const void *b);
//   uint32_t MAP_FN(hash)(const void *a);
//
// The exported functions are:
//
//   void *MAP_FN(get)(map_t *hs, void *key);
//   void *MAP_FN(put)(map_t *hs, void *key, void *val);
//   void *MAP_FN(put_if_null)(map_t *hs, void *key, void *val);
//   void MAP_FN(resize_barrier)(map_t *hs);
//
#if !defined(MAP_HEADER)
#define MAP_HEADER

#include <threads.h>
#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>

// Virtual memory allocation (since the tables are generally nicely page-size friendly)
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define MAP_VIRTUAL_ALLOC(size)     VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)
#define MAP_VIRTUAL_FREE(ptr, size) VirtualFree(ptr, size, MEM_RELEASE)
#else
#include <sys/mman.h>

#define MAP_VIRTUAL_ALLOC(size)     mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)
#define MAP_VIRTUAL_FREE(ptr, size) munmap(ptr, size)
#endif

// traditional heap ops
#if !defined(MAP_REALLOC)
#define MAP_REALLOC(ptr, size) realloc(ptr, size)
#endif

// for the time in the ebr entry
#define MAP_PINNED_BIT (1ull << 63ull)
#define MAP_PRIME_BIT  (1ull << 63ull)

enum {
    MAP_LOAD_FACTOR = 75,
    MAP_MOVE_AMOUNT = 256,
};

typedef struct map_ebr_entry_t {
    _Atomic(struct map_ebr_entry_t*) next;
    _Atomic(uint64_t) time;

    // keep on a separate cacheline to avoid false sharing
    _Alignas(64) int id;
} map_ebr_entry_t;

typedef struct {
    _Atomic(void *) key;
    _Atomic(void *) val;
} map_entry_t;

typedef struct map_table_t map_table_t;
struct map_table_t {
    _Atomic(map_table_t*) prev;

    uint32_t cap;

    // reciprocals to compute modulo
    uint64_t a, sh;

    // tracks how many entries have
    // been moved once we're resizing
    _Atomic uint32_t moved;
    _Atomic uint32_t move_done;
    _Atomic uint32_t count;

    map_entry_t data[];
};

typedef struct {
    _Atomic(map_table_t*) latest;
} map_t;

typedef struct map_freenode_t map_freenode_t;
struct map_freenode_t {
    _Atomic(map_freenode_t*) next;
    map_table_t *table;
};

static inline size_t map_compute_cap(size_t y) {
    // minimum capacity
    if (y < 256) {
        y = 256;
    } else {
        y = ((y + 1) / 3) * 4;
    }

    size_t cap = 1ull << (64 - __builtin_clzll(y - 1));
    return cap - (sizeof(map_table_t) / sizeof(map_entry_t));
}

static inline void map_compute_size(map_table_t *table, size_t cap) {
    // reciprocals to compute modulo
    #if defined(__GNUC__) || defined(__clang__)
        table->sh = 64 - __builtin_clzll(cap);
    #else
        uint64_t sh = 0;
        while (cap > (1ull << sh)){ sh++; }
        table->sh = sh;
    #endif

    table->sh += 63 - 64;

    #if (defined(__GNUC__) || defined(__clang__)) && defined(__x86_64__)
        uint64_t d,e;
        __asm__("divq %[v]" : "=a"(d), "=d"(e) : [v] "r"(cap), "a"(cap - 1), "d"(1ull << table->sh));
        table->a = d;
    #elif defined(_MSC_VER)
        uint64_t rem;
        table->a = _udiv128(1ull << table->sh, cap - 1, cap, &rem);
    #else
        #error "Unsupported target"
    #endif

    table->cap = cap;
}

static inline map_t map_alloc(size_t initial_cap) {
    size_t cap = map_compute_cap(initial_cap);
    map_table_t *table = MAP_VIRTUAL_ALLOC(sizeof(map_table_t) + cap * sizeof(map_entry_t));
    map_compute_size(table, cap);
    return (map_t){ .latest = table };
}

static inline void map_free(map_t *hs) {
    map_table_t *curr = hs->latest;
    while (curr) {
        map_table_t *next = curr->prev;
        MAP_VIRTUAL_FREE(curr, sizeof(map_table_t) + curr->cap * sizeof(map_entry_t));
        curr = next;
    }
}

// for spooky stuff
static inline map_entry_t *map_array(map_t *hs) { return hs->latest->data; }
static inline size_t map_count(map_t *hs)      { return hs->latest->count; }
static inline size_t map_capacity(map_t *hs)   { return hs->latest->cap; }

#define map_for(it, hs) for (map_entry_t *it = map_array(hs), **_map_for_end_ = &it[map_capacity(hs)]; it != _map_for_end_; it++) if (*it != NULL)
#endif
