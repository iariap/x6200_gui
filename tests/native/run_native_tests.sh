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

SOURCES="$SRC/adaptive_tuning.c $SRC/autotune.c $SRC/cw_autotune.c $SRC/audio_enhancement.c $SRC/ft8_udp.c $SRC/update_check.c $SRC/band_plan_data.c $SRC/avi_writer.c $SRC/wfview_server.c $SRC/cluster/spots.c $SRC/cluster/client.c $SRC/field/refs.c $SRC/field/flog.c $SRC/field/bands.c $SRC/field/rbn.c $SRC/theme.c $SRC/cJSON.c $SRC/kbd_rollover.c $SRC/kbd_caps.c $SRC/bt_devices.c $SRC/dxcc.c $SRC/flags.c $SRC/controls_step.c $SRC/voice_text.c $SRC/gps_format.c $SRC/visual_policy.c $SRC/roe_curve.c $SRC/swr_session.c $CW_STAGE/cw_decoder.c \
$DIR/stubs/stubs.c $DIR/test_main.c $DIR/test_framework.c \
$DIR/test_adaptive_tuning.c $DIR/test_autotune.c $DIR/test_cw_autotune.c $DIR/test_audio_enhancement.c $DIR/test_ft8_udp.c $DIR/test_band_plan.c $DIR/test_avi_writer.c $DIR/test_wfview.c $DIR/test_cluster.c $DIR/test_field.c $DIR/test_theme.c $DIR/test_keyboard.c $DIR/test_kbd_caps.c $DIR/test_bt_devices.c $DIR/test_dxcc.c $DIR/test_flags.c $DIR/test_controls.c $DIR/test_voice.c $DIR/test_gps_format.c $DIR/test_visual_policy.c $DIR/test_cw_decoder.c $DIR/test_roe_curve.c $DIR/test_swr_session.c $DIR/swrscan_fixtures.c $DIR/test_swrscan_fixtures.c \
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
for f in adaptive_tuning autotune cw_autotune audio_enhancement ft8_udp update_check band_plan_data avi_writer bt_devices kbd_rollover; do
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
# Los fixtures del owner (issue #125, seccion 7) son el oraculo de la multipasada.
export X6200_SWRSCAN_FIXTURES="${X6200_SWRSCAN_FIXTURES:-$DIR/../fixtures/swrscan}"
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
cp "$SRC/dialog_swrscan.c" "$SRC/dialog_swrscan.h" "$SRC/roe_curve.c" "$SRC/roe_curve.h" \
   "$SRC/swr_session.c" "$SRC/swr_session.h" "$SW_STAGE/"
cp "$DIR/stubs_swrscan/lvgl/lvgl.h" "$SW_STAGE/lvgl/"
cp "$DIR/stubs_swrscan/dialog.h" "$DIR/stubs_swrscan/styles.h" "$DIR/stubs_swrscan/radio.h" \
   "$DIR/stubs_swrscan/events.h" "$DIR/stubs_swrscan/util.h" "$DIR/stubs_swrscan/keyboard.h" \
   "$DIR/stubs_swrscan/main_screen.h" "$DIR/stubs_swrscan/buttons.h" "$DIR/stubs_swrscan/theme.h" \
   "$DIR/stubs_swrscan/scheduler.h" "$SW_STAGE/"
cp "$DIR/stubs_swrscan/params/params.h" "$SW_STAGE/params/"
if [ "$SAN_OK" = yes ]; then
    # shellcheck disable=SC2086
    $CC $BASEFLAGS $SAN -I"$SW_STAGE" \
        "$SW_STAGE/dialog_swrscan.c" "$SW_STAGE/roe_curve.c" "$SW_STAGE/swr_session.c" \
        "$DIR/swrscan_stubs.c" "$DIR/test_swrscan.c" \
        "$DIR/test_framework.c" "$DIR/swrscan_test_main.c" -lm -o "$OUT/swrscan_tests"
else
    # shellcheck disable=SC2086
    $CC $BASEFLAGS -I"$SW_STAGE" \
        "$SW_STAGE/dialog_swrscan.c" "$SW_STAGE/roe_curve.c" "$SW_STAGE/swr_session.c" \
        "$DIR/swrscan_stubs.c" "$DIR/test_swrscan.c" \
        "$DIR/test_framework.c" "$DIR/swrscan_test_main.c" -lm -o "$OUT/swrscan_tests"
