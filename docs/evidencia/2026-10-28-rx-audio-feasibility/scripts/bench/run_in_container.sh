#!/bin/sh
# Corre DENTRO del contenedor del banco (scripts/bench/Dockerfile).
#
# Compila src/audio_enhancement.c COPIADO VERBATIM contra libspeexdsp real y
# ejecuta la matriz de casos. Los montajes los hace bench_speex.py:
#   /repo = worktree de la corrida (read-only)
#   /out  = artifacts/bench (read-write)
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -eu

REPO=${REPO:-/repo}
OUT=${OUT:-/out}
EV="$REPO/docs/evidencia/2026-10-28-rx-audio-feasibility"
BENCH="$EV/scripts/bench"
PCM="$EV/artifacts/pcm"

mkdir -p "$OUT"

# 1) Copia verbatim del archivo de produccion (sin editar una sola linea).
cp "$REPO/src/audio_enhancement.c" .
cp "$REPO/src/audio_enhancement.h" .
echo "--- fuente copiada verbatim ---"
sha256sum audio_enhancement.c audio_enhancement.h | tee "$OUT/source.sha256"

# 2) Toolchain y libreria efectivamente usados.
echo "--- toolchain ---"
gcc --version | head -1
pkg-config --modversion speexdsp
dpkg-query -W -f='libspeexdsp-dev=${Version}\n' libspeexdsp-dev
echo "--- headers reales de Speex ---"
grep -n 'define SPEEX_PREPROCESS_SET_NOISE_SUPPRESS' /usr/include/speex/speex_preprocess.h

# 3) Compilacion: -Werror para que el banco no arrastre warnings propios.
echo "--- compile ---"
set -x
gcc -O2 -std=c11 -Wall -Wextra -Werror -I. -I"$BENCH/stubs" \
    "$BENCH/bench_main.c" "$BENCH/stubs_bench.c" audio_enhancement.c \
    -o bench $(pkg-config --cflags --libs speexdsp) -lm
set +x

: > "$OUT/results.jsonl"

run_case() {
    name=$1
    wav=$2
    preset=$3
    block=$4
    i=1
    while [ "$i" -le 3 ]; do
        ./bench --case "$name" --in "$PCM/$wav" --out "$OUT/$name.raw" \
                --preset "$preset" --block "$block" --seconds 60 >> "$OUT/results.jsonl"
        i=$((i + 1))
    done
}

# Matriz principal: 4 senales x 3 presets, callback nominal de 100 ms a 44.1 kHz.
run_case silence_p0_b4410            silence_44100_s16.wav     0 4410
run_case silence_p2_b4410            silence_44100_s16.wav     2 4410
run_case silence_p3_b4410            silence_44100_s16.wav     3 4410
run_case sine1k_p0_b4410             sine_1k_44100_s16.wav     0 4410
run_case sine1k_p2_b4410             sine_1k_44100_s16.wav     2 4410
run_case sine1k_p3_b4410             sine_1k_44100_s16.wav     3 4410
run_case multisine_p0_b4410          multisine_44100_s16.wav   0 4410
run_case multisine_p2_b4410          multisine_44100_s16.wav   2 4410
run_case multisine_p3_b4410          multisine_44100_s16.wav   3 4410
run_case noise_p0_b4410              white_noise_44100_s16.wav 0 4410
run_case noise_p2_b4410              white_noise_44100_s16.wav 2 4410
run_case noise_p3_b4410              white_noise_44100_s16.wav 3 4410

# Barrido de tamano de bloque sobre la senal mas representativa del NR.
run_case noise_p2_b1                 white_noise_44100_s16.wav 2 1
run_case noise_p2_b256               white_noise_44100_s16.wav 2 256
run_case noise_p2_b257               white_noise_44100_s16.wav 2 257
run_case noise_p2_b8820              white_noise_44100_s16.wav 2 8820

# Determinismo: el mismo caso, dos veces, byte a byte.
./bench --case noise_p2_b4410_repeat --in "$PCM/white_noise_44100_s16.wav" \
        --out "$OUT/noise_p2_b4410_repeat.raw" --preset 2 --block 4410 --seconds 60 \
        >> "$OUT/results.jsonl"
if cmp -s "$OUT/noise_p2_b4410.raw" "$OUT/noise_p2_b4410_repeat.raw"; then
    echo "determinismo: noise_p2_b4410 repetido == original (cmp OK)"
else
    echo "FALLO determinismo: noise_p2_b4410 repetido difiere del original" >&2
    exit 1
fi

# Sondas directas contra la lib real.
./bench --probe polarity --in "$PCM/white_noise_44100_s16.wav" >> "$OUT/results.jsonl"
./bench --probe rate --in "$PCM/white_noise_44100_s16.wav" --suppress -22 >> "$OUT/results.jsonl"
# Mapa level 0..100 -> atenuacion, usando audio_enh_nr_set_level() de produccion.
./bench --probe level --in "$PCM/white_noise_44100_s16.wav" >> "$OUT/results.jsonl"

echo "--- resultados ---"
cat "$OUT/results.jsonl"
