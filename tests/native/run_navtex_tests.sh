#!/bin/sh
#
# NavTex (SITOR-B) codec test: synthesizes a FEC transmission, decodes it with
# the real src/navtex code and checks the message comes back at several SNRs.
#
# Usage: tests/native/run_navtex_tests.sh
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
    g++ -O2 -std=gnu++17 -I "$G/src/navtex" \
        -o "$W/navtex_loopback" \
        "$G/tests/native/test_navtex_loopback.cpp" \
        "$G/src/navtex/decoder.cpp" "$G/src/navtex/navtex_rx.cpp" "$G/src/navtex/fftfilt.cxx"
    "$W/navtex_loopback"
'
