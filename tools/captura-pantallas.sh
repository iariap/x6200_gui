#!/bin/sh
#
# Captura las pantallas principales de la radio en un directorio, para revisar cambios de tema o de
# layout de una sola pasada. Abre cada app por el control remoto (tools/remote/x6200-remote, que hay
# que copiar a la radio: ver tools/remote/compila.sh) y despues captura con tools/captura-radio.sh.
#
#   tools/captura-pantallas.sh /tmp/revision
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -e

DIR=${1:-capturas}
IP=${RADIO_IP:-192.168.0.236}
AQUI=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
mkdir -p "$DIR"

remote() {
    sshpass -p 123 ssh -F /dev/null -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
        -o ConnectTimeout=10 -o LogLevel=ERROR -o PreferredAuthentications=password \
        -o PubkeyAuthentication=no "root@$IP" "/tmp/x6200-remote '$1'" >/dev/null 2>&1 || true
}

# La pantalla principal (cierra lo que este abierto)
remote '{"cmd":"app","text":"off"}'
sleep 2
"$AQUI/captura-radio.sh" "$DIR/00-home.png"

for app in Settings BT WiFi GPS SSTV WeFax CW FT8; do
    remote "{\"cmd\":\"app\",\"text\":\"$app\"}"
    sleep 4
    "$AQUI/captura-radio.sh" "$DIR/$app.png"
done

echo "capturas en $DIR"
