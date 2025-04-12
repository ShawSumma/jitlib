
#include <vmdef/interp.h>

int main() {
    interp_buf_t buf = interp_buf_new();

    interp_mov_rn(&buf, 0, 4984);

    interp_debug_r(&buf, 0);

    interp_run(buf);

    interp_buf_free(buf);

    return 0;
}
