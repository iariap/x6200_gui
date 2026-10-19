#!/bin/bash
#
# Evidencia del banco QEMU para la app Propagation (issue #145).
#
#   CHROOT=/home/pablo/desa/x6200/.vfix3 VOLUMEN=x6200-build-output \
#       docs/evidencia/2026-10-19-prop145-banco/correr.sh
#
# Corre el binario ARM real del worktree en el chroot del banco, abre la app por
# el socket, arranca el barrido, mide el `tx` en cada sondeo, cancela un barrido,
# cierra la app y mide los cambios de contexto (wakeups) con la app cerrada y
# abierta. Deja capturas, logs y el JSON de estado en este directorio.
#
# No transmite y no necesita radio. La restauracion del VFO por hardware es el
# pendiente `requires-radio`: aca se verifica el estado que ve la GUI (freq/mode
# del `get`), que es el que el banco puede medir.
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -euo pipefail

AQUI=$(cd "$(dirname -- "$0")" && pwd)
REPO=$(cd "$AQUI/../../.." && pwd)

CHROOT=${CHROOT:-/home/pablo/desa/x6200/.vfix3}
VOLUMEN=${VOLUMEN:-x6200-build-output}
IMAGEN=${IMAGEN:-x6200-buildroot}
QEMU_CPU=${QEMU_CPU:-cortex-a7}
ESPERA_BOOT=${ESPERA_BOOT:-90}
SALIDA=${SALIDA:-$AQUI}
CONT=${CONT:-prop145-banco}

BIN=/usr/sbin/x6200_gui
LOG=$CHROOT/out-banco.txt
FB=$CHROOT/tmp/fakefb0

fallar() { echo "ERROR: $*" >&2; docker logs "$CONT" --tail 20 2>/dev/null || true; exit 1; }
nota() { echo "  $*"; }

orden() { docker exec "$CONT" chroot /t /tmp/x6200-remote "$1"; }
get() { orden '{"cmd":"get"}'; }
captura() { python3 "$REPO/tools/barrido_apps.py" captura "$FB" "$1"; }

estado() {
    # Una linea con los campos que importan del get.
    get | python3 -c '
import json, sys
d = json.load(sys.stdin)
ax = d.get("ax") or {}
print("app=%s freq=%s mode=%s tx=%s state=%s done=%s heard=%s snr=%s ladder=%s" % (
    d.get("app"), d.get("freq"), d.get("mode"), d.get("tx"),
    ax.get("propagation_state"), ax.get("propagation_done"),
    ax.get("propagation_heard"), ax.get("propagation_snr"),
    ax.get("propagation_ladder")))
'
}

pid_gui() {
    # El primer /proc que menciona x6200_gui es el `sh -c` que lo lanza (no
    # despierta nunca); el proceso real es el que corre qemu-arm-static.
    docker exec "$CONT" sh -c '
        for p in /proc/[0-9]*; do
            case "$(tr "\0" " " < "$p/cmdline" 2>/dev/null)" in
                /usr/bin/qemu-arm-static*) echo "${p##*/}"; exit 0;;
            esac
        done
        exit 1'
}

ctxt() {
    local pid
    pid=$(pid_gui)
    docker exec "$CONT" sh -c "grep -E 'voluntary_ctxt_switches|nonvoluntary_ctxt_switches' /proc/$pid/status | tr '\n' ' '"
}

# Mide el cambio de contexto en una ventana de 20 s.
wakeups() {
    local etiqueta=$1 ini fin
    ini=$(ctxt)
    sleep 20
    fin=$(ctxt)
    echo "$etiqueta 20s: $ini -> $fin" | tee -a "$SALIDA/wakeups.log"
}

lastbreath() {
    docker exec "$CONT" sh -c 'cat /t/mnt/lastbreath 2>/dev/null; echo; cat /proc/uptime' | tee -a "$SALIDA/lastbreath.log"
}

