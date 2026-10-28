#!/bin/sh
# run-bench-arm.sh (host) — corre EN LA RADIO el banco ARM compilado por
# build-bench-arm.sh y recolecta evidencia para el gate G2 del issue #347.
#
# Protocolo (fijado por el issue #347):
#   - mismo binario ARM + fixture S16 44100 del #345 (white_noise_10s, semilla 6200)
#   - 3 warmups descartados + 5 pasadas medidas de 60 s de audio por pasada
#   - nice -n 10, `timeout` de 30 s por pasada (implementado con watchdog: la radio
#     no trae el applet timeout de BusyBox)
#   - monitoreo continuo de CPU de la GUI y de la radio desde /proc (monitor.sh)
#   - snapshot del estado RX (GET read-only) antes y despues; nada se escribe
#   - si hay degradacion (loadavg, CPU saturada, GUI alta), monitor.sh mata el
#     banco y esta wrapper aborta: FAIL_SAFETY
#
# No abre ALSA/PulseAudio en la radio, no toca /dev/fb0, no cambia VFO/modo/
# volumen/mute/NR/BT ni escribe registros del BASE. Copia solo sus binarios y el
# fixture a su propio /tmp, y los borra al salir.
#
# Uso:
#   RADIO_IP=<ip> sh docs/evidencia/2026-10-28-rx-audio-hardware/scripts/run-bench-arm.sh
#
# Variables: SCRATCH, OUT, LOGS, RADIO_IP, SKIP_UPLOAD, ONLY_VARIANT
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -eu

EV=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
REPO=$(CDPATH= cd -- "$EV/../../.." && pwd)
FEAS="$REPO/docs/evidencia/2026-10-28-rx-audio-feasibility"
SCRATCH=${SCRATCH:-"<workspace>/.qa-347-scratch"}
OUT=${OUT:-$SCRATCH/bench-arm}
LOGS=${LOGS:-$EV/logs}
REMOTE_DIR=/tmp/x6200-rnnoise-bench-20261028
RADIO_IP=${RADIO_IP:-}
ONLY_VARIANT=${ONLY_VARIANT:-}
SKIP_UPLOAD=${SKIP_UPLOAD:-0}
TMO=30
SECONDS_PER_PASS=${SECONDS_PER_PASS:-60}
PROTOCOL=${PROTOCOL:-full}
mkdir -p "$LOGS"

log()  { echo "### $*"; }
cmd()  { echo "CMD: $*" >&2; }
fail() { echo "FALLO: $*" >&2; exit 1; }

# Para no publicar IP ni credenciales en los logs: el host/credenciales salen de
# tools/lib-radio.sh (helper existente de este repo) y el IP se enmascara en los
# mensajes.  RADIO_IP se toma del entorno del operador, nunca se escribe aca.
. "$REPO/tools/lib-radio.sh"
[ -n "$RADIO_IP" ] || RADIO_IP=$(radio_ip) || fail "no se encontro la radio (helper lib-radio.sh)"
mask() { echo "<radio>"; }

ssh_r() { cmd "ssh $(mask) $*"; ssh_radio "$RADIO_IP" "$@"; }
scp_r() { cmd "scp $(mask) $*"; sshpass -p "$RADIO_CLAVE" scp -F /dev/null \
    -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR \
    -o PreferredAuthentications=password -o PubkeyAuthentication=no "$@"; }

# ---------------------------------------------------------------------------
# 0. Precondiciones del host
# ---------------------------------------------------------------------------
log "precondiciones del host"
echo "repo=$REPO"
echo "out=$OUT"
echo "logs=$LOGS"
echo "fecha_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "worktree_head=$(git -C "$REPO" rev-parse HEAD)"
[ -x "$OUT/bench_rnnoise_arm_default" ] || fail "falta $OUT/bench_rnnoise_arm_default (correr build-bench-arm.sh)"
[ -x "$OUT/bench_rnnoise_arm_neonoff" ] || fail "falta $OUT/bench_rnnoise_arm_neonoff"
sha256sum "$OUT"/bench_rnnoise_arm_* | tee "$LOGS/bench-bin.sha256"
echo "fixture_origen=gen_pcm.py (issue #345) vector white_noise 10 s, seed 6200"
echo "protocolo=$PROTOCOL seconds_per_pass=$SECONDS_PER_PASS timeout_por_pasada=${TMO}s"

