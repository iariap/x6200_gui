#!/bin/sh
#
# WeFax codec test: encodes a test picture, decimates it to the decoder's rate
# and decodes it with the real src/wefax code in Auto mode, as the app does
# (rows kept only after the fax is confirmed), checking APT start/stop detection
# and the picture by correlation.
#
# Usage: tests/native/run_wefax_tests.sh
#
# SPDX-License-Identifier: LGPL-2.1-or-later
#
set -e

DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PARENT=$(CDPATH= cd -- "$DIR/../../.." && pwd)

docker run --rm --entrypoint="" -v "$PARENT:/src" -w /src/x6200_gui x6200-uiharness bash -c '
    set -e
    G=/src/x6200_gui
    W=$(mktemp -d)
    gcc -O2 -std=gnu11 -w -I "$G/src" -o "$W/wefax_loopback" \
        "$G/tests/native/test_wefax_loopback.c" \
        "$G/src/wefax/encoder.c" "$G/src/wefax/decoder.c" "$G/src/sstv/encoder.c" \
        -lpng -lsndfile -lm -lpthread
    "$W/wefax_loopback"
'
