#!/bin/sh
# run-one.sh — se ejecuta EN LA RADIO (no en el host). Implementa el
# `timeout 30s` por pasada que pide el issue #347: la radio no trae el applet
# `timeout` de BusyBox (v1.36.1 sin timeout), asi que el mismo contrato se
# implementa con un watchdog: si el banco sigue vivo a los N segundos se le
# manda TERM y la pasada se reporta como exit 124 (timeout), igual que timeout(1).
#
# Uso: run-one.sh <out_file> <timeout_s> <programa> [args...]
#   stdout/stderr van a <out_file>.out / <out_file>.err
#   imprime el exit code del programa en stdout
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -u

OUT=${1:?falta out_file}
TMO=${2:?falta timeout_s}
shift 2
[ "$#" -ge 1 ] || { echo "falta el programa" >&2; exit 2; }

rm -f "$OUT.out" "$OUT.err" "$OUT.timeout"
"$@" > "$OUT.out" 2> "$OUT.err" &
pid=$!

(
    sleep "$TMO"
    if kill -0 "$pid" 2>/dev/null; then
        echo "TIMEOUT ${TMO}s" > "$OUT.timeout"
        kill -TERM "$pid" 2>/dev/null
        sleep 1
        kill -KILL "$pid" 2>/dev/null
    fi
) &
wd=$!

wait "$pid"
rc=$?
kill "$wd" 2>/dev/null
wait "$wd" 2>/dev/null
if [ -f "$OUT.timeout" ]; then
    echo 124
    exit 124
fi
echo "$rc"
exit "$rc"
