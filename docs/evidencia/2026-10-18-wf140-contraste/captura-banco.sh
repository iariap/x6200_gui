#!/bin/bash
#
# Captura de una pantalla del banco QEMU con un binario EXACTO y un estado
# (tema + waterfall_center_line) dado. Evidencia del issue #140 (la linea central
# del waterfall).
#
#   ./captura-banco.sh <salida.png> <tema> <on|off> <binario>
#
# A diferencia de tools/captura-banco.sh, este script NO rebuilda: usa el binario
# que le pasan (el mismo que dejo la reconstruccion, con su md5 anotado) para que
# las capturas sean atribuibles a un unico binario.
#
# Toda espera es hasta-condicion con techo (AGENTS.md: nada de sleep a ciegas):
#   1. el socket de control aparece;
#   2. la GUI responde {"cmd":"get"};
#   3. un cambio de frecuencia obliga a repintar (el framebuffer cambia);
#   4. el aviso de arranque ("X6200 de R1CBU ...") sale de pantalla;
#   5. con la linea ENCENDIDA, la tinta del rol `text` del tema se mide en la
#      columna central (x=398..400) del framebuffer: la captura sale con la linea
#      ya dibujada, no "cuando parezca". Con la linea APAGADA la misma medicion
#      tiene que dar cero (control): si aparece tinta, el script falla.
#
# set -u (no -e): queremos imprimir el estado aunque algo falle, y salir con
# error si no hay captura.
set -u

SALIDA=$1
TEMA=$2
LINEA=$3
BINARIO=$4

CH=${CHROOT:-/tmp/opencode/vfix3-wf140}
IMG=${IMAGEN:-x6200-buildroot}
REPO_RAIZ=$(cd "$(dirname "$0")/../../.." && pwd)   # <repo>/docs/evidencia/<run>/ -> <repo>
CLIENTE="chroot /t /tmp/x6200-remote"
TECHO=${TECHO:-30}                      # techo de cada espera, en segundos
FB="$CH/tmp/fakefb0"
BANDA="240 478"                         # filas del waterfall en el framebuffer 800x480
MIN_TINTA=${MIN_TINTA:-100}             # px de tinta en la columna: la linea de 3 px tiene ~673

case "$LINEA" in
    on)  sqlite3 "$CH/mnt/params.db" "delete from params where name='waterfall_center_line';" ;;
    off) sqlite3 "$CH/mnt/params.db" "insert or replace into params(name,val) values('waterfall_center_line',0);" ;;
    *)   echo "estado de linea invalido: $LINEA (on|off)" >&2; exit 2 ;;
esac
sqlite3 "$CH/mnt/params.db" "update params set val='$TEMA' where name='theme_name';"

# La tinta esperada es el rol `text` del tema (src/waterfall.c: la linea usa
# lv_color_hex(theme_hex(TC_text))): sale del propio archivo del tema, no a mano.
TINTA=$(python3 - "$REPO_RAIZ/rootfs/usr/share/x6200/themes/$TEMA.json" <<'PY'
import json, sys
print(json.load(open(sys.argv[1]))["colors"]["text"].lstrip("#").lower())
PY
) || { echo "no pude leer el rol text de $TEMA" >&2; exit 1; }
echo "tinta del tema $TEMA (rol text): #$TINTA"

echo "== estado del chroot =="
sqlite3 "$CH/mnt/params.db" "select name,val from params where name in ('theme_name','waterfall_center_line');"

docker rm -f banco-apps >/dev/null 2>&1 || true
cp "$BINARIO" "$CH/usr/sbin/x6200_gui" || { echo "no pude copiar el binario" >&2; exit 1; }
echo "binario: md5 $(md5sum "$CH/usr/sbin/x6200_gui" | cut -d' ' -f1) ($(stat -c %s "$CH/usr/sbin/x6200_gui") B)"
python3 "$REPO_RAIZ/tools/verifica-firmware-port.py" "$CH/usr/sbin/x6200_gui" || exit 1

rm -f "$FB" "$CH/tmp/x6200_remote.sock" "$CH/crash.txt"
docker run -d --name banco-apps --rm --entrypoint="" -e QEMU_CPU=cortex-a7 -v "$CH:/t" "$IMG" \
    sh -c "chroot /t /bin/busybox env LD_PRELOAD=/lib/libfakefb.so:/lib/libcrash.so:/lib/libx6200trace.so /usr/sbin/x6200_gui > /t/out-banco.txt 2>&1" >/dev/null \
    || { echo "no arranco el contenedor" >&2; exit 1; }

# mide la tinta en la columna central del framebuffer rotado (ROTATE_270, como la
# captura): el pixel (x,y) de la imagen sale del byte ((800-1-x)*480 + y)*4 del
# framebuffer crudo, en orden B,G,R,A. Devuelve la cantidad de px de tinta.
medir_tinta() {
    python3 - "$FB" "$TINTA" "$LINEA" "$BANDA" "$MIN_TINTA" <<'PY'
import sys
raw = open(sys.argv[1], "rb").read()
tinta_hex, estado, (y0, y1), minimo = sys.argv[2], sys.argv[3], [int(v) for v in sys.argv[4].split()], int(sys.argv[5])
tinta = bytes(int(tinta_hex[i:i + 2], 16) for i in (4, 2, 0))     # B,G,R
n = 0
for x in range(398, 401):
    base = (800 - 1 - x) * 480
    for y in range(y0, y1 + 1):
        i = (base + y) * 4
        if raw[i:i + 3] == tinta:
            n += 1
print(n)
sys.exit(0 if (n >= minimo) == (estado == "on") else 1)
PY
}

