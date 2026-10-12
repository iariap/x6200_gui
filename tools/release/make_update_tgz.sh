#!/bin/bash
#
# Arma el tgz de actualización en el lugar a partir del rootfs de la imagen.
#
#   sudo tools/release/make_update_tgz.sh <rootfs.tar> <salida.tgz>
#
# Por qué hay que ser root: el rootfs tiene archivos setuid (busybox, mount, umount,
# dbus-daemon-launch-helper, ssh-keysign). Extrayéndolo como usuario común se pierden
# esos bits, y el tgz los llevaría mal: una actualización en el lugar rompería esos
# binarios. Como root se preservan dueño y permisos exactos.
#
# Por qué el tgz lleva SOLO archivos y enlaces (ni "./" ni entradas de directorio):
# el S02update del firmware hace el backup así:
#
#     zcat $i | tar -tf - | xargs tar -cvf - | gzip > /mnt/backup_...
#
# Si el tgz trae una entrada "./", ese tar recibe "./" y archiva el sistema de archivos
# entero -incluido /mnt, donde está escribiendo el backup-. Con solo archivos, el backup
# contiene exactamente los archivos que se reemplazan.
#
# SPDX-License-Identifier: LGPL-2.1-or-later
#
set -e

ROOTFS=${1:?uso: make_update_tgz.sh <rootfs.tar> <salida.tgz>}
OUT=${2:?uso: make_update_tgz.sh <rootfs.tar> <salida.tgz>}

if [ "$(id -u)" != "0" ]; then
    echo "Hace falta root: el rootfs tiene archivos setuid y hay que preservarlos." >&2
    echo "  sudo $0 $ROOTFS $OUT" >&2
    exit 1
fi

[ -f "$ROOTFS" ] || { echo "no encuentro $ROOTFS" >&2; exit 1; }

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

echo "==> extrayendo el rootfs"
tar -xf "$ROOTFS" -C "$TMP"

echo "==> armando la lista (solo archivos y enlaces, sin ./ ni directorios)"
( cd "$TMP" && find . \( -type f -o -type l \) -print0 | sed -z 's|^\./||' | sort -z > "$TMP.list" )

TOTAL=$(tr -cd '\0' < "$TMP.list" | wc -c)
echo "    $TOTAL entradas"

echo "==> creando $OUT"
tar --null --numeric-owner -C "$TMP" -T "$TMP.list" -czf "$OUT"

echo "==> comprobaciones"
BYTES=$(stat -c%s "$OUT")
DIRS=$(tar -tzf "$OUT" | grep -c "/$" || true)
RAIZ=$(tar -tzf "$OUT" | grep -cE "^\./?$" || true)
SUID=$(tar -tvzf "$OUT" | awk '$1 ~ /^...s/ || $1 ~ /^.....s/' | wc -l)

echo "    tamaño:            $BYTES bytes"
echo "    entradas:          $(tar -tzf "$OUT" | wc -l)"
echo "    directorios:       $DIRS (tiene que ser 0)"
echo "    entrada ./:        $RAIZ (tiene que ser 0)"
echo "    setuid:            $SUID (tienen que ser 5)"
echo "    GUI ejecutable:    $(tar -tvzf "$OUT" | awk '$NF == "usr/sbin/x6200_gui" {print $1}')"

[ "$DIRS" = "0" ] || { echo "FALLA: hay entradas de directorio" >&2; exit 1; }
[ "$RAIZ" = "0" ] || { echo "FALLA: hay entrada ./" >&2; exit 1; }
[ "$SUID" = "5" ] || { echo "FALLA: se perdieron bits setuid" >&2; exit 1; }

echo "==> listo: $OUT"
