
CC = gcc
EXE = 
OUT = ${BIN_DIR}out${EXE}
ARGS =
PRE =

# CC_PRE = env LANG=en_US.ISO-8859-1

BUILD_DIR = build/
OBJ_DIR = ${BUILD_DIR}obj/
BIN_DIR = ${BUILD_DIR}bin/
GEN_DIR = ${BUILD_DIR}gen/
DEP_DIR = ${BUILD_DIR}dep/

FLAGS = 
LDFLAGS = -lm
CFLAGS = -std=c11 -O2 -Wall -Wextra -pedantic -I dep/dasm/src

CFLAGS += -fanalyzer

DASC_FILES = src/asm/jit.dasc
C_FILES = src/nodes.c src/entry.c

LDFLAGS += ${FLAGS}
CFLAGS += ${FLAGS}

LUA = ${BIN_DIR}lua${EXE}

C_FILES += ${DASC_FILES:%.dasc=${GEN_DIR}%.c}

O_FILE = ${C_FILES:%.c=${OBJ_DIR}%.o}

MKDIR = ${QUIET} mkdir -p

DASC_SCAN = ${@:${GEN_DIR}%.c=${DEP_DIR}%.dep}
O_SCAN = ${@:${OBJ_DIR}%.o=${DEP_DIR}%.dep}
C_SCAN = ${@:${OBJ_DIR}%.c=${DEP_DIR}%.dep}

DEP_FILES != find "${DEP_DIR}" -type f -name '*.dep'

QUIET = @

TEST_DEPS = ${O_FILE:${OBJ_DIR}%.o=test -f ${DEP_DIR}%.dep && }

default: run

include ${DEP_FILES}

clean: .dummy
	rm -rf ${BUILD_DIR}

run: bins
	${QUIET} ${PRE} ${OUT} ${ARGS}

bins: ${OUT}

${OUT}: ${O_FILE}
	${MKDIR} ${dir ${@}}
	${CC_PRE} ${CC} ${O_FILE} -o ${OUT} ${LDFLAGS}

${GEN_DIR}%.c: %.dasc ${LUA}
	${MKDIR} ${dir ${@}}
	${MKDIR} ${dir ${DASC_SCAN}}
	${LUA} dep/dasm/src/dynasm.lua --depfile "${DASC_SCAN}" -D X64 -o ${@} ${@:${GEN_DIR}%.c=%.dasc}
	@echo ${DASC_SCAN}

${OBJ_DIR}%.o: %.c
	${MKDIR} ${dir ${@}}
	${MKDIR} ${dir ${O_SCAN}}
	${CC_PRE} ${CC} -MD -MF ${O_SCAN} -c ${@:${OBJ_DIR}%.o=%.c} -o ${@} ${CFLAGS}

${LUA}: dep/minilua.c
	${MKDIR} ${dir ${C_SCAN}}
	${CC_PRE} ${CC} -MD -MF ${C_SCAN} dep/minilua.c -o ${LUA} ${LDFLAGS}

.dummy:

.PRECIOUS: ${C_FILES}

MAKEFLAGS += -j --no-print-directory
