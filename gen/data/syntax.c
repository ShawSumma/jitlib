
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include <stdio.h>
#include <ctype.h>
#include <inttypes.h>

$scope.includes
$scope.globals

bool $syntax.name($scope.context.type *ctx, $scope.buffer.type *out, ptrdiff_t len, const char *str) {
    (void) ctx;

    if (len < 0) {
        len = strlen(str);
    }

    char *buf = malloc(sizeof(char) * (len + 1));
    memcpy(buf, str, len);
    buf[len] = '\0';

    size_t read = 0;

    while (buf[read] == ' ' || buf[read] == '\r' || buf[read] == '\n') {
        read += 1;
    }

    while (true) {
        while (buf[read] == ' ' || buf[read] == '\r' || buf[read] == '\n') {
            read += 1;
        }

        if (buf[read] == '\0') {
            break;
        }

        if (buf[read] == '#') {
            while (buf[read] != '\n') {
                read += 1;
            }
            continue;
        }

        if (buf[read] == '\r' || buf[read] == '\n') {
            read += 1;
            continue;
        }

        if (buf[read] == '\0') {
            break;
        }

        const char *name = &buf[read];

        while (buf[read] != ' ' && buf[read] != '\r' && buf[read] != '\n' && buf[read] != '\0') {
            read += 1;
        }

        buf[read] = '\0';

        read += 1;
        
        size_t argc = 0;
        const char *argv[$syntax.max.args] = { 0 };

        while (buf[read] == ' ') {
            read += 1;
        }

        while (buf[read] != '\r' && buf[read] != '\n' && buf[read] != '\0') {
            if (argc == $syntax.max.args) {
                goto fail;
            }
            
            while (buf[read] == ' ') {
                read += 1;
            }

            argv[argc++] = &buf[read];

            while (buf[read] != ',' && buf[read] != '#' && buf[read] != '\r' && buf[read] != '\n' && buf[read] != '\0') {
                read += 1;
            }

            bool do_break = buf[read] == '\r' || buf[read] == '\n' || buf[read] == '\0';

            buf[read] = '\0';
            read += 1;

            if (do_break) {
                break;
            }
        }

        $syntax.opcodes
    fail:;
        {
            size_t line = 1;
            size_t column = 1;
            size_t chr = 0;
            
            while (chr < read) {
                if (str[chr] == '\n') {
                    line += 1;
                    column = 1;
                } else {
                    column += 1;
                }
                chr += 1;
            }

            fprintf(stderr, "$scope.name asm: parse error on line %zu, column %zu\n", line, column);
            free(buf);
            return false;
        }

    next:;
    }

    free(buf);
    return true;
}
