#!/bin/bash
# Copia la GUI recien compilada (volumen de build x6200-build-output) a la radio por SSH, la
# reinicia (el daemon la vuelve a levantar solo) y muestra las marcas nuevas de arranque.
#
# Es el banco rapido: permite medir un cambio de arranque en el equipo real en ~20 s, sin
# rehacer la tarjeta ni flashear. Requiere que la radio este encendida y en la red.
#
#   ./tools/prueba-gui-en-radio.sh [ip]
#
# Para comparar antes/despues conviene guardar los dos binarios (md5) y el boot.log completo:
# es la unica forma de afirmar cuanto mejoro.
#
# Ojo: compilar la GUI con `docker run ... make x6200-gui-rebuild` (ver AGENTS del proyecto);
# este script solo copia lo que ya esta compilado en el volumen.

set -e

IP="${1:-${RADIO_IP:-192.168.0.236}}"
VOLUMEN="${VOLUMEN:-x6200-build-output}"
IMAGEN="${IMAGEN:-x6200-buildroot}"

SSH_OPTS=(-F /dev/null -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null
          -o ConnectTimeout=15 -o LogLevel=ERROR -o PreferredAuthentications=password
          -o PubkeyAuthentication=no)

echo "== copiando la GUI compilada =="
NUEVO=$(mktemp)
docker run --rm --entrypoint "" -v "$VOLUMEN":/b "$IMAGEN" cat /b/target/usr/sbin/x6200_gui > "$NUEVO"
md5sum "$NUEVO"

echo "== subiendo a la radio ($IP) =="
sshpass -p 123 scp "${SSH_OPTS[@]}" "$NUEVO" "root@$IP:/tmp/x6200_gui.nuevo"
rm -f "$NUEVO"

echo "== reiniciando la GUI (el daemon la levanta de nuevo) =="
sshpass -p 123 ssh "${SSH_OPTS[@]}" "root@$IP" '
    mv /tmp/x6200_gui.nuevo /usr/sbin/x6200_gui
    chmod 755 /usr/sbin/x6200_gui
    sync
    killall x6200_gui 2>/dev/null || true
'

echo "== esperando el arranque =="
sleep 20

sshpass -p 123 ssh "${SSH_OPTS[@]}" "root@$IP" 'tail -25 /mnt/app_logs/boot.log'
