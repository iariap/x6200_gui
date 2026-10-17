#!/bin/sh
#
# Every test of this ROM, in one run, with a summary at the end:
#
#   tests/run_all_tests.sh [--quick]
#
#   config        the cfg struct is fully initialised (a missing field kills the GUI)
#   manual_apps   every app of the firmware has its own section in the operator manual
#   pruebas       docs/PRUEBAS.md: structure and cited evidence exist
#   host          the native suite (401 assertions, no container)
#   wspr/sstv     codec round trips against the X6100's own test tools
#   wefax/navtex
#   js8           the X6100's full JS8 suite (1994 assertions; skipped by --quick)
#   webserver     the configurator's 19 pages, with the target's own python
#
# Needs the docker images x6200-uiharness and x6200-buildroot. The webserver part
# is skipped when its checkout or the build output are not around.
#
# The four codec suites need codec build dependencies (libfftw3-dev for wspr,
# libsndfile1-dev for sstv/wefax) that only the *full* stage of
# tools/ui_harness/Dockerfile installs. When the image in use is the minimal
# `--target ci` one, the runner prints a `SKIP:` reason and exits 77, which
# `run` below counts as omitted, never as a failure nor as a pass. Point IMAGE
# at a full image to run them (see tests/native/codec_docker.sh).
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
        status=0
    else
        status=$?
    fi

    case $status in
        0)
            line=$(grep -E "comprobaciones|assertions|all OK|PASS|all tests passed|todas|todos|OK:" /tmp/"$name".log | tail -1)
            results="$results\n  $name: OK   $line"
            pass=$((pass + 1))
            ;;
        77)
            # El runner declaro un skip hermetico (p. ej. un codec IMAGE con una
            # imagen sin las deps). Se cuenta como omitido: ni OK ni FALLO.
            line=$(grep -E "^SKIP:" /tmp/"$name".log | tail -1)
            printf '    %s\n' "$line"
            results="$results\n  $name: SKIP $line"
            skipped="$skipped $name"
            ;;
        *)
            printf '    FALLÓ, ver /tmp/%s.log\n' "$name"
            tail -5 /tmp/"$name".log | sed 's/^/    /'
            results="$results\n  $name: FALLÓ"
            fail=$((fail + 1))
            ;;
    esac
}

# Chequeo estatico barato: si la struct cfg queda sin inicializar, la GUI se muere antes
# de dibujar (paso con cwt_level y cwt_group y la radio quedo en el logo de arranque).
# Van por el mismo `run` que el resto: un fallo tiene que sumar al contador `fail` (el que
# imprime el resumen y decide el exit). Antes usaban `fallos=$((fallos+1))`, una variable
# que no existia: la falla se anotaba en otro lado y la suite terminaba verde igual.
run config python3 "$DIR/check_cfg_fills.py" "$GUI"
run actions python3 "$DIR/check_actions.py"
run settings_focus python3 "$DIR/check_settings_focus.py"
run apps_menu python3 "$DIR/check_apps_menu.py"
run app_handlers python3 "$DIR/check_app_handlers.py"
run manual_apps python3 "$DIR/check_manual_apps.py"
run pruebas python3 "$DIR/audita-pruebas.py"
run wcag python3 "$DIR/check_wcag.py"
run cw_tune_feed python3 "$DIR/check_cw_tune_feed.py"

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
