#!/bin/bash
#
# Arma el tgz de actualización y su sidecar sha256, en el lugar, a partir del
# rootfs de la imagen.
#
#   sudo tools/release/make_update_tgz.sh <rootfs.tar> <salida.tgz>
#
# Por qué hay que ser root: el rootfs tiene archivos setuid (busybox,
# dbus-daemon-launch-helper, ssh-keysign). Extrayéndolo como usuario común se
# pierden esos bits, y el tgz los llevaría mal: una actualización en el lugar
# rompería esos binarios. Como root se preservan dueño y permisos exactos.
#
# Aplica además tools/release/update-overlay sobre el rootfs antes de empaquetar:
# son archivos que este port corrige y que sólo tienen que llegar por la
# actualización. El overlay sale del checkout (uid 1000): en staging se normaliza
# a root:root 0755, que es como tiene que quedar en la radio. No se reescribe el
# código fuente.
#
# Por qué el tgz lleva SOLO archivos y enlaces (ni "./" ni entradas de directorio):
# el backup del S02update viejo archiva la lista del paquete; una entrada "./"
# haría que ese tar archivara el sistema de archivos entero, incluido /mnt, donde
# está escribiendo el backup. Con sólo archivos, el backup contiene exactamente
# los archivos que se reemplazan.
#
# Publica (o copia junto al tgz) <salida.tgz>.sha256, de una sola línea
# "<hash>  <basename>", que es el formato que verifica `sha256sum -c` y el que el
# S02update del target exige antes de tocar nada (issue #215).
#
# Inventario setuid: la imagen auditada trae exactamente estos tres archivos
# (bin/mount y bin/umount son 0755 - la aserción vieja de "5 bits setuid" era
# falsa y abortaba esta herramienta sobre la imagen buena). Si el rootfs cambia a
# propósito, actualizá esta tabla y la del S02update en el mismo commit.
#
# SPDX-License-Identifier: LGPL-2.1-or-later
#
set -e

ROOTFS=${1:?uso: make_update_tgz.sh <rootfs.tar> <salida.tgz>}
OUT=${2:?uso: make_update_tgz.sh <rootfs.tar> <salida.tgz>}

# path modo uid gid
EXPECTED_SUID='bin/busybox 4755 0 0
usr/libexec/dbus-daemon-launch-helper 4750 0 101
usr/libexec/ssh-keysign 4711 0 0'

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

# Archivos que este port corrige y que viajan en la actualización (por ejemplo el
# S02update con el backup arreglado). No van en la imagen a propósito: ver el propio
# script y docs/TESTING.md.
OVERLAY="$(dirname "$0")/update-overlay"
if [ -d "$OVERLAY" ] && [ -n "$(find "$OVERLAY" -type f 2>/dev/null)" ]; then
    echo "==> aplicando el overlay de la actualización"
    tar -cf - -C "$OVERLAY" . | tar -xf - -C "$TMP" --no-same-owner
    # El checkout es de uid 1000: en STAGING el overlay queda root:root 0755, como
    # tiene que quedar en la radio. Se normaliza la copia, nunca el checkout.
    while IFS= read -r rel; do
        chown 0:0 "$TMP/${rel#./}"
        chmod 0755 "$TMP/${rel#./}"
    done < <( cd "$OVERLAY" && find . -type f )
    ( cd "$OVERLAY" && find . -type f | sed 's|^\./|    |' )
fi

echo "==> comprobando el inventario setuid en staging"
ACTUAL_SUID=$(find "$TMP" -perm -4000 -printf '%P %m %U %G\n' | LC_ALL=C sort)
if [ "$ACTUAL_SUID" != "$EXPECTED_SUID" ]; then
    echo "FALLA: el inventario setuid del rootfs no es el de la imagen auditada." >&2
    echo "  esperado (path modo uid gid):" >&2
    printf '%s\n' "$EXPECTED_SUID" | sed 's/^/    /' >&2
    echo "  encontrado:" >&2
    [ -n "$ACTUAL_SUID" ] && printf '%s\n' "$ACTUAL_SUID" | sed 's/^/    /' >&2
    echo "  Si el rootfs cambió a propósito, actualizá la tabla EXPECTED_SUID de este" >&2
    echo "  script y la del S02update en el mismo commit." >&2
    exit 1
fi
printf '%s\n' "$ACTUAL_SUID" | sed 's/^/    /'

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
# El paquete que se publica tiene que llevar el mismo inventario que staging:
# mismas rutas, mismos dueños (--numeric-owner también al listar, para ver 0/0 y
# no root/root) y el bit setuid puesto. Los modos exactos se validaron arriba,
# sobre el árbol en staging.
PKG_SUID_SHOW=$(tar -tvzf "$OUT" --numeric-owner |
    awk 'substr($1, 1, 1) == "-" && substr($1, 4) ~ /[sS]/ { print $NF " " substr($1, 1, 10) " " $2 }' |
    LC_ALL=C sort)
PKG_SUID=$(printf '%s\n' "$PKG_SUID_SHOW" | awk 'NF { print $1 " " $3 }' | LC_ALL=C sort)
EXPECTED_SUID_PKG=$(printf '%s\n' "$EXPECTED_SUID" | awk '{ print $1 " " $3 "/" $4 }' | LC_ALL=C sort)
UPD=$(tar -tvzf "$OUT" --numeric-owner | awk '$NF == "etc/init.d/S02update" { print $1 " " $2 }')
GUI=$(tar -tvzf "$OUT" --numeric-owner | awk '$NF == "usr/sbin/x6200_gui" { print $1 " " $2 }')

echo "    tamaño:            $BYTES bytes"
echo "    entradas:          $(tar -tzf "$OUT" | wc -l)"
echo "    directorios:       $DIRS (tiene que ser 0)"
echo "    entrada ./:        $RAIZ (tiene que ser 0)"
echo "    setuid:            $(printf '%s\n' "$PKG_SUID" | sed '/^$/d' | wc -l) (tienen que ser 3)"
printf '%s\n' "$PKG_SUID_SHOW" | sed '/^$/d;s/^/      /'
echo "    S02update:         ${UPD:-FALTA} (tiene que ser -rwxr-xr-x 0/0)"
echo "    GUI ejecutable:    ${GUI:-FALTA}"

[ "$DIRS" = "0" ] || { echo "FALLA: hay entradas de directorio" >&2; exit 1; }
[ "$RAIZ" = "0" ] || { echo "FALLA: hay entrada ./" >&2; exit 1; }
[ "$PKG_SUID" = "$EXPECTED_SUID_PKG" ] || {
    echo "FALLA: el tgz no lleva el inventario setuid del rootfs auditado" >&2; exit 1; }
[ "$UPD" = "-rwxr-xr-x 0/0" ] || {
    echo "FALLA: el S02update del tgz no es root:root 0755" >&2; exit 1; }

echo "==> sidecar sha256"
OUTDIR=$(dirname "$OUT")
OUTBASE=$(basename "$OUT")
( cd "$OUTDIR" && sha256sum "$OUTBASE" > "$OUTBASE.sha256" )
( cd "$OUTDIR" && sha256sum -c "$OUTBASE.sha256" ) >/dev/null
echo "    $(cut -d' ' -f1 < "$OUT.sha256")  $OUTBASE"

echo "==> listo: $OUT y $OUT.sha256"
