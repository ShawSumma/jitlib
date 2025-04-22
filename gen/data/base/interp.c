
#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define error(...) (fprintf(stderr, __VA_ARGS__), fprintf(stderr, "\n"), exit(1))

$scope.includes
$interp.globals

#define next(type) (* (type *) ((size_t) ctx->buf.mem + (index += sizeof(type)) - sizeof(type)))

$interp.ret $interp.name($interp.params) {
    (void) ctx;
    
    $interp.enter

    size_t index = 0;

    while (index < ctx->buf.len) {
        size_t index_base = index;
        (void) index_base;
        $scope.opcode.type op = next($scope.opcode.type);
        switch(op) {
            $interp.cases
            default: {
                #if defined(__GNUC__)
                    __builtin_unreachable();
                #elif !defined(_MSC_VER)
                    __assume(false);
                #else

                #endif
            }
        }   
    }

    $interp.exit
}
