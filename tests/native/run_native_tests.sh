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

# --- CW decoder stage (issue #51) --------------------------------------------
# src/cw_decoder.c includes its siblings with `#include "..."`, and for a quoted
# include the directory of the including file always wins over -I: the stub
# headers under tests/native/stubs_cw/ cannot shadow src/pannel.h and
# src/remote_server.h from their own directory. The real cw_decoder.c is staged
# next to a copy of the stubs and of the LVGL stub, the same pattern as the
# clock and backlight tests (see tests/native/test_cw_decoder.c).
CW_STAGE="$OUT/cw_src"
rm -rf "$CW_STAGE"
mkdir -p "$CW_STAGE/lvgl"
cp "$SRC/cw_decoder.c" "$SRC/cw_decoder.h" "$CW_STAGE/"
cp "$DIR/stubs_cw/pannel.h" "$DIR/stubs_cw/remote_server.h" "$CW_STAGE/"
cp "$DIR/stubs/lvgl/lvgl.h" "$CW_STAGE/lvgl/"

SOURCES="$SRC/adaptive_tuning.c $SRC/autotune.c $SRC/audio_enhancement.c $SRC/ft8_udp.c $SRC/update_check.c $SRC/band_plan_data.c $SRC/avi_writer.c $SRC/wfview_server.c $SRC/cluster/spots.c $SRC/cluster/client.c $SRC/field/refs.c $SRC/field/flog.c $SRC/field/bands.c $SRC/field/rbn.c $SRC/theme.c $SRC/cJSON.c $SRC/kbd_rollover.c $SRC/kbd_caps.c $SRC/bt_devices.c $SRC/dxcc.c $SRC/flags.c $SRC/controls_step.c $SRC/voice_text.c $SRC/gps_format.c $SRC/visual_policy.c $SRC/roe_curve.c $CW_STAGE/cw_decoder.c \
$DIR/stubs/stubs.c $DIR/test_main.c $DIR/test_framework.c \
$DIR/test_adaptive_tuning.c $DIR/test_autotune.c $DIR/test_audio_enhancement.c $DIR/test_ft8_udp.c $DIR/test_band_plan.c $DIR/test_avi_writer.c $DIR/test_wfview.c $DIR/test_cluster.c $DIR/test_field.c $DIR/test_theme.c $DIR/test_keyboard.c $DIR/test_kbd_caps.c $DIR/test_bt_devices.c $DIR/test_dxcc.c $DIR/test_flags.c $DIR/test_controls.c $DIR/test_voice.c $DIR/test_gps_format.c $DIR/test_visual_policy.c $DIR/test_cw_decoder.c $DIR/test_roe_curve.c \
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
for f in adaptive_tuning autotune audio_enhancement ft8_udp update_check band_plan_data avi_writer bt_devices kbd_rollover; do
    # shellcheck disable=SC2086
    $CC $BASEFLAGS -fanalyzer -I"$DIR/stubs" -I"$SRC" -c "$SRC/$f.c" -o "$OUT/$f.o" || true
done

echo "== ejecutando =="
# The country lookup test reads the cty.dat the image ships, the same file
export CTY_PATH="${CTY_PATH:-$SRC/../rootfs/usr/share/x6200/cty.dat}"
# Los temas que vienen con la ROM: el test comprueba que carguen todos (ver test_theme.c).
export X6200_SHIPPED_THEMES="${X6200_SHIPPED_THEMES:-$SRC/../rootfs/usr/share/x6200/themes}"
# El diálogo de Field arma la ruta de sus tiles desde un prefijo (FIELD_ICONS):
# el test comprueba que los PNG que pide estén en rootfs/ (ver test_field.c).
export X6200_DIALOG_FIELD_SRC="${X6200_DIALOG_FIELD_SRC:-$SRC/dialog_field.c}"
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