arrancar_banco() {
    docker rm -f "$CONT" banco-apps >/dev/null 2>&1 || true

    # El binario se copia SIN la GUI vieja corriendo: cp sobre un ejecutable en uso
    # falla con ETXTBSY y el banco correria la version anterior sin avisar.
    docker run --rm --entrypoint "" -v "$CHROOT:/t" -v "$VOLUMEN":/b "$IMAGEN" \
        sh -c "cp -f /b/target$BIN /t$BIN && chmod 755 /t$BIN" || fallar "no pude copiar el binario"

    python3 "$REPO/tools/verifica-firmware-port.py" "$CHROOT$BIN" || fallar "el binario no es el firmware del port"

    cp "$REPO/tools/bench/libfakefb.so" "$REPO/tools/bench/libcrash.so" \
       "$REPO/tools/bench/libx6200trace.so" "$CHROOT/lib/"
    cp "$REPO/tools/remote/x6200-remote" "$CHROOT/tmp/x6200-remote"
    chmod 755 "$CHROOT/tmp/x6200-remote"

    rm -f "$CHROOT/tmp/fakefb0" "$CHROOT/crash.txt" "$CHROOT/tmp/x6200_remote.sock"
    docker run -d --name "$CONT" --rm --entrypoint= -e "QEMU_CPU=$QEMU_CPU" \
        -v "$CHROOT:/t" "$IMAGEN" \
        sh -c "chroot /t /bin/busybox env LD_PRELOAD=/lib/libfakefb.so:/lib/libcrash.so:/lib/libx6200trace.so $BIN > /t/out-banco.txt 2>&1"

    local i resp
    for i in $(seq 1 "$ESPERA_BOOT"); do
        [ -S "$CHROOT/tmp/x6200_remote.sock" ] || { sleep 1; continue; }
        resp=$(get 2>/dev/null || true)
        case "$resp" in
            *'"t":"state"'*) nota "banco listo (socket y handshake)"; return 0;;
        esac
        sleep 1
    done
    fallar "la GUI no contesto el handshake en ${ESPERA_BOOT}s"
}

mkdir -p "$SALIDA"
: > "$SALIDA/estado.log"
: > "$SALIDA/wakeups.log"
: > "$SALIDA/lastbreath.log"

echo "== Propagation #145: banco QEMU =="
arrancar_banco

echo "== banda 20m (el banco arranca en CB y la app mide la banda actual) =="
orden '{"cmd":"band","val":20}' | tee -a "$SALIDA/estado.log"
sleep 3
estado | tee -a "$SALIDA/estado.log"

echo "== estado inicial (app cerrada) =="
estado | tee -a "$SALIDA/estado.log"
captura "$SALIDA/00-home.png"
lastbreath | sed 's/^/  /'
wakeups "cerrada-1" | sed 's/^/  /'

echo "== abrir la app =="
orden '{"cmd":"app","text":"Propagation"}' | tee -a "$SALIDA/estado.log"
sleep 2
estado | tee -a "$SALIDA/estado.log"
captura "$SALIDA/01-abrir.png"
wakeups "abierta-idle" | sed 's/^/  /'

echo "== Run: barrido =="
ini_sweep=$(ctxt)
orden '{"cmd":"propagation","val":1}' | tee -a "$SALIDA/estado.log"
sleep 1
estado | tee -a "$SALIDA/estado.log"
captura "$SALIDA/02a-sincronizando.png"
sleep 2
estado | tee -a "$SALIDA/estado.log"
captura "$SALIDA/02-midiendo.png"

# Sondeo del tx durante todo el barrido: tiene que quedar en false siempre.
echo "== sondeo del barrido (tx y estado) =="
for i in $(seq 1 70); do
    linea=$(estado)
    echo "$i $linea" | tee -a "$SALIDA/estado.log"
    case "$linea" in
        *"state=Result"*|*"state=Cancelled"*) break;;
        *"tx=True"*|*"tx=true"*) fallar "tx=true durante el barrido";;
    esac
    sleep 1
done
captura "$SALIDA/03-resultado.png"
fin_sweep=$(ctxt)
echo "barrido: $ini_sweep -> $fin_sweep" | tee -a "$SALIDA/wakeups.log"

echo "== Cancelar un barrido =="
orden '{"cmd":"propagation","val":1}' | tee -a "$SALIDA/estado.log"
sleep 2
orden '{"cmd":"propagation","val":0}' | tee -a "$SALIDA/estado.log"
sleep 1
estado | tee -a "$SALIDA/estado.log"
captura "$SALIDA/04-cancelado.png"

echo "== cerrar la app y verificar el VFO =="
orden '{"cmd":"app","text":"off"}' | tee -a "$SALIDA/estado.log"
sleep 3
estado | tee -a "$SALIDA/estado.log"
captura "$SALIDA/05-cerrada.png"
lastbreath | sed 's/^/  /'
wakeups "cerrada-2" | sed 's/^/  /'

echo "== crash: CRASHLOG y core =="
grep -c "CRASHLOG " "$LOG" 2>/dev/null | sed 's/^/  CRASHLOG: /' | tee -a "$SALIDA/estado.log" || true
if [ -s "$CHROOT/crash.txt" ]; then
    cp "$CHROOT/crash.txt" "$SALIDA/crash.txt"
    echo "  hay crash.txt (ver $SALIDA/crash.txt)" | tee -a "$SALIDA/estado.log"
fi

cp "$LOG" "$SALIDA/out-banco.txt"
echo "== listo: $SALIDA =="
