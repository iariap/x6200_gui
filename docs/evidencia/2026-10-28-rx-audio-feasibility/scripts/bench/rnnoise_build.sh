#!/bin/sh
# Corre DENTRO del contenedor del banco (scripts/bench/Dockerfile.rnnoise).
#
# 1. Verifica el commit pinneado de RNNoise upstream (Xiph) montado read-only
#    en /src/rnnoise y lo copia a /build.
# 2. Corre el build oficial: autogen.sh descarga el modelo (download_model.sh),
#    valida su sha256 contra model_version y genera configure; despues
#    ./configure --disable-examples --disable-x86-rtcd --enable-static y make.
# 3. Compila bench_rnnoise.c (con y sin RNNoise) y ejecuta la matriz.
#
# Los montajes los hace bench_rnnoise.py:
#   /repo        = worktree de la corrida (read-only)
#   /src/rnnoise = clone upstream pinneado (read-only, fuera del repo)
#   /out         = artifacts/rnnoise (read-write)
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -eu

REPO=${REPO:-/repo}
OUT=${OUT:-/out}
SRC=${SRC:-/src/rnnoise}
EV="$REPO/docs/evidencia/2026-10-28-rx-audio-feasibility"
BENCH="$EV/scripts/bench"
PCM="$OUT/in"

RNNOISE_COMMIT=70f1d256acd4b34a572f999a05c87bf00b67730d
RNNOISE_MODEL_URL=https://media.xiph.org/rnnoise/models/rnnoise_data-0a8755f8e2d834eff6a54714ecc7d75f9932e845df35f8b59bc52a7cfe6e8b37.tar.gz
RNNOISE_MODEL_SHA=0a8755f8e2d834eff6a54714ecc7d75f9932e845df35f8b59bc52a7cfe6e8b37

mkdir -p "$OUT"
: > "$OUT/matrix.jsonl"

log() { echo "### $*"; }

# --------------------------------------------------------------------------
# 1. Fuente pinneada
# --------------------------------------------------------------------------
log "commit pinneado de RNNoise"
# El clone se monta read-only con otro uid: git exige declararlo confiable.
git config --global --add safe.directory "$SRC"
GOT_COMMIT=$(git -C "$SRC" rev-parse HEAD)
echo "esperado=$RNNOISE_COMMIT"
echo "obtenido=$GOT_COMMIT"
if [ "$GOT_COMMIT" != "$RNNOISE_COMMIT" ]; then
    echo "FALLO: el clone montado no esta en el commit pinneado" >&2
    exit 1
fi
if [ -n "$(git -C "$SRC" status --porcelain)" ]; then
    echo "FALLO: el clone montado tiene cambios locales" >&2
    git -C "$SRC" status --porcelain >&2
    exit 1
fi

rm -rf /build/rnnoise
cp -a "$SRC" /build/rnnoise
cd /build/rnnoise
log "sha256 de la fuente RNNoise (archivos que entran al build)"
sha256sum COPYING include/rnnoise.h src/denoise.c src/denoise.h src/rnn.c src/nnet.c \
    src/pitch.c src/kiss_fft.c src/celt_lpc.c src/nnet_default.c src/parse_lpcnet_weights.c \
    src/rnnoise_tables.c src/dump_features.c model_version | tee "$OUT/rnnoise-source.sha256"

log "licencia declarada por el commit"
head -3 COPYING

# --------------------------------------------------------------------------
# 2. Build oficial (descarga y valida el modelo)
# --------------------------------------------------------------------------
log "autogen.sh (download_model.sh + autoreconf)"
./autogen.sh 2>&1 | tee "$OUT/rnnoise-build.log"

log "sha256 del archivo de pesos descargado"
ls -l rnnoise_data-*.tar.gz
sha256sum rnnoise_data-*.tar.gz | tee -a "$OUT/rnnoise-build.log"
GOT_MODEL=$(sha256sum rnnoise_data-*.tar.gz | awk '{print $1}')
if [ "$GOT_MODEL" != "$RNNOISE_MODEL_SHA" ]; then
    echo "FALLO: sha256 del modelo $GOT_MODEL != $RNNOISE_MODEL_SHA" >&2
    exit 1