# --- Backlight test (UI-08 incremento 4) -------------------------------------
# src/backlight.c includes its siblings with `#include "..."`, and for a quoted
# include the directory of the including file always wins over -I, so the stub
# headers under tests/native/stubs_backlight/ cannot shadow src/backlight.h,
# src/gpio_safe.h, etc. from their own directory. The real backlight.c is staged
# next to a copy of the stubs in the build dir, the same pattern as the clock
# test (see tests/native/test_backlight.c).
#
# The real module runs here because no other gate compiles it: the UI harness
# mocks backlight (tools/ui_harness/mocks/mock_ui_deps.c) and the host has no
# aether SDK. The increment 4 wire is observed through the stub of
# visual_policy_brightness_rewrite() (tests/native/backlight_stubs.c): the tick
# by input and the idle timer have to hand it the params level, so reverting
# either rewrite to a direct backlight_set_brightness() call leaves the recorder
# empty and the test red.
echo "== backlight (UI-08 incr. 4): el cap en los rewrites del tick/timer =="
BL_STAGE="$OUT/backlight_src"
rm -rf "$BL_STAGE"
mkdir -p "$BL_STAGE/lvgl" "$BL_STAGE/params" "$BL_STAGE/aether_radio/x6200_control/low"
cp "$SRC/backlight.c" "$SRC/backlight.h" "$SRC/backlight_level.h" "$SRC/visual_policy.h" "$BL_STAGE/"
cp "$DIR/stubs_backlight/gpio_safe.h" "$DIR/stubs_backlight/util.h" "$DIR/stubs_backlight/voice.h" "$BL_STAGE/"
cp "$DIR/stubs_backlight/params/params.h" "$BL_STAGE/params/"
cp "$DIR/stubs_backlight/lvgl/lvgl.h" "$BL_STAGE/lvgl/"
cp "$DIR/stubs_backlight/aether_radio/x6200_control/low/gpio.h" "$BL_STAGE/aether_radio/x6200_control/low/"
if [ "$SAN_OK" = yes ]; then
    # shellcheck disable=SC2086
    $CC $BASEFLAGS $SAN -I"$BL_STAGE" -I"$DIR/stubs_backlight" \
        "$BL_STAGE/backlight.c" "$DIR/backlight_stubs.c" "$DIR/test_backlight.c" \
        "$DIR/test_framework.c" "$DIR/backlight_test_main.c" -o "$OUT/backlight_tests"
else
    # shellcheck disable=SC2086
    $CC $BASEFLAGS -I"$BL_STAGE" -I"$DIR/stubs_backlight" \
        "$BL_STAGE/backlight.c" "$DIR/backlight_stubs.c" "$DIR/test_backlight.c" \
        "$DIR/test_framework.c" "$DIR/backlight_test_main.c" -o "$OUT/backlight_tests"
fi
"$OUT/backlight_tests"

# --- Digital modes test (issue #63) ------------------------------------------
# src/cfg/digital_modes.c includes its siblings with `#include "..."` (cfg.h and
# ../lvgl/lvgl.h), and for a quoted include the directory of the including file
# always wins over -I, so the real module is staged next to minimal stubs of
# cfg.h and lvgl.h, the same pattern as the clock/backlight tests (see
# tests/native/test_digital_modes.c). The test drives cfg_digital_load() through
# a fake sqlite3_step() and pins the invariant that cfg_digital_label_get() is
# never NULL: the WSPR dialog hands the label to LVGL's "%s" formatter, which
# dereferences it, and a radio whose database had no WSPR rows crashed with
# SIGSEGV as soon as the app opened.
echo "== digital modes (#63): cfg_digital_label_get() nunca NULL =="
DM_STAGE="$OUT/digital_modes_src"
rm -rf "$DM_STAGE"
mkdir -p "$DM_STAGE/cfg" "$DM_STAGE/lvgl"
cp "$SRC/cfg/digital_modes.c" "$SRC/cfg/digital_modes.h" "$SRC/cfg/digital_modes.private.h" "$DM_STAGE/cfg/"
cp "$DIR/stubs_digital/cfg.h" "$DM_STAGE/cfg/"
cp "$DIR/stubs_digital/lvgl/lvgl.h" "$DM_STAGE/lvgl/"
if [ "$SAN_OK" = yes ]; then
    # shellcheck disable=SC2086
    $CC $BASEFLAGS $SAN -I"$DM_STAGE/cfg" -I"$DIR/stubs_digital" \
        "$DM_STAGE/cfg/digital_modes.c" "$DIR/digital_modes_stubs.c" "$DIR/test_digital_modes.c" \
        "$DIR/test_framework.c" "$DIR/digital_modes_test_main.c" -o "$OUT/digital_modes_tests"
