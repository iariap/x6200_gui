#!/usr/bin/env bash
#
# Harness gate: two runs of the same scenario must produce byte-identical PNGs.
# Builds the harness (Docker image x6200-uiharness) and runs every fixture in
# fixtures/ twice, then compares sha256. Prints PASS when all of them match.
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
RUNS=2

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

# Every fixture is rendered RUNS times; the PNGs of each run must match.
echo "running every fixture $RUNS times..."
run -lc "cd build && mkdir -p out/determinism && \
    for f in ../fixtures/*.scn; do
        n=\$(basename \"\$f\" .scn)
        for r in \$(seq 1 $RUNS); do
            ./ui_harness --scenario \"\$f\" \
                --png \"out/determinism/\$n-\$r.png\" \
                --json \"out/determinism/\$n-\$r.json\" >/dev/null
        done
    done"

status=0
for f in "$HARNESS_DIR"/fixtures/*.scn; do
    n="$(basename "$f" .scn)"
    ref=""
    for r in $(seq 1 "$RUNS"); do
        png="$BUILD_DIR/out/determinism/$n-$r.png"
        json="$BUILD_DIR/out/determinism/$n-$r.json"
        [ -f "$png" ] || fail "missing $png"
        [ -f "$json" ] || fail "missing $json (each fixture must emit PNG + JSON)"
        h="$(sha256sum "$png" | cut -d' ' -f1)"
        if [ -z "$ref" ]; then
            ref="$h"
            echo "$n: $h"
        elif [ "$h" != "$ref" ]; then
            echo "$n: run $r differs ($h != $ref)" >&2
            status=1
        fi
    done
done

[ "$status" -eq 0 ] || fail "some fixtures produced different PNGs across runs"

echo "PASS: deterministic (identical PNGs for every fixture)"
