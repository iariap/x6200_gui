#!/bin/bash
# Secuencia de cambios de frecuencia/modo/banda/filtro contra la GUI ARM del banco QEMU,
# para el issue #131 (reabierto). Cada orden va por el socket de control (el mismo camino
# que la pagina remota: scheduler -> hilo LVGL) y se verifica que la GUI conteste ok.
# Se aborta apenas aparece crash.txt: el script del banco NO se usa entre ordenes porque su
# arranque borra crash.txt y reinicia la GUI, lo que taparia justo lo que se quiere medir.
#
# El banco NO tiene flow: no corre radio_tick() (teclas del panel) ni cw_put_audio_samples()
# (hilo de audio). Esta secuencia ejercita el camino de ajustes (freq/mode/band/filter), que
# es el que reconstruye la pantalla y donde caia el SIGSEGV. La concurrencia real es requires-radio.
set -u
CHROOT=${CHROOT:-/home/pablo/desa/asf/state/worktrees/x6200/lvgl131b-chroot}
BANCO=${BANCO:-banco-apps}
CLIENTE='chroot /t /tmp/x6200-remote'
CMD=0
ERR=0

enviar() {
    CMD=$((CMD + 1))
    local r
    r=$(docker exec "$BANCO" sh -c "$CLIENTE '$1'" 2>&1)
    if ! echo "$r" | grep -q '"t":"ok"'; then
        ERR=$((ERR + 1))
        echo "  [$CMD] ERROR $1 -> $r"
    fi
    if [ -f "$CHROOT/crash.txt" ]; then
        echo "  crash.txt APARECIO despues de $1 (orden $CMD)"
        cat "$CHROOT/crash.txt"
        exit 90
    fi
}

for r in 1 2 3 4 5 6; do
    echo "== ronda $r =="
    for ord in \
        '{"cmd":"freq","val":7074000}' \
        '{"cmd":"mode","val":"USB"}' \
        '{"cmd":"filter","val":3000}' \
        '{"cmd":"band","val":40}' \
        '{"cmd":"freq","val":14074000}' \
        '{"cmd":"mode","val":"LSB"}' \
        '{"cmd":"filter","val":2400}' \
        '{"cmd":"band","val":20}' \
        '{"cmd":"freq","val":21074000}' \
        '{"cmd":"mode","val":"CW"}' \
        '{"cmd":"filter","val":500}' \
        '{"cmd":"band","val":15}' \
        '{"cmd":"freq","val":3573000}' \
        '{"cmd":"mode","val":"USB"}' \
        '{"cmd":"filter","val":1800}' \
        '{"cmd":"band","val":80}' \
        '{"cmd":"freq","val":28400000}' \
        '{"cmd":"mode","val":"LSB"}' \
        '{"cmd":"filter","val":200}' \
        '{"cmd":"band","val":10}'
    do
        enviar "$ord"
    done
done

# Handshake final: la GUI sigue viva y dibujando.
sleep 2
FINAL=$(docker exec "$BANCO" sh -c "$CLIENTE '{\"cmd\":\"get\"}'" 2>&1)
echo "estado final: $FINAL"
echo "framebuffer: $(stat -c %s "$CHROOT/tmp/fakefb0" 2>/dev/null || echo FALTA) bytes"

echo "ordenes=$CMD errores=$ERR"
[ "$ERR" -eq 0 ] || exit 92
[ -f "$CHROOT/crash.txt" ] && { echo "crash.txt presente al final"; exit 93; }
echo "$FINAL" | grep -q '"t":"state"' || { echo "la GUI no contesto el estado final"; exit 94; }
[ "$(stat -c %s "$CHROOT/tmp/fakefb0" 2>/dev/null || echo 0)" -ge 1536000 ] || { echo "framebuffer chico"; exit 95; }
echo "OK: sin crash.txt, GUI viva y framebuffer completo"
