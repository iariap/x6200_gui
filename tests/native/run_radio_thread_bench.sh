#!/bin/sh
#
# Banco host antes/despues de radio_thread (issue #219): compila el lazo real de
# src/radio.c contra la base y contra el arbol actual y mide, por ventana,
# iteraciones/s, sleeps y CPU de proceso con x6200_flow_read() devolviendo true
# (radio_tick() false: el caso que giraba sin dormir).
#
#   tests/native/run_radio_thread_bench.sh [out-dir]
#
# Variables: SECONDS_WIN (segundos por ventana, default 10), WINDOWS (default 3),
# BASE_REF (default origin/lu2dsx/x6200-rom), CC (default gcc).
#
# El banco NO es un gate: depende del tiempo y de la carga del host. La evidencia
# del #219 lo corre con el host descargado y lo archiva en
# docs/evidencia/2026-10-22-next219/.
#
# SPDX-License-Identifier: LGPL-2.1-or-later
#
set -eu

DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$DIR/../.." && pwd)
SRC="$ROOT/src"
OUT="${1:-$DIR/build/radio_bench}"
SECONDS_WIN="${SECONDS_WIN:-10}"
WINDOWS="${WINDOWS:-3}"
BASE_REF="${BASE_REF:-origin/lu2dsx/x6200-rom}"
CC="${CC:-gcc}"

FLAGS="-std=gnu11 -O2 -g -Wall -Wextra -Wno-unused-parameter -pthread -ffunction-sections -fdata-sections"

mkdir -p "$OUT"

# Stage por variante: el radio.c de la version pedida + los headers reales que
# necesita enteros + los stubs minimos/copias del SDK (mismo layout que el stage
# del gate, ver tests/native/stubs_radio/README.md).
stage_for() {
    stagedir="$OUT/${1}_src"
    radioc="$2"

    rm -rf "$stagedir"
    mkdir -p "$stagedir/cfg" "$stagedir/params" "$stagedir/lvgl" \
             "$stagedir/aether_radio/x6200_control/low"
    cp "$SRC/radio.h" "$SRC/visual_policy.h" "$SRC/cb.h" "$SRC/pubsub_ids.h" "$stagedir/"
    cp "$SRC/cfg/subjects.h" "$SRC/cfg/common.h" "$SRC/cfg/band.h" "$SRC/cfg/atu.h" \
       "$SRC/cfg/transverter.h" "$SRC/cfg/cfg.h" "$stagedir/cfg/"
    cp "$DIR/stubs_radio/"*.h "$stagedir/"
    cp "$DIR/stubs_radio/params/params.h" "$stagedir/params/"
    cp "$DIR/stubs_radio/lvgl/lvgl.h" "$stagedir/lvgl/"
    cp "$DIR/stubs_radio/aether_radio/x6200_control/"*.h "$stagedir/aether_radio/x6200_control/"
    cp "$DIR/stubs_radio/aether_radio/x6200_control/low/"*.h "$stagedir/aether_radio/x6200_control/low/"
    cp "$radioc" "$stagedir/radio.c"
}

echo "== banco radio_thread: base=$BASE_REF  arbol=$SRC/radio.c ==" >&2

git -C "$ROOT" show "$BASE_REF:src/radio.c" > "$OUT/radio_base.c"
stage_for base "$OUT/radio_base.c"
cp "$SRC/radio.c" "$OUT/radio_head.c"
stage_for head "$OUT/radio_head.c"

for variant in base head; do
    stagedir="$OUT/${variant}_src"

    # shellcheck disable=SC2086
    $CC $FLAGS -I"$stagedir" -I"$DIR" \
        -DRADIO_C_PATH="\"$stagedir/radio.c\"" -Wl,--gc-sections \
        "$DIR/radio_bench.c" "$DIR/radio_stubs.c" -o "$OUT/radio_bench_$variant"
done

printf 'variante\tventana\twall_ms\titeraciones\tsleeps\tcpu_ms\tcpu_pct\n'

for i in $(seq 1 "$WINDOWS"); do
    for variant in base head; do
        "$OUT/radio_bench_$variant" "$SECONDS_WIN" | awk -v v="$variant" -v w="$i" 'BEGIN { OFS="\t" } { print v, w, $2, $3, $4, $5, $6 }'
    done
done
