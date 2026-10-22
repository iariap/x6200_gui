#!/bin/sh
#
# Saca del rootfs de la imagen los archivos del rootfs del vendor que NO deben
# viajar: aca el port declara lo prohibido (lista explicita, por path exacto).
#
# Por que: el origen que Buildroot cachea para el paquete x6200-gui (el rootfs del
# vendor) trae /etc/init.d/S95gui, que arranca el MISMO x6200_daemon que el S20gui
# del port: dos GUIs dibujan sobre /dev/fb0 (flicker + doble CPU, issue #89). El
# target de Buildroot es incremental y no limpia lo que un paquete dejo de
# instalar, asi que ese S95gui sobrevive a cada build hasta que se lo saca.
#
# La lista va por path exacto, NUNCA con comodines: `S*gui*` tambien borraria el
# S20gui del port. Cada entrada tiene que ser un archivo del vendor que duplique
# uno portado. El script rechaza comodines, paths absolutos y `..`, y rechaza
# cualquier path que el port mismo instale (rootfs/), para no borrar de mas.
#
# Uso: prune-vendor-rootfs.sh <rootfs-dir>
#   <rootfs-dir> es la raiz del rootfs target (Buildroot pasa DESTDIR).
#   Es seguro correrlo sobre una copia: solo borra los paths de la lista.
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -e

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PORT_ROOTFS=${PORT_ROOTFS:-$SCRIPT_DIR/../rootfs}

ROOT=${1:?uso: prune-vendor-rootfs.sh <rootfs-dir>}
[ -d "$ROOT" ] || { echo "prune-vendor-rootfs: no existe el rootfs: $ROOT" >&2; exit 2; }
# Sin el rootfs del port no se puede comprobar que la lista no nombre un archivo
# propio: fail-closed, nunca borrar a ciegas.
[ -d "$PORT_ROOTFS" ] || { echo "prune-vendor-rootfs: no encuentro el rootfs del port ($PORT_ROOTFS)" >&2; exit 2; }

# --- archivos del rootfs del vendor que NO deben viajar en la imagen ---------
FORBIDDEN="
etc/init.d/S95gui
"
# -----------------------------------------------------------------------------

removed=0
for f in $FORBIDDEN; do
    [ -n "$f" ] || continue
    case "$f" in
        /*|*..*|*\**|*\?*|*\[*)
            echo "prune-vendor-rootfs: entrada invalida (tiene que ser un path relativo exacto): $f" >&2
            exit 2
            ;;
    esac
    if [ -e "$PORT_ROOTFS/$f" ]; then
        echo "prune-vendor-rootfs: '$f' tambien lo instala el port ($PORT_ROOTFS/$f): no se borra" >&2
        exit 2
    fi
    if [ -e "$ROOT/$f" ]; then
        rm -f "$ROOT/$f"
        echo "prune-vendor-rootfs: borrado $f"
        removed=$((removed + 1))
    fi
done

echo "prune-vendor-rootfs: $removed archivo(s) del vendor fuera de $ROOT"
