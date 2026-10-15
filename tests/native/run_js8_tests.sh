#!/bin/sh
#
# JS8 test suite: builds the X6100's tests/test_js8.cpp (Catch2) against this
# repo's src/js8 and the vendored js8core, and runs it. Needs Catch2, the Boost
# headers and libfftw3f, which only the full UI-harness container image provides;
# when the image in use lacks any of them the runner skips with a reason instead
# of failing: see tests/native/codec_docker.sh.
#
# Usage: tests/native/run_js8_tests.sh
#
# SPDX-License-Identifier: LGPL-2.1-or-later
#
set -e

DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO=$(CDPATH= cd -- "$DIR/../.." && pwd)
# shellcheck source=tests/native/codec_docker.sh
. "$DIR/codec_docker.sh"

codec_env js8 \
    /usr/include/fftw3.h \
    /usr/include/boost/version.hpp \
    /usr/local/include/catch2/catch_test_macros.hpp

codec_run "$REPO" '
    set -e
    G=/repo
    W=$(mktemp -d)
    cd "$W"

    # js8core and the app library both have a resampler.cpp: build them in
    # separate directories so the object files do not collide.
    mkdir -p core app

    for f in $(find "$G/third-party/js8core/src" -name "*.cpp"); do
        (cd core && g++ -std=c++20 -O2 -w -I "$G/third-party/js8core/include" \
            -I "$G/third-party/js8core" -c "$f")
    done

    for f in $(find "$G/src/js8" -name "*.cpp"); do
        g++ -std=c++20 -O2 -w -I "$G/third-party/js8core/include" \
            -I "$G/third-party/js8core" -I "$G/src/js8" -c "$f" -o "app/$(basename "$f" .cpp).o"
    done

    g++ -std=c++20 -O2 -w -I "$G/src/js8" -I "$G/third-party/js8core/include" \
        -o test_js8 "$G/tests/native/test_js8_suite.cpp" core/*.o app/*.o \
        -lfftw3f -lCatch2Main -lCatch2 -lpthread -lm

    ./test_js8
'
