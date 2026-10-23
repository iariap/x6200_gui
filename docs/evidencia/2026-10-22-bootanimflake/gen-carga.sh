#!/usr/bin/env bash
# Compara el test de la cesion ORIGINAL (tiempo de pared, sha base) contra el
# NUEVO (criterio de sondeos) bajo carga, para reproducir el falso rojo del gate
# "boot anim gate (deterministic, no docker)".
#
#   bash docs/evidencia/2026-10-22-bootanimflake/gen-carga.sh [BASE_SHA] [N]
#
# Imprime, por escenario, cuantas de N corridas fallan con cada version.
# No usa docker ni deps fuera de cc/bash/taskset/nice.
set -euo pipefail
cd "$(dirname "$0")/../../.."

BASE="${1:-35bad790}"
N="${2:-40}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

mkdir -p "$WORK/old"
git show "$BASE:tests/bootanim/handoff_test.c" > "$WORK/old/handoff_test.c"
git show "$BASE:src/boot_splash.c" > "$WORK/old/boot_splash.c"
git show "$BASE:src/boot_splash.h" > "$WORK/old/boot_splash.h"

cc -std=c11 -Wall -Wextra -Werror -I "$WORK/old" -I tests/bootanim \
	-o "$WORK/handoff_old" "$WORK/old/handoff_test.c" "$WORK/old/boot_splash.c"
cc -std=c11 -Wall -Wextra -Werror -DBOOTANIM_TEST -I src -I src/bootanim -I tests/bootanim \
	-o "$WORK/handoff_new" tests/bootanim/handoff_test.c src/boot_splash.c
cc -std=c11 -Wall -Wextra -Werror -I src -I src/bootanim -I tests/bootanim \
	-o "$WORK/fake_animator" tests/bootanim/fake_animator.c

scenario() { # <nombre> <hogs> <opts>
	local name="$1" hogs="$2" opts="$3" i fails_old=0 fails_new=0
	for ((i = 0; i < hogs; i++)); do ( while :; do :; done ) & done
	sleep 1
	for ((i = 0; i < N; i++)); do
		# shellcheck disable=SC2086
		$opts "$WORK/handoff_old" "$WORK/fake_animator" >/dev/null 2>&1 || fails_old=$((fails_old + 1))
		# shellcheck disable=SC2086
		$opts "$WORK/handoff_new" "$WORK/fake_animator" >/dev/null 2>&1 || fails_new=$((fails_new + 1))
	done
	echo "$name (hogs=$hogs, opts='$opts'): OLD $fails_old/$N fallos | NEW $fails_new/$N fallos"
	for p in $(jobs -p); do kill "$p" 2>/dev/null || true; done
	wait 2>/dev/null || true
}

echo "# BASE=$BASE | N=$N por version y escenario | $(uptime)"
scenario "A runner ocupado" 16 ""
scenario "B runner muy ocupado, test en 2 cores" 32 "taskset -c 0-1"
scenario "C nice 19 + 24 hogs" 24 "nice -n 19"
scenario "D taskset 1 core + nice 19 + 24 hogs" 24 "taskset -c 0 nice -n 19"
