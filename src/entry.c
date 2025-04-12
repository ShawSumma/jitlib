
#include <vmdef/interp.h>

#define ELEM char
#define TYPE(t) list_char_ ## t
#include <generic/lists.h>

int main(int argc, char **argv) {
    interp_buf_t buf = interp_buf_new();

    list_char_t ls = list_char_new();

    if (argc < 2) {
        fprintf(stderr, "error: no arguments at command line\n");
        return 1;
    }

    FILE *f = fopen(argv[1], "r");

    if (f == NULL) {
        fprintf(stderr, "error: file open error\n");
        return 1;
    }

    while (!feof(f)) {
        char c = fgetc(f);
        if (c == EOF) {
            break;
        }
        list_char_push(&ls, c);
    }
    list_char_push(&ls, 0);

    if (!interp_asm(&buf, ls.data)) {
        fprintf(stderr, "error: parse error\n");
        return 1;
    }

    list_char_delete(ls);

    interp_run(buf);

    interp_buf_free(buf);

    return 0;
}
