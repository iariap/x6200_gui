#!/bin/sh
#
# Mide el reparto de tiempos del arranque de la GUI en el banco: la corre en un chroot de la
# imagen (sin radio) con pantalla falsa y muestra las marcas de /mnt/app_logs/boot.log, que
# traen cuanto duro cada etapa.
#
#   CHROOT=/ruta/al/chroot ./tools/mide-arranque-banco.sh [segundos]
#
# El banco corre con QEMU, asi que los segundos absolutos NO son los de la radio: sirve para
# saber QUE etapa se lleva el tiempo y para atribuir, no para afirmar cuanto mejora. Los tramos
# que en la radio son hardware (abrir los /dev/input/event*) aca dan casi cero, porque los
# resuelve el sistema de archivos del host: por eso los 2 s del arranque no aparecian aca.
#
# El chroot tiene que tener la GUI y las libs de la imagen; en este proyecto se arma con
# captura-banco.sh (ver docs/HALLAZGOS.md §8).
set -e

SEGUNDOS=${1:-45}
T="${CHROOT:-$PWD/.vfix3}"
VOLUMEN="${VOLUMEN:-x6200-build-output}"
IMAGEN="${IMAGEN:-x6200-buildroot}"

[ -d "$T" ] || { echo "falta el chroot $T (pasalo en CHROOT=...)" >&2; exit 1; }

mkdir -p "$T/mnt/app_logs" "$T/dev/shm" "$T/tmp"
rm -f "$T/mnt/app_logs/boot.log"

docker rm -f banco-arranque >/dev/null 2>&1 || true
docker run -d --name banco-arranque --rm --entrypoint="" \
    -v "$T:/t" -v "$VOLUMEN":/b "$IMAGEN" \
    sh -c 'cp /b/target/usr/sbin/x6200_gui /t/usr/sbin/x6200_gui; chroot /t /bin/busybox env LD_PRELOAD=/lib/libfakefb.so:/lib/libcrash.so:/lib/libx6200trace.so /usr/sbin/x6200_gui > /t/out-banco.txt 2>&1' >/dev/null

sleep "$SEGUNDOS"
docker rm -f banco-arranque >/dev/null 2>&1 || true

echo "== marcas del arranque (banco) =="
cat "$T/mnt/app_logs/boot.log" 2>/dev/null || echo "(no quedo ninguna marca)"
