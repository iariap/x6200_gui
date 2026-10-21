#!/usr/bin/env bash
# run-logged.sh <log-relative-name> <shell-command>
#
# Orquesta herramientas existentes: ejecuta <shell-command> con bash, guarda stdout+stderr
# completos en logs/<log-relative-name>.log, y agrega una fila a commands.jsonl con el comando
# exacto, cwd, variables relevantes no secretas, inicio/fin UTC, exit code y referencia al log.
# No sustituye harness, tests ni contrato de evidencia.
set -u
LOGREL=$1; shift
CMD=$*
OUT=${OUT:-/home/pablo/desa/x6200/assessment-194/wt-assessment-196/docs/evidencia/assessment-rom-194/02-rom}
LOG="$OUT/logs/$LOGREL.log"
mkdir -p "$(dirname "$LOG")"
START=$(date -u +%Y-%m-%dT%H:%M:%SZ)
START_NS=$(date +%s%N)
# shellcheck disable=SC2086
bash -c "$CMD" >"$LOG" 2>&1
RC=$?
END=$(date -u +%Y-%m-%dT%H:%M:%SZ)
END_NS=$(date +%s%N)
MS=$(( (END_NS - START_NS) / 1000000 ))
{
  printf '{"ts_start":"%s","ts_end":"%s","duration_ms":%s,"cwd":"%s","command":' "$START" "$END" "$MS" "$(pwd)"
  printf '%s' "$CMD" | python3 -c 'import json,sys; sys.stdout.write(json.dumps(sys.stdin.read()))'
  printf ',"env":{"OUT":"%s","SCRATCH":"%s"},"exit_code":%s,"log":"logs/%s.log"}\n' \
    "${OUT:-}" "${SCRATCH:-}" "$RC" "$LOGREL"
} >> "$OUT/commands.jsonl"
echo "$LOG  exit=$RC  ${MS}ms"
exit $RC
