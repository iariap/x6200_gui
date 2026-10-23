#!/bin/sh
#
# Compila y corre el fixture del issue #292 contra el spectrum.c REAL y los
# stubs de tests/native. Sin radio, sin red, sin hardware.
#
# Uso: sh run.sh [dir-de-build]
#
# SPDX-License-Identifier: LGPL-2.1-or-later
#
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO=$(CDPATH= cd -- "$HERE/../../.." && pwd)   # x6200_gui/
SRC="$REPO/src"
DIR="$REPO/tests/native"
OUT="${1:-$HERE/build}"

rm -rf "$OUT"
mkdir -p "$OUT/spectrum_src/lvgl" "$OUT/spectrum_src/params" "$OUT/spectrum_src/cfg"

SP="$OUT/spectrum_src"
cp "$SRC/spectrum.c" "$SRC/spectrum.h" "$SRC/scheduler.h" "$SP/"
cp "$DIR/stubs_spectrum/lvgl/lvgl.h"   "$SP/lvgl/"
cp "$DIR/stubs_spectrum/params/params.h" "$SP/params/"
cp "$DIR/stubs_spectrum/cfg/cfg.h"     "$SP/cfg/"
cp "$DIR/stubs_spectrum/theme.h" "$DIR/stubs_spectrum/styles.h" "$DIR/stubs_spectrum/meter.h" \
   "$DIR/stubs_spectrum/dsp.h" "$DIR/stubs_spectrum/events.h" "$DIR/stubs_spectrum/radio.h" \
   "$DIR/stubs_spectrum/recorder.h" "$DIR/stubs_spectrum/rtty.h" "$DIR/stubs_spectrum/util.h" \
   "$DIR/stubs_spectrum/pubsub_ids.h" "$SP/"

CC="${CC:-gcc}"
BASEFLAGS="-std=gnu11 -O1 -g -Wall -Wextra -Wno-unused-parameter -pthread"

$CC $BASEFLAGS -I"$SP" -I"$DIR" -I"$HERE" \
    "$SP/spectrum.c" "$DIR/spectrum_stubs.c" "$HERE/lsb_axis_fixture.c" \
    -o "$OUT/lsb_axis_fixture"

"$OUT/lsb_axis_fixture"
