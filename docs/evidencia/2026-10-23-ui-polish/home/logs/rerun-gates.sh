#!/bin/sh
# Verificacion post-arreglo del CI (coherencia de la evidencia natural): corre
# los gates de contraste + el proxy de flushes + la suite local, sobre el arbol
# congelado (el arreglo solo toca la evidencia, los pins del framework y docs).
#
# SPDX-License-Identifier: GPL-2.0-or-later
set -e
WT="$(cd "$(dirname "$0")/../../../../.." && pwd)"
H="$WT/docs/evidencia/2026-10-23-ui-polish/home/logs"
SCRATCH=/tmp/opencode/ui-polish-home-284
cd "$WT"

echo "== gates de contraste / barra =="
python3 tests/check_info_bar.py > "$H/check-info-bar.log" 2>&1
python3 tests/check_wcag.py > "$H/check-wcag.log" 2>&1
python3 tests/check_wcag_rendered.py > "$H/wcag-rendered.log" 2>&1

echo "== harness natural (rebuild) + wakeups + determinism =="
sh tests/check_harness_natural.sh > "$H/check-harness-natural.log" 2>&1
SKIP_BUILD=1 sh tests/check_harness_wakeups.sh > "$H/check-harness-wakeups.log" 2>&1
SKIP_BUILD=1 sh tests/check_harness_determinism.sh > "$H/harness-determinism.log" 2>&1

echo "== proxy de flushes sobre las 205 fixtures =="
(cd "$WT/docs/evidencia/2026-10-23-ui-polish/home" && \
 python3 medir_flushes.py --before "$SCRATCH/build-base" --after "$SCRATCH/build-post" \
   --scratch "$SCRATCH/perf" > "$H/medir-flushes.log" 2>&1)

echo "== run_all_tests --quick =="
sh tests/run_all_tests.sh --quick > "$H/run_all_tests-quick.log" 2>&1

echo GATES_OK