# ---------------------------------------------------------------------------
# 1. Fixture (mismo del #345) + verificacion del manifiesto
# ---------------------------------------------------------------------------
log "fixture: verificar gen_pcm.py contra el manifiesto del #345 (read-only)"
( cd "$FEAS" && python3 scripts/gen_pcm.py --verify ) | tee "$LOGS/gen_pcm-verify.log"
[ "$(tail -1 "$LOGS/gen_pcm-verify.log")" = "OK: los cuatro WAV regenerados coinciden byte a byte con el manifiesto" ] \
    || fail "gen_pcm.py --verify no dio OK"

log "fixture: construir el .raw S16LE 44100 mono desde el blob verificado (sin reescribir el manifiesto)"
FIXTURE="$OUT/fixture-white-noise-10s.raw"
python3 - "$FEAS/scripts/gen_pcm.py" "$FIXTURE" <<'PY' | tee "$LOGS/fixture.log"
import hashlib, importlib.util, io, sys, wave

src, dst = sys.argv[1], sys.argv[2]
spec = importlib.util.spec_from_file_location("gen_pcm", src)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)
manifest, blobs = mod.build_manifest()
entry = next(f for f in manifest["files"] if f["file"] == "white_noise_44100_s16.wav")
wav = blobs[entry["file"]]
with wave.open(io.BytesIO(wav)) as w:
    assert w.getnchannels() == 1 and w.getsampwidth() == 2 and w.getframerate() == 44100
    frames = w.readframes(w.getnframes())
assert hashlib.sha256(wav).hexdigest() == entry["sha256"], "el WAV no coincide con el manifiesto"
open(dst, "wb").write(frames)
print(f"wav_sha256={entry['sha256']}")
print(f"raw_sha256={hashlib.sha256(frames).hexdigest()}")
print(f"raw_bytes={len(frames)} samples={len(frames)//2}")
PY
[ -s "$FIXTURE" ] || fail "no se genero el fixture $FIXTURE"

# ---------------------------------------------------------------------------
# 2. Preflight de la radio (primero, cero efectos)
# ---------------------------------------------------------------------------
log "preflight read-only de la radio"
ssh_r 'date -u; uname -a' | tee "$LOGS/radio-preflight.txt"
ssh_r 'cat /proc/cpuinfo | grep -E "^(processor|model name|CPU part|Features)" ' | tee -a "$LOGS/radio-preflight.txt"
ssh_r 'cat /proc/asound/cards; df -h /tmp; free -m; cat /proc/loadavg' | tee -a "$LOGS/radio-preflight.txt"
GUI_PID=$(ssh_r 'pidof x6200_gui 2>/dev/null | awk "{print \$1}"')
[ -n "$GUI_PID" ] || GUI_PID=$(ssh_r 'ps w | awk "/x6200_gui/ && !/awk/ {print \$1; exit}"')
[ -n "$GUI_PID" ] || fail "la GUI no esta corriendo en la radio (no hay x6200_gui): BLOCKED"
echo "gui_pid=$GUI_PID" | tee -a "$LOGS/radio-preflight.txt"
GUI_SHA=$(ssh_r "sha256sum /usr/sbin/x6200_gui 2>/dev/null | cut -d' ' -f1" || true)
GUI_EXE=$(ssh_r "readlink -f /proc/$GUI_PID/exe")
echo "gui_sha256=$GUI_SHA" | tee -a "$LOGS/radio-preflight.txt"
echo "gui_exe=$GUI_EXE" | tee -a "$LOGS/radio-preflight.txt"
ssh_r 'pactl list short sources; echo ---; pactl list short sinks; echo ---; pactl list short modules' \
    2>&1 | tee "$LOGS/radio-pa-pre.txt"
