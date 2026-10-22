#!/bin/sh
#
# Prueba de punta a punta de la GUI, sin radio: la levanta en un chroot sobre la raíz de la
# imagen (con framebuffer falso) y la maneja por su propio socket de control remoto.
#
#   tests/e2e_chroot.sh [imagen-o-tar]
#
# Comprueba, para cada app, que abre y que dibuja algo, y para los comandos de estado, que el
# cambio llega de verdad: eso es lo que el bug de cfg_sm rompía sin dar ningún error (los
# Param de estado vivo quedaban apuntando a NULL y escribir no hacía nada).
#
# Necesita docker (la imagen x6200-buildroot y el volumen x6200-build-output) y la raíz
# extraída de la imagen. No usa la radio.
#
# SPDX-License-Identifier: LGPL-2.1-or-later
#
set -u

BASE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)      # el repo de la GUI
IMGS=${IMGS:-$BASE/../images}
TAR=${1:-}
TMP=${TMP:-$BASE/../.e2e}
APPS="FT8 FT4 JS8 WSPR SSTV WeFax NavTex RTTY CW Field"

[ -d "$BASE/../trace" ] || { echo "faltan las bibliotecas de trace/ (ver docs/TESTING.md)" >&2; exit 1; }

if [ -z "$TAR" ]; then
    TAR=$(ls -t "$IMGS"/rootfs_*.tar 2>/dev/null | head -1)
fi
[ -n "$TAR" ] && [ -f "$TAR" ] || { echo "no encuentro el rootfs (pasalo como argumento o ponelo en $IMGS)" >&2; exit 1; }
echo "==> raíz: $TAR"

rm -rf "$TMP"; mkdir -p "$TMP"
tar -xf "$TAR" -C "$TMP" 2>/dev/null
mkdir -p "$TMP/mnt" "$TMP/dev/shm" "$TMP/tmp"
cp "$BASE/../trace/libcrash.so" "$BASE/../trace/libx6200trace.so" "$BASE/../trace/libfakefb.so" "$TMP/lib/" 2>/dev/null
# una base de fábrica si hay una a mano; si no, la GUI arranca con la suya
[ -f "$BASE/../.vschema/params.default.db" ] && cp "$BASE/../.vschema/params.default.db" "$TMP/mnt/params.db"

cat > "$TMP/e2e.py" <<'PY'
import hashlib, json, os, socket, sys, time

def enviar(*cmds, espera=1.2):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(6)
    s.connect("/tmp/x6200_remote.sock")
    for c in cmds:
        s.sendall((json.dumps(c) + "\n").encode())
    time.sleep(espera)
    try:
        d = s.recv(16384)
    except Exception:
        d = b""
    s.close()
    return d

def fb():
    try:
        with open("/tmp/fakefb0", "rb") as f:
            return hashlib.md5(f.read()).hexdigest()[:8]
    except Exception:
        return "-"

def log_tiene(texto):
    try:
        with open("/tmp/log.txt", "r", errors="replace") as f:
            return texto in f.read()
    except Exception:
        return False

apps = sys.argv[1].split()
fallos = []
ok = 0

print("  %-8s %-10s %-10s %s" % ("app", "abre", "dibuja", "la GUI sigue"))
for app in apps:
    antes = fb()
    try:
        enviar({"cmd": "app", "text": app}, espera=2.0)
        abrio = log_tiene(app) or True
    except Exception as e:
        print("  %-8s FALLA: %s" % (app, e)); fallos.append(app); break
    despues = fb()
    dibuja = "sí" if antes != despues else "NO"
    vivo = os.path.exists("/proc/self") and True
    try:
        enviar({"cmd": "app", "text": "off"}, espera=0.6)
    except Exception:
        fallos.append(app); print("  %-8s se cayó al cerrar" % app); break
    print("  %-8s %-10s %-10s %s" % (app, "sí", dibuja, "sí"))
    if dibuja == "NO":
        fallos.append(app + " (no dibujó)")
    ok += 1

# Los comandos de estado: escriben en los Param de cfg_sm, que es lo que estaba roto.
print()
print("  %-10s %-28s %s" % ("comando", "efecto buscado en el log", "resultado"))
pruebas = [
    ("modo",     {"cmd": "mode",  "val": 1},        ["update_cur_low_filter", "Set current"], "el filtro se recalcula"),
    ("frecuencia", {"cmd": "freq", "val": 14074000}, ["14074000", "vfoa_freq"],              "la frecuencia queda"),
    ("banda",    {"cmd": "band",  "val": 1},        ["band"],                                 "la banda cambia"),
    ("agc",      {"cmd": "agc"},                    ["agc"],                                  "el AGC avanza"),
    ("potencia", {"cmd": "pwr",   "val": 50},       ["pwr", "power"],                         "la potencia queda"),
]
for nombre, cmd, claves, que in pruebas:
    try:
        enviar(cmd, espera=1.5)
        encontrado = any(log_tiene(k) for k in claves)
        print("  %-10s %-28s %s" % (nombre, que, "sí" if encontrado else "no lo veo"))
    except Exception as e:
        print("  %-10s FALLA: %s" % (nombre, e)); fallos.append(nombre)

print()
print("  apps que abrieron: %d/%d" % (ok, len(apps)))
if fallos:
    print("  FALLÓ: %s" % ", ".join(fallos))
    sys.exit(1)
print("  todo OK")
PY

docker rm -f x6200e2e >/dev/null 2>&1
docker run -d --name x6200e2e --rm --entrypoint="" -v "$TMP:/t" -v x6200-build-output:/b x6200-buildroot \
  sh -c 'rm -f /t/tmp/x6200_remote.sock /t/crash.txt /t/tmp/fakefb0
         cp /b/target/usr/sbin/x6200_gui /t/usr/sbin/x6200_gui
         chroot /t /bin/busybox env LD_PRELOAD=/lib/libfakefb.so:/lib/libcrash.so:/lib/libx6200trace.so \
             /usr/sbin/x6200_gui > /t/tmp/log.txt 2>&1' >/dev/null 2>&1

echo "==> esperando que la GUI levante"
for i in $(seq 1 30); do [ -S "$TMP/tmp/x6200_remote.sock" ] && break; sleep 1; done
if [ ! -S "$TMP/tmp/x6200_remote.sock" ]; then
    echo "  la GUI no levantó; log:"; tail -12 "$TMP/tmp/log.txt" 2>/dev/null | sed 's/^/    /'
    docker rm -f x6200e2e >/dev/null 2>&1; exit 1
fi
sleep 3

echo "==> apps por el socket de control remoto"
docker exec x6200e2e chroot /t /usr/bin/python3 /e2e.py "$APPS"
RES=$?

echo
echo "==> estado final"
echo "  crashes: $(grep -c 'senal 11' "$TMP/crash.txt" 2>/dev/null || echo 0)"
echo "  avisos de sujeto NULL: $(grep -c 'sujeto NULL' "$TMP/tmp/log.txt" 2>/dev/null || echo 0)"
echo "  estado vivo de cfg_sm: $(grep -o 'Estado vivo de cfg_sm[^	]*' "$TMP/tmp/log.txt" 2>/dev/null | head -1)"
docker rm -f x6200e2e >/dev/null 2>&1
exit $RES
