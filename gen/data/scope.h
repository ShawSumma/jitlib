
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

struct $scope.buffer.type;
typedef struct $scope.buffer.type $scope.buffer.type;

struct $scope.context.type;
typedef struct $scope.context.type $scope.context.type;

enum {
    $scope.opcodes.list
};

struct $scope.buffer.type {
    size_t len;
    void *mem;
    size_t alloc;
};

struct $scope.context.type {
    $scope.context.fields
};

static inline $scope.buffer.type $scope.buffer.new(void) {
    return ($scope.buffer.type) {
        .len = 0,
        .mem = NULL,
        .alloc = 0,
    };
}

static inline void $scope.buffer.free($scope.buffer.type buf) {
    free(buf.mem);
}

static inline void $scope.buffer.push($scope.buffer.type *buf, size_t len, const void *src) {
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

// static inline void $scope.debug(FILE *out, $scope.buffer.type buf) {
//     for (size_t i = 0; i < buf.len; i++) {
//         fprintf(out, "%zu\n", (size_t) ((uint8_t *) buf.mem)[i]);
//     }
// }

struct $scope.context.type;
typedef struct $scope.context.type $scope.context.type;

// class decl
$scope.class.decls

// user defined header
$scope.header

// opcode builders
$scope.builders

// sytnax parsers
$scope.syntax.decls

// interpreters
$scope.interp.decls
