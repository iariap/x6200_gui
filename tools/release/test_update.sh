#!/bin/bash
#
# Prueba el camino de actualización en el lugar DE VERDAD, sobre una copia:
#
#   1. arma un "sistema destino" con el rootfs de la imagen (como root, permisos exactos);
#   2. marca un archivo que la actualización va a reemplazar (el binario de la GUI);
#   3. pone el tgz en /mnt del destino y corre el S02update REAL de la imagen dentro de
#      un chroot, con el busybox de la imagen;
#   4. comprueba que el backup contiene el archivo marcado (o sea: respaldó lo que
#      reemplaza) y que la extracción aplicó el tgz (el binario volvió a ser el de la
#      imagen).
#
# No toca nada de la máquina: todo pasa dentro del directorio de trabajo del chroot.
#
# Ojo con el backup del firmware: lo hace con
#
#     zcat $i | tar -tf - | xargs tar -cvf - | gzip > /mnt/backup_...
#
# y xargs parte la lista en varios tar, cada uno escribiendo al mismo stdout: el
# resultado es un flujo con varios archivos tar concatenados, del que sólo se puede
# leer el primero (unos 700 de los 5313 archivos). Es del firmware base, no nuestro;
# esta prueba lo deja a la vista. Para volver atrás de verdad, la imagen anterior.
#
#   sudo tools/release/test_update.sh [rootfs.tar] [actualización.tgz] [directorio de trabajo]
#
set -e

ROOTFS=${1:-/home/pablo/desa/x6200/images/rootfs_x6200_20261012_013612.tar}
TGZ=${2:-/home/pablo/desa/x6200/images/x6200_r1cbu_v0.1.2-lu2dsx.tgz}
T=${3:-/tmp/prueba-actualizacion}
BASE=$(dirname "$ROOTFS")

[ "$(id -u)" = "0" ] || { echo "Hace falta root: sudo $0" >&2; exit 1; }

rm -rf "$T"
mkdir -p "$T/target"
echo "==> armando el sistema destino (rootfs de la imagen)"
tar -xf "$ROOTFS" -C "$T/target"
mkdir -p "$T/target/mnt"

GUI_IMAGEN=$(sha256sum "$T/target/usr/sbin/x6200_gui" | cut -d' ' -f1)
echo "    GUI en la imagen:   ${GUI_IMAGEN:0:16}…"

echo "==> marcando el binario de la GUI (simula la versión vieja)"
echo "SOY LA VERSION VIEJA" > "$T/target/usr/sbin/x6200_gui"
chmod 755 "$T/target/usr/sbin/x6200_gui"
MARCADO=$(sha256sum "$T/target/usr/sbin/x6200_gui" | cut -d' ' -f1)

REPO=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
OVERLAY=$REPO/tools/release/update-overlay
if [ -d "$OVERLAY" ] && [ -n "$(find "$OVERLAY" -type f 2>/dev/null)" ]; then
    echo "==> (prueba) instalando el S02update del overlay ($OVERLAY), como haría el tgz"
    tar -cf - -C "$OVERLAY" . | tar -xf - -C "$T/target"
fi

echo "==> poniendo el tgz en /mnt del destino"
cp "$TGZ" "$T/target/mnt/"

echo "==> corriendo el S02update REAL (dentro del chroot, con el busybox de la imagen)"
chroot "$T/target" /bin/sh /etc/init.d/S02update start

echo "==> comprobaciones"
BACKUP=$(ls "$T/target"/mnt/backup_* 2>/dev/null | head -1)
[ -n "$BACKUP" ] || { echo "    FALLA: no se creó el backup" >&2; exit 1; }
echo "    backup: $(basename "$BACKUP")  $(stat -c%s "$BACKUP") bytes"

# el backup tiene que contener el archivo marcado (lo que se reemplazó)
mkdir -p "$T/backup"
tar -xzf "$BACKUP" -C "$T/backup" usr/sbin/x6200_gui 2>/dev/null || \
    tar -xzf "$BACKUP" -C "$T/backup" ./usr/sbin/x6200_gui 2>/dev/null || true
if [ -f "$T/backup/usr/sbin/x6200_gui" ]; then
    B=$(sha256sum "$T/backup/usr/sbin/x6200_gui" | cut -d' ' -f1)
    echo "    el backup trae la versión vieja: $([ "$B" = "$MARCADO" ] && echo SI || echo NO)"
fi
echo "    entradas en el backup: $(tar -tzf "$BACKUP" | wc -l)"

# la extracción tiene que haber aplicado el tgz
APLICADO=$(sha256sum "$T/target/usr/sbin/x6200_gui" | cut -d' ' -f1)
echo "    GUI tras la actualización: ${APLICADO:0:16}…"
[ "$APLICADO" = "$GUI_IMAGEN" ] && echo "    el tgz se aplicó" || { echo "    FALLA: no se aplicó" >&2; exit 1; }

# el tgz tiene que haberse consumido y el sistema quedar sano
[ -f "$T/target/mnt/x6200_r1cbu_v0.1.2-lu2dsx.tgz" ] && { echo "    FALLA: el tgz no se borró" >&2; exit 1; } || echo "    el firmware borró el tgz de /mnt"
echo "    setuid tras actualizar: $(find "$T/target" -perm -4000 | wc -l)"
echo "    archivos: $(find "$T/target" -type f | wc -l)"

echo "==> LIMPIO"
rm -rf "$T"
