
#pragma once
#if !defined(JITLIB_IO_HEADER)
#define JITLIB_IO_HEADER

#define ELEM char
#define TYPE(t) io_list_char_ ## t
#include <generic/lists.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>

enum {
    IO_READ_OK,
    IO_READ_NO_SUCH_FILE,
};

struct io_read_t;
typedef struct io_read_t io_read_t;

struct io_read_t {
    union {
        io_list_char_t chars;
        const char *error;
    };
    uint8_t code;
};

static inline io_read_t io_read(const char *file_name) {
    FILE *f = fopen(file_name, "r");

    if (f == NULL) {
        return (io_read_t) {
            .code = IO_READ_NO_SUCH_FILE,
            .error = strerror(errno),
        };
    }

    io_list_char_t chars = io_list_char_new();

    while (!feof(f)) {
        char c = fgetc(f);
        if (c == EOF) {
            break;
        }
        io_list_char_push(&chars, c);
    }

    io_list_char_push(&chars, 0);

    return (io_read_t) {
        .code = IO_READ_OK,
        .chars = chars,
    };
}

static inline void io_read_free(io_read_t read) {
    if (read.code == IO_READ_OK) {
        io_list_char_delete(read.chars);
    } else if (read.code == IO_READ_NO_SUCH_FILE) {
    } else {
        
    }
}

#endif
