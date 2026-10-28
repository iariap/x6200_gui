#!/bin/sh
#
# build-bench-arm.sh — compila para la radio (ARMv7 Cortex-A7, Buildroot de esta ROM)
# el banco RNNoise del issue #345, sin tocar el banco original ni src/.
#
# Gate G2 del issue #347: costo de RNNoise real + resampling 44.1->48->44.1 en el R16.
# Se reusa TAL CUAL:
#   - docs/evidencia/2026-10-28-rx-audio-feasibility/scripts/bench/bench_rnnoise.c
#   - docs/evidencia/2026-10-28-rx-audio-feasibility/scripts/bench/stubs_bench.c
#   - docs/evidencia/2026-10-28-rx-audio-feasibility/scripts/bench/stubs/**        (stubs de LVGL/cfg/params/liquid)
#   - src/audio_enhancement.c/.h copiados VERBATIM (sha256 se compara con el del #345)
#   - RNNoise upstream en el commit pinneado por el #345 y su modelo oficial
#
# Lo unico nuevo es este wrapper de build: no modifica ningun artefacto del #345.
# El codigo del banco no abre ALSA/PulseAudio/I2C ni llama a x6200_control_*:
# procesa archivos PCM offline (ver run-bench-arm.sh para la prueba en la radio).
#
# Uso (desde cualquier directorio):
#   sh docs/evidencia/2026-10-28-rx-audio-hardware/scripts/build-bench-arm.sh
#
# Variables opcionales:
#   SCRATCH  directorio de trabajo (default <workspace>/.qa-347-scratch)
#   OUT      salida de binarios    (default $SCRATCH/bench-arm)
#   CC_ARM   cross-compiler        (default toolchain Buildroot de la ROM)
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -eu

EV=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
REPO=$(CDPATH= cd -- "$EV/../../.." && pwd)
FEAS="$REPO/docs/evidencia/2026-10-28-rx-audio-feasibility"
BENCH="$FEAS/scripts/bench"
SCRATCH=${SCRATCH:-"<workspace>/.qa-347-scratch"}
OUT=${OUT:-$SCRATCH/bench-arm}
RN=${RN:-$SCRATCH/rnnoise-src}
CC_ARM=${CC_ARM:-"<workspace>/rom-build/clone-buildroot/build/host/bin/arm-buildroot-linux-gnueabihf-gcc"}
SYS=${SYS:-"<workspace>/rom-build/clone-buildroot/build/host/arm-buildroot-linux-gnueabihf/sysroot"}
MODEL_TGZ=${MODEL_TGZ:-$SCRATCH/rnnoise_data.tar.gz}
TOOLCHAIN_ID=${TOOLCHAIN_ID:-arm-buildroot-linux-gnueabihf-gcc 12.3.0 (Buildroot 2023.02.11-dirty)}

RNNOISE_COMMIT=70f1d256acd4b34a572f999a05c87bf00b67730d
RNNOISE_MODEL_SHA=0a8755f8e2d834eff6a54714ecc7d75f9932e845df35f8b59bc52a7cfe6e8b37
# hashes anclados por el #345 (rnnoise-bench.json) — se verifican antes de compilar
H_DENOISE=f12ac3cc97d798cdfd7c7bd4366a08a72c5516c382056c891b61a61cb17eb031
H_RNN=c40bbe40082448005873fe970d8963870ab2c9993b7c38d6939ca48f4f31a7bd
H_NNET=3a2c1dc09ad7e9dfe3cf32cfb94694935dc462488da8f513d1f308db39f7c087
H_DATA=d6021b7697677c4d2274c912975e143765552b0e6f25500aa660fdb4a9849be5
H_DATA_H=09ff880bddd0fc74a2ae0e5ec6c8d65714031b08d0c3f672493acd9e189c5855
H_AENH_C=acab3aad158097cd5a8ab1c6e6eb83e8b37011a0098eb91d39dc1c5d2366b25f
H_AENH_H=f37d09cad52d1891b3042337e282f28c34b644ebecb58e408aed1439fc33a68d

log()  { echo "### $*"; }
fail() { echo "FALLO: $*" >&2; exit 1; }
need_file() { [ -e "$1" ] || fail "falta $1"; }

