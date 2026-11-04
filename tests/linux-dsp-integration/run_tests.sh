#!/bin/sh
# QA-LINUX-DSP-INTEGRATION — tests host + ARM cross (sin hardware).
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
SRC="$ROOT/src/linux_dsp_route"
OUT=${OUT:-/tmp/qa-linux-dsp-integration}
mkdir -p "$OUT"
CC=${CC:-gcc}
"$CC" -std=c11 -O2 -Wall -Wextra -Werror -I"$SRC" \
    "$SRC/route_shadow.c" "$SRC/route_arbiter.c" "$SRC/route_ipc.c" "$SRC/route_supervisor.c" \
    "$HERE/test_integration.c" -o "$OUT/test_integration" -lpthread
"$OUT/test_integration"
ARMCC=${ARMCC:-$ROOT/../rom-build/clone-buildroot/build/host/bin/arm-buildroot-linux-gnueabihf-gcc}
if [ -x "$ARMCC" ]; then
    "$ARMCC" -std=c11 -O2 -Wall -Wextra -Werror -I"$SRC" \
        "$SRC/route_shadow.c" "$SRC/route_arbiter.c" "$SRC/route_ipc.c" "$SRC/route_supervisor.c" \
        "$SRC/route_daemon_main.c" -o "$OUT/route_arbiter.arm" -lpthread
    file "$OUT/route_arbiter.arm"
    QEMU=${QEMU:-/usr/bin/qemu-arm-static}
    SYSROOT=${SYSROOT:-$ROOT/../rom-build/clone-buildroot/build/host/arm-buildroot-linux-gnueabihf/sysroot}
    if [ -x "$QEMU" ] && [ -d "$SYSROOT" ]; then
        if "$QEMU" -L "$SYSROOT" "$OUT/route_arbiter.arm" --self-check; then
            echo "QEMU_SELF_CHECK=PASS"
        else
            echo "QEMU_SELF_CHECK=FAIL (se registra, no se oculta)"
        fi
    else
        echo "QEMU_SELF_CHECK=SKIP (falta qemu-arm-static o sysroot)"
    fi
else
    echo "ARM_BUILD=SKIP (toolchain no encontrado)"
fi
