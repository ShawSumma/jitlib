
#include <stdint.h>

struct jit_node_t;
typedef struct jit_node_t jit_node_t;

#define ELEM jit_node_t *
#define TYPE(n) jit_nodes_ ## n
#include "generic/lists.inc"

enum {
    JIT_NODE_START,
    JIT_NODE_RETURN,
    JIT_NODE_CONSTANT,
};

struct jit_node_t {
    uint64_t id: 48;
    uint64_t type: 16;
    jit_nodes_t inputs;
    jit_nodes_t outputs;
};
