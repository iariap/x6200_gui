#!/bin/sh
#
# WSPR round-trip test.
#
# Needs libfftw3f as a development package, which the host does not have, so it
# runs in the UI-harness container (tools/ui_harness/Dockerfile). It compiles the
# real src/wspr code plus K1JT's wsprd engine and decodes a synthesized signal.
#
# Usage: tests/native/run_wspr_tests.sh
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
    cd "$W"

    # K1JT'"'"'s engine is vendored code with its own warnings: compiled quietly
    for f in fano nhash tab wsprd wsprd_utils wsprsim_utils; do
        gcc -std=gnu11 -O2 -w -c "$G/src/wspr/wsprd/$f.c" -o "$f.o"
    done

    gcc -std=gnu11 -O1 -g -Wall -Wextra -Wno-unused-parameter -pthread \
        -I "$G/src" -I "$G/src/wspr/wsprd" \
        -o test_wspr \
        "$G/src/wspr/wspr.c" "$G/src/wspr/wsprnet.c" \
        "$G/tests/native/test_wspr.c" "$G/tests/native/test_framework.c" \
        "$G/tests/native/test_wspr_main.c" *.o \
        -lfftw3f -lliquid -lm

    ./test_wspr
'
