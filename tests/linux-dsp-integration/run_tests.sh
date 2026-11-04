#!/bin/sh
# QA-LINUX-DSP-INTEGRATION — tests host (arbiter/IPC/supervisor + wiring worker) + ARM/QEMU.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
SRC="$ROOT/src/linux_dsp_route"
OUT=${OUT:-/tmp/qa-linux-dsp-integration}
mkdir -p "$OUT"
CC=${CC:-gcc}
COMMON="$SRC/route_shadow.c $SRC/route_arbiter.c $SRC/route_ipc.c $SRC/route_supervisor.c"
"$CC" -std=c11 -O2 -Wall -Wextra -Werror -I"$SRC" $COMMON \
    "$HERE/test_integration.c" -o "$OUT/test_integration" -lpthread
"$OUT/test_integration"
"$CC" -std=c11 -O2 -Wall -Wextra -Werror -I"$SRC" $COMMON \
    "$SRC/route_client_worker.c" "$SRC/route_wiring.c" \
    "$HERE/test_wiring.c" -o "$OUT/test_wiring" -lpthread
"$OUT/test_wiring"
"$CC" -std=c11 -O2 -Wall -Wextra -Werror -I"$SRC" $COMMON \
    "$SRC/route_proxy.c" "$HERE/test_proxy.c" -o "$OUT/test_proxy" -lpthread
"$OUT/test_proxy"
"$CC" -std=c11 -O2 -Wall -Wextra -Werror -I"$SRC" -I"$HERE/stubs" \
    -DROUTE_LIB_PROXY_TEST -DX6200_LINUX_DSP_ROUTE=1 $COMMON \
    "$SRC/route_proxy.c" "$SRC/route_libaether_proxy.c" \
    "$HERE/test_callsites.c" -o "$OUT/test_callsites" -lpthread
"$OUT/test_callsites"
ARMCC=${ARMCC:-$ROOT/../rom-build/clone-buildroot/build/host/bin/arm-buildroot-linux-gnueabihf-gcc}
if [ -x "$ARMCC" ]; then
    "$ARMCC" -std=c11 -O2 -Wall -Wextra -Werror -I"$SRC" $COMMON \
        "$SRC/route_client_worker.c" "$SRC/route_wiring.c" \
        "$SRC/route_service_main.c" -o "$OUT/route_service.arm" -lpthread
    file "$OUT/route_service.arm"
    # Feature ON (host stubs del SDK): compila adaptador + servicio con X6200_LINUX_DSP_ROUTE=1.
    "$ARMCC" -std=c11 -O2 -Wall -Wextra -Werror -DX6200_LINUX_DSP_ROUTE=1 \
        -I"$SRC" -I"$HERE/stubs" -c "$SRC/route_transport_x6200.c" -o "$OUT/route_transport_x6200.arm.on.o"
    file "$OUT/route_transport_x6200.arm.on.o"
    "$ARMCC" -std=c11 -O2 -Wall -Wextra -Werror -DX6200_LINUX_DSP_ROUTE=1 \
        -I"$SRC" -I"$HERE/stubs" -c "$SRC/route_libaether_proxy.c" -o "$OUT/route_libaether_proxy.arm.on.o"
    file "$OUT/route_libaether_proxy.arm.on.o"
    "$CC" -std=c11 -O2 -Wall -Wextra -Werror -DX6200_LINUX_DSP_ROUTE=1 \
        -I"$SRC" -I"$HERE/stubs" $COMMON \
        "$SRC/route_client_worker.c" "$SRC/route_wiring.c" "$SRC/route_hooks.c" \
        "$SRC/route_transport_x6200.c" "$SRC/route_proxy.c" "$SRC/route_service_main.c" \
        "$HERE/stubs/x6200_stub.c" -o "$OUT/route_service_feature_on" -lpthread
    "$OUT/route_service_feature_on" --self-check && echo "FEATURE_ON_BUILD=RUN_OK"
    QEMU=${QEMU:-/usr/bin/qemu-arm-static}
    SYSROOT=${SYSROOT:-$ROOT/../rom-build/clone-buildroot/build/host/arm-buildroot-linux-gnueabihf/sysroot}
    if [ -x "$QEMU" ] && [ -d "$SYSROOT" ]; then
        "$QEMU" -L "$SYSROOT" "$OUT/route_service.arm" --self-check && echo "QEMU_SELF_CHECK=PASS"
    else
        echo "QEMU_SELF_CHECK=SKIP (falta qemu-arm-static o sysroot)"
    fi
else
    echo "ARM_BUILD=SKIP (toolchain no encontrado)"
fi
