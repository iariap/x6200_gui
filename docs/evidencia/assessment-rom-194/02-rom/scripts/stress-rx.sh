#!/usr/bin/env bash
# stress-rx.sh <cycles> <out.jsonl> [pause_between_cycles_s]
#
# Orquesta el cliente existente tools/remote/x6200-remote DENTRO del contenedor privado
# assessment196-qemu (chroot /t), repitiendo la secuencia RX del issue #196:
#   app off -> mode USB -> freq 14074000 -> filter 2400 -> mode CW -> app Settings -> app off
# Tras cada paso pide {"cmd":"get"} y espera (poll cada 100 ms, maximo 5 s) a que el estado
# observado coincida con el esperado; registra el tiempo de aplicacion. NO transmite: todos
# los comandos son de recepcion/UI. Se detiene en el primer timeout/crash y preserva lo medido.
#
# Escribe una linea JSON por paso en <out.jsonl> con: ciclo, paso, comando, esperado,
# inicio/fin UTC, duracion del comando, settle_ms (hasta ver el estado), polls, exit,
# respuesta, estado observado y verdict (ok|diverged|error).
set -u
N=${1:?cycles}
OUTJ=${2:?out.jsonl}
PAUSE=${3:-0.05}
CT=${CT:-assessment196-qemu}
CLIENT="chroot /t /tmp/x6200-remote"

ts() { date -u +%Y-%m-%dT%H:%M:%S.%3NZ; }
ms() { date +%s%3N; }

send() { # send <json> ; sets RC RESP DUR
    local json=$1 t0 t1
    t0=$(ms)
    RESP=$(timeout 5 docker exec "$CT" sh -c "$CLIENT '$json'" 2>&1)
    RC=$?
    t1=$(ms)
    DUR=$((t1 - t0))
}

state_get() { timeout 5 docker exec "$CT" sh -c "$CLIENT '{\"cmd\":\"get\"}'" 2>&1; }

get_field() { # get_field <state-json> <key>
    local state=$1 key=$2
    STATE="$state" python3 - "$key" <<'PY' 2>/dev/null
import json, os, sys
try:
    d = json.loads(os.environ["STATE"])
    v = d.get(sys.argv[1], "")
    print("" if v is None else v)
except Exception:
    print("__PARSE_ERROR__")
PY
}

record() { # record <cycle> <step> <cmd> <expected> <verdict> <state> <settle_ms> <polls>
    python3 - "$OUTJ" "$1" "$2" "$3" "$4" "$5" "$6" "$RC" "$DUR" "$START" "$(ts)" "$RESP" "$7" "$8" <<'PY'
import json, sys
out, cyc, step, cmd, exp, verdict, state, rc, dur, st, en, resp, settle, polls = sys.argv[1:]
rec = {"cycle": int(cyc), "step": int(step), "command": cmd, "expected": exp,
       "observed_state": state, "verdict": verdict, "exit": int(rc),
       "response": resp, "start_utc": st, "end_utc": en, "duration_ms": int(dur),
       "settle_ms": int(settle), "state_polls": int(polls)}
with open(out, "a") as f:
    f.write(json.dumps(rec, ensure_ascii=False) + "\n")
PY
}

# un paso: envia, espera el estado esperado y registra
step() { # step <cycle> <n> <json> <key> <value>
    local c=$1 n=$2 json=$3 key=$4 want=$5 t0 t1 settle polls=0 st got v
    START=$(ts); t0=$(ms)
    send "$json"
    got=""
    while :; do
        st=$(state_get)
        polls=$((polls + 1))
        got=$(get_field "$st" "$key")
        [ "$got" = "$want" ] && break
        t1=$(ms); [ $((t1 - t0)) -ge 5000 ] && break
        ! docker ps --filter name="$CT" --format '{{.Names}}' | grep -qx "$CT" && break
        sleep 0.1
    done
    t1=$(ms); settle=$((t1 - t0))
    if [ "$RC" -ne 0 ]; then v=error
    elif [ "$got" = "$want" ]; then v=ok
    else v=diverged; fi
    record "$c" "$n" "$json" "$key=$want" "$v" "$st" "$settle" "$polls"
    [ "$v" = ok ]
}

: > "$OUTJ"
DIVERGED=0; ERRORS=0; FIRST_DIV=""
for c in $(seq 1 "$N"); do
    ok=1
    step "$c" 1 '{"cmd":"app","text":"off"}' app "" || ok=0
    step "$c" 2 '{"cmd":"mode","val":"USB"}' mode USB || ok=0
    step "$c" 3 '{"cmd":"freq","val":14074000}' freq 14074000 || ok=0
    step "$c" 4 '{"cmd":"filter","val":2400}' filter 2400 || ok=0
    step "$c" 5 '{"cmd":"mode","val":"CW"}' mode CW || ok=0
    step "$c" 6 '{"cmd":"app","text":"Settings"}' app Settings || ok=0
    step "$c" 7 '{"cmd":"app","text":"off"}' app "" || ok=0
    if [ "$ok" -eq 0 ]; then
        DIVERGED=1
        FIRST_DIV=${FIRST_DIV:-"cycle $c"}
        echo "STOP cycle=$c: divergencia o error (ver $OUTJ)" >&2
        break
    fi
    ! docker ps --filter name="$CT" --format '{{.Names}}' | grep -qx "$CT" && {
        echo "STOP cycle=$c: el contenedor ya no esta" >&2; ERRORS=1; break; }
    sleep "$PAUSE"
done
echo "cycles_done=$(grep -c '"step": 7' "$OUTJ" 2>/dev/null || echo 0) diverged=$DIVERGED errors=$ERRORS first_divergence=${FIRST_DIV:-none}"
