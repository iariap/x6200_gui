#!/bin/sh
#
# Captura la pantalla de la radio por SSH: lee el framebuffer (/dev/fb0, 480x800x32 BGRA), lo
# rota 270 grados como en el banco y guarda un PNG. Opcionalmente recorta y amplia una franja:
#
#   tools/captura-radio.sh salida.png
#   tools/captura-radio.sh franja.png 0 390 800 480     # x1 y1 x2 y2 (en la imagen ya rotada)
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -e

IP=${RADIO_IP:-192.168.0.236}
SALIDA=${1:-captura.png}
FB=$(mktemp)

sshpass -p 123 ssh -F /dev/null -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
    -o ConnectTimeout=10 -o LogLevel=ERROR -o PreferredAuthentications=password \
    -o PubkeyAuthentication=no "root@$IP" 'dd if=/dev/fb0 bs=1920 count=800 2>/dev/null' > "$FB"

FB="$FB" SALIDA="$SALIDA" X1="$2" Y1="$3" X2="$4" Y2="$5" python3 - <<'PY'
import os
from PIL import Image

W, H = 480, 800
raw = open(os.environ['FB'], 'rb').read()
assert len(raw) == W * H * 4, "el framebuffer no mide 480x800x32 (%d bytes)" % len(raw)

img = Image.new('RGB', (W, H))
px = img.load()
for y in range(H):
    base = y * W * 4
    for x in range(W):
        i = base + x * 4
        px[x, y] = (raw[i + 2], raw[i + 1], raw[i])   # BGRA -> RGB

out = img.transpose(Image.ROTATE_270)
if os.environ.get('X1'):
    out = out.crop((int(os.environ['X1']), int(os.environ['Y1']), int(os.environ['X2']), int(os.environ['Y2'])))
out.save(os.environ['SALIDA'])
print("%s: %s" % (os.environ['SALIDA'], out.size))
PY
rm -f "$FB"