fi

echo "== SWR Scan: analisis estatico (gcc -fanalyzer) =="
# shellcheck disable=SC2086
$CC $BASEFLAGS -fanalyzer -I"$SW_STAGE" \
    -c "$SW_STAGE/dialog_swrscan.c" -o "$OUT/dialog_swrscan.o" || true

"$OUT/swrscan_tests"

# --- SWR Scan multipasada, 3/5 pasadas con el backend fake (issue #125) --------
# El dialogo gatea la multipasada sobre TX real (SWRSCAN_MULTIPASS_TX_ENABLED = 0, el default):
# el binario que se flashea la rechaza (lo pinnea tests/native/test_swrscan.c). Este segundo
# build del MISMO dialogo con -D...=1 es el unico lugar donde el camino de 3/5 pasadas corre de
# punta a punta: contra los fixtures del owner y con la radio stubbeada, o sea CERO RF real. Es
# la evidencia de que el motor y el dialogo soportan N pasadas y de que el gate es lo unico que
# las apaga (la validacion en hardware sigue pendiente: requires-radio).
echo "== SWR Scan (#125): 3/5 pasadas con el backend fake (gate ON solo en este binario) =="
SW_MP_STAGE="$OUT/swrscan_mp_src"
rm -rf "$SW_MP_STAGE"
mkdir -p "$SW_MP_STAGE/lvgl" "$SW_MP_STAGE/params"
cp "$SRC/dialog_swrscan.c" "$SRC/dialog_swrscan.h" "$SRC/roe_curve.c" "$SRC/roe_curve.h" \
   "$SRC/swr_session.c" "$SRC/swr_session.h" "$SW_MP_STAGE/"
cp "$DIR/stubs_swrscan/lvgl/lvgl.h" "$SW_MP_STAGE/lvgl/"
cp "$DIR/stubs_swrscan/dialog.h" "$DIR/stubs_swrscan/styles.h" "$DIR/stubs_swrscan/radio.h" \
   "$DIR/stubs_swrscan/events.h" "$DIR/stubs_swrscan/util.h" "$DIR/stubs_swrscan/keyboard.h" \
   "$DIR/stubs_swrscan/main_screen.h" "$DIR/stubs_swrscan/buttons.h" "$DIR/stubs_swrscan/theme.h" \
   "$DIR/stubs_swrscan/scheduler.h" "$SW_MP_STAGE/"
cp "$DIR/stubs_swrscan/params/params.h" "$SW_MP_STAGE/params/"
if [ "$SAN_OK" = yes ]; then
    # shellcheck disable=SC2086
    $CC $BASEFLAGS $SAN -DSWRSCAN_MULTIPASS_TX_ENABLED=1 -I"$SW_MP_STAGE" -I"$DIR" \
        "$SW_MP_STAGE/dialog_swrscan.c" "$SW_MP_STAGE/roe_curve.c" "$SW_MP_STAGE/swr_session.c" \
        "$DIR/swrscan_stubs.c" "$DIR/swrscan_fixtures.c" "$DIR/test_swrscan_multipass.c" \
        "$DIR/test_framework.c" "$DIR/swrscan_multipass_main.c" -lm -o "$OUT/swrscan_mp_tests"
else
    # shellcheck disable=SC2086
    $CC $BASEFLAGS -DSWRSCAN_MULTIPASS_TX_ENABLED=1 -I"$SW_MP_STAGE" -I"$DIR" \
        "$SW_MP_STAGE/dialog_swrscan.c" "$SW_MP_STAGE/roe_curve.c" "$SW_MP_STAGE/swr_session.c" \
        "$DIR/swrscan_stubs.c" "$DIR/swrscan_fixtures.c" "$DIR/test_swrscan_multipass.c" \
        "$DIR/test_framework.c" "$DIR/swrscan_multipass_main.c" -lm -o "$OUT/swrscan_mp_tests"
fi

"$OUT/swrscan_mp_tests"

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

