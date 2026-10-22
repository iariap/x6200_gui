#!/bin/bash
# ============================================================================
#  FASE B (#199) — QA físico REMOTO RX (HR-01..HR-06) — versión corregida
#
#  Ejecuta el runbook HR-01..06 del dossier (05-summary/hardware-plan.md) sobre
#  la radio ENCENDIDA y en red, con sesión EXCLUSIVA, SOLO RX, sin TX, sin flash,
#  sin reformatear. Copia el cliente de control ARM al equipo y guarda evidencia.
#
#  USO:  ./phase-b-remote.sh --i-have-authorization [--label <nombre>]
#  Sin autorización: exit 3.  Sin radio: BLOCKED, exit 4.
# ============================================================================
set -uo pipefail
AUFLAG=0; LABEL=""
while [ $# -gt 0 ]; do
  case "$1" in
    --i-have-authorization) AUFLAG=1 ;;
    --label) shift; LABEL="${1:-}" ;;
    *) echo "argumento desconocido: $1"; exit 2 ;;
  esac; shift
done
[ "$AUFLAG" = "1" ] || { echo "RECHAZADO: falta --i-have-authorization"; exit 3; }

BASE=/home/pablo/desa/x6200; QA=$BASE/x6200_qa
TS=$(date -u +%Y%m%dT%H%M%SZ); OUT=$BASE/assessment-194/fase-b/${LABEL:-$TS}
mkdir -p "$OUT"/{capturas,logs,json}
exec > >(tee -a "$OUT/logs/run.log") 2>&1
say(){ echo "[$(date -u +%H:%M:%SZ)] $*"; }
blocked(){ say "BLOCKED: $*"; printf '%s\n' "$*" > "$OUT/BLOCKED"; exit 4; }

say "===== FASE B #199 — QA físico remoto RX (HR-01..06) ====="
say "salida: $OUT"
cd "$QA" || blocked "no existe $QA"
. tools/lib-radio.sh 2>/dev/null || blocked "no carga tools/lib-radio.sh"
IP=$(radio_ip 2>/dev/null); [ -n "$IP" ] || blocked "radio no detectada"
ping -c1 -W2 "$IP" >/dev/null 2>&1 || blocked "radio sin ping ($IP)"
say "radio_ip=$IP ✓"

SSH_OPTS=(-F /dev/null -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout=8 -o LogLevel=ERROR -o PreferredAuthentications=password -o PubkeyAuthentication=no)
rsh(){ sshpass -p 123 ssh "${SSH_OPTS[@]}" root@"$IP" "$@" 2>/dev/null | tr -d '\r'; }
rget(){ rsh "/tmp/x6200-remote '{\"cmd\":\"get\"}'"; }
rapp(){ rsh "/tmp/x6200-remote '{\"cmd\":\"app\",\"text\":\"$1\"}'"; }
rfreq(){ rsh "/tmp/x6200-remote '{\"cmd\":\"freq\",\"val\":$1}'"; }
sping(){ date +%s%3N 2>/dev/null || date +%s; }

# ---------- cliente de control ----------
say "--- preparación: cliente de control ARM -> /tmp/x6200-remote ---"
if rsh 'test -x /tmp/x6200-remote && echo ok' | grep -q ok; then
  say "  cliente ya presente ✓"
else
  sshpass -p 123 scp "${SSH_OPTS[@]}" "$QA/tools/remote/x6200-remote" root@"$IP":/tmp/x6200-remote >/dev/null 2>&1 && say "  cliente copiado ✓" || blocked "no pude copiar el cliente de control"
  rsh 'chmod 755 /tmp/x6200-remote' >/dev/null 2>&1
fi
rget > "$OUT/json/hr01-get-inicial.json"
[ -s "$OUT/json/hr01-get-inicial.json" ] || blocked "el cliente no responde (get vacío)"
say "  get inicial: $(head -c 160 "$OUT/json/hr01-get-inicial.json")"

# ---------- HR-01 ----------
say "--- HR-01: exclusividad, identidad y snapshot (solo lectura) ---"
rsh 'ss -tn 2>/dev/null | grep ":22" | grep -vE "0.0.0.0|LISTEN" || true' > "$OUT/logs/hr01-ssh-conns.txt"
OURIP=$(ip -4 addr show 2>/dev/null | grep -oE 'inet 192\.168\.[0-9]+\.[0-9]+' | head -1 | cut -d' ' -f2)
AJENAS=$(grep -v "${OURIP:-noip}" "$OUT/logs/hr01-ssh-conns.txt" 2>/dev/null | grep -c ESTABLISHED || true)
say "  conexiones SSH ajenas (no de $OURIP): $AJENAS"
rsh 'echo "== identidad =="; echo "boot_id=$(cat /proc/sys/kernel/random/boot_id)"; echo "uptime=$(cut -d" " -f1 /proc/uptime)"; echo "kernel=$(uname -r)"; echo "gui_pid=$(pidof x6200_gui)"; echo "daemon_pid=$(pidof x6200_daemon)"; echo "gui_md5=$(md5sum /usr/sbin/x6200_gui | cut -d" " -f1)"; echo "gui_size=$(wc -c < /usr/sbin/x6200_gui)"; echo "boot_identity=$(grep -ai boot_identity /tmp/x6200_log.txt | tail -1)"; echo "== montajes =="; mount | grep -E " /mnt | / "; echo "== lastbreath =="; cat /mnt/lastbreath; echo; echo "== crashlog =="; grep -ac "CRASHLOG sig" /tmp/x6200_log.txt; echo "== espacio =="; df -h / /mnt' > "$OUT/logs/hr01-identidad.txt"
sed 's/^/    /' "$OUT/logs/hr01-identidad.txt" | head -26

