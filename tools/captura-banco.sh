#!/bin/bash
#
# Banco de pruebas SIN radio: corre el x6200_gui real (el binario ARM de la imagen) con
# qemu-arm-static, dentro de un chroot del rootfs y con framebuffer falso, y lo comanda por su
# socket de control. Saca una captura de la pantalla.
#
#   tools/captura-banco.sh --check                     # verifica todo y dice que falta (no toca nada)
#   CHROOT=../.vfix3 tools/captura-banco.sh salida.png
#   CHROOT=../.vfix3 tools/captura-banco.sh salida.png '{"cmd":"app","text":"Status"}'
#   CHROOT=../.vfix3 tools/captura-banco.sh salida.png '{"cmd":"freq","val":14074000}'
#
# Garantias (ver .agents/pruebas-sin-radio/SKILL.md):
#   - Prueba la ULTIMA version: reconstruye si el hash de los fuentes cambio, si hay cambios sin
#     commitear o si un fuente es mas nuevo que el binario (RECONSTRUIR=auto|si|no). Imprime commit
#     y md5 del binario que corrio.
#   - Usa el socket CORRECTO: borra socket/framebuffer viejos, espera el socket, hace handshake
#     {"cmd":"get"}, comprueba que la GUI este viva y reintenta reiniciando el banco una vez.
#   - No deja evidencia falsa: si la captura sale vacia (o el framebuffer es uniforme) falla.
#
# QEMU_CPU (por defecto cortex-a7, el core de la R16) es necesario: con el modelo por defecto la GUI
# muere a los pocos segundos con "qemu-arm-static: QEMU internal SIGBUS {code=ADERR}".
#
# Deja la GUI corriendo entre llamadas (contenedor banco-apps) para encadenar capturas; se termina
# con: docker rm -f banco-apps
#
# SPDX-License-Identifier: GPL-2.0-or-later
set -u

AQUI=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$AQUI/.." && pwd)

SALIDA=${1:-banco.png}
ORDEN=${2:-}
CHROOT=${CHROOT:-$REPO/../.vfix3}
VOLUMEN=${VOLUMEN:-x6200-build-output}
IMAGEN=${IMAGEN:-x6200-buildroot}
BUILDROOT=${BUILDROOT:-$REPO/../buildroot_new/AetherX6200Buildroot}
ESPERA=${ESPERA:-6}
RECONSTRUIR=${RECONSTRUIR:-auto}
QEMU_CPU=${QEMU_CPU:-cortex-a7}
MINIMO_PNG=${MINIMO_PNG:-4000}          # menos que esto = captura vacia
BIN=/usr/sbin/x6200_gui
MODO_CHECK=no
[ "${1:-}" = "--check" ] && MODO_CHECK=si

fallar() { echo "ERROR: $*" >&2; exit 1; }
nota()   { echo "  $*"; }
cola_log() { [ -f "$CHROOT/out-banco.txt" ] && tail -5 "$CHROOT/out-banco.txt" | sed 's/^/    | /'; }
docker_ok() { docker "$1" >/dev/null 2>&1; }

# --- 0. comprobaciones previas: que exista todo lo que hace falta, con mensajes claros -----------
FALTAN=0
falta() { echo "  FALTA: $*"; FALTAN=$((FALTAN+1)); }
echo "== comprobando el banco =="

