#!/bin/bash
#
# Abre cada app por el control remoto y detecta cual reinicia el equipo o cuelga la GUI: mide el
# uptime antes y despues (si baja, reinicio) y si el control remoto sigue respondiendo.
#
#   RADIO_IP=192.168.18.67 tools/prueba-apps-radio.sh
#
# Deja el detalle en /tmp/prueba-apps.log. No transmite.
#
# SPDX-License-Identifier: GPL-2.0-or-later
set -u

IP=${1:-${RADIO_IP:-192.168.0.236}}
BASE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SSH_OPTS=(-F /dev/null -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout=8 -o LogLevel=ERROR -o PreferredAuthentications=password -o PubkeyAuthentication=no)
LOG=${LOG:-$PWD/prueba-apps-$(date +%H%M%S).log}
PERMANENCIA=${PERMANENCIA:-3}   # segundos que se deja cada app abierta
APPS=${APPS:-"off FT8 FT4 JS8 WSPR SSTV WeFax NavTex RTTY CW Field GPS Bands Settings BT Cluster CwTrainer Eq Status QTH Callsign Recorder WiFi"}

SSH() { timeout 12 sshpass -p 123 ssh "${SSH_OPTS[@]}" "root@${IP}" "$@"; }

# El binario vive en /tmp, que se borra al reiniciar
preparar() {
    SSH 'test -x /tmp/x6200-remote' && return 0
    timeout 20 sshpass -p 123 scp "${SSH_OPTS[@]}" "$BASE/tools/remote/x6200-remote" "root@${IP}:/tmp/x6200-remote" >/dev/null 2>&1
    SSH 'chmod 755 /tmp/x6200-remote'
}

uptime_radio() { SSH 'cut -d" " -f1 /proc/uptime' 2>/dev/null | tr -d "\r"; }
gui_pid()    { SSH 'pidof x6200_gui 2>/dev/null || ps | awk "/[x]6200_gui/{print \$1; exit}"' 2>/dev/null | tr -d "\r"; }
logs_n()     { SSH 'ls /mnt/app_logs/x6200_log_*.txt 2>/dev/null | wc -l' 2>/dev/null | tr -d "\r"; }

# Manda una orden y devuelve la respuesta ("(sin respuesta)" si no contesta)
orden() { SSH "/tmp/x6200-remote '$1'" 2>/dev/null | tail -1; }

echo "== prueba de apps contra $IP ==" | tee "$LOG"
preparar
echo "  uptime inicial: $(uptime_radio) s" | tee -a "$LOG"

# Modo botones: la fila de abajo de cada pagina APP, apretando cada boton (el camino del usuario)
if [ "${MODO:-apps}" = "botones" ]; then
    orden '{"cmd":"app","text":"off"}' >/dev/null
    orden '{"cmd":"page","text":"app"}' >/dev/null
    sleep 1
    for pagina in 1 2 3 4 5 6; do
        for b in 1 2 3 4; do
            p0=$(gui_pid); u0=$(uptime_radio); l0=$(logs_n)
            resp=$(orden "{\"cmd\":\"btn\",\"val\":$b}")
            sleep 3
            u1=$(uptime_radio); p1=$(gui_pid); l1=$(logs_n)
            cierre=$(orden '{"cmd":"app","text":"off"}')
            estado="ok"
            if [ -z "$u1" ]; then estado="REINICIO (dejo de responder)"
            elif awk -v a="$u0" -v b="$u1" 'BEGIN{exit !(b < a)}'; then estado="REINICIO (uptime $u0 -> $u1)"
            elif [ -n "$p0" ] && [ "$p0" != "$p1" ]; then estado="SE CAYO LA GUI (pid $p0 -> ${p1:-?})"
            elif [ "$resp" != '{"t":"ok"}' ]; then estado="raro: $resp"; fi
            printf "  APP %s boton %s  %-44s\n" "$pagina" "$b" "$estado" | tee -a "$LOG"
            if [ "$estado" != "ok" ]; then
                SSH 'tail -4 /mnt/app_logs/boot.log 2>/dev/null' 2>/dev/null | sed 's/^/      /' | tee -a "$LOG"
                for i in $(seq 1 12); do sleep 5; u2=$(uptime_radio); [ -n "$u2" ] && [ "$u2" != "$u1" ] && break; done
                preparar; orden '{"cmd":"page","text":"app"}' >/dev/null; sleep 1
            fi
        done
        orden '{"cmd":"btn","val":0}' >/dev/null
        sleep 1
    done
    echo "== fin (detalle en $LOG) ==" | tee -a "$LOG"
    exit 0
fi

for a in $APPS; do
    [ "$a" = "off" ] && continue

    u0=$(uptime_radio)
    [ -z "$u0" ] && { echo "  $a: la radio no responde (¿apagada?)" | tee -a "$LOG"; sleep 5; continue; }

    p0=$(gui_pid); l0=$(logs_n)
    resp=$(orden "{\"cmd\":\"app\",\"text\":\"$a\"}")
    sleep "$PERMANENCIA"
    u1=$(uptime_radio); p1=$(gui_pid); l1=$(logs_n)
    cierre=$(orden '{"cmd":"app","text":"off"}')

    estado="ok"
    if [ -z "$u1" ]; then
        estado="REINICIO (dejo de responder)"
    elif awk -v a="$u0" -v b="$u1" 'BEGIN{exit !(b < a)}'; then
        estado="REINICIO (uptime $u0 -> $u1)"
    elif [ -n "$p0" ] && [ "$p0" != "$p1" ]; then
        estado="SE CAYO Y VOLVIO LA GUI (pid $p0 -> ${p1:-?})"
    elif [ -n "$l0" ] && [ "$l1" -gt "$l0" ] 2>/dev/null; then
        estado="SE CAYO Y VOLVIO LA GUI (log nuevo)"
    elif [ "$resp" = "(sin respuesta)" ] || [ -z "$resp" ]; then
        estado="CUELGUE (sin respuesta del control remoto)"
    elif [ "$cierre" != '{"t":"ok"}' ]; then
        estado="cierre raro: $cierre"
    fi

    printf "  %-10s %-44s (abre: %s, cierra: %s)\n" "$a" "$estado" "$resp" "$cierre" | tee -a "$LOG"

    if [ "$estado" != "ok" ]; then
        # esperar a que vuelva y juntar evidencia
        for i in $(seq 1 12); do
            sleep 5
            u2=$(uptime_radio)
            [ -n "$u2" ] && [ "$u2" != "$u1" ] && break
        done
        echo "      volvio con uptime $(uptime_radio) s" | tee -a "$LOG"
        SSH 'ls -la /mnt/app_logs/ 2>/dev/null | tail -5; echo "--- ultimas lineas de boot.log ---"; tail -5 /mnt/app_logs/boot.log 2>/dev/null' 2>/dev/null | sed 's/^/      /' | tee -a "$LOG"
        preparar
        sleep 3
    fi
done

echo "== fin (detalle en $LOG) ==" | tee -a "$LOG"
