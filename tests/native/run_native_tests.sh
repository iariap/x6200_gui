#!/bin/sh
#
# Native (host) unit tests for the modules fixed in the 2026-10-11 ROM revision.
# No network, no Catch2, no target hardware: LVGL/liquid/Speex are stubbed.
#
# Usage: tests/native/run_native_tests.sh [output-dir]
#
# Sanitizers are used when the toolchain runtime is available; otherwise the
# script falls back to plain compilation plus GCC's static analyzer.
#
# SPDX-License-Identifier: LGPL-2.1-or-later
#
set -e

DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SRC="$DIR/../../src"
OUT="${1:-$DIR/build}"
CC="${CC:-gcc}"

mkdir -p "$OUT"

BASEFLAGS="-std=gnu11 -O1 -g -Wall -Wextra -Wno-unused-parameter -pthread"
SAN="-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover"

SOURCES="$SRC/adaptive_tuning.c $SRC/audio_enhancement.c $SRC/ft8_udp.c $SRC/update_check.c $SRC/band_plan_data.c $SRC/avi_writer.c $SRC/wfview_server.c $SRC/cluster/spots.c $SRC/cluster/client.c $SRC/field/refs.c $SRC/field/flog.c $SRC/field/bands.c $SRC/field/rbn.c $SRC/theme.c $SRC/cJSON.c $SRC/kbd_rollover.c $SRC/kbd_caps.c $SRC/bt_devices.c $SRC/dxcc.c $SRC/flags.c $SRC/controls_step.c $SRC/voice_text.c $SRC/gps_format.c \
$DIR/stubs/stubs.c $DIR/test_main.c $DIR/test_framework.c \
$DIR/test_adaptive_tuning.c $DIR/test_audio_enhancement.c $DIR/test_ft8_udp.c $DIR/test_band_plan.c $DIR/test_avi_writer.c $DIR/test_wfview.c $DIR/test_cluster.c $DIR/test_field.c $DIR/test_theme.c $DIR/test_keyboard.c $DIR/test_kbd_caps.c $DIR/test_bt_devices.c $DIR/test_dxcc.c $DIR/test_flags.c $DIR/test_controls.c $DIR/test_voice.c $DIR/test_gps_format.c \
$DIR/test_update_check.c"

echo "== compilador: $($CC --version | head -1) =="

# Detect sanitizer runtime support (some hosts ship the compiler without libasan).
if echo 'int main(void){return 0;}' | $CC -x c - $SAN -o "$OUT/.san_probe" >/dev/null 2>&1; then
    SAN_OK=yes
    echo "== sanitizers: ASan + UBSan =="
else
    SAN_OK=no
    echo "== sanitizers: NO disponibles en este toolchain (se omite) =="
fi

if [ "$SAN_OK" = yes ]; then
    # shellcheck disable=SC2086
    $CC $BASEFLAGS $SAN -I"$DIR/stubs" -I"$SRC" $SOURCES -lm -lpng -o "$OUT/native_tests"
else
    # shellcheck disable=SC2086
    $CC $BASEFLAGS -I"$DIR/stubs" -I"$SRC" $SOURCES -lm -lpng -o "$OUT/native_tests"
fi

echo "== analisis estatico (gcc -fanalyzer) =="
# wfview_server stays out: it opens the three server sockets on purpose and keeps
# them for the life of the process, which the leak checker reads as a leak.
for f in adaptive_tuning audio_enhancement ft8_udp update_check band_plan_data avi_writer bt_devices kbd_rollover; do
    # shellcheck disable=SC2086
    $CC $BASEFLAGS -fanalyzer -I"$DIR/stubs" -I"$SRC" -c "$SRC/$f.c" -o "$OUT/$f.o" || true
done

echo "== ejecutando =="
# The country lookup test reads the cty.dat the image ships, the same file
export CTY_PATH="${CTY_PATH:-$SRC/../rootfs/usr/share/x6200/cty.dat}"
# Los temas que vienen con la ROM: el test comprueba que carguen todos (ver test_theme.c).
export X6200_SHIPPED_THEMES="${X6200_SHIPPED_THEMES:-$SRC/../rootfs/usr/share/x6200/themes}"
"$OUT/native_tests"

# --- Clock test (UI-009) ------------------------------------------------------
# src/clock.c includes its sibling firmware headers with `#include "..."`, and
# for a quoted include the directory of the including file always wins over -I.
# So the committed stub headers under tests/native/stubs_clock/ cannot shadow
# src/styles.h etc. from their own directory: the real clock.c is staged next to
# a copy of the stubs in the build dir, where `#include "styles.h"` resolves to
# the stub. See tests/native/test_clock.c and docs/UI_KNOWN_ISSUES.md (UI-009).
echo "== clock (UI-009): reloj de potencia =="
# The defect was an *uninitialized pointer read* (undefined behaviour), which a
# normal build may mask with whatever the stack held. `-ftrivial-auto-var-init=pattern`
# poisons automatic variables that are read before being written, so a
# reintroduced `const char * bat_sym;` makes this test crash deterministically
# instead of passing by luck. Probed, since it needs gcc>=12 / clang>=8.
if echo 'int main(void){return 0;}' | $CC -x c - -ftrivial-auto-var-init=pattern -o "$OUT/.pattern_probe" >/dev/null 2>&1; then
    POISON="-ftrivial-auto-var-init=pattern"
    echo "== clock: -ftrivial-auto-var-init=pattern (defecto observable) =="
else
    POISON=""
    echo "== clock: sin -ftrivial-auto-var-init; el defecto podria no manifestarse =="
fi
CLOCK_STAGE="$OUT/clock_src"
rm -rf "$CLOCK_STAGE"
mkdir -p "$CLOCK_STAGE/lvgl" "$CLOCK_STAGE/params"
cp "$SRC/clock.c" "$SRC/clock.h" "$CLOCK_STAGE/"
cp "$DIR/stubs_clock/styles.h" "$DIR/stubs_clock/radio.h" "$DIR/stubs_clock/util.h" \
   "$DIR/stubs_clock/backlight.h" "$DIR/stubs_clock/voice.h" "$DIR/stubs_clock/msg.h" \
   "$CLOCK_STAGE/"
cp "$DIR/stubs_clock/params/params.h" "$CLOCK_STAGE/params/"
cp "$DIR/stubs_clock/lvgl/lvgl.h" "$CLOCK_STAGE/lvgl/"
if [ "$SAN_OK" = yes ]; then
    # shellcheck disable=SC2086
    $CC $BASEFLAGS $SAN $POISON -I"$CLOCK_STAGE" -I"$DIR/stubs_clock" \
        "$CLOCK_STAGE/clock.c" "$DIR/clock_stubs.c" "$DIR/test_clock.c" \
        "$DIR/test_framework.c" "$DIR/clock_test_main.c" -o "$OUT/clock_tests"
else
    # shellcheck disable=SC2086
    $CC $BASEFLAGS $POISON -I"$CLOCK_STAGE" -I"$DIR/stubs_clock" \
        "$CLOCK_STAGE/clock.c" "$DIR/clock_stubs.c" "$DIR/test_clock.c" \
        "$DIR/test_framework.c" "$DIR/clock_test_main.c" -o "$OUT/clock_tests"
fi
"$OUT/clock_tests"
