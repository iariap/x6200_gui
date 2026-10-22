#!/bin/sh
# run-212-instalado.sh — corre el banco de #212 contra el S01create_data y el
# fstab **instalados** en la rootfs construida del par integrado (no contra el
# archivo del repo): prueba de integración de que el artefacto lleva el script
# del PR y de que script+fstab instalados funcionan juntos.
#
#   OUT=<salida Buildroot privada> [BR=<clon Buildroot>] foco/run-212-instalado.sh
#
# Usa un contenedor privado y desechable (imagen h212-x6200-data-arm, la misma
# receta que documenta br2_external/tests/README.md).
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -u

DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
OUT=${OUT:-/home/pablo/desa/asf/state/worktrees/x6200/next-211-combined-lab/out}
BR=${BR:-/home/pablo/desa/asf/state/worktrees/x6200/next-211-combined-buildroot}
LAB=${LAB:-/home/pablo/desa/asf/state/worktrees/x6200/next-211-combined-lab/foco212-inst}
IMG=${IMG:-h212-x6200-data-arm:latest}
BASE="$LAB/inst-repo/br2_external/board/X6200/linux/rootfs-overlay"
BANK="$BR/br2_external/tests/test_S01create_data.sh"

[ -f "$OUT/target/etc/init.d/S01create_data" ] || { echo "FALLO: no hay rootfs construida ($OUT/target)" >&2; exit 2; }
docker image inspect "$IMG" >/dev/null 2>&1 || { echo "FALLO: no existe la imagen $IMG" >&2; exit 2; }

mkdir -p "$LAB/inst-repo/br2_external/tests" "$BASE/etc/init.d" "$LAB/out"
cp "$OUT/target/etc/init.d/S01create_data" "$BASE/etc/init.d/S01create_data"
cp "$OUT/target/etc/fstab" "$BASE/etc/fstab"
# El banco vive fuera de la rootfs (no se instala); se copia al layout del repo.
cp "$BANK" "$LAB/inst-repo/br2_external/tests/test_S01create_data.sh"

echo "== hash del script instalado vs fuente del repo =="
sha256sum "$OUT/target/etc/init.d/S01create_data" "$BR/br2_external/board/X6200/linux/rootfs-overlay/etc/init.d/S01create_data" 2>&1
if cmp -s "$OUT/target/etc/init.d/S01create_data" "$BR/br2_external/board/X6200/linux/rootfs-overlay/etc/init.d/S01create_data"; then
    echo "S01create_data instalado == fuente del repo"
else
    echo "FALLO: S01create_data instalado DIFIERE de la fuente"; exit 1
fi

echo "== banco #212 contra los archivos INSTALADOS (contenedor privado) =="
docker run --rm --privileged -v "$LAB/inst-repo":/repo:ro -v "$LAB/out":/lab \
    "$IMG" bash /repo/br2_external/tests/test_S01create_data.sh
rc=$?
echo "banco (instalado) rc=$rc"
exit "$rc"
