#!/bin/sh
#
# Encuentra la radio en la red. La direccion cambio varias veces (192.168.18.67 y
# 192.168.0.236 son las dos que uso), asi que las herramientas no deberian tener una fija.
#
#   . "$(dirname "$0")/lib-radio.sh"
#   IP=${1:-$(radio_ip)}          # o: IP=$(radio_ip) || exit 1
#
# Orden: RADIO_IP si esta puesta, despues las direcciones conocidas, y si ninguna contesta barre la
# red actual (la del default gateway) buscando SSH. Para no confundir la radio con otra maquina,
# cada candidata se verifica con el login y comprobando que exista /usr/sbin/x6200_gui.
#
# SPDX-License-Identifier: GPL-2.0-or-later

RADIO_CLAVE=${RADIO_CLAVE:-123}
RADIO_USUARIO=${RADIO_USUARIO:-root}
RADIO_CONOCIDAS=${RADIO_CONOCIDAS:-"192.168.18.67 192.168.0.236"}

ssh_radio() {
    ip=$1
    shift
    timeout 20 sshpass -p "$RADIO_CLAVE" ssh -F /dev/null \
        -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout=6 \
        -o LogLevel=ERROR -o PreferredAuthentications=password -o PubkeyAuthentication=no \
        "$RADIO_USUARIO@$ip" "$@"
}

es_la_radio() {
    ssh_radio "$1" 'test -x /usr/sbin/x6200_gui' >/dev/null 2>&1
}

radio_ip() {
    if [ -n "${RADIO_IP:-}" ]; then
        printf '%s\n' "$RADIO_IP"
        return 0
    fi

    for ip in $RADIO_CONOCIDAS; do
        if es_la_radio "$ip"; then
            printf '%s\n' "$ip"
            return 0
        fi
    done

    red=$(ip route 2>/dev/null | awk '/^default/{print $3; exit}' | cut -d. -f1-3)
    [ -n "$red" ] || return 1

    lista=$(mktemp)
    for h in $(seq 1 254); do
        ( timeout 1 sh -c "echo > /dev/tcp/$red.$h/22" 2>/dev/null && echo "$red.$h" >> "$lista" ) &
    done
    wait

    for ip in $(sort -t. -k4 -n "$lista" 2>/dev/null); do
        if es_la_radio "$ip"; then
            rm -f "$lista"
            printf '%s\n' "$ip"
            return 0
        fi
    done
    rm -f "$lista"
    return 1
}
