#!/bin/sh
# Corre DENTRO del contenedor del banco (scripts/bench/Dockerfile).
#
# Compila src/audio_enhancement.c COPIADO VERBATIM contra libspeexdsp real y
# ejecuta la matriz de casos del issue #351 (parte 3a). Los montajes los hace
# bench_capture.py:
#   /repo = worktree de la corrida (read-only)
#   /out  = artifacts/bench (read-write)
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -eu

REPO=${REPO:-/repo}
OUT=${OUT:-/out}
EV="$REPO/docs/evidencia/2026-10-29-audio-capture-351"
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
echo "--- header real de Speex ---"
grep -n 'define SPEEX_PREPROCESS_SET_NOISE_SUPPRESS' /usr/include/speex/speex_preprocess.h
grep -n 'define SPEEX_PREPROCESS_GET_NOISE_SUPPRESS' /usr/include/speex/speex_preprocess.h

# 3) Compilacion: -Werror para que el banco no arrastre warnings propios.
echo "--- compile ---"
set -x
gcc -O2 -std=c11 -Wall -Wextra -Werror -I. -I"$BENCH/stubs" \
    "$BENCH/bench_main.c" "$BENCH/stubs_bench.c" audio_enhancement.c \
    -o bench $(pkg-config --cflags --libs speexdsp) -lm -lpthread
set +x

: > "$OUT/results.jsonl"
: > "$OUT/repeats.sha256"

# Matriz: 3 senales x 3 presets x 4 particiones, 3 corridas cada una.
#   b4410   callback nominal de 100 ms (4410 = 10 x 441)
#   b441    callback de una trama (10 ms)
#   bmixed  1,440,441,442,257 ciclados (fragmentacion arbitraria)
#   b8820   callback del doble del nominal
run_case() {
    name=$1
    wav=$2
    preset=$3
    blocks=$4
    i=1
    while [ "$i" -le 3 ]; do
        ./bench --case "$name" --in "$PCM/$wav" --out "$OUT/$name.raw" \
                --preset "$preset" --blocks "$blocks" --seconds 10 >> "$OUT/results.jsonl"
        sha256sum "$OUT/$name.raw" | sed "s|$OUT/||" >> "$OUT/repeats.sha256"
        i=$((i + 1))
    done
}

for sig in noise:white_noise_44100_s16.wav multisine:multisine_44100_s16.wav sine1k:sine_1k_44100_s16.wav; do
    tag=${sig%%:*}
    wav=${sig#*:}
    for preset in 0 2 3; do
        run_case "${tag}_p${preset}_b4410"  "$wav" "$preset" "4410"
        run_case "${tag}_p${preset}_b441"   "$wav" "$preset" "441"
        run_case "${tag}_p${preset}_bmixed" "$wav" "$preset" "1,440,441,442,257"
        run_case "${tag}_p${preset}_b8820"  "$wav" "$preset" "8820"
    done
done

# Particion en un solo proceso, con reset explicito de estado entre los dos pases.
# Off y SSB voice tienen que dar la MISMA secuencia; AM broadcast no (el nivelador
# AF mide el pico por bloque y el bloque interno es <= 441 muestras).
for preset in 0 2 3; do
    ./bench --partition-check "noise_p${preset}_reset" --in "$PCM/white_noise_44100_s16.wav" \
            --preset "$preset" --blocks-a "4410" --blocks-b "1,440,441,442,257" \
            --out-a "$OUT/noise_p${preset}_reset_a.raw" \
            --out-b "$OUT/noise_p${preset}_reset_b.raw" >> "$OUT/results.jsonl"
done

# Sonda directa contra la lib real: audio_enh_nr_set_level() y GET del control.
./bench --probe levels --in "$PCM/white_noise_44100_s16.wav" >> "$OUT/results.jsonl"

# Sonda del retardo propio de SpeexDSP: estado SOLO de la lib (sin la cadena del
# modulo), misma trama y misma tasa. El host lo correlaciona contra el input.
./bench --probe speex-only --in "$PCM/white_noise_44100_s16.wav" \
        --out "$OUT/probe_speex_only.raw" >> "$OUT/results.jsonl"

echo "--- resultados ---"
cat "$OUT/results.jsonl"
