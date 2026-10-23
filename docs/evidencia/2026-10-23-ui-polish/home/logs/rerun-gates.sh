#!/bin/sh
# Re-corre la bateria de gates sobre el arbol CONGELADO (despues del ultimo
# comentario en src/clock.c) para que los logs de la evidencia correspondan al
# codigo exacto que se commitea. Secuencial a proposito: los gates de costo
# (flushes / natural) no se corren mientras otro contenedor ocupa la maquina.
#
# SPDX-License-Identifier: GPL-2.0-or-later
set -e
WT="$(cd "$(dirname "$0")/../../../../.." && pwd)"
H="$WT/docs/evidencia/2026-10-23-ui-polish/home/logs"
SCRATCH=/tmp/opencode/ui-polish-home-284
cd "$WT"

echo "== determinism (rebuild del arnes en tools/ui_harness/build) =="
sh tests/check_harness_determinism.sh > "$H/harness-determinism.log" 2>&1

echo "== natural / wakeups (build ya fresco) =="
SKIP_BUILD=1 sh tests/check_harness_natural.sh > "$H/check-harness-natural.log" 2>&1
SKIP_BUILD=1 sh tests/check_harness_wakeups.sh > "$H/check-harness-wakeups.log" 2>&1

echo "== proxy de flushes sobre las 205 fixtures =="
(cd "$WT/docs/evidencia/2026-10-23-ui-polish/home" && \
 python3 medir_flushes.py --before "$SCRATCH/build-base" --after "$SCRATCH/build-post" \
   --scratch "$SCRATCH/perf" > "$H/medir-flushes.log" 2>&1)

echo "== run_all_tests --quick =="
sh tests/run_all_tests.sh --quick > "$H/run_all_tests-quick.log" 2>&1

echo GATES_OK
