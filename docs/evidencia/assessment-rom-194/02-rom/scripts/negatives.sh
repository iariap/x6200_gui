#!/usr/bin/env bash
# negatives.sh <out.jsonl>
#
# Negativas de protocolo/entrada contra el proceso QEMU_USER privado assessment196-neg
# (chroot /t, GUI ARM real del candidato). NO transmite y NO toca DATA del equipo:
# todo ocurre en la copia privada. Verifica err/no cambio de estado/no crash.
# Los negativos nativos de events/scheduler/spectrum/restore/remote/lastbreath/params/
# migrations se reusan de #195 (no se repiten aca).
set -u
OUTJ=${1:?out.jsonl}
CT=assessment196-neg
CLIENT="chroot /t /tmp/x6200-remote"
TREE=${TREE:-/home/pablo/desa/x6200/assessment-194/scratch-196/rootfs2/tree}

: > "$OUTJ"
crash_count() { grep -c 'CRASHLOG sig=' "$TREE/out-banco.txt" 2>/dev/null || true; }

get_state() { timeout 5 docker exec "$CT" sh -c "$CLIENT '{\"cmd\":\"get\"}'" 2>&1; }
send_raw() { timeout 5 docker exec "$CT" sh -c "$CLIENT '$1'" 2>&1; }

record() { # record <case> <cmd> <resp> <rc> <state_before> <state_after> <crash_delta> <verdict> <note>
    python3 - "$OUTJ" "$1" "$2" "$3" "$4" "$5" "$6" "$7" "$8" "$9" <<'PY'
import json, sys
out, case, cmd, resp, rc, before, after, cdelta, verdict, note = sys.argv[1:]
with open(out, "a") as f:
    f.write(json.dumps({"case": case, "command": cmd, "response": resp, "exit": int(rc),
                        "crashlog_delta": int(cdelta), "verdict": verdict, "note": note,
                        "state_before": before, "state_after": after}, ensure_ascii=False) + "\n")
PY
}

field() { # field <state> <key>
    STATE="$1" python3 - "$2" <<'PY' 2>/dev/null
import json, os, sys
try:
    d = json.loads(os.environ["STATE"]); v = d.get(sys.argv[1], "")
    print("" if v is None else v)
except Exception:
    print("__PARSE_ERROR__")
PY
}

echo "== negativas basicas =="
before=$(get_state); c0=$(crash_count)
for spec in \
  'cmd_inexistente|{"cmd":"no_such_cmd"}' \
  'app_inexistente|{"cmd":"app","text":"NoSuchApp"}' \
  'mode_invalido|{"cmd":"mode","val":"XYZ"}' \
  'mode_tipo_invalido|{"cmd":"mode","val":123}' \
  'freq_negativa|{"cmd":"freq","val":-1}' \
  'pwr_fuera_rango|{"cmd":"pwr","val":999}' \
  'filter_fuera_rango|{"cmd":"filter","val":999999}' \
  'json_malformado|{not json}' \
  'campo_val_faltante|{"cmd":"freq"}' ; do
    case=${spec%%|*}; json=${spec#*|}
    resp=$(send_raw "$json"); rc=$?
    sleep 0.3
    after=$(get_state); c1=$(crash_count); cdelta=$((c1 - c0))
    # verdicto por caso: debe ser err/sin crash; para los de rango, estado sin cambio
    verdict=ok; note=""
    case $case in
      cmd_inexistente|app_inexistente|mode_invalido|mode_tipo_invalido|json_malformado|campo_val_faltante)
          printf '%s' "$resp" | grep -q '"t":"err"' || { verdict=review; note="no respondio err"; }
          ;;
      freq_negativa)
          [ "$(field "$after" freq)" = "$(field "$before" freq)" ] || { verdict=review; note="freq cambio"; }
          ;;
      pwr_fuera_rango)
          [ "$(field "$after" pwr)" = "$(field "$before" pwr)" ] || { verdict=review; note="pwr cambio"; }
          ;;
      filter_fuera_rango)
          [ "$(field "$after" filter)" = "$(field "$before" filter)" ] || { verdict=review; note="filter cambio"; }
          ;;
    esac
    [ "$cdelta" -ne 0 ] && { verdict=fail; note="CRASHLOG nuevo"; }
    record "$case" "$json" "$resp" "$rc" "$before" "$after" "$cdelta" "$verdict" "$note"
    printf '%-22s rc=%d verdict=%-6s resp=%s\n' "$case" "$rc" "$verdict" "${resp:0:90}"
done

echo "== off duplicado =="
before=$(get_state); c0=$(crash_count)
r1=$(send_raw '{"cmd":"app","text":"off"}'); sleep 0.3
r2=$(send_raw '{"cmd":"app","text":"off"}'); sleep 0.3
after=$(get_state); c1=$(crash_count)
app_ok=1; [ "$(field "$after" app)" = "" ] || app_ok=0
record "off_duplicado" '{"cmd":"app","text":"off"} x2' "$r1 | $r2" 0 "$before" "$after" $((c1-c0)) "$([ $app_ok -eq 1 ] && echo ok || echo review)" "app final='$(field "$after" app)'"

echo "== desconexion abrupta (JSON parcial y cierre) =="
before=$(get_state); c0=$(crash_count)
docker exec "$CT" chroot /t /usr/bin/python3 -c '
import socket, time
s=socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect("/tmp/x6200_remote.sock")
s.sendall(b"{\"cmd\":\"get\"")
s.close()
' 2>&1
sleep 1
after=$(get_state); c1=$(crash_count)
alive=review; printf '%s' "$after" | grep -q '"t":"state"' && alive=ok
record "desconexion_abrupta" 'connect+send parcial+close' "post_get_alive=$alive" 0 "$before" "$after" $((c1-c0)) "$alive" ""

echo "== reconexion: 10 handshakes consecutivos =="
ok=0; for i in $(seq 1 10); do r=$(get_state); printf '%s' "$r" | grep -q '"t":"state"' && ok=$((ok+1)); done
after=$(get_state)
record "reconexion_10" '10 x {"cmd":"get"}' "ok=$ok/10" 0 "" "$after" 0 "$([ "$ok" -eq 10 ] && echo ok || echo review)" "clientes efimeros"

echo "== estado final =="
get_state
echo "crashlog_total=$(crash_count)"