docker_ok "info"                       && nota "docker: ok" || falta "docker no responde"
docker image inspect "$IMAGEN" >/dev/null 2>&1      && nota "imagen $IMAGEN: ok"    || falta "falta la imagen $IMAGEN"
docker volume inspect "$VOLUMEN" >/dev/null 2>&1    && nota "volumen $VOLUMEN: ok"  || falta "falta el volumen $VOLUMEN (build del GUI)"
[ -d "$CHROOT" ]                    && nota "chroot $CHROOT: ok"          || falta "falta el chroot ($CHROOT): extraer el rootfs de la imagen, ver docs/TESTING.md nivel 4"
[ -x "$CHROOT/bin/busybox" ]        || [ -x "$CHROOT/bin/sh" ]            || falta "el chroot no parece un rootfs (sin /bin/busybox)"
[ -d "$CHROOT/lib" ]                                                       || falta "el chroot no tiene /lib"
command -v python3 >/dev/null       && nota "python3: ok"                 || falta "falta python3 (para rotar el framebuffer)"
python3 -c "import PIL" 2>/dev/null && nota "PIL: ok"                     || falta "falta Pillow (pip install pillow)"
[ -f "$AQUI/remote/x6200-remote" ]  && nota "cliente de control: ok"      || falta "falta $AQUI/remote/x6200-remote (compilar con $AQUI/remote/compila.sh)"
if [ "$RECONSTRUIR" != "no" ]; then
    [ -d "$BUILDROOT" ] && nota "buildroot: ok" || nota "aviso: sin $BUILDROOT no se puede reconstruir (RECONSTRUIR=no para saltearlo)"
fi
BENCH_FALTA=
for l in libfakefb.so libcrash.so libx6200trace.so; do [ -f "$AQUI/bench/$l" ] || BENCH_FALTA="$BENCH_FALTA $l"; done
if [ -n "$BENCH_FALTA" ]; then
    if [ -x "$AQUI/bench/compila.sh" ] && docker_ok "info"; then
        nota "compilando libs del banco que faltan:$BENCH_FALTA"
        "$AQUI/bench/compila.sh" >/dev/null 2>&1 || falta "no se pudieron compilar las libs del banco ($AQUI/bench/compila.sh)"
    else
        falta "faltan las libs del banco:$BENCH_FALTA (correr $AQUI/bench/compila.sh)"
    fi
fi
[ -f "$AQUI/bench/libfakefb.so" ] && nota "libs del banco: ok"

if [ "$FALTAN" -gt 0 ]; then
    echo "== $FALTAN cosa(s) faltan: el banco no puede correr ==" >&2
    exit 1
fi

# qemu utilizable: se prueba con el busybox del chroot, adentro del contenedor
if ! docker run --rm --entrypoint "" -v "$CHROOT:/t" "$IMAGEN" sh -c 'chroot /t /bin/busybox true' >/dev/null 2>&1; then
    falta "qemu-arm-static no puede ejecutar el chroot (¿binfmt_misc cargado?)"
    echo "== no se puede ejecutar ARM: probar 'ls /proc/sys/fs/binfmt_misc' en el host ==" >&2
    exit 1
fi
nota "qemu-arm-static ejecutando ARM: ok"

if [ "$MODO_CHECK" = "si" ]; then
    echo "== todo listo para el banco (no se toco nada) =="
    docker image inspect "$IMAGEN" --format '  imagen: {{.Id}}' 2>/dev/null | cut -c1-40
    echo "  version del repo: $(git -C "$REPO" log -1 --format='%h %s' 2>/dev/null | cut -c1-60)"
    [ -n "$(git -C "$REPO" status --porcelain -- src lvgl/src 2>/dev/null)" ] && echo "  aviso: hay cambios sin commitear en src/ (se reconstruye antes de probar)"
    echo "  capturas mas chicas que $MINIMO_PNG bytes se consideran vacias"
    exit 0
fi

mkdir -p "$CHROOT/mnt" "$CHROOT/dev/shm" "$CHROOT/tmp"

