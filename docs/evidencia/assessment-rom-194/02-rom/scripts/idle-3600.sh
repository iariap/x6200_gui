#!/usr/bin/env bash
# idle-3600.sh <out.jsonl> [duration_s]
#
# Reposo de 1 h sobre el MISMO proceso QEMU_USER (contenedor privado assessment196-qemu):
#   - sondeo {"cmd":"get"} cada 5 s (12 por minuto) y verificacion de que contesta state;
#   - cada 60 s: PID, RSS, threads, fds, CRASHLOG, Overflow/cola, tamano de out-banco.txt,
#     existencia del socket y latencia maxima del minuto.
# No reinicia la GUI. Se detiene si el contenedor/proceso muere o si un get falla.
# Escribe una linea JSON por minuto y un resumen final.
set -u
OUTJ=${1:?out.jsonl}
DUR=${2:-3600}
CT=${CT:-assessment196-qemu}
T=/t
CLIENT="chroot $T /tmp/x6200-remote"
LOG=${LOG:-$T/out-banco.txt}

ts() { date -u +%Y-%m-%dT%H:%M:%S.%3NZ; }
ms() { date +%s%3N; }
: > "$OUTJ"
START=$(date +%s)
GLOBAL_OK=0; GLOBAL_ERR=0; STOP_REASON="duration"

for m in $(seq 1 $((DUR / 60))); do
    ok=0; err=0; maxlat=0
    for j in $(seq 1 12); do
        t0=$(ms)
        resp=$(timeout 5 docker exec "$CT" sh -c "$CLIENT '{\"cmd\":\"get\"}'" 2>&1)
        rc=$?
        t1=$(ms); lat=$((t1 - t0))
        [ "$lat" -gt "$maxlat" ] && maxlat=$lat
        if [ "$rc" -eq 0 ] && printf '%s' "$resp" | grep -q '"t":"state"'; then
            ok=$((ok + 1)); GLOBAL_OK=$((GLOBAL_OK + 1))
        else
            err=$((err + 1)); GLOBAL_ERR=$((GLOBAL_ERR + 1))
        fi
        [ "$j" -lt 12 ] && sleep 5
    done
    # muestreo del proceso (PID estable)
    sample=$(docker exec "$CT" sh -c '
        p=$(pgrep -x x6200_gui | head -1)
        [ -n "$p" ] || p=$(pgrep -f "^/usr/bin/qemu-arm-static /usr/sbin/x6200_gui" | head -1)
        if [ -z "$p" ]; then echo "PID=0"; exit 0; fi
        echo "PID=$p"
        awk "/^(Name|State|Threads|VmRSS|VmSize|FDSize):/{printf \"%s=%s \",\$1,\$2}" /proc/$p/status
        echo "fds=$(ls /proc/$p/fd 2>/dev/null | wc -l)"
        echo "start_ticks=$(awk "{print \$22}" /proc/$p/stat)"
        echo "uptime_s=$(cut -d. -f1 /proc/uptime)"
        crash=$(grep -c "CRASHLOG sig=" "$LOG" 2>/dev/null || true)
        overflow=$(grep -c -iE "Overflow|queue overflow" "$LOG" 2>/dev/null || true)
        echo "crash=${crash:-0}"
        echo "overflow=${overflow:-0}"
        echo "log_bytes=$(stat -c%s "$LOG" 2>/dev/null || echo 0)"
        echo "socket=$([ -S /t/tmp/x6200_remote.sock ] && echo yes || echo no)"
    ' 2>&1)
    python3 - "$OUTJ" "$m" "$(ts)" "$START" "$ok" "$err" "$maxlat" "$sample" <<'PY'
import json, sys
out, m, now, start, ok, err, maxlat, sample = sys.argv[1:]
d = {"minute": int(m), "ts_utc": now, "gets_ok": int(ok), "gets_err": int(err),
     "max_get_latency_ms": int(maxlat), "raw_sample": sample.strip()}
with open(out, "a") as f:
    f.write(json.dumps(d, ensure_ascii=False) + "\n")
PY
    # fin anticipado si el proceso/contenedor no esta
    if ! docker ps --filter name="$CT" --format '{{.Names}}' | grep -qx "$CT"; then
        STOP_REASON="container_gone_at_minute_$m"; break
    fi
    if [ "$err" -gt 0 ]; then
        STOP_REASON="get_error_at_minute_$m"; break
    fi
done
ELAPSED=$(( $(date +%s) - START ))
echo "idle_done elapsed_s=$ELAPSED gets_ok=$GLOBAL_OK gets_err=$GLOBAL_ERR stop=$STOP_REASON"
