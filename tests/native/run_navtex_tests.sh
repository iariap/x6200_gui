#!/bin/sh
#
# NavTex (SITOR-B) codec test: synthesizes a FEC transmission, decodes it with
# the real src/navtex code and checks the message comes back at several SNRs.
# It needs only the base C++ toolchain, so it runs even in the minimal
# `--target ci` image; when there is no docker/image at all the runner skips
# with a reason instead of failing: see tests/native/codec_docker.sh.
#
# Usage: tests/native/run_navtex_tests.sh
#
# SPDX-License-Identifier: LGPL-2.1-or-later
#
set -e

DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO=$(CDPATH= cd -- "$DIR/../.." && pwd)
# shellcheck source=tests/native/codec_docker.sh
. "$DIR/codec_docker.sh"

codec_env navtex

codec_run "$REPO" '
    set -e
    G=/repo
    W=$(mktemp -d)
    g++ -O2 -std=gnu++17 -I "$G/src/navtex" \
        -o "$W/navtex_loopback" \
        "$G/tests/native/test_navtex_loopback.cpp" \
        "$G/src/navtex/decoder.cpp" "$G/src/navtex/navtex_rx.cpp" "$G/src/navtex/fftfilt.cxx"
    "$W/navtex_loopback"
'
