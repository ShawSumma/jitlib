
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
    $scope.buffer.type buf;
    $scope.context.fields
};

static inline $scope.context.type $scope.context.new(void) {
    return ($scope.context.type) {
        .buf = ($scope.buffer.type) {
            .len = 0,
            .mem = NULL,
            .alloc = 0,
        },
    };
}

static inline void $scope.context.free($scope.context.type ctx) {
    free(ctx.buf.mem);
}

static inline void $scope.context.push($scope.context.type *ctx, size_t len, const void *src) {
    if (ctx->buf.len + len >= ctx->buf.alloc || ctx->buf.mem == NULL) {
        ctx->buf.alloc = (ctx->buf.len + len) * 2;
        void *next = realloc(ctx->buf.mem, ctx->buf.alloc);
        if (next == NULL) {
            free(ctx->buf.mem);
            ctx->buf.mem = NULL;
            ctx->buf.alloc = 0;
            ctx->buf.len = 0;
            return;
        }
        ctx->buf.mem = next;
    }
    memcpy((void *) ((size_t) ctx->buf.mem + ctx->buf.len), src, len);
    ctx->buf.len += len;
}

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
