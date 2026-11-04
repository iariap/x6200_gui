#!/bin/sh
# Tests nativos del modelo (sin hardware). Uso: ./run_tests.sh [--arm]
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
CC=${CC:-gcc}
OUT=${OUT:-/tmp/qa-linux-dsp-safe}
mkdir -p "$OUT"
"$CC" -std=c11 -O2 -Wall -Wextra -Werror -I "$ROOT/tools/linux-dsp-safe-speaker" \
    "$ROOT/tools/linux-dsp-safe-speaker/route_arbiter_model.c" \
    "$HERE/test_route_arbiter.c" -o "$OUT/test_route_arbiter"
"$OUT/test_route_arbiter"
if [ "${1:-}" = "--arm" ]; then
    ARMCC=${ARMCC:-$ROOT/../rom-build/clone-buildroot/build/host/bin/arm-buildroot-linux-gnueabihf-gcc}
    if [ -x "$ARMCC" ]; then
        "$ARMCC" -std=c11 -O2 -Wall -Wextra -Werror -I "$ROOT/tools/linux-dsp-safe-speaker" \
            -c "$ROOT/tools/linux-dsp-safe-speaker/route_arbiter_model.c" -o "$OUT/route_arbiter_model.arm.o"
        file "$OUT/route_arbiter_model.arm.o"
    else
        echo "SKIP ARM: toolchain no encontrado en $ARMCC"
    fi
fi
