#!/usr/bin/env bash
#
# Harness gate: the drawn ROE/SWR curve is a VALLEY, never the inverted parabola
# (the owner's order of 2026-10-17: "la parabola esta invertida: base abajo y las
# puntas tienen que tender a infinito positivo").
#
# Builds the harness (Docker image x6200-uiharness) and renders two fixtures of
# the REAL window (src/dialog_swrscan.c + src/roe_curve.c) over the fake
# acquisition backend (src/swr_backend.c):
#
#   swrscan-owner-40m-domo  the owner's DOMO (ROE ~1.0 at the edges, ~3.8 in the
#                           middle, n=44/51 -- the captured inverted curve). With
#                           the render of #113/#119 it draws the parabola upside
#                           down and the check below is RED; with the #125-fix it
#                           draws the valley.
#   swrscan-owner-40m       the owner's real antenna valley (the #125 seed). It
#                           must STAY a valley: the fix disturbs nothing here.
#
# The shape is asserted on the PNG pixels (tests/swrscan_curve_evidence.py): the
# base of the curve in the bottom quarter of the chart band and both edges rising
# above it. No OCR, no radio.
#
# Usage: tests/check_harness_swrscan_curve.sh
#   SKIP_BUILD=1   reuse the existing build/
#   UI_HARNESS_IMAGE=<name>  override the Docker image (default x6200-uiharness)
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HARNESS_DIR="$ROOT/tools/ui_harness"
BUILD_DIR="$HARNESS_DIR/build"
OUT_DIR="$BUILD_DIR/out/swrscan-curve"
IMAGE="${UI_HARNESS_IMAGE:-x6200-uiharness}"

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

echo "rendering the DOMO and the real valley..."
run -lc "cd build && rm -rf out/swrscan-curve && mkdir -p out/swrscan-curve && \
    ./ui_harness --scenario ../fixtures/swrscan-owner-40m-domo.scn \
        --png out/swrscan-curve/domo.png --json out/swrscan-curve/domo.json >/dev/null 2>&1 && \
    ./ui_harness --scenario ../fixtures/swrscan-owner-40m.scn \
        --png out/swrscan-curve/valle.png --json out/swrscan-curve/valle.json >/dev/null 2>&1"

for f in domo valle; do
    [ -f "$OUT_DIR/$f.png" ] || fail "missing $OUT_DIR/$f.png"
done

python3 "$ROOT/tests/swrscan_curve_evidence.py" "$OUT_DIR/domo.png" "$OUT_DIR/valle.png"
