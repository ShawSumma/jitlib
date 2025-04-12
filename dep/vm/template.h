
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

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

$builders

void $scope_run($scope_buf_t bytecode);