# ---------------------------------------------------------------------------
# 0. Entorno
# ---------------------------------------------------------------------------
log "worktree / evidencia"
echo "repo=$REPO"
echo "scratch=$SCRATCH"
echo "out=$OUT"
[ -n "$SCRATCH" ] && [ -n "$OUT" ] || fail "SCRATCH/OUT vacios"
mkdir -p "$OUT" "$SCRATCH"

log "toolchain"
need_file "$CC_ARM"
"$CC_ARM" --version | head -1
"$CC_ARM" -dumpmachine
"$CC_ARM" -print-sysroot
[ -d "$SYS" ] || fail "no existe el sysroot $SYS"
echo "toolchain_id=$TOOLCHAIN_ID"

log "flags fijados por el toolchain (no se agregan -mfpu/-march a mano)"
printf '' | "$CC_ARM" -dM -E - 2>/dev/null | grep -E '^#define __ARM_ARCH |^#define __ARM_NEON |^#define __VFP_FP__ |^#define __ARM_ARCH_7A__ ' || true
printf '' | "$CC_ARM" -Q --help=target 2>/dev/null | grep -E '^  -m(cpu|fpu|float-abi|arch)=' || true

log "libspeexdsp real del sysroot (para la cadena speex del banco, igual que el #345)"
ls -l "$SYS/usr/lib/libspeexdsp.so" "$SYS/usr/lib/libspeexdsp.so.1" 2>/dev/null || fail "no hay libspeexdsp en el sysroot"
need_file "$SYS/usr/include/speex/speex_preprocess.h"
sha256sum "$SYS/usr/lib/libspeexdsp.so.1.5.2" | sed 's/^/speexdsp_sha256= /'

# ---------------------------------------------------------------------------
# 1. Fuente pinenada de RNNoise (fuera del repo) y modelo oficial
# ---------------------------------------------------------------------------
log "commit pinenado de RNNoise"
[ -d "$RN/.git" ] || fail "no existe el clone $RN (git clone https://github.com/xiph/rnnoise)"
GOT_COMMIT=$(git -C "$RN" rev-parse HEAD)
echo "esperado=$RNNOISE_COMMIT"
echo "obtenido=$GOT_COMMIT"
[ "$GOT_COMMIT" = "$RNNOISE_COMMIT" ] || fail "el clone no esta en el commit pinenado"
# solo archivos versionados: el modelo se extrae como archivos no versionados (fuera del repo, como en el #345)
[ -z "$(git -C "$RN" status --porcelain --untracked-files=no)" ] || { git -C "$RN" status --porcelain --untracked-files=no >&2; fail "el clone tiene cambios locales en archivos versionados"; }

sha256sum "$RN/src/denoise.c" "$RN/src/rnn.c" "$RN/src/nnet.c" | sed 's/^/rnnoise_src_sha256= /'
[ "$(sha256sum "$RN/src/denoise.c" | cut -d' ' -f1)" = "$H_DENOISE" ] || fail "denoise.c no coincide con el #345"
[ "$(sha256sum "$RN/src/rnn.c"     | cut -d' ' -f1)" = "$H_RNN"     ] || fail "rnn.c no coincide con el #345"
[ "$(sha256sum "$RN/src/nnet.c"    | cut -d' ' -f1)" = "$H_NNET"    ] || fail "nnet.c no coincide con el #345"

log "modelo (pesos) pinenado"
need_file "$MODEL_TGZ"
GOT_MODEL=$(sha256sum "$MODEL_TGZ" | cut -d' ' -f1)
echo "modelo_sha256=$GOT_MODEL"
echo "esperado=$RNNOISE_MODEL_SHA"
[ "$GOT_MODEL" = "$RNNOISE_MODEL_SHA" ] || fail "sha256 del modelo != el del #345"
if [ ! -f "$RN/src/rnnoise_data.c" ] || [ ! -f "$RN/src/rnnoise_data.h" ]; then
    log "extrayendo pesos en el clone (solo rnnoise_data.c/.h, fuera del repo)"
    tar xzf "$MODEL_TGZ" -C "$RN"
fi
sha256sum "$RN/src/rnnoise_data.c" "$RN/src/rnnoise_data.h" | sed 's/^/model_sha256= /'
[ "$(sha256sum "$RN/src/rnnoise_data.c"   | cut -d' ' -f1)" = "$H_DATA"   ] || fail "rnnoise_data.c no coincide con el #345"
[ "$(sha256sum "$RN/src/rnnoise_data.h"   | cut -d' ' -f1)" = "$H_DATA_H" ] || fail "rnnoise_data.h no coincide con el #345"

