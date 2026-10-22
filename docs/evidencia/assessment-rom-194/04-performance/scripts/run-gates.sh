#!/usr/bin/env bash
# run-gates.sh — #198 PERF-03: run the three existing harness gates on the
# frozen candidate and archive their raw JSON/PNG plus the full logs.
# It does not modify tests, lvgl, src or budgets: the gates are run as shipped.
set -u
REPO=${REPO:-/home/pablo/desa/x6200/assessment-194/clone-gui}
HERE=$(cd "$(dirname "$0")" && pwd)
OUT=${OUT:-$HERE/../capturas}
LOGS=${LOGS:-$HERE/../logs/step3-gates}
mkdir -p "$OUT" "$LOGS" "$OUT/natural" "$OUT/wakeups"
cd "$REPO" || exit 1

run() {  # $1 = name, $2 = script, rest = env assignments
    local name=$1 script=$2; shift 2
    echo "== $name start $(date -u +%FT%TZ)" | tee "$LOGS/$name.log"
    env "$@" bash "$script" >> "$LOGS/$name.log" 2>&1
    echo "== $name exit=$? end $(date -u +%FT%TZ)" | tee -a "$LOGS/$name.log"
    return 0
}

run natural tests/check_harness_natural.sh
cp -a tools/ui_harness/build/out/natural/. "$OUT/natural/" 2>/dev/null

run wakeups tests/check_harness_wakeups.sh SKIP_BUILD=1
cp -a tools/ui_harness/build/out/wakeups/. "$OUT/wakeups/" 2>/dev/null

run waterfall tests/check_waterfall_incremental.sh
