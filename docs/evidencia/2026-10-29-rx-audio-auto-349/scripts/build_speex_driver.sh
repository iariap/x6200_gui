#!/bin/sh
# Compila el driver mínimo de #349 contra el módulo de producción COPIADO
# VERBATIM y libspeexdsp real 1.2.1, reutilizando la imagen del banco de #351.
#
# Uso:
#   IMAGE=x6200-speexdsp-capture-bench:1.2.1-1 \
#   REPO=/ruta/al/worktree OUT=/ruta/fuera/del/repo sh build_speex_driver.sh
#
# Salida: $OUT/speex_driver y $OUT/build.json con hashes/versiones.
set -eu

REPO=${REPO:-$(cd "$(dirname "$0")/../../../.." && pwd)}
OUT=${OUT:?falta OUT (directorio fuera del repositorio)}
IMAGE=${IMAGE:-x6200-speexdsp-capture-bench:1.2.1-1}
BENCH="$REPO/docs/evidencia/2026-10-29-audio-capture-351/scripts/bench"

mkdir -p "$OUT"

docker run --rm \
    -v "$REPO":/repo:ro \
    -v "$OUT":/out:rw \
    -w /build \
    "$IMAGE" sh -c '
set -eu
BENCH=/repo/docs/evidencia/2026-10-29-audio-capture-351/scripts/bench
cp /repo/src/audio_enhancement.c /repo/src/audio_enhancement.h .
gcc -O2 -std=c11 -Wall -Wextra -Werror -I. -I"$BENCH/stubs" \
    /repo/docs/evidencia/2026-10-29-rx-audio-auto-349/scripts/speex_driver.c \
    "$BENCH/stubs_bench.c" audio_enhancement.c \
    -o /out/speex_driver $(pkg-config --cflags --libs speexdsp) -lm -lpthread
sha256sum audio_enhancement.c audio_enhancement.h /out/speex_driver > /out/build.sha256
{
  echo "image='"$IMAGE"'"
  echo "gcc=$(gcc --version | head -1)"
  echo "speexdsp=$(pkg-config --modversion speexdsp)"
  echo "source_c=$(sha256sum audio_enhancement.c | cut -d" " -f1)"
  echo "source_h=$(sha256sum audio_enhancement.h | cut -d" " -f1)"
  echo "driver=$(sha256sum /out/speex_driver | cut -d" " -f1)"
} > /out/build.json
echo "driver listo: /out/speex_driver"
'
