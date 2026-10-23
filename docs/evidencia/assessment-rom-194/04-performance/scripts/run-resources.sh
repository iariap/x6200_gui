#!/usr/bin/env bash
# run-resources.sh — #198 PERF-04 orchestration (local capture script).
#
# Drives the *documented* control socket of the candidate GUI running in a private
# QEMU_USER chroot (container `assessment198-res`) through the firmware states the
# issue lists, and samples /proc + `get` RTT. It does not implement firmware, mocks
# or a harness: it only sends the existing socket commands and reads /proc.
#
#   CHROOT=... OUT=... ./run-resources.sh [state ...]
#
# Every state gets 10 s of stabilisation and a 60 s window, three repetitions,
# returning to the home snapshot (app off, USB, 14.074 MHz, 2400 Hz filter) between
# states. The RTT sampler runs concurrently with the /proc sampler (at 1 Hz except
# the explicit 5 Hz state); `home-rx` runs with no client at all.
set -u

HERE=$(cd "$(dirname "$0")" && pwd)
SCR=${SCR:-/home/pablo/desa/x6200/assessment-194/scratch-198}
CHROOT=${CHROOT:-$SCR/rootfs-res}
OUT=${OUT:-$HERE/../capturas/resources}
LOGS=${LOGS:-$HERE/../logs/step4-resources}
CT=${CT:-assessment198-res}
REPS=${REPS:-3}
STAB=${STAB:-10}
WIN=${WIN:-60}
mkdir -p "$OUT" "$LOGS" "$CHROOT/tmp/res"

r() { docker exec "$CT" chroot /t /tmp/x6200-remote "$1" 2>>"$LOGS/remote-errors.log"; }
get_state() { r '{"cmd":"get"}'; }
app_of() { get_state | python3 -c 'import json,sys
try:
    d=json.load(sys.stdin)
    print(d.get("app",""))
except Exception:
    print("<no-json>")'; }
wait_app() {
    local want=$1 tries=${2:-40} got=""
    for _ in $(seq 1 "$tries"); do
        got=$(app_of)
        [ "$got" = "$want" ] && return 0
        sleep 0.25
    done
    echo "  WARN: app requested=$want observed=$got" >&2
    return 1
}
lastbreath_app() { grep -o 'app=[^ ]*' "$CHROOT/mnt/lastbreath" 2>/dev/null | tail -1 | cut -d= -f2; }

reset_home() {
    local a
    a=$(app_of)
    if [ "$a" = "CW" ]; then r '{"cmd":"cw_dec","val":0}' >/dev/null; fi
    if [ "$a" = "Propagation" ]; then r '{"cmd":"propagation","val":0}' >/dev/null; fi
    r '{"cmd":"app","text":"off"}' >/dev/null   # always: DFLEditor is not reported by get's "app"
    sleep 0.5
    r '{"cmd":"mode","val":"USB"}' >/dev/null
    r '{"cmd":"freq","val":14074000}' >/dev/null
    r '{"cmd":"filter","val":2400}' >/dev/null
    sleep 1
}

apply_state() {  # $1 = state name
    case "$1" in
        home-rx|home-rx-get1hz|home-rx-get5hz) reset_home ;;
        settings) reset_home; r '{"cmd":"app","text":"Settings"}' >/dev/null; wait_app Settings ;;
        dfl-editor) reset_home; r '{"cmd":"app","text":"DFLEditor"}' >/dev/null; sleep 2 ;;
        recorder-idle) reset_home; r '{"cmd":"app","text":"Recorder"}' >/dev/null; wait_app Recorder ;;
        cw-decoder) reset_home; r '{"cmd":"app","text":"CW"}' >/dev/null; wait_app CW; r '{"cmd":"cw_dec","val":1}' >/dev/null ;;
        prop-open) reset_home; r '{"cmd":"app","text":"Propagation"}' >/dev/null; wait_app Propagation ;;
        prop-sweep) reset_home; r '{"cmd":"app","text":"Propagation"}' >/dev/null; wait_app Propagation; r '{"cmd":"propagation","val":1}' >/dev/null ;;
        prop-cancel) reset_home; r '{"cmd":"app","text":"Propagation"}' >/dev/null; wait_app Propagation; r '{"cmd":"propagation","val":1}' >/dev/null; sleep 3; r '{"cmd":"propagation","val":0}' >/dev/null ;;
        *) echo "unknown state $1" >&2; return 1 ;;
    esac
}

