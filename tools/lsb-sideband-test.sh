#!/bin/sh
#
# Validacion de banda lateral LSB/USB (#292) en la radio, con portadora real
# (AM 790 kHz de noche) y con FT8 en 40 m.
#
# Uso:  tools/lsb-sideband-test.sh
#       tools/lsb-sideband-test.sh --ft8     (solo la parte de 7.4/7.2 MHz)
#       tools/lsb-sideband-test.sh --carrier (solo la portadora)
#
# Que mide: con el dial 1 kHz ABAJO de la portadora de 790 kHz, un LSB correcto
# la FILTRA (tono 1 kHz bajo) y un USB correcto la OYE (tono alto); con el dial
# 1 kHz ARRIBA es al reves. Eso distingue la banda lateral sin depender del oido.
#
# Requisitos: radio por WiFi (tools/lib-radio.sh), sshpass y el cliente
# tools/remote/x6200-remote (se sube solo). Deja la radio en 14.192410 LSB.
#
# Salida: PASS/FAIL por prueba + codigo de salida 0/1.

set -u

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
QA_DIR=$(CDPATH= cd -- "$HERE/.." && pwd)
cd "$QA_DIR" || exit 1

# shellcheck source=tools/lib-radio.sh
. "$HERE/lib-radio.sh"

SSH_OPTS="-F /dev/null -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout=10 -o LogLevel=ERROR -o PreferredAuthentications=password -o PubkeyAuthentication=no"
OUT="$QA_DIR/assessment-194/scratch/cw244/lsb-test"
mkdir -p "$OUT"

MODO=${1:-todo}
[ "$MODO" = "--ft8" ] && MODO=ft8
[ "$MODO" = "--carrier" ] && MODO=carrier

IP=$(radio_ip 2>/dev/null)
if [ -z "$IP" ] || ! ping -c1 -W2 "$IP" >/dev/null 2>&1; then
  echo "FAIL: la radio no responde por WiFi (ip='${IP}')"
  exit 1
fi
echo "Radio: $IP"

r() { sshpass -p 123 ssh $SSH_OPTS "root@$IP" "$@" 2>/dev/null | tr -d '\r'; }

# Cliente remoto: se sube siempre (en /tmp se pierde en cada reboot)
sshpass -p 123 scp $SSH_OPTS "$HERE/remote/x6200-remote" "root@$IP:/tmp/x6200-remote" >/dev/null 2>&1 || {
  echo "FAIL: no se pudo subir x6200-remote"; exit 1; }
r 'chmod 755 /tmp/x6200-remote'

echo "GUI: $(r 'pidof x6200_gui')  md5: $(r 'md5sum /usr/sbin/x6200_gui | cut -d" " -f1 | cut -c1-12')"

# Estado de audio: si el BASE quedo muteado por una prueba de CW (#244) no se
# puede medir nada de audio; se avisa y se corta.
RMS=$(r 'parec --format=s16le --rate=44100 --channels=1 --device=alsa_input.platform-sound.stereo-fallback >/tmp/lsbt.raw 2>/dev/null & P=$!; sleep 2; kill $P 2>/dev/null; od -An -td2 -v /tmp/lsbt.raw 2>/dev/null | awk "{for(i=1;i<=NF;i++){v=\$i<0?-\$i:\$i;s+=v;n++}} END{printf \"%.0f\",(n?s/n:0)}"')
echo "Audio RX rms: $RMS (sano > 2000)"
if [ "${RMS:-0}" -lt 800 ]; then
  echo "FAIL: el audio de RX esta muteado (estado de BASE; requiere power-cycle, ver #244)."
  echo "      Hace falta apagar y encender la radio y volver a correr el test."
  exit 1
fi

# Captura 3 s con el estado pedido
cap() { # $1=nombre  $2=freq  $3=modo  $4=segundos
  r "/tmp/x6200-remote \"{\\\"cmd\\\":\\\"freq\\\",\\\"val\\\":$2}\" >/dev/null 2>&1
     /tmp/x6200-remote \"{\\\"cmd\\\":\\\"mode\\\",\\\"val\\\":\\\"$3\\\"}\" >/dev/null 2>&1
     sleep 2
     parec --format=s16le --rate=44100 --channels=1 --device=alsa_input.platform-sound.stereo-fallback >/tmp/lsb-$1.raw 2>/dev/null & P=\$!; sleep ${4:-3}; kill \$P 2>/dev/null"
  sshpass -p 123 scp $SSH_OPTS "root@$IP:/tmp/lsb-$1.raw" "$OUT/$1.raw" >/dev/null 2>&1
}