else
    # shellcheck disable=SC2086
    $CC $BASEFLAGS -I"$DM_STAGE/cfg" -I"$DIR/stubs_digital" \
        "$DM_STAGE/cfg/digital_modes.c" "$DIR/digital_modes_stubs.c" "$DIR/test_digital_modes.c" \
        "$DIR/test_framework.c" "$DIR/digital_modes_test_main.c" -o "$OUT/digital_modes_tests"
fi

echo "== digital modes: analisis estatico (gcc -fanalyzer) =="
# shellcheck disable=SC2086
$CC $BASEFLAGS -fanalyzer -I"$DM_STAGE/cfg" -I"$DIR/stubs_digital" \
    -c "$DM_STAGE/cfg/digital_modes.c" -o "$OUT/digital_modes.o" || true

"$OUT/digital_modes_tests"

# --- SWR Scan dialog test (issue #85) ----------------------------------------
# src/dialog_swrscan.c includes its siblings with `#include "..."` (dialog.h,
# buttons.h, params/params.h, ...), and for a quoted include the directory of the
# including file always wins over -I, so the committed stub headers under
# tests/native/stubs_swrscan/ cannot shadow src/dialog.h etc. from their own
# directory: the real dialog_swrscan.c is staged next to a copy of the stubs, the
# same pattern as the clock/backlight/digital-modes tests (see
# tests/native/test_swrscan.c). The test pins that closing the dialog with the
# sweep running stops the sweep without handing a NULL item to a button callback
# (the destructor used to call dialog_swrscan_run_cb(NULL) -> button_update_label
# dereferenced item->type = NULL+4, the fallo=0x4 of issue #85).
echo "== SWR Scan (#85): cerrar con el barrido corriendo no desreferencia NULL =="
SW_STAGE="$OUT/swrscan_src"
rm -rf "$SW_STAGE"
mkdir -p "$SW_STAGE/lvgl" "$SW_STAGE/params"
cp "$SRC/dialog_swrscan.c" "$SRC/dialog_swrscan.h" "$SRC/roe_curve.c" "$SRC/roe_curve.h" "$SW_STAGE/"
cp "$DIR/stubs_swrscan/lvgl/lvgl.h" "$SW_STAGE/lvgl/"
cp "$DIR/stubs_swrscan/dialog.h" "$DIR/stubs_swrscan/styles.h" "$DIR/stubs_swrscan/radio.h" \
   "$DIR/stubs_swrscan/events.h" "$DIR/stubs_swrscan/util.h" "$DIR/stubs_swrscan/keyboard.h" \
   "$DIR/stubs_swrscan/main_screen.h" "$DIR/stubs_swrscan/buttons.h" "$DIR/stubs_swrscan/theme.h" "$SW_STAGE/"
cp "$DIR/stubs_swrscan/params/params.h" "$SW_STAGE/params/"
if [ "$SAN_OK" = yes ]; then
    # shellcheck disable=SC2086
    $CC $BASEFLAGS $SAN -I"$SW_STAGE" \
        "$SW_STAGE/dialog_swrscan.c" "$SW_STAGE/roe_curve.c" "$DIR/swrscan_stubs.c" "$DIR/test_swrscan.c" \
        "$DIR/test_framework.c" "$DIR/swrscan_test_main.c" -lm -o "$OUT/swrscan_tests"
else
    # shellcheck disable=SC2086
    $CC $BASEFLAGS -I"$SW_STAGE" \
        "$SW_STAGE/dialog_swrscan.c" "$SW_STAGE/roe_curve.c" "$DIR/swrscan_stubs.c" "$DIR/test_swrscan.c" \
        "$DIR/test_framework.c" "$DIR/swrscan_test_main.c" -lm -o "$OUT/swrscan_tests"
fi

echo "== SWR Scan: analisis estatico (gcc -fanalyzer) =="
# shellcheck disable=SC2086
$CC $BASEFLAGS -fanalyzer -I"$SW_STAGE" \
    -c "$SW_STAGE/dialog_swrscan.c" -o "$OUT/dialog_swrscan.o" || true

"$OUT/swrscan_tests"

