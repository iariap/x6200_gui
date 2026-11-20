#!/bin/sh
# Pipeline offline: prepara corpus/degradaciones, corre DSP real (ARM/QEMU), métricas y paquete ciego.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../.." && pwd)
OUT=${OUT:-$HERE/out}; ARMCC=${ARMCC:-/home/pablo/desa/x6200/rom-build/clone-buildroot/build/host/bin/arm-buildroot-linux-gnueabihf-gcc}
SYS=${SYSROOT:-/home/pablo/desa/x6200/rom-build/clone-buildroot/build/host/arm-buildroot-linux-gnueabihf/sysroot}
mkdir -p "$OUT/processed"
python3 "$HERE/prepare.py" --corpus "$HERE/corpus" --out "$OUT"
"$ARMCC" -std=gnu11 -O2 -Wall -Wextra -Wno-unused-parameter -pthread -I"$ROOT/src" -I"$ROOT/tests/native/stubs" -I"$SYS/usr/include" \
  "$ROOT/src/audio_enhancement.c" "$ROOT/tests/native/audio_enh_stubs.c" "$HERE/dsp_run.c" \
  -L"$SYS/usr/lib" -lspeexdsp -lliquid -lm -lpthread -o "$OUT/dsp_run"
for mode in B C D1 D2; do mkdir -p "$OUT/processed/$mode"; done
for f in "$OUT"/degraded/*.wav; do b=$(basename "$f" .wav); for mode in B C D1 D2; do
  /usr/bin/qemu-arm-static -L "$SYS" "$OUT/dsp_run" "$f" "$OUT/processed/$mode/$b.wav" "$mode"
done; done
python3 "$HERE/analyze.py" --out "$OUT"
python3 "$HERE/make_blind.py" --out "$OUT"
( cd "$OUT" && find degraded processed -name '*.wav' -print0 | sort -z | xargs -0 sha256sum > wav.sha256 )
echo "PASS pipeline"; echo "salida: $OUT"