ssh_r "cat /proc/asound/card0/pcm0c/sub0/status 2>/dev/null; echo ---play---; cat /proc/asound/card0/pcm0p/sub0/status 2>/dev/null" \
    | tee "$LOGS/radio-asound-pre.txt"
ssh_r 'ps w | grep -E "x6200_gui|pulseaudio|bluetoothd" | grep -v grep' | tee -a "$LOGS/radio-preflight.txt"
# snapshot RX (solo GET read-only) — si no esta el cliente, queda UNVERIFIED
if ssh_r 'test -x /tmp/x6200-remote'; then
    ssh_r '/tmp/x6200-remote "{\"cmd\":\"get\"}"' | tee "$LOGS/get-pre.json"
    echo "estado_rx=MEASURED (GET read-only)" | tee -a "$LOGS/radio-preflight.txt"
else
    echo "estado_rx=UNVERIFIED (no existe /tmp/x6200-remote en la radio)" | tee -a "$LOGS/radio-preflight.txt"
fi
# espacio/RAM minimos para copiar el binario (3.6 MB) + fixture (883 KB)
ssh_r 'df -k /tmp | tail -1 | awk "{print \$4}"' | tee "$LOGS/radio-free-kb.txt"
ssh_r 'free -m | awk "/Mem:/{print \$7}"' | tee -a "$LOGS/radio-free-kb.txt"
FREE_KB=$(ssh_r 'df -k /tmp | tail -1 | awk "{print \$4}"')
[ "$FREE_KB" -ge 51200 ] || fail "menos de 50 MB libres en /tmp de la radio: BLOCKED"
MEM_AVAIL=$(ssh_r 'free -m | awk "/Mem:/{print \$7}"')
[ "$MEM_AVAIL" -ge 128 ] || fail "menos de 128 MB disponibles de RAM en la radio: BLOCKED"
echo "preflight OK: /tmp libre=${FREE_KB}kB RAM disponible=${MEM_AVAIL}MB"

# ---------------------------------------------------------------------------
# 3. Copiar SOLO nuestros archivos a un directorio temporal propio
# ---------------------------------------------------------------------------
log "copia del banco a $REMOTE_DIR (despues del preflight)"
ssh_r "rm -rf $REMOTE_DIR && mkdir -p $REMOTE_DIR"
for f in bench_rnnoise_arm_default bench_rnnoise_arm_neonoff; do
    scp_r "$OUT/$f" "$RADIO_USUARIO@$RADIO_IP:$REMOTE_DIR/$f"
done
scp_r "$EV/scripts/radio/monitor.sh" "$RADIO_USUARIO@$RADIO_IP:$REMOTE_DIR/monitor.sh"
scp_r "$EV/scripts/radio/run-one.sh" "$RADIO_USUARIO@$RADIO_IP:$REMOTE_DIR/run-one.sh"
scp_r "$FIXTURE" "$RADIO_USUARIO@$RADIO_IP:$REMOTE_DIR/fixture.raw"
ssh_r "chmod 755 $REMOTE_DIR/monitor.sh $REMOTE_DIR/run-one.sh $REMOTE_DIR/bench_rnnoise_arm_*"
for f in bench_rnnoise_arm_default bench_rnnoise_arm_neonoff; do
    lsha=$(sha256sum "$OUT/$f" | cut -d' ' -f1)
    rsha=$(ssh_r "sha256sum $REMOTE_DIR/$f | cut -d' ' -f1")
    [ "$lsha" = "$rsha" ] || fail "sha256 de $f no coincide en la radio"
