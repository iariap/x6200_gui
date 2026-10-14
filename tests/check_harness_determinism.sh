#!/usr/bin/env bash
#
# Milestone-1 gate: two runs of the same scenario must produce byte-identical
# PNGs. Builds the harness (Docker image x6200-uiharness) and runs the
# home-usb-20m fixture twice, then compares sha256.
#
# Usage: tests/check_harness_determinism.sh
#   SKIP_BUILD=1   reuse the existing build/
#   UI_HARNESS_IMAGE=<name>  override the Docker image (default x6200-uiharness)
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HARNESS_DIR="$ROOT/tools/ui_harness"
BUILD_DIR="$HARNESS_DIR/build"
IMAGE="${UI_HARNESS_IMAGE:-x6200-uiharness}"
FIXTURE="fixtures/home-usb-20m.scn"

PNG_A="out/determinism-a.png"
PNG_B="out/determinism-b.png"

fail() { echo "FAIL: $*" >&2; exit 1; }

command -v docker >/dev/null 2>&1 || fail "docker not found"
docker image inspect "$IMAGE" >/dev/null 2>&1 || \
    fail "image '$IMAGE' not found (build it: docker build -t $IMAGE tools/ui_harness)"

run() {
    docker run --rm --entrypoint bash --user "$(id -u):$(id -g)" \
        -v "$ROOT:/src" -w /src/tools/ui_harness "$IMAGE" "$@"
}

if [ "${SKIP_BUILD:-0}" != "1" ]; then
    REV="$(git -C "$ROOT" rev-parse HEAD 2>/dev/null || echo unknown)"
    echo "building ui_harness (revision $REV)..."
    run -lc "mkdir -p build && cd build && \
        cmake .. -DHARNESS_REVISION=$REV >/dev/null && make -j\$(nproc) >/dev/null"
fi

[ -x "$BUILD_DIR/ui_harness" ] || fail "ui_harness not built ($BUILD_DIR/ui_harness)"

echo "running the scenario twice..."
run -lc "cd build && mkdir -p out && \
    ./ui_harness --scenario ../$FIXTURE --png $PNG_A && \
    ./ui_harness --scenario ../$FIXTURE --png $PNG_B"

A="$BUILD_DIR/$PNG_A"
B="$BUILD_DIR/$PNG_B"

[ -f "$A" ] || fail "missing $A"
[ -f "$B" ] || fail "missing $B"

HA=$(sha256sum "$A" | cut -d' ' -f1)
HB=$(sha256sum "$B" | cut -d' ' -f1)

echo "run 1 sha256: $HA"
echo "run 2 sha256: $HB"

if [ "$HA" != "$HB" ]; then
    fail "the two runs produced different PNGs"
fi

echo "PASS: deterministic (identical PNGs)"
