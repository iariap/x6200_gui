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

SOURCES="$SRC/adaptive_tuning.c $SRC/audio_enhancement.c $SRC/ft8_udp.c $SRC/update_check.c $SRC/band_plan_data.c $SRC/avi_writer.c $SRC/wfview_server.c $SRC/cluster/spots.c $SRC/cluster/client.c $SRC/field/refs.c $SRC/field/flog.c $SRC/field/bands.c $SRC/field/rbn.c $SRC/theme.c $SRC/cJSON.c $SRC/kbd_rollover.c $SRC/bt_devices.c $SRC/dxcc.c $SRC/flags.c $SRC/controls_step.c $SRC/voice_text.c $SRC/gps_format.c \
$DIR/stubs/stubs.c $DIR/test_main.c $DIR/test_framework.c \
$DIR/test_adaptive_tuning.c $DIR/test_audio_enhancement.c $DIR/test_ft8_udp.c $DIR/test_band_plan.c $DIR/test_avi_writer.c $DIR/test_wfview.c $DIR/test_cluster.c $DIR/test_field.c $DIR/test_theme.c $DIR/test_keyboard.c $DIR/test_bt_devices.c $DIR/test_dxcc.c $DIR/test_flags.c $DIR/test_controls.c $DIR/test_voice.c $DIR/test_gps_format.c \
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
