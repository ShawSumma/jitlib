
#include <vmdef/x86.h>

#include <io.h>

#include <inttypes.h>
#include <stdio.h>
#include <assert.h>

uint64_t time_jit(x86_context_t *ctx, int64_t iters) {
    uint64_t start = __builtin_ia32_rdtsc();

    size_t (*fn)(void) = x86_jit(ctx);

    int64_t value = 0;
    for (int64_t i = 0; i < iters; i++) {
        value += fn();
    }

    uint64_t end = __builtin_ia32_rdtsc();

    printf("%"PRIi64"\n", value / iters);

    return end - start;
}

uint64_t time_int(x86_context_t *ctx, int64_t iters) {
    uint64_t start = __builtin_ia32_rdtsc();

    int64_t value = 0;
    for (int64_t i = 0; i < iters; i++) {
        value += x86_int(ctx);
    }

    uint64_t end = __builtin_ia32_rdtsc();

    printf("%"PRIi64"\n", value / iters);

    return end - start;
}

int main(int argc, char **argv) {
    x86_context_t ctx = x86_new();

    if (argc < 2) {
        fprintf(stderr, "error: no arguments at command line\n");
        return 1;
    }

    io_read_t read = io_read(argv[1]);

    if (read.code != IO_READ_OK) {
        fprintf(stderr, "%s\n", read.error);
        return 1;
    }

    x86_op_mov(&ctx, 0, 3);
    x86_op_mov(&ctx, 1, 4);
    x86_op_imul(&ctx, 0, 0, 1);
    x86_op_ret(&ctx);
    // if (!x86_asm(&ctx, read.chars.len, read.chars.data)) {
    //     fprintf(stderr, "error: parse error\n");
    //     return 1;
    // }

    int64_t iters = 1000 * 1000 * 500;

    uint64_t rjit = time_jit(&ctx, iters); (void) rjit;
    uint64_t rint = time_int(&ctx, iters); (void) rint;
    printf("%f\n", (double) rint / (double) rjit);

    x86_free(ctx);

    return 0;
}
