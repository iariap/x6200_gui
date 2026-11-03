#!/usr/bin/env bash
#
# Gate del contrato de la matriz de checks app-owned (.factory/check-matrix.sh).
#
# La matriz es la ÚNICA validación de un PR de x6200, así que su contrato no
# puede romperse en silencio: este test es determinista y barato (sin docker, sin
# red) y valida, en un repo temporal, lo que el harness ya no puede garantizar:
#
#   1. la matriz existe, es ejecutable y parsea (`bash -n`);
#   2. es standalone: no sourcea helpers ni resuelve metadata del harness, y
#      define `has`/`skip` en el propio archivo;
#   3. conserva el contrato de salida: un ref ausente es exit 2 con FAIL
#      explícito (nunca "sin cambios"), base==head es exit 0 con "sin cambios",
#      un diff de sólo documentación es exit 0 con [SIN GATE] y un cambio de
#      código sin bloque es exit 2 (cobertura del diff);
#   4. el default del checkout se resuelve desde la ubicación del script (no del
#      cwd): se invoca desde / y con base==head;
#   5. un cambio a `.factory/**` dispara este mismo gate (no queda sin cubrir).
#
# Uso: bash tests/check_factory_matrix.sh
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MATRIX="$ROOT/.factory/check-matrix.sh"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

# --- 1) existencia, modo y sintaxis -----------------------------------------
[ -f "$MATRIX" ] || fail "no existe $MATRIX"
[ -x "$MATRIX" ] || fail "la matriz no es ejecutable: $MATRIX"
bash -n "$MATRIX" || fail "la matriz no parsea (bash -n)"

# --- 2) standalone -----------------------------------------------------------
for needle in 'scripts/lib/check_matrix.sh' 'scripts/app_config.py'; do
	if grep -qF "$needle" "$MATRIX"; then
		fail "la matriz depende del harness: $needle"
	fi
done
grep -qE '^has\(\)' "$MATRIX" || fail "la matriz no define has()"
grep -qE '^skip\(\)' "$MATRIX" || fail "la matriz no define skip()"
if grep -qE '^[[:space:]]*(source|\.)[[:space:]]' "$MATRIX"; then
	fail "la matriz sourcea helpers externos (no es standalone)"
fi

# --- 3) contrato de refs/salida (sobre el repo real, sin correr gates) -------
rc=0
out="$(cd / && "$MATRIX" ref-inexistente HEAD 2>&1)" || rc=$?
[ "$rc" -eq 2 ] || fail "ref ausente: exit $rc (esperado 2): $out"
grep -q 'FAIL: ref inexistente' <<<"$out" || fail "ref ausente sin FAIL explícito: $out"

rc=0
out="$(cd / && "$MATRIX" HEAD HEAD 2>&1)" || rc=$?
[ "$rc" -eq 0 ] || fail "base==head: exit $rc (esperado 0): $out"
grep -q '==> sin cambios entre HEAD y HEAD' <<<"$out" || fail "base==head sin 'sin cambios': $out"

# --- 4) repo temporal: default del checkout, [SIN GATE] y cobertura ----------
# La corrida del bloque `.factory/**` invoca este mismo test; con el guard de
# profundidad la corrida anidada no vuelve a armar el repo temporal (si no,
# recursión infinita). Las secciones 1-3 ya se validaron arriba.
if [ "${CHECK_FACTORY_MATRIX_NESTED:-0}" = "1" ]; then
	echo "  OK: contrato de la matriz app-owned (corrida anidada)"
	exit 0
fi

REPO="$TMP/repo"
mkdir -p "$REPO/.factory" "$REPO/tests"
cp "$MATRIX" "$REPO/.factory/check-matrix.sh"
chmod +x "$REPO/.factory/check-matrix.sh"
# El test se copia para que el bloque `.factory/**` de la matriz lo encuentre.
cp "$ROOT/tests/check_factory_matrix.sh" "$REPO/tests/check_factory_matrix.sh"
git -C "$REPO" init -q
git -C "$REPO" -c user.name=test -c user.email=test@example.invalid add -A
git -C "$REPO" -c user.name=test -c user.email=test@example.invalid commit -qm base

# Diff de sólo documentación: no hay gate que correr, pero no es un FAIL.
echo '# notas' >"$REPO/NOTES.md"
git -C "$REPO" -c user.name=test -c user.email=test@example.invalid add -A
git -C "$REPO" -c user.name=test -c user.email=test@example.invalid commit -qm docs
rc=0
out="$(cd / && "$REPO/.factory/check-matrix.sh" HEAD~1 HEAD 2>&1)" || rc=$?
[ "$rc" -eq 0 ] || fail "diff de documentación: exit $rc (esperado 0): $out"
grep -q '\[SIN GATE\]' <<<"$out" || fail "diff de documentación sin [SIN GATE]: $out"
grep -q 'sólo documentación' <<<"$out" || fail "diff de documentación sin el motivo: $out"

# Cambio de código sin bloque: la cobertura del diff lo rechaza (exit 2).
echo 'int main(void) { return 0; }' >"$REPO/roto.c"
git -C "$REPO" -c user.name=test -c user.email=test@example.invalid add -A
git -C "$REPO" -c user.name=test -c user.email=test@example.invalid commit -qm codigo
rc=0
out="$(cd / && "$REPO/.factory/check-matrix.sh" HEAD~1 HEAD 2>&1)" || rc=$?
[ "$rc" -eq 2 ] || fail "código sin gate: exit $rc (esperado 2): $out"
grep -q 'FAIL: hay paths sin gate' <<<"$out" || fail "código sin gate sin FAIL explícito: $out"

# Un cambio a .factory/** dispara este mismo gate y el resto queda [SKIP].
printf '{"id":"x6200"}\n' >"$REPO/.factory/app.json"
git -C "$REPO" -c user.name=test -c user.email=test@example.invalid add -A
git -C "$REPO" -c user.name=test -c user.email=test@example.invalid commit -qm factory
rc=0
out="$(cd / && CHECK_FACTORY_MATRIX_NESTED=1 "$REPO/.factory/check-matrix.sh" HEAD~1 HEAD 2>&1)" || rc=$?
[ "$rc" -eq 0 ] || fail ".factory/**: exit $rc (esperado 0): $out"
grep -q '==> \[check\] check_factory_matrix' <<<"$out" || fail ".factory/** no disparó el gate: $out"
grep -q '\[SKIP\]' <<<"$out" || fail ".factory/** no dejó [SKIP] visible: $out"

echo "  OK: contrato de la matriz app-owned"
