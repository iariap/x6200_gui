#!/usr/bin/env bash
#
# Harness gate: the wakeup profile of the ROM's GUI thread (issue #13, UI-07
# remainder).
#
# The ROM has two wakeup sources: its own main loop (src/main.c: it sleeps until
# the LVGL timer handler says a timer is due) and its tick thread (src/main.c:
# `while (1) { usleep(5 * 1000); lv_tick_inc(5); }`). `--bench-wakeups` measures
# the first one on the firmware's own timer set and reports it next to the second
# one's cadence, so "how much does the 5 ms tick wake the CPU" is a number.
#
# Builds the harness (Docker image x6200-uiharness) and runs
# fixtures/main-natural.scn five ways:
#   1. `--bench-wakeups` twice, with the same output paths: the PNG sha256 and
#      the whole JSON must be byte-identical (no wall clock is read);
#   2. `--bench-wakeups 3000`: the window is a CLI knob and the reported counts
#      scale with it;
#   3. a plain capture (no mode): the `wakeups` block is extra, it never appears
#      in the plain capture the A/B pipeline reads;
#   4. `--bench-wakeups --bench-natural` and `--bench-wakeups 0`: refused (exit
#      2), a mode cannot be measured on top of another and a non-positive window
#      is a typo, never a default.
# Then tests/wakeup_evidence.py checks the numbers: the ROM's own refresh period
# read from lv_conf.h, the tick's cadence read from src/main.c, the loop's
# sleep accounting, that the loop does not busy-spin, and the headline -- the
# tick thread's share of the wakeups.
#
# Usage: tests/check_harness_wakeups.sh
#   SKIP_BUILD=1   reuse the existing build/
#   UI_HARNESS_IMAGE=<name>  override the Docker image (default x6200-uiharness)
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HARNESS_DIR="$ROOT/tools/ui_harness"
BUILD_DIR="$HARNESS_DIR/build"
OUT_DIR="$BUILD_DIR/out/wakeups"
FIXTURE="main-natural"
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

echo "running the wakeup profile on $FIXTURE..."
run -lc "cd build && rm -rf out/wakeups && mkdir -p out/wakeups && \
    ./ui_harness --scenario ../fixtures/$FIXTURE.scn \
        --png out/wakeups/wakeups.png --json out/wakeups/wakeups.json \
        --bench-wakeups >/dev/null 2>&1 && \
    cp out/wakeups/wakeups.png out/wakeups/run1.png && \
    cp out/wakeups/wakeups.json out/wakeups/run1.json && \
    ./ui_harness --scenario ../fixtures/$FIXTURE.scn \
        --png out/wakeups/wakeups.png --json out/wakeups/wakeups.json \
        --bench-wakeups >/dev/null 2>&1 && \
    ./ui_harness --scenario ../fixtures/$FIXTURE.scn \
        --png out/wakeups/window.png --json out/wakeups/window.json \
        --bench-wakeups 3000 >/dev/null 2>&1 && \
    ./ui_harness --scenario ../fixtures/$FIXTURE.scn \
        --png out/wakeups/plain.png --json out/wakeups/plain.json >/dev/null 2>&1"

for f in wakeups run1 window plain; do
    [ -f "$OUT_DIR/$f.json" ] || fail "missing $OUT_DIR/$f.json"
done

# 1. Idempotency: same seed, same window -> the same bytes, PNG included.
sha_a="$(sha256sum "$OUT_DIR/run1.png" | cut -d' ' -f1)"
sha_b="$(sha256sum "$OUT_DIR/wakeups.png" | cut -d' ' -f1)"
[ "$sha_a" = "$sha_b" ] || fail "the two wakeup runs rendered different PNGs ($sha_a != $sha_b)"
cmp -s "$OUT_DIR/run1.json" "$OUT_DIR/wakeups.json" || \
    fail "the two wakeup runs wrote different JSON (the wakeup section is not deterministic)"

# 2..5. Numbers, the sources they mirror and the refused combinations.
run -lc "cd build && \
    (./ui_harness --scenario ../fixtures/$FIXTURE.scn --png out/wakeups/no.png \
        --bench-wakeups --bench-natural >/dev/null 2>&1; [ \$? -eq 2 ]) && \
    (./ui_harness --scenario ../fixtures/$FIXTURE.scn --png out/wakeups/no.png \
        --bench-wakeups 0 >/dev/null 2>&1; [ \$? -eq 2 ])" || \
    fail "--bench-wakeups accepted a combination or a window it must refuse (exit 2)"

python3 "$ROOT/tests/wakeup_evidence.py" "$OUT_DIR" "$ROOT" || exit 1

echo "PASS: wakeup profile measured, sourced and delimited ($FIXTURE)"
echo "  png sha256: $sha_a"
