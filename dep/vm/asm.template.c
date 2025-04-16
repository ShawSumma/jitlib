
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

$includes

typedef struct {
    const char *name;
    size_t nargs;
    const char **args;
} $scope_asm_instr_t;

static inline void read_name(size_t max, char *out, const char **pstr) {
    const char *str = *pstr;
    while (*str == ' ') {
        str ++;
    }
    for (size_t i = 0; i + 1 < max; i++) {
        if (
            *str == '_'
            || ('a' <= *str && *str <= 'z')
            || ('A' <= *str && *str <= 'Z')
            || ('0' <= *str && *str <= '9')
        ) {
            *out++ = *str++;
        } else {
            break;
        }
    }
    *out++ = '\0';
    *pstr = str;
}

typedef struct {
    const char *base;
    const char *next;
} read_arg_t;

static inline read_arg_t read_arg(const char *str) {
    read_arg_t ret;
    if (str == NULL) {
        ret.base = NULL;
        ret.next = NULL;
    } else {
        while (*str == ' ') {
            str++;
        }
    
        ret.base = str;
        
        while (*str != '\0' && *str != ',' && *str != '\n' && *str != '\r') {
            str++;
        }
    
        if (*str == ',') {
            str++;
        }
        ret.next = str;
    }

    return ret;
}

static inline bool read_done(const char **pstr) {
    const char *str = *pstr;
    
    while (*str == ' ') {
        str++;
    }

    if (*str == '\n' || *str == '\r') {
        *pstr = str;
        return true;
    }
    
    str++;
    *pstr = str;
    return false;
}

bool $scope_asm($scope_ctx_t *ctx, $scope_buf_t *buf, const char *str) {
    (void) ctx;

    size_t line_num = 1;

    while (*str == ' ' || *str == '\r' || *str == '\n') {
        if (*str == '\n') {
            line_num += 1;
        }
        str++;
    }

    while (*str != '\0') {
        $parsers

        if (op_name[0] == '\0') {
            goto next;
        }

        fprintf(stderr, "$scope asm: parse error on Ln %zu\n", line_num);
        return false;

    next:;

        while (*str == ' ' || *str == '\r' || *str == '\n') {
            if (*str == '\n') {
                line_num += 1;
            }
            str++;
        }
    }

    return true;
}