# --- CRASHLOG module table (issue #131) -------------------------------------
# El handler de crashes solo puede resolver un PC que cae en una libreria si el
# log dice que .so y en que base estaba cargada. El bloque CRASHLOG-MOD se arma
# UNA vez al arrancar (sin heap) y el handler lo escribe tal cual. El test fija
# el formato, el truncado de paths largos y el MD5 contra vectores conocidos.
echo "== CRASHLOG-MOD (#131): tabla de modulos y md5 =="
if [ "$SAN_OK" = yes ]; then
    # shellcheck disable=SC2086
    $CC $BASEFLAGS $SAN -I"$SRC" \
        "$SRC/crashlog_modules.c" "$DIR/test_crashlog_modules.c" \
        "$DIR/test_framework.c" "$DIR/crashlog_modules_test_main.c" -o "$OUT/crashlog_modules_tests"
else
    # shellcheck disable=SC2086
    $CC $BASEFLAGS -I"$SRC" \
        "$SRC/crashlog_modules.c" "$DIR/test_crashlog_modules.c" \
        "$DIR/test_framework.c" "$DIR/crashlog_modules_test_main.c" -o "$OUT/crashlog_modules_tests"
fi

echo "== CRASHLOG-MOD: analisis estatico (gcc -fanalyzer) =="
# shellcheck disable=SC2086
$CC $BASEFLAGS -fanalyzer -I"$SRC" \
    -c "$SRC/crashlog_modules.c" -o "$OUT/crashlog_modules.o" || true

"$OUT/crashlog_modules_tests"

# --- Last breath (issue #121) -----------------------------------------------
# La GUI escribe cada N segundos una linea con uptime, estado de radio, app, TX,
# bateria y ultimo evento; al arrancar reporta la anterior como LASTBREATH si no
# hubo apagado limpio. El modulo es puro (solo libc): el test fija el formato
# determinista, la escritura atomica (tmp + rename) y la degradacion sin fs.
echo "== Last breath (#121): formato, rename atomico y LASTBREATH =="
if [ "$SAN_OK" = yes ]; then
    # shellcheck disable=SC2086
    $CC $BASEFLAGS $SAN -I"$SRC" \
        "$SRC/lastbreath.c" "$DIR/test_lastbreath.c" \
        "$DIR/test_framework.c" "$DIR/lastbreath_test_main.c" -o "$OUT/lastbreath_tests"
else
    # shellcheck disable=SC2086
    $CC $BASEFLAGS -I"$SRC" \
        "$SRC/lastbreath.c" "$DIR/test_lastbreath.c" \
        "$DIR/test_framework.c" "$DIR/lastbreath_test_main.c" -o "$OUT/lastbreath_tests"
fi

echo "== Last breath: analisis estatico (gcc -fanalyzer) =="
# shellcheck disable=SC2086
$CC $BASEFLAGS -fanalyzer -I"$SRC" \
    -c "$SRC/lastbreath.c" -o "$OUT/lastbreath.o" || true

"$OUT/lastbreath_tests"

