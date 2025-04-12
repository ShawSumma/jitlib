
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

struct $scope_buf_t;
typedef struct $scope_buf_t $scope_buf_t;

typedef uint16_t $scope_opcode_t;

enum {
    $ops
};

struct $scope_buf_t {
    size_t len;
    void *mem;
    size_t alloc;
};

static inline $scope_buf_t $scope_buf_new(void) {
    return ($scope_buf_t) {
        .len = 0,
        .mem = NULL,
        .alloc = 0,
    };
}

static inline void $scope_buf_free($scope_buf_t buf) {
    free(buf.mem);
}

static inline void $scope_buf_push($scope_buf_t *buf, size_t len, const void *src) {
    if (buf->len + len >= buf->alloc || buf->mem == NULL) {
        buf->alloc = (buf->len + len) * 2;
        void *next = realloc(buf->mem, buf->alloc);
        if (next == NULL) {
            free(buf->mem);
            buf->mem = NULL;
            buf->alloc = 0;
            buf->len = 0;
            return;
        }
        buf->mem = next;
    }
    memcpy((void *) ((size_t) buf->mem + buf->len), src, len);
    buf->len += len;
}

static inline void $scope_debug(FILE *out, $scope_buf_t buf) {
    for (size_t i = 0; i < buf.len; i++) {
        fprintf(out, "%zu\n", (size_t) ((uint8_t *) buf.mem)[i]);
    }
}

$builders

bool $scope_asm($scope_buf_t *buf, const char *str);

void $scope_run(const $scope_buf_t buf);