restore() {
  r '/tmp/x6200-remote "{\"cmd\":\"freq\",\"val\":14192410}" >/dev/null 2>&1
     /tmp/x6200-remote "{\"cmd\":\"mode\",\"val\":\"LSB\"}" >/dev/null 2>&1
     /tmp/x6200-remote "{\"cmd\":\"vol\",\"val\":16}" >/dev/null 2>&1
     hciconfig hci0 down 2>/dev/null'
}

FAIL=0

if [ "$MODO" = todo ] || [ "$MODO" = carrier ]; then
  echo
  echo "== Portadora AM 790 kHz =="
  cap 789-LSB 789000 LSB
  cap 789-USB 789000 USB
  cap 791-LSB 791000 LSB
  cap 791-USB 791000 USB
fi

if [ "$MODO" = todo ] || [ "$MODO" = ft8 ]; then
  echo
  echo "== FT8 40 m (7.072 arriba del FT8) =="
  cap 7072-LSB 7072000 LSB
  cap 7072-USB 7072000 USB
  cap 7074-LSB 7074000 LSB
  cap 7074-USB 7074000 USB
fi

restore

python3 - "$OUT" "$MODO" <<'PY'
import sys, os
import numpy as np

out, modo = sys.argv[1], sys.argv[2]

def spec(name):
    path = os.path.join(out, name + '.raw')
    try:
        a = np.fromfile(path, dtype='<i2').astype(float)
    except OSError:
        return None
    a = a[:len(a)//2*2]
    if len(a) < 20000:
        return None
    sp = np.abs(np.fft.rfft(a*np.hanning(len(a))))
    fr = np.fft.rfftfreq(len(a), 1/44100)
    return sp, fr, float(np.sqrt(np.mean(a**2)))

def tone(sp, fr, f0=1000.0, w=150.0):
    m = (fr > f0-w) & (fr < f0+w)
    ref = (fr > 2000) & (fr < 4000)
    return 20*np.log10((np.max(sp[m])+1e-9)/(np.median(sp[ref])+1e-9))

ok = True

if modo in ('todo', 'carrier'):
    print()
    print("Portadora 790 kHz (esperado LSB: 789 bajo, 791 alto):")
    vals = {}
    for c in ('789-LSB', '789-USB', '791-LSB', '791-USB'):
        s = spec(c)
        if s is None:
            print(f"  {c:9s} SIN DATOS"); ok = False; continue
        sp, fr, rms = s
        d = tone(sp, fr)
        vals[c] = d
        print(f"  {c:9s} tono1k={d:5.1f} dB  rms={rms:5.0f}")
    if len(vals) == 4:
        cond = (vals['789-LSB'] < 48 and vals['789-USB'] > 55 and
                vals['791-LSB'] > 55 and vals['791-USB'] < 48)
        print("  RESULTADO:", "PASS (LSB y USB demodulan lados opuestos)" if cond
              else "FAIL (la banda lateral no cambia como en el firmware original)")
        ok = ok and cond

if modo in ('todo', 'ft8'):
    print()
    print("FT8 7.072/7.074: energia relativa en 1.7-2.5 kHz")
    e = {}
    for c in ('7072-LSB', '7072-USB', '7074-LSB', '7074-USB'):
        s = spec(c)
        if s is None:
            print(f"  {c:9s} SIN DATOS"); ok = False; continue
        sp, fr, rms = s
        m = (fr > 1700) & (fr < 2500)
        ref = (fr > 600) & (fr < 1000)
        d = 20*np.log10((np.max(sp[m])+1e-9)/(np.median(sp[ref])+1e-9))
        e[c] = d
        print(f"  {c:9s} E+2k={d:5.1f} dB  rms={rms:5.0f}")
    # Informativo: en 7.072 un LSB correcto deberia oir bastante menos que USB
    if '7072-LSB' in e and '7072-USB' in e:
        print("  RESULTADO FT8:", "PASS (LSB oye menos arriba que USB)" if e['7072-LSB'] < e['7072-USB'] - 6
              else "INCONCLUSIVO (banda con senales en ambos lados; vale la portadora)")

print()
print("VEREDICTO:", "PASS" if ok else "FAIL")
sys.exit(0 if ok else 1)
PY
