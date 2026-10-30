#!/usr/bin/env bash
#
# mayhem/build.sh — build the permafrost-engine fuzz harnesses and the test probe.
#
# The engine is a GL/SDL/Python game; only its file-format parsers are fuzzed. Every engine
# source file (minus the Python scripting layer and the executable's main.c) is compiled once with
# the sanitizers for the fuzz binaries and once with the engine's normal release flags for the
# test probe. Window / GL / render-thread / scripting entry points are stubbed by
# mayhem/engine_stubs.c, and asset_load.c and sched.c are compiled through shim translation units
# (mayhem/*_shim.c) so the harnesses can reach their static helpers without editing upstream.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"

SAN_OBJ="$SRC/obj/mayhem-san"
PLAIN_OBJ="$SRC/obj/mayhem-plain"
rm -rf "$SAN_OBJ" "$PLAIN_OBJ"
mkdir -p "$SAN_OBJ" "$PLAIN_OBJ"

# Flags the upstream Makefile uses for every engine file (-std=c99 ... -DNDEBUG), minus -Werror.
ENGINE_CFLAGS="-std=c99 -fno-strict-aliasing -fwrapv -D_DEFAULT_SOURCE -DNDEBUG -DGIT_VERSION=\"mayhem\" \
 -ffunction-sections -fdata-sections -w -I$SRC/deps/mimalloc/include $(sdl2-config --cflags)"
LIBS="$(sdl2-config --libs) -lGLEW -lGL -lopenal -lm -lpthread -ldl"

# Engine translation units: everything but the scripting layer (needs a Python 2.7 build), main.c,
# and the three files replaced by mayhem/ (render.c, asset_load.c and sched.c via shims).
mapfile -t ENGINE_SRCS < <(find src -name '*.c' ! -path 'src/script/*' ! -path src/main.c \
    ! -path src/render/render.c ! -path src/asset_load.c ! -path src/sched.c | LC_ALL=C sort)
SHIM_SRCS=(mayhem/engine_stubs.c mayhem/asset_load_shim.c mayhem/sched_shim.c)
ALL_SRCS=("${ENGINE_SRCS[@]}" "${SHIM_SRCS[@]}" deps/mimalloc/src/static.c)

obj_name() { local s="${1//\//_}"; echo "$s.o"; }

# compile_all <objdir> <flags...>: compile every source in ALL_SRCS in parallel.
compile_all() {
    local objdir="$1"; shift
    local mi_def=""
    printf '%s\n' "${ALL_SRCS[@]}" | xargs -P "$MAYHEM_JOBS" -I{} bash -c '
        f="$1"; objdir="$2"; shift 2
        o="$objdir/$(echo "$f" | tr / _).o"
        extra=""; [ "$f" = deps/mimalloc/src/static.c ] && extra="$MI_DEF"
        '"$CC"' "$@" $extra -c "$f" -o "$o"' _ {} "$objdir" "$@"
    "$CC" -c src/context_switch.S -o "$objdir/context_switch.S.o"
}

# 1) Sanitized + DWARF-3 engine objects for the fuzz binaries.
case "$SANITIZER_FLAGS" in *address*) export MI_DEF="-DMI_TRACK_ASAN=1" ;; *) export MI_DEF="" ;; esac
compile_all "$SAN_OBJ" -O1 $SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link $ENGINE_CFLAGS

# 2) Harness objects, LSan hook, and the run-once driver (a C file even for the C++ hook).
for h in attr pfobj pfmap stb_image; do
    $CC -O1 $SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link $ENGINE_CFLAGS \
        -c "mayhem/harness_$h.c" -o "$SAN_OBJ/harness_$h.o"
done
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o "$SAN_OBJ/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -x c -c "$STANDALONE_FUZZ_MAIN" -o "$SAN_OBJ/standalone_main.o"

# 3) Each harness twice: libFuzzer, and a run-once -standalone reproducer.
ENGINE_OBJS=("$SAN_OBJ"/*.c.o "$SAN_OBJ/context_switch.S.o")
for h in attr pfobj pfmap stb_image; do
    $CC $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE -Wl,--gc-sections \
        "$SAN_OBJ/harness_$h.o" "${ENGINE_OBJS[@]}" "$SAN_OBJ/lsan_off.o" $LIBS \
        -o "$SRC/fuzz_$h"
    $CC $SANITIZER_FLAGS $DEBUG_FLAGS -Wl,--gc-sections \
        "$SAN_OBJ/harness_$h.o" "${ENGINE_OBJS[@]}" "$SAN_OBJ/lsan_off.o" \
        "$SAN_OBJ/standalone_main.o" $LIBS -o "$SRC/fuzz_$h-standalone"
done

# 4) The test probe: the same engine code with the project's normal flags, no sanitizers.
export MI_DEF=""
compile_all "$PLAIN_OBJ" -O2 $COVERAGE_FLAGS $ENGINE_CFLAGS
$CC -O2 $COVERAGE_FLAGS $ENGINE_CFLAGS -c mayhem/pf_probe.c -o "$PLAIN_OBJ/pf_probe.o"
$CC $COVERAGE_FLAGS -Wl,--gc-sections "$PLAIN_OBJ/pf_probe.o" "$PLAIN_OBJ"/*.c.o \
    "$PLAIN_OBJ/context_switch.S.o" $LIBS -o "$SRC/pf_probe"