# --- HKEY put thread-safe (issue #131) ---------------------------------------
# src/hkey.c se incluye con `#include "..."` (hkey.h, events.h, scheduler.h,
# keyboard.h) y para un include entre comillas manda el directorio del archivo
# que incluye, asi que los headers stub de tests/native/stubs_hkey/ no pueden
# tapar los de src/ desde su propio directorio: el modulo real se stagea al lado
# de una copia de los stubs, el mismo patron que clock/backlight/digital-modes/
# SWR-Scan/restore/events (ver tests/native/test_hkey.c).
#
# El defecto del #131: hkey_put() lo llama el hilo de radio (radio_tick ->
# hkey_put(pack->hkey)) y el codigo tocaba LVGL ahi mismo (lv_group_get_editing,
# event_send_key -> lv_group_get_focused, lv_timer_create/del/set_repeat_count y
# backlight_tick -> lv_timer_*). El test registra el hilo de cada llamada: con
# el hkey.c de la base, hkey_stub_lvgl_foreign_calls() > 0 y falla.
echo "== HKEY (#131): hkey_put desde el hilo de radio no toca LVGL =="
HK_STAGE="$OUT/hkey_src"
rm -rf "$HK_STAGE"
mkdir -p "$HK_STAGE/lvgl" "$HK_STAGE/aether_radio/x6200_control/low"
cp "$SRC/hkey.c" "$SRC/hkey.h" "$SRC/events.h" "$SRC/scheduler.h" "$HK_STAGE/"
cp "$DIR/stubs_hkey/lvgl/lvgl.h" "$HK_STAGE/lvgl/"
cp "$DIR/stubs_hkey/backlight.h" "$DIR/stubs_hkey/keyboard.h" "$HK_STAGE/"
cp "$DIR/stubs_hkey/aether_radio/x6200_control/low/flow.h" "$HK_STAGE/aether_radio/x6200_control/low/"
if [ "$SAN_OK" = yes ]; then
    # shellcheck disable=SC2086
    $CC $BASEFLAGS $SAN -I"$HK_STAGE" -I"$DIR" \
        "$HK_STAGE/hkey.c" "$DIR/hkey_stubs.c" "$DIR/test_hkey.c" \
        "$DIR/test_framework.c" "$DIR/hkey_test_main.c" -o "$OUT/hkey_tests"
else
    # shellcheck disable=SC2086
    $CC $BASEFLAGS -I"$HK_STAGE" -I"$DIR" \
        "$HK_STAGE/hkey.c" "$DIR/hkey_stubs.c" "$DIR/test_hkey.c" \
        "$DIR/test_framework.c" "$DIR/hkey_test_main.c" -o "$OUT/hkey_tests"
fi

echo "== HKEY: analisis estatico (gcc -fanalyzer) =="
# shellcheck disable=SC2086
$CC $BASEFLAGS -fanalyzer -I"$HK_STAGE" \
    -c "$HK_STAGE/hkey.c" -o "$OUT/hkey.o" || true

"$OUT/hkey_tests"

# --- Spectrum snapshots (issue #149) -----------------------------------------
# src/spectrum.c se incluye con `#include "..."` (spectrum.h, theme.h, dsp.h,
# params/params.h, ...) y para un include entre comillas manda el directorio del
# archivo que incluye, asi que los headers stub de tests/native/stubs_spectrum/
# no pueden tapar los de src/ desde su propio directorio: el modulo real se
# stagea al lado de una copia de los stubs, el mismo patron que
# clock/backlight/digital-modes/SWR-Scan/restore/events/hkey (ver
# tests/native/test_spectrum.c).
#
# El defecto del #149: spectrum_data() publica el frame bajo data_mux, pero el
# callback de dibujo lo leia sin el mutex (y spectrum_clear() lo reiniciaba sin
# lock). El test intercala al productor EN MITAD del dibujo (pausa determinista
# en la linea 700 de la traza) y exige una unica generacion por dibujo: con el
# spectrum.c de la base las lineas 0..699 salen de la generacion A, el resto de
# la B, y el rectangulo del filtro (que el callback lee despues del lazo) sale
# de la B. El mismo binario cubre spectrum_clear() concurrente, el backlog del
# scheduler (50 frames sin drenar -> 1 refresco encolado, no 50), los bounds del
# frame y el estres de productor+config+dibujo en paralelo.
echo "== Spectrum (#149): snapshot por frame, sin lock durante el dibujo =="
SP_STAGE="$OUT/spectrum_src"
rm -rf "$SP_STAGE"
mkdir -p "$SP_STAGE/lvgl" "$SP_STAGE/params" "$SP_STAGE/cfg"
cp "$SRC/spectrum.c" "$SRC/spectrum.h" "$SRC/scheduler.h" "$SP_STAGE/"
cp "$DIR/stubs_spectrum/lvgl/lvgl.h" "$SP_STAGE/lvgl/"
cp "$DIR/stubs_spectrum/params/params.h" "$SP_STAGE/params/"
cp "$DIR/stubs_spectrum/cfg/cfg.h" "$SP_STAGE/cfg/"
cp "$DIR/stubs_spectrum/theme.h" "$DIR/stubs_spectrum/styles.h" "$DIR/stubs_spectrum/meter.h" \
   "$DIR/stubs_spectrum/dsp.h" "$DIR/stubs_spectrum/events.h" "$DIR/stubs_spectrum/radio.h" \
   "$DIR/stubs_spectrum/recorder.h" "$DIR/stubs_spectrum/rtty.h" "$DIR/stubs_spectrum/util.h" \
   "$DIR/stubs_spectrum/pubsub_ids.h" "$SP_STAGE/"