# espera a que el aviso de arranque (src/msg.c: `msg.bin`, caja de 603x67 en
# x=98 y=300 -- src/styles.c:195-196 -- sobre el waterfall) haya salido de
# pantalla. Sobre el piso negro del banco el velo del aviso deja pixeles no
# negros en toda la caja; sin aviso, la unica tinta ahi son las columnas de la
# linea (x=398..400). Ademas de sacar el cartel de la captura, deja el waterfall
# quieto: el aviso se dibuja con una animacion de opacidad y sus frames
# repintan la zona.
aviso_fuera() {
    python3 - "$FB" <<'PY'
import sys
raw = open(sys.argv[1], "rb").read()
W, H = 480, 800
no_negros = 0
for x in range(98, 702):
    if 396 <= x <= 402:                  # las columnas de la linea central
        continue
    base = (H - 1 - x) * W
    for y in range(300, 367):
        i = (base + y) * 4
        if raw[i] or raw[i + 1] or raw[i + 2]:
            no_negros += 1
print("pixeles no negros en la caja del aviso: %d" % no_negros)
sys.exit(0 if no_negros == 0 else 1)
PY
}

# hasta-condicion con techo: repite `cmd` cada segundo hasta que sale 0.
esperar() {
    local desc=$1; shift
    local inicio=$SECONDS fin=$((SECONDS + TECHO))
    printf 'esperando: %s' "$desc"
    until "$@" >/tmp/wf140-espera.out 2>&1; do
        if [ "$SECONDS" -ge "$fin" ]; then
            echo " -> TIMEOUT ($TECHO s)" >&2
            cat /tmp/wf140-espera.out >&2
            tail -5 "$CH/out-banco.txt" 2>/dev/null | sed 's/^/    | /' >&2
            return 1
        fi
        sleep 1
    done
    echo " -> ok ($((SECONDS - inicio)) s)"
}

gui_responde() {
    docker run --rm --entrypoint "" -v "$CH:/t" "$IMG" sh -c "$CLIENTE '{\"cmd\":\"get\"}'" 2>/dev/null \
        | grep -q '"freq"'
}

esperar "el socket de control" test -S "$CH/tmp/x6200_remote.sock" || exit 1
esperar "la respuesta de {\"cmd\":\"get\"}" gui_responde || exit 1
echo "get: $(docker run --rm --entrypoint "" -v "$CH:/t" "$IMG" sh -c "$CLIENTE '{\"cmd\":\"get\"}'" 2>&1 | head -c 200)"

# Un cambio de frecuencia obliga a repintar espectro y waterfall: sin eso, el
# primer frame que dibujo la linea puede quedar en zonas que nadie vuelve a pintar
# (el waterfall sin radio no recibe filas nuevas). La espera es "el framebuffer
# cambio", no un tiempo fijo.
FB_ANTES=$(md5sum "$FB" | cut -d' ' -f1)
docker run --rm --entrypoint "" -v "$CH:/t" "$IMG" sh -c "$CLIENTE '{\"cmd\":\"freq\",\"val\":14074000}'" >/dev/null 2>&1
fb_cambio() { [ "$(md5sum "$FB" | cut -d' ' -f1)" != "$FB_ANTES" ]; }
esperar "el repintado despues del cambio de frecuencia" fb_cambio || exit 1

esperar "que el aviso de arranque salga de pantalla" aviso_fuera || exit 1
esperar "la medicion de la tinta en la columna central (estado $LINEA)" medir_tinta || exit 1
echo "tinta #$TINTA en x=398..400 (y=$BANDA): $(medir_tinta) px (estado $LINEA)"

python3 - "$FB" "$SALIDA" <<'PY' || exit 1
import sys
from PIL import Image
raw = open(sys.argv[1], 'rb').read()
W, H = 480, 800
if len(raw) < W * H * 4:
    sys.exit("framebuffer corto: %d bytes" % len(raw))
img = Image.new('RGB', (W, H)); px = img.load()
for y in range(H):
    for x in range(W):
        i = (y * W + x) * 4
        px[x, y] = (raw[i + 2], raw[i + 1], raw[i])
img.transpose(Image.ROTATE_270).save(sys.argv[2])
print("captura: %s" % sys.argv[2])
PY

echo "md5 final del binario en el chroot: $(md5sum "$CH/usr/sbin/x6200_gui" | cut -d' ' -f1)"
docker rm -f banco-apps >/dev/null 2>&1 || true
echo "contenedores banco-apps vivos al cerrar: $(docker ps -q --filter name=^banco-apps$ | wc -l)"