# --- Dialog restore test (issue #94) -----------------------------------------
# src/dialog.c includes its siblings with `#include "..."` (dialog.h,
# cfg/subjects.h, ...), and for a quoted include the directory of the including
# file always wins over -I, so the committed stub headers under
# tests/native/stubs_restore/ cannot shadow src/dialog.h etc. from their own
# directory: the real dialog.c (and its real dialog.h) are staged next to a copy
# of the stubs, the same pattern as the clock/backlight/digital-modes/SWR-Scan
# tests (see tests/native/test_dialog_restore.c). The test pins that the app
# frame restores the radio state (dial + mode) an app changed on open: the
# backup is the in-memory radio_state_save/restore (issue #94), not the SQLite
# slot the pre-fix code used (the stub cfg_memory_save/load is a no-op, so the
# old path cannot restore anything and the test is red before the fix).
echo "== Restore (#94): el frame devuelve el dial y el modo al cerrar la app =="
RS_STAGE="$OUT/restore_src"
rm -rf "$RS_STAGE"
mkdir -p "$RS_STAGE/cfg" "$RS_STAGE/lvgl" "$RS_STAGE/liquid"
cp "$SRC/dialog.c" "$SRC/dialog.h" "$RS_STAGE/"
# dialog_construct() cancela el asistente de sintonia fina (issue #100): el header
# real se copia al stage y el stub de restore_stubs.c da el no-op (aca no hay
# asistente corriendo).
cp "$SRC/autotune_ui.h" "$RS_STAGE/"
cp "$DIR/stubs_restore/lvgl/lvgl.h" "$RS_STAGE/lvgl/"
cp "$DIR/stubs_restore/helpers.h" "$DIR/stubs_restore/buttons.h" "$DIR/stubs_restore/styles.h" \
   "$DIR/stubs_restore/keyboard.h" "$DIR/stubs_restore/events.h" "$DIR/stubs_restore/waterfall.h" \
   "$DIR/stubs_restore/main_screen.h" "$DIR/stubs_restore/cluster_column.h" "$DIR/stubs_restore/radio.h" "$RS_STAGE/"
cp "$DIR/stubs_restore/cfg/cfg.h" "$DIR/stubs_restore/cfg/subjects.h" "$DIR/stubs_restore/cfg/memory.h" "$RS_STAGE/cfg/"
cp "$DIR/stubs_restore/liquid/liquid.h" "$RS_STAGE/liquid/"
if [ "$SAN_OK" = yes ]; then
    # shellcheck disable=SC2086
    $CC $BASEFLAGS $SAN -I"$RS_STAGE" \
        "$RS_STAGE/dialog.c" "$DIR/restore_stubs.c" "$DIR/test_dialog_restore.c" \
        "$DIR/test_framework.c" "$DIR/restore_test_main.c" -o "$OUT/restore_tests"
else
    # shellcheck disable=SC2086
    $CC $BASEFLAGS -I"$RS_STAGE" \
        "$RS_STAGE/dialog.c" "$DIR/restore_stubs.c" "$DIR/test_dialog_restore.c" \
        "$DIR/test_framework.c" "$DIR/restore_test_main.c" -o "$OUT/restore_tests"
fi

echo "== Restore: analisis estatico (gcc -fanalyzer) =="
# shellcheck disable=SC2086
$CC $BASEFLAGS -fanalyzer -I"$RS_STAGE" \
    -c "$RS_STAGE/dialog.c" -o "$OUT/dialog.o" || true

"$OUT/restore_tests"

# --- Event queue test (issue #111) -------------------------------------------
# src/events.c includes its siblings with `#include "..."` (events.h, backlight.h,
# keyboard.h), and for a quoted include the directory of the including file wins
# over -I, so the real events.c is staged next to minimal stubs of LVGL (event
# codes + a capturable LV_LOG_ERROR), keyboard.h and backlight.h, the same pattern
# as the clock/backlight/digital-modes/SWR-Scan/restore tests (see
# tests/native/test_events.c). The test pins that the ring never laps its reader
# (a full queue drops cleanly instead of stranding every item and dropping the
# rest forever) and that the overflow log is rate-limited (issue #111: the queue
# saturated at ~2.4 lines/s of `Overflow` and buried the log).
echo "== Eventos (#111): la cola no se traba al llenarse y el log acota =="
EV_STAGE="$OUT/events_src"
rm -rf "$EV_STAGE"
mkdir -p "$EV_STAGE/lvgl"
cp "$SRC/events.c" "$SRC/events.h" "$EV_STAGE/"
cp "$DIR/stubs_events/lvgl/lvgl.h" "$EV_STAGE/lvgl/"
cp "$DIR/stubs_events/keyboard.h" "$DIR/stubs_events/backlight.h" "$EV_STAGE/"
if [ "$SAN_OK" = yes ]; then
    # shellcheck disable=SC2086
    $CC $BASEFLAGS $SAN -I"$EV_STAGE" -I"$DIR" \
        "$EV_STAGE/events.c" "$DIR/events_stubs.c" "$DIR/test_events.c" \
        "$DIR/test_framework.c" "$DIR/events_test_main.c" -o "$OUT/events_tests"