observe() {  # $1 = state, $2 = file stem
    get_state > "$OUT/$2-state-before.json" 2>/dev/null || echo '{}' > "$OUT/$2-state-before.json"
    printf '  requested=%s observed_app=%s ax=%s lastbreath_app=%s\n' \
        "$1" "$(app_of)" "$(get_state | python3 -c 'import json,sys
try:
    d=json.load(sys.stdin); print(json.dumps(d.get("ax",{})))
except Exception:
    print("<no-json>")')" "$(lastbreath_app)" | tee -a "$LOGS/states.log"
}

run_window() {  # $1 = state, $2 = stem, $3 = rtt mode (none|1hz|5hz)
    local st=$1 stem=$2 rtt=$3
    if [ "$rtt" != "none" ]; then
        local iv=1000 mode=s limit=$WIN
        [ "$rtt" = "5hz" ] && iv=200
        docker exec "$CT" /tmp/rtt-sampler /t/tmp/x6200_remote.sock "$limit" "$iv" "/t/tmp/res/$stem-rtt.csv" s \
            > "$LOGS/$stem-rtt.stdout" 2>&1 &
        RTT_PID=$!
    fi
    docker exec "$CT" /tmp/res-sampler "$st" 1 "$WIN" 1000 "/t/tmp/res/$stem.csv" \
        > "$LOGS/$stem-sampler.stdout" 2>&1
    local rc=$?
    [ "$rtt" != "none" ] && wait "${RTT_PID:-}" 2>/dev/null
    get_state > "$OUT/$stem-state-after.json" 2>/dev/null || echo '{}' > "$OUT/$stem-state-after.json"
    cp "$CHROOT/tmp/res/$stem.csv" "$OUT/$stem.csv" 2>/dev/null
    [ "$rtt" != "none" ] && cp "$CHROOT/tmp/res/$stem-rtt.csv" "$OUT/$stem-rtt.csv" 2>/dev/null
    echo "  window $stem rc=$rc samples=$(($(wc -l < "$OUT/$stem.csv" 2>/dev/null || echo 1)-1))" | tee -a "$LOGS/windows.log"
    return "$rc"
}

STATES=${*:-"home-rx home-rx-get1hz settings dfl-editor recorder-idle cw-decoder prop-open prop-sweep prop-cancel home-rx-get5hz"}

{
    echo "start_utc=$(date -u +%FT%TZ)"
    echo "container=$CT chroot=$CHROOT reps=$REPS stab=$STAB win=$WIN"
    docker inspect -f 'container_pid={{.State.Pid}} image={{.Image}}' "$CT"
} | tee "$LOGS/run.txt"

for rep in $(seq 1 "$REPS"); do
    for st in $STATES; do
        stem="$st-r$rep"
        echo "== $stem $(date -u +%FT%TZ) ==" | tee -a "$LOGS/windows.log"
        apply_state "$st" || { echo "  apply failed" | tee -a "$LOGS/windows.log"; continue; }
        sleep "$STAB"
        observe "$st" "$stem"
        case "$st" in
            home-rx) run_window "$st" "$stem" none ;;
            home-rx-get1hz) run_window "$st" "$stem" 1hz ;;
            home-rx-get5hz) run_window "$st" "$stem" 5hz ;;
            *) run_window "$st" "$stem" 1hz ;;
        esac
        sleep 2
    done
done
reset_home
echo "end_utc=$(date -u +%FT%TZ)" | tee -a "$LOGS/run.txt"
