
#include <vmdef/interp.h>

#include <io.h>

#include <inttypes.h>
#include <stdio.h>
#include <assert.h>

int main(int argc, char **argv) {
    interp_context_t ctx = interp_new();

    if (argc < 2) {
        fprintf(stderr, "error: no arguments at command line\n");
        return 1;
    }

    io_read_t read = io_read(argv[1]);

    if (read.code != IO_READ_OK) {
        fprintf(stderr, "%s\n", read.error);
        return 1;
    }

    if (!interp_asm(&ctx, read.chars.len, read.chars.data)) {
        fprintf(stderr, "error: parse error\n");
        return 1;
    }

    interp_run(&ctx);

    interp_free(ctx);

    return 0;
}