# ---------------------------------------------------------------------------
# 2. Libreria RNNoise ARM (C portable; mismos flags que el build del #345: -O2)
#    DISABLE_DEBUG_FLOAT es lo que define el configure default del commit.
# ---------------------------------------------------------------------------
log "compilando librnnoise ARM (2 variantes: toolchain-default y DISABLE_NEON)"
RN_SRCS="src/denoise.c src/rnn.c src/pitch.c src/kiss_fft.c src/celt_lpc.c \
         src/nnet.c src/nnet_default.c src/parse_lpcnet_weights.c \
         src/rnnoise_data.c src/rnnoise_tables.c"

build_rnnoise() {
    variant=$1
    extra=$2
    dir="$OUT/rnnoise-$variant"
    rm -rf "$dir"
    mkdir -p "$dir"
    log "variante $variant (extra flags: ${extra:-ninguno})"
    for s in $RN_SRCS; do
        o="$dir/$(echo "$s" | tr '/' '_').o"
        # shellcheck disable=SC2086
        "$CC_ARM" -O2 -DDISABLE_DEBUG_FLOAT $extra -I"$RN/include" -I"$RN/src" \
            -c "$RN/$s" -o "$o" || fail "compilando $s ($variant)"
    done
    # shellcheck disable=SC2086
    ar rcs "$dir/librnnoise.a" "$dir"/*.o
    sha256sum "$dir/librnnoise.a" | sed "s/^/librnnoise_${variant}_sha256= /"
}

build_rnnoise default ""
build_rnnoise neonoff "-DDISABLE_NEON"

# ---------------------------------------------------------------------------
# 3. Banco ARM: bench_rnnoise.c y stubs_bench.c del #345 + audio_enhancement.c verbatim
# ---------------------------------------------------------------------------
log "copiando audio_enhancement.c/.h VERBATIM (deben coincidir con el #345)"
cp "$REPO/src/audio_enhancement.c" "$REPO/src/audio_enhancement.h" "$OUT/"
sha256sum "$OUT/audio_enhancement.c" "$OUT/audio_enhancement.h" | sed 's/^/source_sha256= /'
[ "$(sha256sum "$OUT/audio_enhancement.c" | cut -d' ' -f1)" = "$H_AENH_C" ] || fail "audio_enhancement.c no coincide con el del #345"
[ "$(sha256sum "$OUT/audio_enhancement.h" | cut -d' ' -f1)" = "$H_AENH_H" ] || fail "audio_enhancement.h no coincide con el del #345"

build_bench() {
    variant=$1
    lib="$OUT/rnnoise-$variant/librnnoise.a"
    bin="$OUT/bench_rnnoise_arm_$variant"
    log "compilando banco ARM variante $variant -> $(basename "$bin")"
    "$CC_ARM" -O2 -std=c11 -Wall -Wextra -Werror \
        -I"$OUT" -I"$BENCH/stubs" -I"$RN/include" \
        "$BENCH/bench_rnnoise.c" "$BENCH/stubs_bench.c" "$OUT/audio_enhancement.c" \
        "$lib" -lspeexdsp -lm -o "$bin"
    sha256sum "$bin" | sed 's/^/bench_bin_sha256= /'
    ls -l "$bin"
    file "$bin"
    readelf -h "$bin" | grep -E 'Class|Data|Machine|Type|Entry'
    readelf -d "$bin" | grep -E 'NEEDED|RPATH|RUNPATH' || true
    echo "--- referencias a hardware/radio en el binario (debe estar vacio) ---"
    { nm -u "$bin" 2>/dev/null || true; strings "$bin" 2>/dev/null; } | \
        grep -Ei 'x6200_control_|pa_stream|pulse|alsa|snd_|i2c|record_set|audio_play' | head -5 || true
    echo "--- (fin) ---" 
    echo "--- fin referencias ---"
}

build_bench default
build_bench neonoff

log "archivos producidos"
sha256sum "$OUT"/bench_rnnoise_arm_* "$OUT"/rnnoise-*/librnnoise.a | tee "$OUT/bench-bin.sha256"
log "fin: build OK"
