#!/bin/sh
#
# SSTV codec test: encodes a test picture in every mode the app sends, adds
# noise, decodes it with the real src/sstv code (VIS detection + the same RAM
# decoder used live) and compares the picture. Needs libsndfile as a development
# package, so it runs in the full UI-harness container; when the image lacks
# sndfile.h the runner skips with a reason instead of failing: see
# tests/native/codec_docker.sh.
#
# Usage: tests/native/run_sstv_tests.sh
#
# SPDX-License-Identifier: LGPL-2.1-or-later
#
set -e

DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO=$(CDPATH= cd -- "$DIR/../.." && pwd)
# shellcheck source=tests/native/codec_docker.sh
. "$DIR/codec_docker.sh"

codec_env sstv /usr/include/sndfile.h

codec_run "$REPO" '
    set -e
    G=/repo
    W=$(mktemp -d)
    gcc -O2 -std=gnu11 -w -I "$G/src" -o "$W/sstv_loopback" \
        "$G/tests/native/test_sstv_loopback.c" \
        "$G/src/sstv/encoder.c" "$G/src/sstv/decoder.c" \
        "$G/src/sstv/vis.c" "$G/src/sstv/linesync.c" \
        -lpng -lsndfile -lm -lpthread
    "$W/sstv_loopback"
'