# --- 1. ultima version: hash de los fuentes + cambios sin commitear + timestamps ----------------
hash_fuentes() {
    { find "$REPO/src" "$REPO/lvgl/src" -type f \( -name '*.c' -o -name '*.cpp' -o -name '*.h' \) -print0 2>/dev/null \
        | sort -z | xargs -0 md5sum 2>/dev/null; } | md5sum | cut -d' ' -f1
}
SELLO="$CHROOT/.banco-fuente.md5"
HASH=$(hash_fuentes)
SUCIO=$(git -C "$REPO" status --porcelain -- src lvgl/src 2>/dev/null | head -1)
MAS_NUEVO=$(docker run --rm --entrypoint "" -v "$VOLUMEN":/b -v "$REPO":/g:ro "$IMAGEN" sh -c "
    [ -f /b/target$BIN ] || { echo FALTA; exit 0; }
    find /g/src /g/lvgl/src -newer /b/target$BIN \\( -name '*.c' -o -name '*.cpp' -o -name '*.h' \\) 2>/dev/null | head -1" 2>/dev/null || true)
SELLO_VIEJO=$(cat "$SELLO" 2>/dev/null || echo "")
MOTIVO=
[ "$SELLO_VIEJO" != "$HASH" ] && MOTIVO="el hash de los fuentes cambio"
[ -n "$SUCIO" ] && MOTIVO="${MOTIVO:+$MOTIVO y }hay cambios sin commitear"
[ -n "$MAS_NUEVO" ] && MOTIVO="${MOTIVO:+$MOTIVO y }un fuente es mas nuevo que el binario"
if [ "$RECONSTRUIR" = "si" ]; then MOTIVO="RECONSTRUIR=si"; fi

if [ -n "$MOTIVO" ] && [ "$RECONSTRUIR" != "no" ]; then
    [ -d "$BUILDROOT" ] || fallar "hay que reconstruir ($MOTIVO) y no esta $BUILDROOT (usar RECONSTRUIR=no para probar el binario que ya esta)"
    echo "  reconstruyendo la GUI: $MOTIVO"
    docker run --rm --entrypoint "" \
        -v "$BUILDROOT":/workspace -v "$VOLUMEN":/workspace/build -v "$REPO":/workspace_gui:ro \
        -e FORCE_UNSAFE_CONFIGURE=1 -w /workspace/build "$IMAGEN" \
        make x6200-gui-rebuild -j"$(nproc)" >/dev/null 2>&1 || fallar "fallo la reconstruccion (make x6200-gui-rebuild)"
    echo "$HASH" > "$SELLO"
elif [ -n "$MOTIVO" ]; then
    echo "  aviso: $MOTIVO, pero RECONSTRUIR=no: se prueba el binario que ya esta"
fi

# --- 2. el binario y las libs del banco --------------------------------------------------------
docker run --rm --entrypoint "" -v "$CHROOT:/t" -v "$VOLUMEN":/b "$IMAGEN" \
    sh -c "cp /b/target$BIN /t$BIN" 2>/dev/null || fallar "no pude copiar el binario desde el volumen $VOLUMEN (¿corriste make?)"
[ -x "$CHROOT$BIN" ] || fallar "quedo sin permiso de ejecucion $CHROOT$BIN"
cp "$AQUI/bench/libfakefb.so" "$AQUI/bench/libcrash.so" "$AQUI/bench/libx6200trace.so" "$CHROOT/lib/" || fallar "no pude copiar las libs del banco al chroot"
cp "$AQUI/remote/x6200-remote" "$CHROOT/tmp/x6200-remote" || fallar "no pude copiar el cliente de control al chroot"
chmod 755 "$CHROOT/tmp/x6200-remote"

# --- 3. arrancar la GUI (una sola vez por contenedor) ------------------------------------------
arrancar_banco() {
    docker rm -f banco-apps >/dev/null 2>&1 || true
    # socket y framebuffer viejos: si quedan, cualquier espera pasa de largo y la captura sale vacia
    rm -f "$CHROOT/tmp/fakefb0" "$CHROOT/crash.txt" "$CHROOT/tmp/x6200_remote.sock"
    docker run -d --name banco-apps --rm --entrypoint="" -e "QEMU_CPU=$QEMU_CPU" -v "$CHROOT:/t" "$IMAGEN" \
        sh -c "chroot /t /bin/busybox env LD_PRELOAD=/lib/libfakefb.so:/lib/libcrash.so:/lib/libx6200trace.so $BIN > /t/out-banco.txt 2>&1" >/dev/null \
        || fallar "no pude arrancar el contenedor banco-apps"
    for i in $(seq 1 90); do [ -S "$CHROOT/tmp/x6200_remote.sock" ] && break; sleep 1; done
    [ -S "$CHROOT/tmp/x6200_remote.sock" ] || { echo "la GUI no creo el socket en 90 s:"; cola_log; return 1; }
}
CLIENTE="chroot /t /tmp/x6200-remote"
preguntar() { docker run --rm --entrypoint "" -v "$CHROOT:/t" "$IMAGEN" sh -c "$CLIENTE '{\"cmd\":\"get\"}'" 2>&1 | head -c 200; }
# la GUI esta viva si contesta el handshake: no hace falta pidof (y en busybox no existe)

if ! docker ps --filter name=banco-apps --format '{{.Names}}' | grep -q banco-apps; then
    arrancar_banco || true
fi

# handshake + GUI viva, con un reinicio de reintento
RESP=$(preguntar || true)
if ! echo "$RESP" | grep -q '"t"'; then
    echo "  el banco no contesta o la GUI no esta viva; reiniciando..."
    arrancar_banco || true
    RESP=$(preguntar || true)
fi
if ! echo "$RESP" | grep -q '"t"'; then
    echo "el socket no responde {\"cmd\":\"get\"}: $RESP" >&2; cola_log; exit 1
fi

# --- 4. la orden -------------------------------------------------------------------------------
if [ -n "$ORDEN" ]; then
    R=$(docker run --rm --entrypoint "" -v "$CHROOT:/t" "$IMAGEN" sh -c "$CLIENTE '$ORDEN'" 2>&1)
    echo "$R" | sed 's/^/  respuesta: /'
    echo "$R" | grep -q '"t":"err"' && nota "aviso: la GUI contesto un error a la orden (arriba)"
    sleep "$ESPERA"
fi

# --- 5. captura: el framebuffer falso es 480x800x32 BGRA, se rota 270 como en la radio ---------
[ -s "$CHROOT/tmp/fakefb0" ] || { echo "no hay framebuffer ($CHROOT/tmp/fakefb0)" >&2; cola_log; exit 1; }
python3 - "$CHROOT/tmp/fakefb0" "$SALIDA" <<'PY' || exit 1
import sys
from PIL import Image
raw = open(sys.argv[1], 'rb').read()
W, H = 480, 800
if len(raw) < W*H*4:
    sys.exit("el framebuffer es mas chico de lo esperado (%d bytes)" % len(raw))
img = Image.new('RGB', (W, H)); px = img.load()
for y in range(H):
    for x in range(W):
        i = (y * W + x) * 4
        px[x, y] = (raw[i+2], raw[i+1], raw[i])
img.transpose(Image.ROTATE_270).save(sys.argv[2])
colores = img.getcolors(maxcolors=1 << 20) or []
print("  captura: %s (%d colores distintos)" % (sys.argv[2], len(colores)))
PY

# --- 6. que se probo, y que la captura no sea basura -------------------------------------------
TAM=$(stat -c %s "$SALIDA" 2>/dev/null || echo 0)
echo "  version: $(git -C "$REPO" log -1 --format='%h %s' 2>/dev/null | cut -c1-60)"
echo "  binario: md5 $(md5sum "$CHROOT$BIN" | cut -c1-12)   qemu-cpu: $QEMU_CPU"
if [ "$TAM" -lt "$MINIMO_PNG" ]; then
    echo "ERROR: la captura parece vacia ($TAM bytes < $MINIMO_PNG): la GUI no dibujo la pantalla" >&2
    echo "  (si es la primera captura, subir ESPERA; el banco muere solo despues de un rato)" >&2
    cola_log; exit 1
fi
nota "captura util ($TAM bytes)"