else
    # shellcheck disable=SC2086
    $CC $BASEFLAGS -I"$EV_STAGE" -I"$DIR" \
        "$EV_STAGE/events.c" "$DIR/events_stubs.c" "$DIR/test_events.c" \
        "$DIR/test_framework.c" "$DIR/events_test_main.c" -o "$OUT/events_tests"
fi

echo "== Eventos: analisis estatico (gcc -fanalyzer) =="
# shellcheck disable=SC2086
$CC $BASEFLAGS -fanalyzer -I"$EV_STAGE" -I"$DIR" \
    -c "$EV_STAGE/events.c" -o "$OUT/events.o" || true

"$OUT/events_tests"

# --- CW encoder test (issue #116) --------------------------------------------
# src/cw_encoder.c includes its siblings with `#include "..."` (cfg/cfg_api.h,
# cw_decoder.h, params/params.h, radio.h, ...), and for a quoted include the
# directory of the including file always wins over -I, so the committed stub
# headers under tests/native/stubs_cwenc/ cannot shadow src/cfg/cfg_api.h etc.
# from their own directory: the real cw_encoder.c is staged next to a copy of
# the stubs, the same pattern as the clock/backlight/digital-modes/SWR-Scan/
# restore tests (see tests/native/test_cw_encoder.c). The test pins that
# cw_encoder_send(NULL) is a no-op instead of `strdup(NULL)` -> strlen(NULL):
# the radio crashed with `CRASHLOG sig=11 fallo=(nil)` (pc inside strlen, lr
# inside strdup, both in libc) when the CW messages window (empty msg_cw table)
# handed the keyer a NULL message.
echo "== CW encoder (#116): cw_encoder_send(NULL) no desreferencia NULL =="
CWENC_STAGE="$OUT/cwenc_src"
rm -rf "$CWENC_STAGE"
mkdir -p "$CWENC_STAGE/cfg" "$CWENC_STAGE/params" "$CWENC_STAGE/lvgl"
cp "$SRC/cw_encoder.c" "$SRC/cw_encoder.h" "$SRC/cw_decoder.h" "$CWENC_STAGE/"
cp "$DIR/stubs_cwenc/cfg/cfg.h" "$DIR/stubs_cwenc/cfg/cfg_api.h" "$CWENC_STAGE/cfg/"
cp "$DIR/stubs_cwenc/params/params.h" "$CWENC_STAGE/params/"
cp "$DIR/stubs_cwenc/radio.h" "$DIR/stubs_cwenc/msg.h" "$DIR/stubs_cwenc/buttons.h" "$CWENC_STAGE/"
cp "$DIR/stubs_cwenc/lvgl/lvgl.h" "$CWENC_STAGE/lvgl/"
if [ "$SAN_OK" = yes ]; then
    # shellcheck disable=SC2086
    $CC $BASEFLAGS $SAN -I"$CWENC_STAGE" \
        "$CWENC_STAGE/cw_encoder.c" "$DIR/cwenc_stubs.c" "$DIR/test_cw_encoder.c" \
        "$DIR/test_framework.c" "$DIR/cwenc_test_main.c" -o "$OUT/cwenc_tests"
else
    # shellcheck disable=SC2086
    $CC $BASEFLAGS -I"$CWENC_STAGE" \
        "$CWENC_STAGE/cw_encoder.c" "$DIR/cwenc_stubs.c" "$DIR/test_cw_encoder.c" \
        "$DIR/test_framework.c" "$DIR/cwenc_test_main.c" -o "$OUT/cwenc_tests"
fi

echo "== CW encoder: analisis estatico (gcc -fanalyzer) =="
# shellcheck disable=SC2086
$CC $BASEFLAGS -fanalyzer -I"$CWENC_STAGE" \
    -c "$CWENC_STAGE/cw_encoder.c" -o "$OUT/cw_encoder.o" || true

"$OUT/cwenc_tests"
