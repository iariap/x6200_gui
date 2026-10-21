#!/usr/bin/env bash
# Bisect: build the harness at a given revision and run the two mirror fixtures.
# Usage: bisect.sh <sha> [image]
set -uo pipefail
WT=${WT:-/home/pablo/desa/asf/state/worktrees/x6200/dfl-mirror-gate}
SHA="$1"
IMG="${2:-x6200-uiharness}"
DEST="/tmp/opencode/dflmirror/rev-$SHA"
mkdir -p "$DEST"
if [ ! -f "$DEST/tools/ui_harness/CMakeLists.txt" ]; then
    git -C "$WT" archive "$SHA" | tar -x -C "$DEST"
    cp -a "$WT/lvgl/." "$DEST/lvgl/" 2>/dev/null || { mkdir -p "$DEST/lvgl"; cp -a "$WT/lvgl/." "$DEST/lvgl/"; }
fi
docker run --rm --entrypoint bash --user "$(id -u):$(id -g)" \
    -v "$DEST:/src" -w /src/tools/ui_harness "$IMG" -lc \
    "mkdir -p build && cd build && cmake .. -DHARNESS_REVISION=$SHA >/dev/null 2>&1 && make -j\$(nproc) >/dev/null 2>&1" \
    || { echo "$SHA: BUILD FAILED"; exit 3; }
echo "--- revision $SHA"
for s in dfl-mirror-lsb dfl-mirror-usb; do
    out=$(docker run --rm --entrypoint bash --user "$(id -u):$(id -g)" \
        -v "$DEST:/src" -w /src/tools/ui_harness/build "$IMG" -lc \
        "./ui_harness --scenario ../fixtures/$s.scn --png /tmp/$s.png --json /tmp/$s.json" 2>&1)
    rc=$?
    echo "  $s: exit=$rc $(echo "$out" | grep -v 'cfg_sm enlazado' | grep -v buttons_init | tail -2)"
done
