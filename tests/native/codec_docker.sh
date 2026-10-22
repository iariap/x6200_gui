#!/bin/sh
#
# Shared, hermetic runner for the IMAGE codec suites (wspr/sstv/wefax/navtex and
# the full JS8 suite). Each run_*_tests.sh says what its suite checks; this file
# owns the two things that used to be hardcoded and made the suite lie on a
# healthy host (issue #47):
#
#   * the checkout directory name. The runners mounted the *parent* of the repo
#     (`-v "$parent:/src"`) and then hardcoded `-w /src/x6200_gui` and
#     `G=/src/x6200_gui`, so they only worked when the repo lived in a directory
#     literally called `x6200_gui`. In a build worktree or any renamed clone the
#     container had no such path and the codec died with a cryptic
#     `cc1: fatal error: /src/x6200_gui/src/...: No such file or directory`.
#     The repo root itself now mounts at a fixed /repo, so the name never matters.
#
#   * the image build dependencies. A plain `docker run x6200-uiharness`
#     assumed the image shipped the whole codec toolchain. The A/B workflow
#     rebuilds and re-tags `x6200-uiharness` from the minimal `--target ci` stage
#     (tools/ui_harness/Dockerfile), which has no fftw3/sndfile/Catch2/Boost, so
#     on a healthy host the codecs failed compiling. `codec_env` probes the image
#     first: if the toolchain cannot possibly build the suite it prints a `SKIP:`
#     line with the reason and exits 77. tests/run_all_tests.sh counts 77 as
#     *skipped* (never as a failure, never as a pass).
#
# To actually run them, point IMAGE at an image built from the full stage (the
# Dockerfile default target):
#
#   docker build -t x6200-uiharness tools/ui_harness
#   IMAGE=x6200-uiharness tests/run_all_tests.sh
#
# SPDX-License-Identifier: LGPL-2.1-or-later

CODEC_IMAGE="${IMAGE:-x6200-uiharness}"
CODEC_SKIP=77

# codec_env <suite> [probe-path...]: exit 77 with a clear `SKIP:` reason when the
# suite cannot succeed on this host. Each <probe-path> is a full path the suite
# needs inside the image (e.g. /usr/include/fftw3.h); pass none when the suite
# needs only the base toolchain (gcc/g++).
codec_env() {
    suite="$1"
    shift

    if ! command -v docker >/dev/null 2>&1; then
        printf 'SKIP: %s necesita docker en el host (no está instalado)\n' "$suite"
        exit "$CODEC_SKIP"
    fi
    if ! docker image inspect "$CODEC_IMAGE" >/dev/null 2>&1; then
        printf 'SKIP: %s: falta la imagen %s (docker build -t %s tools/ui_harness)\n' \
            "$suite" "$CODEC_IMAGE" "$CODEC_IMAGE"
        exit "$CODEC_SKIP"
    fi
    for probe in "$@"; do
        if ! docker run --rm --entrypoint="" "$CODEC_IMAGE" test -e "$probe" >/dev/null 2>&1; then
            printf 'SKIP: %s: la imagen %s no trae %s (build full: docker build -t %s tools/ui_harness)\n' \
                "$suite" "$CODEC_IMAGE" "$probe" "$CODEC_IMAGE"
            exit "$CODEC_SKIP"
        fi
    done
}

# codec_run <repo-root> <bash-body>: mount the repo root at the fixed /repo path
# and run <bash-body> there. The body sets G=/repo so the sources and
# tests/native stay reachable by a name that does not depend on the checkout.
codec_run() {
    codec_root="$1"
    shift
    docker run --rm --entrypoint="" -v "$codec_root:/repo" -w /repo "$CODEC_IMAGE" bash -c "$1"
}
