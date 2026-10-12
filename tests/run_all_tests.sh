#!/bin/sh
#
# Every test of this ROM, in one run, with a summary at the end:
#
#   tests/run_all_tests.sh [--quick]
#
#   host          the native suite (401 assertions, no container)
#   wspr/sstv     codec round trips against the X6100's own test tools
#   wefax/navtex
#   js8           the X6100's full JS8 suite (1994 assertions; skipped by --quick)
#   webserver     the configurator's 19 pages, with the target's own python
#
# Needs the docker images x6200-uiharness and x6200-buildroot. The webserver part
# is skipped when its checkout or the build output are not around.
#
# SPDX-License-Identifier: LGPL-2.1-or-later
#
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
GUI=$(CDPATH= cd -- "$DIR/.." && pwd)
WS=${WS:-$GUI/../.x6200_webserver}
QUICK=${1:-}

pass=0
fail=0
skipped=""
results=""

run() {
    name=$1
    shift

    printf '\n=== %s ===\n' "$name"

    if "$@" > /tmp/"$name".log 2>&1; then
        line=$(grep -E "comprobaciones|assertions|all OK|PASS|all tests passed" /tmp/"$name".log | tail -1)
        results="$results\n  $name: OK   $line"
        pass=$((pass + 1))
    else
        printf '    FALLÓ, ver /tmp/%s.log\n' "$name"
        tail -5 /tmp/"$name".log | sed 's/^/    /'
        results="$results\n  $name: FALLÓ"
        fail=$((fail + 1))
    fi
}

run host sh "$DIR/native/run_native_tests.sh"
run wspr sh "$DIR/native/run_wspr_tests.sh"
run sstv sh "$DIR/native/run_sstv_tests.sh"
run wefax sh "$DIR/native/run_wefax_tests.sh"
run navtex sh "$DIR/native/run_navtex_tests.sh"

if [ "$QUICK" = "--quick" ]; then
    skipped="$skipped js8"
else
    run js8 sh "$DIR/native/run_js8_tests.sh"
fi

if [ -f "$WS/tests/run_pages_test.py" ] && docker image inspect x6200-buildroot >/dev/null 2>&1; then
    run webserver docker run --rm --entrypoint="" -e QEMU_LD_PREFIX=/b/target \
        -e PYTHONDONTWRITEBYTECODE=1 -v "$WS:/repo" -v x6200-build-output:/b \
        x6200-buildroot bash -c 'cd /repo && /b/target/usr/bin/python3 tests/run_pages_test.py tests/params.default.db'
else
    skipped="$skipped webserver"
fi

printf '\n================ resumen ================'
printf "$results\n"
[ -n "$skipped" ] && printf '  (omitidos:%s)\n' "$skipped"
printf '  %d OK, %d fallos\n' "$pass" "$fail"

[ "$fail" = "0" ]
