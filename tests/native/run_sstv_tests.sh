#!/bin/sh
#
# SSTV codec test: encodes a test picture in every mode the app sends, adds
# noise, decodes it with the real src/sstv code (VIS detection + the same RAM
# decoder used live) and compares the picture.
#
# Usage: tests/native/run_sstv_tests.sh
#
# SPDX-License-Identifier: LGPL-2.1-or-later
#
set -e

DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PARENT=$(CDPATH= cd -- "$DIR/../../.." && pwd)
IMAGE="${IMAGE:-x6200-uiharness}"

docker run --rm --entrypoint="" -v "$PARENT:/src" -w /src/x6200_gui x6200-uiharness bash -c '
    set -e
    G=/src/x6200_gui
    W=$(mktemp -d)
    gcc -O2 -std=gnu11 -w -I "$G/src" -o "$W/sstv_loopback" \
        "$G/tests/native/test_sstv_loopback.c" \
        "$G/src/sstv/encoder.c" "$G/src/sstv/decoder.c" \
        "$G/src/sstv/vis.c" "$G/src/sstv/linesync.c" \
        -lpng -lsndfile -lm -lpthread
    "$W/sstv_loopback"
'
