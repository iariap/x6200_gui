#!/bin/sh
# Build/run del banco REAL (audio_enhancement.c + SpeexDSP/liquid reales) en ARM + QEMU.
# Offline: no radio/captura física. Ajustar TOOLCHAIN/SYSROOT si cambia el entorno.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
ARMCC=${ARMCC:-/home/pablo/desa/x6200/rom-build/clone-buildroot/build/host/bin/arm-buildroot-linux-gnueabihf-gcc}
SYS=${SYSROOT:-/home/pablo/desa/x6200/rom-build/clone-buildroot/build/host/arm-buildroot-linux-gnueabihf/sysroot}
OUT=${OUT:-$HERE/build}; mkdir -p "$OUT"
"$ARMCC" -std=gnu11 -O2 -Wall -Wextra -Wno-unused-parameter -pthread \
  -I"$ROOT/src" -I"$ROOT/tests/native/stubs" -I"$SYS/usr/include" \
  "$ROOT/src/audio_enhancement.c" "$ROOT/tests/native/audio_enh_stubs.c" \
  "$HERE/voice_quality_bench.c" -L"$SYS/usr/lib" -lspeexdsp -lliquid -lm -lpthread -o "$OUT/bench"
/usr/bin/qemu-arm-static -L "$SYS" "$OUT/bench" "$OUT"
sha256sum "$OUT"/fixture_*.wav > "$OUT/fixtures.sha256" 2>/dev/null || true
