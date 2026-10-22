#!/bin/sh
# manifiesto.sh — manifiesto único del par GUI+Buildroot y de los artefactos de
# la reconstrucción reproducible de la épica #211.
#
#   WT=<worktree GUI> BR=<clon Buildroot> OUT=<salida Buildroot privada> \
#   manifiesto/manifiesto.sh > manifiesto/manifiesto.txt
#
# Sólo lee: no modifica nada de los repos ni de la salida.
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -u

DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
WT=${WT:-/home/pablo/desa/asf/state/worktrees/x6200/next-211-combined}
BR=${BR:-/home/pablo/desa/asf/state/worktrees/x6200/next-211-combined-buildroot}
OUT=${OUT:-/home/pablo/desa/asf/state/worktrees/x6200/next-211-combined-lab/out}
DHIST=${DHIST:-/home/pablo/desa/x6200/images/x6200_r1cbu_v0.1.2-lu2dsx.tgz}

h() { sha256sum "$@" 2>/dev/null; }

echo "=== identidad del par ==="
echo "GUI HEAD          $(git -C "$WT" rev-parse HEAD)   rama $(git -C "$WT" rev-parse --abbrev-ref HEAD)"
echo "Buildroot HEAD    $(git -C "$BR" rev-parse HEAD)   rama $(git -C "$BR" rev-parse --abbrev-ref HEAD)"
echo "GUI base (epic)   d4fb57f8ff8ccbcfdef13fc191cc9977511b6845"
echo "BR  base (epic)   f23d14f975f2d12466274aeeb4932bd191f6f6e5"
echo "submodulo GUI     $(git -C "$WT" ls-tree HEAD lvgl | awk '{print $3}') = $(git -C "$WT/lvgl" describe --tags 2>/dev/null || echo '?')"
echo "submodulo BR      $(git -C "$BR" ls-tree HEAD buildroot | awk '{print $3}') (gitlink; version del arbol upstream)"
echo "sello fuente GUI  $(sh "$WT/tools/fuente-md5.sh" "$WT")"
echo "arbol GUI sucio?  $(git -C "$WT" status --porcelain -- src lvgl/src | wc -l) archivos con cambios en src/ + lvgl/src"
echo "arbol BR sucio?   $(git -C "$BR" status --porcelain | wc -l) archivos (debe ser 0)"

echo
echo "=== fuentes de los cambios de los hijos (hashes de archivo) ==="
h "$BR/br2_external/board/X6200/linux/rootfs-overlay/etc/init.d/S01create_data"
h "$BR/br2_external/board/X6200/linux/rootfs-overlay/etc/init.d/S02update"
h "$BR/br2_external/board/X6200/linux/rootfs-overlay/etc/fstab"
h "$WT/rootfs/usr/sbin/x6200_daemon"
h "$WT/tools/release/update-overlay/etc/init.d/S02update"
h "$BR/br2_external/tests/test_S01create_data.sh"
h "$WT/tests/daemon/run_daemon_tests.sh"
h "$WT/tests/release/test_safe_update.sh"

echo
echo "=== paridad del updater (#215: un solo archivo en los dos puntos de instalacion) ==="
if cmp -s "$WT/tools/release/update-overlay/etc/init.d/S02update" "$BR/br2_external/board/X6200/linux/rootfs-overlay/etc/init.d/S02update"; then
    echo "cmp GUI <-> Buildroot: IDENTICOS"
else
    echo "cmp GUI <-> Buildroot: DIFIEREN (FAIL)"
fi
( cd "$WT" && BUILDROOT_S02UPDATE="$BR/br2_external/board/X6200/linux/rootfs-overlay/etc/init.d/S02update" sh tools/release/check_updater_parity.sh 2>&1; echo "check_updater_parity rc=$?" )

echo
echo "=== artefactos de la reconstruccion (salida privada) ==="
if [ -d "$OUT/images" ]; then
    ( cd "$OUT/images" && ls -la && echo "--- sha256:" && h sdcard.img rootfs.tar boot.vfat 2>/dev/null )
else
    echo "(PENDING: $OUT/images no existe todavia)"
fi
echo "--- target (rootfs) ---"
if [ -x "$OUT/target/usr/sbin/x6200_gui" ]; then
    h "$OUT/target/usr/sbin/x6200_gui"
    echo "tamano: $(stat -c%s "$OUT/target/usr/sbin/x6200_gui") B"
    echo "sello embebido en el binario: $(strings "$OUT/target/usr/sbin/x6200_gui" | grep -m1 -x '[0-9a-f]\{32\}' || echo '(no encontrado)')"
    echo "--- scripts instalados en la rootfs ---"
    h "$OUT/target/etc/init.d/S01create_data" "$OUT/target/etc/init.d/S02update" "$OUT/target/etc/fstab"
    echo "--- comparacion instalado vs fuente ---"
    for p in "etc/init.d/S01create_data" "etc/init.d/S02update" "etc/fstab"; do
        f="$OUT/target/$p"
        case "$p" in
            etc/init.d/S01create_data) src="$BR/br2_external/board/X6200/linux/rootfs-overlay/$p" ;;
            etc/init.d/S02update)      src="$BR/br2_external/board/X6200/linux/rootfs-overlay/$p" ;;
            etc/fstab)                 src="$BR/br2_external/board/X6200/linux/rootfs-overlay/$p" ;;
        esac
        if cmp -s "$f" "$src"; then echo "$p: instalado == fuente del repo"; else echo "$p: DIFIERE de $src"; fi
    done
    echo "--- init.d de la rootfs construida ---"
    ls "$OUT/target/etc/init.d"
else
    echo "(PENDING: $OUT/target/usr/sbin/x6200_gui no existe todavia)"
fi

echo
echo "=== inventario de init.d: imagen historica vs rootfs construida ==="
if [ -f "$DHIST" ] && [ -d "$OUT/target/etc/init.d" ]; then
    echo "historica ($(basename "$DHIST")):"
    tar -tzf "$DHIST" 2>/dev/null | sed -n 's|^\.\?/*etc/init.d/\(.*\)$|  \1|p' | sort
    echo "construida:"
    ( cd "$OUT/target/etc/init.d" && find . -maxdepth 1 -type f -o -type l | sed 's|^\./|  |' | sort )
else
    echo "(PENDING)"
fi