# ---------- HR-02 ----------
say "--- HR-02: disponibilidad y baseline física ---"
rget > "$OUT/json/hr02-get-inicial.json"
[ -x tools/captura-radio.sh ] && { timeout 90 tools/captura-radio.sh "$OUT/capturas/home-before.png" > "$OUT/logs/hr02-captura.log" 2>&1 && say "  captura home-before ✓" || say "  captura con aviso"; }
[ -x tools/verifica-radio.sh ] && { timeout 180 tools/verifica-radio.sh > "$OUT/logs/hr02-verifica.log" 2>&1 && say "  verifica-radio OK ✓" || say "  verifica-radio con avisos"; }

# ---------- HR-03 ----------
say "--- HR-03: navegación RX verificable (8 apps, 20 s c/u) ---"
for APP in Settings BT WiFi GPS SSTV WeFax CW FT8; do
  rapp "$APP" > "$OUT/json/hr03-$APP-open.json"
  sleep 20
  rget > "$OUT/json/hr03-$APP-get.json"
  [ -x tools/captura-radio.sh ] && timeout 90 tools/captura-radio.sh "$OUT/capturas/hr03-$APP.png" >/dev/null 2>&1
  rapp off > "$OUT/json/hr03-$APP-close.json"
  sleep 3
  say "  $APP: abierto, capturado, cerrado ✓"
done

# ---------- HR-04 ----------
say "--- HR-04: QSY reversible 20 repeticiones ---"
F0=$(python3 - "$OUT/json/hr02-get-inicial.json" <<'PY'
import json,sys
try:
    d=json.load(open(sys.argv[1])); print(d.get("freq") or d.get("freq_a") or "")
except Exception: print("")
PY
)
say "  F0=$F0"
if [ -n "${F0:-}" ] && [ "$F0" != "0" ]; then
  F1=$((F0+100)); ok=0
  for i in $(seq 1 20); do
    T0=$(sping); rfreq "$F1" > "$OUT/json/hr04-set-$i.json"; T1=$(sping)
    sleep 1
    rfreq "$F0" > "$OUT/json/hr04-restore-$i.json"; T2=$(sping)
    sleep 1
    G=$(rget); echo "$G" > "$OUT/json/hr04-get-$i.json"
    echo "$G" | grep -q "\"freq\":$F0" && ok=$((ok+1))
    echo "rep=$i set_ms=$((T1-T0)) restore_ms=$((T2-T1))" >> "$OUT/logs/hr04-latencias.txt"
  done
  say "  QSY reversible: $ok/20 volvieron a $F0"
else say "  BLOCKED HR-04: F0 no disponible"; fi

# ---------- HR-05 ----------
say "--- HR-05: estabilidad y recursos 60 min RX ---"
END=$(( $(date +%s) + 3600 ))
: > "$OUT/logs/hr05-serie.tsv"
printf 'epoch,utc,uptime_s,boot_id,crashlog,gui_pid,daemon_pid,utime,stime,rss_kb,fds,threads,params_md5,get_bytes,get_ms\n' >> "$OUT/logs/hr05-serie.tsv"
while [ "$(date +%s)" -lt "$END" ]; do
  NOW=$(date +%s); UTC=$(date -u +%H:%M:%S)
  P=$(rsh 'U=$(cut -d" " -f1 /proc/uptime); B=$(cat /proc/sys/kernel/random/boot_id); C=$(grep -ac "CRASHLOG sig" /tmp/x6200_log.txt); G=$(pidof x6200_gui); D=$(pidof x6200_daemon); if [ -n "$G" ]; then T=$(awk "{print \$14\",\"\$15}" /proc/$G/stat); R=$(awk "/VmRSS/{print \$2}" /proc/$G/status); F=$(ls /proc/$G/fd|wc -l); TH=$(awk "/Threads/{print \$2}" /proc/$G/status); else T=","; R=; F=; TH=; fi; PM=$(md5sum /mnt/params.db|cut -d" " -f1); echo "$U|$B|$C|$G|$D|$T|$R|$F|$TH|$PM"')
  T0=$(sping); GB=$(rget); T1=$(sping)
  GB_N=$(printf '%s' "$GB" | wc -c); GB_MS=$((T1-T0))
  echo "$NOW,$UTC,$P,$GB_N,$GB_MS" >> "$OUT/logs/hr05-serie.tsv"
  sleep 60
done
say "  serie: $(wc -l < "$OUT/logs/hr05-serie.tsv") muestras"

# ---------- HR-06 ----------
say "--- HR-06: cierre y restauración ---"
[ -n "${F0:-}" ] && { rfreq "$F0" >/dev/null 2>&1; say "  freq restaurada a $F0 ✓"; }
rapp off >/dev/null 2>&1
rget > "$OUT/json/hr06-get-final.json"
[ -x tools/captura-radio.sh ] && timeout 90 tools/captura-radio.sh "$OUT/capturas/home-after.png" >/dev/null 2>&1
say "  estado final capturado ✓"
say "===== FIN HR-01..06. HW-02..HW-08 requieren instrumentos y autorización específica (TX aparte) ====="
echo "$OUT" > "$BASE/assessment-194/fase-b/ultimo.txt"
