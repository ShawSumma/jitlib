
CC = gcc
EXE = .exe
OUT = ./bin/out$(EXE)
ARGS =

LDFLAGS =
CFLAGS = -O2
LUA = bin/lua$(EXE)
DASC_FILE = src/asm/jit.dasc
DO_FILE = $(DASC_FILE:%.dasc=%.o)
C_FILE = src/entry.c
O_FILE = $(C_FILE:%.c=%.o)

default: bins

bins: run

run: $(OUT) .dummy
	$(OUT) $(ARGS)

$(OUT): $(O_FILE) $(DO_FILE)
	$(CC) $(O_FILE) $(DO_FILE) -o $(OUT) $(LDFLAGS)

$(DO_FILE): $(@:%.o=%.dasc) $(LUA)
	$(LUA) src/asm/dasm/src/dynasm.lua -D X64 -o $(@:%.o=%.c) $(@:%.o=%.dasc)
	$(CC) -c $(@:%.o=%.c) -o $(@) $(CFLAGS)

$(O_FILE): $(@:%.o=%.c) $(LUA)
	$(CC) -c $(@:%.o=%.c) -o $(@) $(CFLAGS)

$(LUA): | bin
	test -f $(LUA) || $(CC) src/asm/dasm/minilua/minilua.c -o $(LUA)

bin:
	test -d bin || mkdir bin

.dummy:
