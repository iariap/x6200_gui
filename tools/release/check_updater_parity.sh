#!/bin/bash
#
# Verifica que el S02update sea el MISMO archivo en los dos puntos de instalación
# (contrato del issue iariap/x6200_gui#215):
#
#   tools/release/update-overlay/etc/init.d/S02update          (este repo, GUI):
#       lo que instala una actualización, o sea el que corren las siguientes;
#   br2_external/board/X6200/linux/rootfs-overlay/etc/init.d/S02update
#       (AetherX6200Buildroot): el que corre una imagen recién grabada.
#
# Si difieren, el gate de release tiene que fallar: dos updaters distintos
# significan dos comportamientos distintos según de dónde venga la tarjeta.
#
#   tools/release/check_updater_parity.sh <ruta al S02update de Buildroot>
#   tools/release/check_updater_parity.sh <checkout de AetherX6200Buildroot>
#   BUILDROOT_S02UPDATE=/ruta/al/S02update tools/release/check_updater_parity.sh
#
# Sale 0 si son idénticos, 1 si difieren, 2 si no encuentra el archivo.
#
# SPDX-License-Identifier: LGPL-2.1-or-later
#
set -e

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
MINE=$HERE/update-overlay/etc/init.d/S02update
REL=br2_external/board/X6200/linux/rootfs-overlay/etc/init.d/S02update

[ -f "$MINE" ] || { echo "no encuentro $MINE" >&2; exit 2; }

OTHER=
case "${1:-}" in
    "")
        OTHER=${BUILDROOT_S02UPDATE:-}
        ;;
    */S02update) OTHER=$1 ;;
    *)
        if [ -f "$1" ]; then
            OTHER=$1
        else
            OTHER=$1/$REL
        fi
        ;;
esac

if [ -z "$OTHER" ]; then
    echo "falta la ruta del S02update de AetherX6200Buildroot" >&2
    echo "  uso: $0 <ruta al S02update de Buildroot | checkout de AetherX6200Buildroot>" >&2
    echo "  (o poné BUILDROOT_S02UPDATE=/ruta/al/S02update)" >&2
    echo "  no adivino un checkout hermano a propósito: hay varios worktrees en vuelo y uno ajeno" >&2
    echo "  daría un FAIL falso." >&2
    exit 2
fi
if [ ! -f "$OTHER" ]; then
    echo "no encuentro el S02update de AetherX6200Buildroot ($OTHER)" >&2
    exit 2
fi

MINE_SHA=$(sha256sum "$MINE" | awk '{ print $1 }')
OTHER_SHA=$(sha256sum "$OTHER" | awk '{ print $1 }')
echo "==> GUI:       $MINE_SHA  $MINE"
echo "==> Buildroot: $OTHER_SHA  $OTHER"
if [ "$MINE_SHA" != "$OTHER_SHA" ]; then
    echo "FALLA: el S02update de los dos puntos de instalación no es el mismo archivo" >&2
    exit 1
fi
echo "==> paridad OK (mismo archivo, mismo hash)"
