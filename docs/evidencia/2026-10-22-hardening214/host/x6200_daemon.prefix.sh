#!/bin/sh
mkdir -p /mnt/app_logs/
# Los assets (4,4 MB) los lee la GUI de /dev/shm/images. Antes se copiaban ahi en cada
# arranque: esa copia cuesta ~0,24 s y 4,4 MB de RAM. Un bind mount es instantaneo, asi que
# se engancha la carpeta de la raiz; si el bind falla (kernel sin soporte) se copia como
# antes, que es lo que se hacia.
mkdir -p /dev/shm/images
if ! mountpoint -q /dev/shm/images; then
    mount --bind /usr/share/x6200/images /dev/shm/images 2>/dev/null || {
        rm -rf /dev/shm/images
        cp -R /usr/share/x6200/images /dev/shm/
    }
fi
while true; do
    /usr/sbin/x6200_gui > /tmp/x6200_log.txt 2>&1 || sync; mv /tmp/x6200_log.txt /mnt/app_logs/x6200_log_$(date +%Y-%m-%d_%H-%M-%S).txt
done
