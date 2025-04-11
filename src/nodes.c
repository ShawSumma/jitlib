
#include "nodes.h"

#include <stdio.h>
#include <inttypes.h>

void jit_node_to_graph(FILE *out, jit_node_t *node) {
    fprintf(out, "%zu\n", (size_t) node->id);
}

void jit_nodes_to_graph(FILE *out, jit_nodes_t *nodes) {
    fprintf(out, "graph {\n");
    for (size_t i = 0; i < jit_nodes_size(nodes); i++) {
        jit_node_to_graph(out, *jit_nodes_index(nodes, i));
    }
    fprintf(out, "}\n");
}