done
echo "binarios copiados y verificados (sha256)"
local_sha=$(sha256sum "$FIXTURE" | cut -d' ' -f1)
remote_sha=$(ssh_r "sha256sum $REMOTE_DIR/fixture.raw | cut -d' ' -f1")
[ "$local_sha" = "$remote_sha" ] || fail "el fixture en la radio no coincide (sha256)"
log "fixture copiado y verificado (sha256=$local_sha)"

# ---------------------------------------------------------------------------
# 4. Controles del binario ARM (antes de la matriz)
# ---------------------------------------------------------------------------
log "controles: frame size de RNNoise y bypass off bit-exacto, en la radio"
ssh_r "$REMOTE_DIR/bench_rnnoise_arm_default --rnnoise-frame-size" | tee "$LOGS/control-frame-size.txt"
FRAME=$(tail -1 "$LOGS/control-frame-size.txt")
[ "$FRAME" = "480" ] || fail "--rnnoise-frame-size no dio 480 (dio '$FRAME')"
ssh_r "$REMOTE_DIR/bench_rnnoise_arm_neonoff --rnnoise-frame-size" | tee -a "$LOGS/control-frame-size.txt"
ssh_r "cd $REMOTE_DIR && ./bench_rnnoise_arm_default --case control_off --in fixture.raw --method off --seconds 10 --out control_off.raw" \
    | tee "$LOGS/control-off.json"
ssh_r "cd $REMOTE_DIR && cmp -s control_off.raw fixture.raw && echo OFF_BIT_EXACT_OK || echo OFF_DIFF" \
    | tee -a "$LOGS/control-off.json"

# ---------------------------------------------------------------------------
# 5. Matriz: 3 warmups + 5 pasadas por variante
# ---------------------------------------------------------------------------
get_state() { ssh_r '/tmp/x6200-remote "{\"cmd\":\"get\"}"' 2>/dev/null || echo "UNVERIFIED"; }
get_state > "$LOGS/get-before.json"
START_UPTIME=$(ssh_r 'cut -d" " -f1 /proc/uptime')
MON_MAX=900
log "arranca monitor pasivo en la radio (1 Hz, corta el banco si hay degradacion)"
MON_PID=$(ssh_r "cd $REMOTE_DIR && { nohup sh monitor.sh $GUI_PID $REMOTE_DIR/monitor.log $MON_MAX >/dev/null 2>&1 & echo \$!; }")
echo "monitor_pid=$MON_PID"
sleep 2
ssh_r "test -s $REMOTE_DIR/monitor.log && echo MONITOR_UP || echo MONITOR_DOWN" | tail -1

aborted=0
run_pass() {
    variant=$1
    case_name=$2
    outfile=$3
    log "pasada $variant $case_name (nice -n 10, timeout ${TMO}s)"
    set +e
    rc_line=$(ssh_r "cd $REMOTE_DIR && nice -n 10 sh run-one.sh $outfile $TMO ./bench_rnnoise_arm_$variant --case $case_name --in fixture.raw --method rnnoise --seconds 60" 2>/dev/null)
    ssh_rc=$?
    set -e
    pass_rc=$(printf '%s\n' "$rc_line" | tail -1)
    case "$pass_rc" in
        ''|*[!0-9]*) pass_rc=$ssh_rc ;;
    esac
    echo "$pass_rc" > "$LOGS/exit_$outfile.txt"
    echo "PASS_RESULT variant=$variant case=$case_name exit=$pass_rc"
    scp_r "$RADIO_USUARIO@$RADIO_IP:$REMOTE_DIR/$outfile.out" "$LOGS/$outfile.json" 2>/dev/null || true
    scp_r "$RADIO_USUARIO@$RADIO_IP:$REMOTE_DIR/$outfile.err" "$LOGS/$outfile.err" 2>/dev/null || true
    # seguridad: revisar el monitor despues de cada pasada
    mon=$(ssh_r "grep -c SAFETY_ABORT $REMOTE_DIR/monitor.log 2>/dev/null || echo 0" | tail -1)
    case "$mon" in ''|*[!0-9]*) mon=0 ;; esac
    if [ "$mon" -gt 0 ]; then
        log "SAFETY_ABORT detectado por monitor.sh — se detiene la matriz (FAIL_SAFETY)"
        aborted=1
    fi
    if [ "$pass_rc" = "124" ]; then
        log "TIMEOUT de ${TMO}s en $variant/$case_name (presupuesto excedido)"
        aborted=2
    fi
}

