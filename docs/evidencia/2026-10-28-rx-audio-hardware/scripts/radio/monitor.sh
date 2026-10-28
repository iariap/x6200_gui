#!/bin/sh
# monitor.sh — se ejecuta EN LA RADIO (no en el host).
#
# Muestreo pasivo 1 Hz de CPU de la GUI, CPU total y loadavg mientras corre el
# banco ARM, con corte de seguridad: si la radio se degrada, mata el banco y
# deja un marcador SAFETY_ABORT en el log. No escribe nada de la radio: solo lee
# /proc y mata, si hace falta, el proceso del banco que subimos nosotros.
#
# Uso: monitor.sh <gui_pid> <out_file> <max_seconds>
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -u

GUI=${1:?falta gui_pid}
OUT=${2:?falta out_file}
MAX=${3:?falta max_seconds}
CORES=$(grep -c "^processor" /proc/cpuinfo 2>/dev/null || echo 4)
[ "$CORES" -ge 1 ] 2>/dev/null || CORES=4

kill_bench() {
    for p in $(ps | awk '/bench_rnnoise_arm/ && !/awk/ && !/grep/ {print $1}'); do
        kill -TERM "$p" 2>/dev/null
    done
}

printf '# t_uptime gui_utime gui_stime load1 idle_ticks total_ticks gui_cpu_pct idle_pct note\n' > "$OUT"
prev_u=""
prev_s=""
prev_idle=""
prev_total=""
prev_t=""
bad_load=0
bad_idle=0
bad_gui=0
i=0

while [ "$i" -lt "$MAX" ]; do
    t=$(cut -d' ' -f1 /proc/uptime)
    if [ ! -d "/proc/$GUI" ]; then
        echo "SAFETY_ABORT GUI_GONE t=$t" >> "$OUT"
        kill_bench
        exit 3
    fi
    stat=$(cat "/proc/$GUI/stat" 2>/dev/null)
    u=$(echo "$stat" | awk '{print $14}')
    s=$(echo "$stat" | awk '{print $15}')
    load=$(cut -d' ' -f1 /proc/loadavg)
    idle=$(awk '/^cpu /{print $5}' /proc/stat)
    total=$(awk '/^cpu /{s=0; for (i=2; i<=9; i++) s+=$i; print s}' /proc/stat)

    gui_pct=0
    idle_pct=0
    if [ -n "$prev_u" ] && [ "$total" != "$prev_total" ]; then
        du=$((u - prev_u))
        ds=$((s - prev_s))
        d_idle=$((idle - prev_idle))
        d_total=$((total - prev_total))
        [ "$d_total" -gt 0 ] || d_total=1
        gui_pct=$(awk -v a="$((du + ds))" -v b="$d_total" -v c="$CORES" 'BEGIN{printf "%.2f", 100.0*a*c/b}')
        idle_pct=$(awk -v a="$d_idle" -v b="$d_total" 'BEGIN{printf "%.2f", 100.0*a/b}')
    fi

    note=""
    # S4: loadavg1 sostenido alto (4 nucleos; umbral pre-registrado 3.20)
    if awk -v l="$load" 'BEGIN{exit !(l > 3.20)}'; then
        bad_load=$((bad_load + 1))
    else
        bad_load=0
    fi
    [ "$bad_load" -ge 3 ] && note="LOAD_HIGH" && echo "SAFETY_TRIGGER LOAD_HIGH load=$load" >> "$OUT"
    # S2: CPU total ocupada: idle < 20 % sostenido 2 muestras
    if [ -n "$prev_total" ] && awk -v p="$idle_pct" 'BEGIN{exit !(p < 20.0)}'; then
        bad_idle=$((bad_idle + 1))
    else
        bad_idle=0
    fi
    [ "$bad_idle" -ge 2 ] && note="${note:+$note,}CPU_SATURATED" && echo "SAFETY_TRIGGER CPU_SATURATED idle_pct=$idle_pct" >> "$OUT"
    # S3: CPU de la GUI > 95 % de un nucleo sostenido 3 muestras (baseline ~67 %)
    if [ -n "$prev_total" ] && awk -v g="$gui_pct" 'BEGIN{exit !(g > 95.0)}'; then
        bad_gui=$((bad_gui + 1))
    else
        bad_gui=0
    fi
    [ "$bad_gui" -ge 3 ] && note="${note:+$note,}GUI_HIGH" && echo "SAFETY_TRIGGER GUI_HIGH gui_cpu_pct=$gui_pct" >> "$OUT"

    echo "$t $u $s $load $idle $total $gui_pct $idle_pct $note" >> "$OUT"

    if [ "$bad_load" -ge 3 ] || [ "$bad_idle" -ge 2 ] || [ "$bad_gui" -ge 3 ]; then
        echo "SAFETY_ABORT killing bench at t=$t (load=$load gui_pct=$gui_pct idle_pct=$idle_pct)" >> "$OUT"
        kill_bench
        exit 4
    fi

    prev_u=$u
    prev_s=$s
    prev_idle=$idle
    prev_total=$total
    prev_t=$t
    i=$((i + 1))
    sleep 1
done
echo "MONITOR_DONE t=$(cut -d' ' -f1 /proc/uptime)" >> "$OUT"
exit 0