fi
echo "modelo: $RNNOISE_MODEL_URL"
echo "modelo sha256: $GOT_MODEL"
log "pesos extraidos (no se versionan ni entran a la ROM)"
sha256sum src/rnnoise_data.c src/rnnoise_data.h | tee "$OUT/rnnoise-model.sha256"
tar tzf rnnoise_data-*.tar.gz | tee -a "$OUT/rnnoise-build.log"

log "configure + make"
./configure --disable-examples --disable-x86-rtcd --disable-shared --enable-static \
    CFLAGS="-O2" 2>&1 | tee -a "$OUT/rnnoise-build.log"
make -j"$(nproc)" 2>&1 | tee -a "$OUT/rnnoise-build.log"

ls -l .libs/librnnoise.a
sha256sum .libs/librnnoise.a | tee "$OUT/rnnoise-lib.sha256"

# --------------------------------------------------------------------------
# 3. Compilacion del banco
# --------------------------------------------------------------------------
log "toolchain"
gcc --version | head -1
pkg-config --modversion speexdsp
dpkg-query -W -f='libspeexdsp-dev=${Version}\n' libspeexdsp-dev
uname -m
grep -m1 'model name' /proc/cpuinfo || true
grep -n 'define SPEEX_PREPROCESS_SET_NOISE_SUPPRESS' /usr/include/speex/speex_preprocess.h
grep -n '^#define FRAME_SIZE' /build/rnnoise/src/denoise.h

cp "$REPO/src/audio_enhancement.c" .
cp "$REPO/src/audio_enhancement.h" .
sha256sum audio_enhancement.c audio_enhancement.h | tee "$OUT/source.sha256"

log "compile bench_rnnoise (con RNNoise)"
set -x
gcc -O2 -std=c11 -Wall -Wextra -Werror -I. -I"$BENCH/stubs" -I/build/rnnoise/include \
    "$BENCH/bench_rnnoise.c" "$BENCH/stubs_bench.c" audio_enhancement.c \
    /build/rnnoise/.libs/librnnoise.a \
    $(pkg-config --cflags --libs speexdsp) -lm -o bench_rnnoise
log "compile bench_rnnoise_absent (SIN RNNoise: ni header ni libreria)"
gcc -O2 -std=c11 -Wall -Wextra -Werror -DX6200_BENCH_NO_RNNOISE \
    -I. -I"$BENCH/stubs" \
    "$BENCH/bench_rnnoise.c" "$BENCH/stubs_bench.c" audio_enhancement.c \
    $(pkg-config --cflags --libs speexdsp) -lm -o bench_rnnoise_absent
set +x

sha256sum bench_rnnoise bench_rnnoise_absent | tee "$OUT/bench-bin.sha256"
if ldd bench_rnnoise_absent | grep -q rnnoise; then
    echo "FALLO: el binario absent enlaza rnnoise" >&2
    exit 1
fi
echo "OK: bench_rnnoise_absent no enlaza rnnoise"

log "frame size reportado por la libreria"
./bench_rnnoise --rnnoise-frame-size | tee "$OUT/rnnoise-frame-size.txt"
if ./bench_rnnoise_absent --rnnoise-frame-size >/dev/null 2>&1; then
    echo "FALLO: el binario sin RNNoise reporta frame size" >&2
    exit 1
fi
echo "bench_rnnoise_absent: sin RNNoise (exit != 0, esperado)"

# --------------------------------------------------------------------------
# 4. Matriz: 5 metodos x 4 vectores, warmup 3 + 5 repeticiones de 60 s
# --------------------------------------------------------------------------
run_case() {
    name=$1
    wav=$2
    method=$3
    i=1
    while [ "$i" -le 3 ]; do
        ./bench_rnnoise --case "${name}_warmup$i" --in "$PCM/$wav" --method "$method" \
            --seconds 60 >> "$OUT/matrix.jsonl"
        i=$((i + 1))
    done
    i=1
    while [ "$i" -le 5 ]; do
        if [ "$i" -eq 1 ]; then
            ./bench_rnnoise --case "$name" --in "$PCM/$wav" --method "$method" --seconds 60 \
                --out "$OUT/$name.raw" >> "$OUT/matrix.jsonl"
        else
            ./bench_rnnoise --case "$name" --in "$PCM/$wav" --method "$method" --seconds 60 \
                >> "$OUT/matrix.jsonl"
        fi
        i=$((i + 1))
    done
}