SP_SRC="$SP_STAGE/spectrum.c $DIR/spectrum_stubs.c $DIR/test_spectrum.c \
$DIR/test_framework.c $DIR/spectrum_test_main.c"
if [ "$SAN_OK" = yes ]; then
    # shellcheck disable=SC2086
    $CC $BASEFLAGS $SAN -I"$SP_STAGE" -I"$DIR" $SP_SRC -o "$OUT/spectrum_tests"
else
    # shellcheck disable=SC2086
    $CC $BASEFLAGS -I"$SP_STAGE" -I"$DIR" $SP_SRC -o "$OUT/spectrum_tests"
fi

echo "== Spectrum: analisis estatico (gcc -fanalyzer) =="
# shellcheck disable=SC2086
$CC $BASEFLAGS -fanalyzer -I"$SP_STAGE" -I"$DIR" \
    -c "$SP_STAGE/spectrum.c" -o "$OUT/spectrum.o" || true

"$OUT/spectrum_tests"

# --- ThreadSanitizer (issue #149, experimento 6) -----------------------------
# Cobertura declarada: TSan corre en el HOST (x86_64, gcc). El binario ARM de la
# radio NO corre bajo TSan en este arnes, asi que estos resultados NO se
# atribuyen al Cortex-A7: lo que se prueba es el codigo de sincronizacion del
# modulo (el mismo fuente que se compila para ARM).
#
# 1) El mismo binario del test con -fsanitize=thread: el modulo arreglado tiene
#    que terminar sin reportes (el estres y la pausa determinista hacen que el
#    productor y el hilo de GUI se pisen de verdad).
# 2) El fixture de carrera deliberada (escritor con mutex + lector sin mutex,
#    el patron exacto del spectrum.c de la base): TIENE que reportar una data
#    race. Si no reporta, el detector no sirve y el punto (1) no valdria nada.
if echo 'int main(void){return 0;}' | $CC -x c - -pthread -fsanitize=thread -o "$OUT/.tsan_probe" >/dev/null 2>&1 \
   && "$OUT/.tsan_probe" >/dev/null 2>&1; then
    TSAN_OK=yes
    echo "== TSan: disponible en este host =="
else
    TSAN_OK=no
    echo "== TSan: NO disponible en este host (se omite; cobertura x86_64, no ARM) =="
fi

if [ "$TSAN_OK" = yes ]; then
    # shellcheck disable=SC2086
    $CC $BASEFLAGS -fsanitize=thread -fno-omit-frame-pointer -I"$SP_STAGE" -I"$DIR" \
        $SP_SRC -o "$OUT/spectrum_tsan_tests"

    echo "== TSan: el modulo del espectro no reporta razas =="
    TSAN_OPTIONS="halt_on_error=1" "$OUT/spectrum_tsan_tests"

    echo "== TSan: fixture de carrera deliberada (el detector tiene que disparar) =="
    # shellcheck disable=SC2086
    $CC $BASEFLAGS -fsanitize=thread -fno-omit-frame-pointer \
        "$DIR/spectrum_race_fixture.c" -o "$OUT/spectrum_race_fixture"

    if TSAN_OPTIONS="halt_on_error=1" "$OUT/spectrum_race_fixture" \
            > "$OUT/spectrum_race_fixture.log" 2>&1; then
        echo "FAIL: el fixture de carrera deliberada no disparo el detector"
        cat "$OUT/spectrum_race_fixture.log"
        exit 1
    fi

    if ! grep -q "data race" "$OUT/spectrum_race_fixture.log"; then
        echo "FAIL: el fixture termino con error pero sin reportar una data race"
        cat "$OUT/spectrum_race_fixture.log"
        exit 1
    fi

    echo "ok: el detector reporto la carrera deliberada ($OUT/spectrum_race_fixture.log)"
fi
