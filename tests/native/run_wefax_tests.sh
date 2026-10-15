#!/bin/sh
#
# WeFax codec test: encodes a test picture, decimates it to the decoder's rate
# and decodes it with the real src/wefax code in Auto mode, as the app does
# (rows kept only after the fax is confirmed), checking APT start/stop detection
# and the picture by correlation. Needs libsndfile as a development package, so
# it runs in the full UI-harness container; when the image lacks sndfile.h the
# runner skips with a reason instead of failing: see
# tests/native/codec_docker.sh.
#
# Usage: tests/native/run_wefax_tests.sh
#
# SPDX-License-Identifier: LGPL-2.1-or-later
#
set -e

DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO=$(CDPATH= cd -- "$DIR/../.." && pwd)
# shellcheck source=tests/native/codec_docker.sh
. "$DIR/codec_docker.sh"

codec_env wefax /usr/include/sndfile.h

codec_run "$REPO" '
    set -e
    G=/repo
    W=$(mktemp -d)
    gcc -O2 -std=gnu11 -w -I "$G/src" -o "$W/wefax_loopback" \
        "$G/tests/native/test_wefax_loopback.c" \
        "$G/src/wefax/encoder.c" "$G/src/wefax/decoder.c" "$G/src/sstv/encoder.c" \
        -lpng -lsndfile -lm -lpthread
    "$W/wefax_loopback"
'
