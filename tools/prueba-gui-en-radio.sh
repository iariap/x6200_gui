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
#
# Antes de subirla, el binario pasa por tools/verifica-firmware-port.py: un build desde el `main`
# del remoto (issue #86) sale con exit 0 y deja un x6200_gui que no es el firmware; el gate lo
# frena antes de tocar la radio.

set -e

AQUI=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$AQUI/lib-radio.sh"
IP="${1:-$(radio_ip)}"
VOLUMEN="${VOLUMEN:-x6200-build-output}"
IMAGEN="${IMAGEN:-x6200-buildroot}"

SSH_OPTS=(-F /dev/null -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null
          -o ConnectTimeout=15 -o LogLevel=ERROR -o PreferredAuthentications=password
          -o PubkeyAuthentication=no)

echo "== copiando la GUI compilada =="
NUEVO=$(mktemp)
docker run --rm --entrypoint "" -v "$VOLUMEN":/b "$IMAGEN" cat /b/target/usr/sbin/x6200_gui > "$NUEVO"
md5sum "$NUEVO"

# GATE fail-closed (issue #86): no subir a la radio un binario que no es el firmware del port.
# Un build desde el `main` del remoto sale con exit 0 y deja un x6200_gui de 776.884 B que no es
# el firmware (el del port pesa 13.737.784 B): subirlo deja la radio sin GUI. Senales y
# calibracion: ver el encabezado de tools/verifica-firmware-port.py.
if ! python3 "$AQUI/verifica-firmware-port.py" "$NUEVO"; then
    rm -f "$NUEVO"
    echo "ERROR: el binario compilado no es el firmware del port (issue #86): NO se sube a la radio" >&2
    exit 1
fi

echo "== subiendo a la radio ($IP) =="
sshpass -p 123 scp "${SSH_OPTS[@]}" "$NUEVO" "root@$IP:/tmp/x6200_gui.nuevo"

# Las librerias tambien: la GUI las usa compartidas y una lib vieja en la radio hace que el binario
# nuevo corra contra otro LVGL sin que se note (me paso hoy: el cambio en lvgl/ no llegaba a la
# radio y no habia forma de verlo desde el log). docs/HALLAZGOS.md 34.
if [ "${SIN_LIBS:-0}" != "1" ]; then
    docker run --rm --entrypoint "" -v x6200-build-output:/b x6200-buildroot \
        sh -c 'cd /b/target/usr/lib && tar -cf - liblv*.so* 2>/dev/null' | \
        sshpass -p 123 ssh "${SSH_OPTS[@]}" "root@$IP" 'tar -C /usr/lib -xf - 2>/dev/null; sync'
fi
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