if [ "$SKIP_UPLOAD" = "1" ]; then
    log "SKIP_UPLOAD=1: se omite la corrida (solo preflight/controles)"
else
    for variant in default neonoff; do
        [ -n "$ONLY_VARIANT" ] && [ "$variant" != "$ONLY_VARIANT" ] && continue
        if [ "$PROTOCOL" = "diag" ]; then
            pass_total=2      # diagnostico acotado: 2 pasadas de $SECONDS_PER_PASS s
            measured_from=1
        elif [ "$variant" = "default" ]; then
            pass_total=8      # 3 warmups + 5 medidas (protocolo del issue)
            measured_from=4
        else
            pass_total=4      # sonda de sensibilidad: 1 warmup + 3 pasadas
            measured_from=2
        fi
        i=1
        while [ "$i" -le "$pass_total" ]; do
            if [ "$PROTOCOL" = "diag" ]; then
                tag="diag$i"
            elif [ "$i" -lt "$measured_from" ]; then
                tag="warmup$i"
            else
                tag="pass$((i - measured_from + 1))"
            fi
            run_pass "$variant" "${variant}_${tag}" "bench_${variant}_${tag}"
            [ "$aborted" -eq 0 ] || break
            i=$((i + 1))
        done
        [ "$aborted" -eq 0 ] || break
    done
fi

# ---------------------------------------------------------------------------
# 6. Cortar el monitor, estado despues y limpieza del temporal propio
# ---------------------------------------------------------------------------
log "corta el monitor pasivo"
ssh_r "kill $MON_PID 2>/dev/null || true"
sleep 1
scp_r "$RADIO_USUARIO@$RADIO_IP:$REMOTE_DIR/monitor.log" "$LOGS/monitor.log" 2>/dev/null || true
for f in $(ssh_r "ls $REMOTE_DIR/*.out $REMOTE_DIR/*.json 2>/dev/null"); do
    base=$(basename "$f")
    scp_r "$RADIO_USUARIO@$RADIO_IP:$f" "$LOGS/remote-$base" 2>/dev/null || true
done

get_state > "$LOGS/get-after.json"
ssh_r 'pactl list short sources; echo ---; pactl list short sinks; echo ---; pactl list short modules' \
    2>&1 | tee "$LOGS/radio-pa-post.txt"
ssh_r "cat /proc/asound/card0/pcm0c/sub0/status 2>/dev/null; echo ---play---; cat /proc/asound/card0/pcm0p/sub0/status 2>/dev/null" \
    | tee "$LOGS/radio-asound-post.txt"
ssh_r "ps w | grep -E 'x6200_gui|pulseaudio' | grep -v grep" | tee "$LOGS/radio-ps-post.txt"
ssh_r 'free -m; cat /proc/loadavg; dmesg 2>/dev/null | tail -5' | tee "$LOGS/radio-post-load.txt"
ssh_r "sha256sum /usr/sbin/x6200_gui 2>/dev/null | cut -d' ' -f1" | tee "$LOGS/gui-sha-post.txt"
GUI_PID_POST=$(ssh_r 'pidof x6200_gui 2>/dev/null | awk "{print \$1}"')
echo "gui_pid_post=$GUI_PID_POST" | tee -a "$LOGS/radio-ps-post.txt"

log "limpieza: se borra SOLO $REMOTE_DIR"
ssh_r "rm -rf $REMOTE_DIR"
ssh_r "test -e $REMOTE_DIR && echo LEFTOVER || echo CLEAN" | tee "$LOGS/cleanup.txt"

log "fin. exit_aborted=$aborted"
exit 0
