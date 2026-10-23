#!/usr/bin/env bash
#
# Harness gate: the natural bench measures the firmware's own invalidation,
# deterministically, and correlates with the UI-07 increment-1 profile.
#
# Builds the harness (Docker image x6200-uiharness) and runs
# fixtures/main-natural.scn five ways:
#   1. `--bench-natural` twice, with the same output paths: the PNG sha256 and
#      the whole JSON must be byte-identical (the idempotency self-test: the
#      natural section carries no wall clock at all);
#   2. `--bench` (forced): flushes = 1 and 384000 px per frame, so the forced
#      mode and its `metrics`/`benchmark` blocks are untouched by the natural
#      one (format and pipeline compatibility);
#   3. `--bench-natural --natural-static`: the delimitation control -- with the
#      data drive off the spectrum/waterfall areas disappear, so the profiled
#      areas cannot come from anything but the deterministic per-frame drive;
#   4. `--bench-natural 10`: the CLI frame count overrides the fixture;
#   5. `--bench-natural --display-off`: the ROM's own display-off route (the
#      POWER key -> backlight_switch()) before the run. With the panel dark the
#      whole 40-frame run has to invalidate and flush 0 px -- and its capture is
#      exactly the full-screen repaint lighter -- which is how the gate locks
#      that the blanked screen is not doing render work.
# Then it checks the produced JSON against the profile (spectrum 132000 px per
# FFT frame, waterfall 196000 px per row, clock 10486 px every 500 ms -- the
# rectangles UI-07 increment 1 measured, with the clock re-measured for the
# permanent clock of issue #250 and again for the taller clock of issue #285
# (10272 -> 10486 px, 586,0,214,49, see tests/natural_evidence.py), against the
# radio's DSP cadence (1000/15 and 1000/25, src/dsp.cpp) and that every
# invalidated rectangle is attributable to a firmware timer (no region is
# invalidated by the bench).
#
# Usage: tests/check_harness_natural.sh
#   SKIP_BUILD=1   reuse the existing build/
#   UI_HARNESS_IMAGE=<name>  override the Docker image (default x6200-uiharness)
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HARNESS_DIR="$ROOT/tools/ui_harness"
BUILD_DIR="$HARNESS_DIR/build"
OUT_DIR="$BUILD_DIR/out/natural"
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

# The harness runs headless in the container; the JSON and the PNGs land in the
# mounted build/out so the checks below read them from the host.
echo "running the natural bench on $FIXTURE..."
run -lc "cd build && rm -rf out/natural && mkdir -p out/natural && \
    ./ui_harness --scenario ../fixtures/$FIXTURE.scn \
        --png out/natural/natural.png --json out/natural/natural.json \
        --bench-natural >/dev/null 2>&1 && \
    cp out/natural/natural.png out/natural/run1.png && \
    cp out/natural/natural.json out/natural/run1.json && \
    ./ui_harness --scenario ../fixtures/$FIXTURE.scn \
        --png out/natural/natural.png --json out/natural/natural.json \
        --bench-natural >/dev/null 2>&1 && \
    ./ui_harness --scenario ../fixtures/$FIXTURE.scn \
        --png out/natural/forced.png --json out/natural/forced.json \
        --bench --warmup 1 --repeats 3 >/dev/null 2>&1 && \
    ./ui_harness --scenario ../fixtures/$FIXTURE.scn \
        --png out/natural/static.png --json out/natural/static.json \
        --bench-natural --natural-static >/dev/null 2>&1 && \
    ./ui_harness --scenario ../fixtures/$FIXTURE.scn \
        --png out/natural/cli.png --json out/natural/cli.json \
        --bench-natural 10 >/dev/null 2>&1 && \
    ./ui_harness --scenario ../fixtures/$FIXTURE.scn \
        --png out/natural/off.png --json out/natural/off.json \
        --bench-natural --display-off >/dev/null 2>&1"

for f in natural run1 forced static cli off; do
    [ -f "$OUT_DIR/$f.json" ] || fail "missing $OUT_DIR/$f.json"
done

# 1. Idempotency: same seed, same frames -> the same bytes, PNG included.
sha_a="$(sha256sum "$OUT_DIR/run1.png" | cut -d' ' -f1)"
sha_b="$(sha256sum "$OUT_DIR/natural.png" | cut -d' ' -f1)"
[ "$sha_a" = "$sha_b" ] || fail "the two natural runs rendered different PNGs ($sha_a != $sha_b)"
cmp -s "$OUT_DIR/run1.json" "$OUT_DIR/natural.json" || \
    fail "the two natural runs wrote different JSON (the natural section is not deterministic)"

# 2..5. Profile correlation, cadence, delimitation and forced-mode compatibility.
python3 "$ROOT/tests/natural_evidence.py" "$OUT_DIR" || exit 1

echo "PASS: natural bench deterministic, profiled and delimited ($FIXTURE)"
echo "  png sha256: $sha_a"
