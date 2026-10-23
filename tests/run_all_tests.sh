#!/bin/sh
#
# Every test of this ROM, in one run, with a summary at the end:
#
#   tests/run_all_tests.sh [--quick]
#
#   config        the cfg struct is fully initialised (a missing field kills the GUI)
#   manual_apps   every app of the firmware has its own section in the operator manual
#   pruebas       docs/PRUEBAS.md: structure and cited evidence exist
#   curve_evidence the drawn ROE curve on committed PNGs: U / dome / branch
#   host          the native suite (401 assertions, no container)
#   host_ci_parity  the same suite with lvgl/ hidden (the tree the CI job sees)
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
run settings_grid python3 "$DIR/check_settings_grid.py"
run info_bar python3 "$DIR/check_info_bar.py"
run apps_menu python3 "$DIR/check_apps_menu.py"
run app_handlers python3 "$DIR/check_app_handlers.py"
run manual_apps python3 "$DIR/check_manual_apps.py"
run pruebas python3 "$DIR/audita-pruebas.py"
run wcag python3 "$DIR/check_wcag.py"
run cw_tune_feed python3 "$DIR/check_cw_tune_feed.py"
run autotune_v2 python3 "$DIR/check_autotune_v2.py"
run build_identity python3 "$DIR/check_build_identity.py"
# Inventario de los 22 dialogos que el arnes de host NO renderiza (issue #222): la
# tabla tests/qemu/dialog-coverage.csv tiene que coincidir con los mocks, con las
# fuentes reales de cada dialogo y con las 25 apps de remote_server.cpp::app_names[].
run dialog_coverage python3 "$DIR/check_dialog_coverage.py"
run curve_evidence python3 "$DIR/test_swrscan_curve_evidence.py"

run host sh "$DIR/native/run_native_tests.sh"
# Supervisor de la GUI (issue #214): backoff, logs unicos, una sola instancia y
# senales, contra el rootfs/usr/sbin/x6200_daemon REAL en un chroot privado.
# Sin user namespaces el stage sale 77 (SKIP), nunca PASS.
run daemon sh "$DIR/daemon/run_daemon_tests.sh"
# Paridad host<->CI: el job `functional` de ab-testing.yml no inicializa el
# submodulo lvgl, asi que el build host tiene que pasar tambien sin el. Si un
# stage vuelve a apoyarse en el header real, aca se ve en local (en el PR solo se
# veria en CI). Ver docs/evidencia/2026-10-21-ciparity/.
run host_ci_parity sh "$DIR/native/run_native_tests.sh" --ci-parity
# Persistencia fail-closed con /mnt REAL (issue #213): namespace + chroot
# privados. Declara SKIP (exit 77) si el host no permite user namespaces.
run db_failclosed sh "$DIR/native/run_db_failclosed.sh"
# Barrido QEMU de las 25 apps reales (issue #222): binario ARM real en un chroot
# PRIVADO (contenedor/volumen/lab propios de la corrida). Sin ese entorno privado
# declara SKIP (exit 77) con los requisitos exactos: no corre a medias ni declara
# PASS, y nunca usa los recursos del banco del owner (banco-apps/x6200-build-output).
run qemu_apps sh "$DIR/qemu/run_apps_qemu.sh"
run drawbuf_partial sh "$DIR/check_drawbuf_partial.sh"
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
