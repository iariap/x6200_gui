#!/bin/bash
# Verifica en la radio lo que tiene que andar despues de un cambio, por SSH. Es el "esta bien?"
# del ciclo: actualizar por WiFi -> reiniciar -> verificar.
#
#   ./tools/verifica-radio.sh [ip]
#
# Comprueba:
#   - que la radio este en la red y la GUI corriendo
#   - el arranque en frio del boot.log (kernel, scripts, daemon y las etapas de la GUI)
#   - que los assets esten enganchados por bind mount (y no copiados a /dev/shm)
#   - que la GUI tenga abiertos los dispositivos de entrada (sin cambiarlos)
#   - que el usuario pulse este en el grupo audio y que PulseAudio tenga el sink real
#   - que el log de la GUI no traiga senales de crash
set -u

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/lib-radio.sh"
IP="${1:-$(radio_ip)}"
SSH_OPTS=(-F /dev/null -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null
          -o ConnectTimeout=10 -o LogLevel=ERROR -o PreferredAuthentications=password
          -o PubkeyAuthentication=no)
SSH() { sshpass -p 123 ssh "${SSH_OPTS[@]}" "root@$IP" "$@"; }

fallos=0
ok()      { printf '  OK   %s\n' "$1"; }
falla()   { printf '  FALLA %s\n' "$1"; fallos=$((fallos+1)); }

ping -c1 -W3 "$IP" >/dev/null 2>&1 || { echo "la radio ($IP) no responde"; exit 1; }

echo "== arranque (ultimo bloque del boot.log) =="
SSH 'awk "/kernel listo/{b=\"\"} {b=b \$0 \"\n\"} END{printf \"%s\", b}" /mnt/app_logs/boot.log' | sed 's/^/  /'

echo "== GUI =="
PID=$(SSH 'pidof x6200_gui' 2>/dev/null | tr -d '\r')
[ -n "$PID" ] && ok "x6200_gui corriendo (pid $PID)" || falla "x6200_gui no esta corriendo"

echo "== assets (bind mount en /dev/shm/images) =="
if SSH 'mountpoint -q /dev/shm/images' 2>/dev/null; then
    ok "/dev/shm/images es un punto de montaje"
else
    falla "/dev/shm/images NO es un punto de montaje (se estara copiando)"
fi
USADO=$(SSH 'df -k /dev/shm | awk "NR==2{print \$3}"' 2>/dev/null | tr -d '\r')
if [ "${USADO:-999999}" -lt 1024 ]; then
    ok "/dev/shm casi vacio (${USADO} kB usados): no se copiaron los 4,4 MB"
else
    falla "/dev/shm tiene ${USADO} kB usados: los assets se copiaron"
fi
CANT=$(SSH 'ls /dev/shm/images | wc -l' 2>/dev/null | tr -d '\r')
[ "${CANT:-0}" = 16 ] && ok "los 16 assets se leen por /dev/shm/images" || falla "assets: $CANT archivos (esperaba 16)"

echo "== entrada (los mismos dispositivos de siempre) =="
SSH "ls -l /proc/$PID/fd 2>/dev/null | grep -o '/dev/input/[a-z0-9]*' | sort | uniq -c" | sed 's/^/  /'
ABIERTOS=$(SSH "ls -l /proc/$PID/fd 2>/dev/null | grep -c '/dev/input/'" 2>/dev/null | tr -d '\r')
[ "${ABIERTOS:-0}" -ge 5 ] && ok "$ABIERTOS dispositivos de entrada abiertos" || falla "solo $ABIERTOS dispositivos de entrada"
LEIDOS=$(SSH "ls -l /proc/$PID/fd 2>/dev/null | grep -c 'shm/images'" 2>/dev/null | tr -d '\r')
IGUAL=$(SSH 'cmp /dev/shm/images/panel.bin /usr/share/x6200/images/panel.bin >/dev/null 2>&1 && echo igual' 2>/dev/null | tr -d '\r')
[ "$IGUAL" = igual ] && ok "los assets del bind mount son los mismos que /usr/share" \
                     || falla "los assets de /dev/shm/images no coinciden con /usr/share"
ERRIMG=$(SSH 'grep -ciE "failed to open|no such file|cannot open" /tmp/x6200_log.txt' 2>/dev/null | tr -d '\r')
[ "${ERRIMG:-1}" = 0 ] && ok "la GUI no reporta errores al abrir archivos" || falla "la GUI reporta $ERRIMG errores de archivos"

echo "== audio (la voz necesita el grupo y el sink real) =="
SSH 'grep -q "^audio:x:29:pulse" /etc/group' 2>/dev/null && ok "pulse esta en el grupo audio" \
                                                         || falla "pulse NO esta en el grupo audio"
SINK=$(SSH 'pactl list short sinks 2>/dev/null | head -3' | tr -d '\r')
echo "$SINK" | sed 's/^/  /'
echo "$SINK" | grep -q "alsa_output" && ok "PulseAudio tiene el sink real" || falla "PulseAudio no tiene el sink real (auto_null)"

echo "== crash? =="
SENAL=$(SSH 'grep -c "senal 11\|segmentation\|Aborted" /mnt/app_logs/voice.log /tmp/x6200_log.txt 2>/dev/null | awk -F: "{s+=\$2} END{print s+0}"' 2>/dev/null | tr -d '\r')
[ "${SENAL:-0}" = 0 ] && ok "sin senales de crash en los logs" || falla "hay $SENAL marcas de crash"

echo
if [ "$fallos" -eq 0 ]; then
    echo "TODO OK"
else
    echo "$fallos comprobaciones fallaron"
    exit 1
fi
