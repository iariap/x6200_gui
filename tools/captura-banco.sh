#!/bin/bash
#
# Corre la GUI en el banco (chroot de la imagen + QEMU + framebuffer falso) y saca una captura,
# opcionalmente apretando botones o abriendo apps por su socket de control. No usa la radio.
#
#   CHROOT=../.vfix3 tools/captura-banco.sh salida.png                 # solo la home
#   CHROOT=../.vfix3 tools/captura-banco.sh salida.png '{"cmd":"btn","val":0}'
#
# Deja la GUI corriendo entre llamadas (el contenedor se llama banco-apps) para poder encadenar
# capturas; termina con:  docker rm -f banco-apps
#
# SPDX-License-Identifier: GPL-2.0-or-later
set -e

SALIDA=${1:-banco.png}
ORDEN=${2:-}
CHROOT=${CHROOT:-$PWD/../.vfix3}
VOLUMEN=${VOLUMEN:-x6200-build-output}
IMAGEN=${IMAGEN:-x6200-buildroot}
ESPERA=${ESPERA:-6}

[ -d "$CHROOT" ] || { echo "falta el chroot $CHROOT" >&2; exit 1; }
mkdir -p "$CHROOT/mnt" "$CHROOT/dev/shm" "$CHROOT/tmp"

# la GUI y las libs del banco
docker run --rm --entrypoint "" -v "$CHROOT:/t" -v "$VOLUMEN":/b "$IMAGEN" \
    sh -c 'cp /b/target/usr/sbin/x6200_gui /t/usr/sbin/x6200_gui' 2>/dev/null
cp "$(dirname "$0")/../../trace/libfakefb.so" "$(dirname "$0")/../../trace/libx6200trace.so" \
   "$(dirname "$0")/../../trace/libcrash.so" "$CHROOT/lib/" 2>/dev/null || true

if ! docker ps --filter name=banco-apps --format '{{.Names}}' | grep -q banco-apps; then
    docker rm -f banco-apps >/dev/null 2>&1 || true
    rm -f "$CHROOT/tmp/fakefb0" "$CHROOT/crash.txt"
    docker run -d --name banco-apps --rm --entrypoint="" -v "$CHROOT:/t" "$IMAGEN" \
        sh -c 'chroot /t /bin/busybox env LD_PRELOAD=/lib/libfakefb.so:/lib/libcrash.so:/lib/libx6200trace.so /usr/sbin/x6200_gui > /t/out-banco.txt 2>&1' >/dev/null
    sleep 12
fi

# la orden, por el socket de control de la GUI. Se usa el cliente de la radio DENTRO del chroot
# (corre como root): el socket queda de nobody con permisos 600 y desde el host no se puede.
if [ -n "$ORDEN" ]; then
    cp "$(dirname "$0")/remote/x6200-remote" "$CHROOT/tmp/x6200-remote" 2>/dev/null || true
    chmod 755 "$CHROOT/tmp/x6200-remote" 2>/dev/null || true
    docker run --rm --entrypoint "" -v "$CHROOT:/t" "$IMAGEN" \
        sh -c "chroot /t /tmp/x6200-remote '$ORDEN'" 2>&1 | sed 's/^/  /'
    sleep "$ESPERA"
fi

python3 - "$CHROOT/tmp/fakefb0" "$SALIDA" <<'PY'
import sys
from PIL import Image
raw = open(sys.argv[1], 'rb').read()
W, H = 480, 800
img = Image.new('RGB', (W, H)); px = img.load()
for y in range(H):
    for x in range(W):
        i = (y * W + x) * 4
        px[x, y] = (raw[i+2], raw[i+1], raw[i])
img.transpose(Image.ROTATE_270).save(sys.argv[2])
print("  captura: %s" % sys.argv[2])
PY
