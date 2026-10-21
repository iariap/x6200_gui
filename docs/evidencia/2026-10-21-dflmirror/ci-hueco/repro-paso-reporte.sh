#!/usr/bin/env bash
# Corre el cuerpo EXACTO del paso "Report the contracts and fail on a violation"
# del workflow ab-testing.yml (extraido del YAML, con la misma desindentacion que
# hace YAML en un block scalar) sobre payloads reales de contracts.json, y muestra
# el exit code que GitHub usaria para el job.
#
#   $ docs/evidencia/2026-10-21-dflmirror/ci-hueco/repro-paso-reporte.sh
#
# Dos versiones del paso:
#   - la del arbol (con el fix),
#   - la de la base e6925db6 (`git show e6925db6:.github/workflows/ab-testing.yml`),
#     para que se vea el agujero: con la vieja, un payload BLOCKED daba exit 0.
#
# Payloads (ci-hueco/payloads/):
#   blocked-real.json      el artefacto ui-contracts del run 39644351915: BLOCKED
#                          (con el paso viejo el job quedaba VERDE)
#   pass-real.json         el artefacto ui-contracts del run 39511316773: PASS
#   fail-synthetic.json    el PASS de arriba con status FAIL (el camino FAIL)
#   review-required.json   status REVIEW REQUIRED (el header lo declara legitimo)
#   inconclusive.json      status INCONCLUSIVE (idem)
#   MISSING                sin contracts.json (el paso no produjo resultados)
set -euo pipefail

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
WT=$(git -C "$HERE" rev-parse --show-toplevel)
BASE=${BASE:-e6925db6}

# El cuerpo va desde la linea que alimenta a python3 hasta el terminador PY, con
# los 10 espacios que YAML le quita a un block scalar de `run: |`.
extract_body() {
    awk '
      /python3 - contracts\.json <<.PY./ { grab = 1 }
      grab { print }
      grab && /^ +PY$/ { exit }
    ' "$1" | sed 's/^          //'
}

run_payload() {
    local body="$1" payload="$2" tmp rc=0
    tmp=$(mktemp -d)
    [ "$payload" = MISSING ] || cp "$payload" "$tmp/contracts.json"
    (cd "$tmp" && GITHUB_STEP_SUMMARY="$tmp/summary.md" bash -c "$body") >"$tmp/out.txt" 2>&1 || rc=$?
    echo "--- $(basename "$payload")"
    echo "    exit (resultado del job) = $rc"
    grep -E 'status:|blocking' "$tmp/summary.md" 2>/dev/null | sed 's/^/    /' | cut -c1-150 || true
    rm -rf "$tmp"
}

body_new=$(extract_body "$WT/.github/workflows/ab-testing.yml")
[ -n "$body_new" ] || { echo "no pude extraer el paso del arbol" >&2; exit 2; }

echo "== paso del arbol (con el fix) =="
for p in blocked-real pass-real fail-synthetic review-required inconclusive MISSING; do
    if [ "$p" = MISSING ]; then run_payload "$body_new" MISSING
    else run_payload "$body_new" "$HERE/payloads/$p.json"; fi
done

old=$(mktemp)
git -C "$WT" show "$BASE:.github/workflows/ab-testing.yml" > "$old"
body_old=$(extract_body "$old")
[ -n "$body_old" ] || { echo "no pude extraer el paso de $BASE" >&2; exit 2; }

echo
echo "== paso de la base $BASE (sin el fix: el agujero) =="
for p in blocked-real pass-real; do run_payload "$body_old" "$HERE/payloads/$p.json"; done
rm -f "$old"