for spec in "silence:silence_44100_s16.raw" "sine1k:sine_1k_44100_s16.raw" \
            "multisine:multisine_44100_s16.raw" "noise:white_noise_44100_s16.raw"; do
    vec=${spec%%:*}
    raw=${spec##*:}
    for method in off resample speex rnnoise rnnoise_norm; do
        run_case "${vec}_${method}" "$raw" "$method"
    done
done

# --------------------------------------------------------------------------
# 5. Controles
# --------------------------------------------------------------------------
log "control: pasada de 10 s == primeros 10 s de la pasada de 60 s (estado fresco)"
for method in speex rnnoise; do
    ./bench_rnnoise --case "noise_${method}_10s" --in "$PCM/white_noise_44100_s16.raw" \
        --method "$method" --seconds 10 --out "$OUT/noise_${method}_10s.raw" >> "$OUT/matrix.jsonl"
    ./bench_rnnoise --case "noise_${method}_10s_repeat" --in "$PCM/white_noise_44100_s16.raw" \
        --method "$method" --seconds 10 --out "$OUT/noise_${method}_10s_repeat.raw" >> "$OUT/matrix.jsonl"
done

log "control: repetibilidad de la pasada completa de 60 s (misma entrada/version)"
for method in speex rnnoise; do
    ./bench_rnnoise --case "noise_${method}_repeat60" --in "$PCM/white_noise_44100_s16.raw" \
        --method "$method" --seconds 60 --out "$OUT/noise_${method}_repeat60.raw" >> "$OUT/matrix.jsonl"
done

log "sonda de latencia: Speex con un frame por callback (256 muestras, sin empalme)"
./bench_rnnoise --case noise_speex_b256 --in "$PCM/white_noise_44100_s16.raw" \
    --method speex --block 256 --seconds 60 --out "$OUT/noise_speex_b256.raw" >> "$OUT/matrix.jsonl"

log "control: RNNoise ausente -> la cadena cae a off (sin silencio ni archivo corto)"
./bench_rnnoise_absent --case noise_rnnoise_absent --in "$PCM/white_noise_44100_s16.raw" \
    --method rnnoise --seconds 10 --out "$OUT/noise_rnnoise_absent.raw" 2>> "$OUT/fallback.log" \
    | tee -a "$OUT/matrix.jsonl"
./bench_rnnoise_absent --case silence_rnnoise_absent --in "$PCM/silence_44100_s16.raw" \
    --method rnnoise --seconds 10 --out "$OUT/silence_rnnoise_absent.raw" 2>> "$OUT/fallback.log" \
    | tee -a "$OUT/matrix.jsonl"
./bench_rnnoise --case noise_rnnoise_simulated_missing --in "$PCM/white_noise_44100_s16.raw" \
    --method rnnoise --seconds 10 --simulate-missing --out "$OUT/noise_rnnoise_simulated_missing.raw" \
    2>> "$OUT/fallback.log" | tee -a "$OUT/matrix.jsonl"

log "verificacion de los controles de fallback"
for f in noise_rnnoise_absent silence_rnnoise_absent noise_rnnoise_simulated_missing; do
    src=white_noise_44100_s16.raw
    [ "$f" = silence_rnnoise_absent ] && src=silence_44100_s16.raw
    if cmp -s "$OUT/$f.raw" "$PCM/$src"; then
        echo "OK $f: salida bit-exacta al input ($(stat -c %s "$OUT/$f.raw") bytes)"
    else
        echo "FALLO $f: la salida difiere del input" >&2
        exit 1
    fi
done

log "fin de la matriz"
wc -l "$OUT/matrix.jsonl"
