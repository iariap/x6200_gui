#!/usr/bin/env bash
#
# Corrida del binario ARM del animador bajo qemu-arm-static (issue #204).
#
# No hay radio ni chroot del banco en este entorno, asi que se corre el MISMO
# codigo de src/bootanim/** compilado para ARM con el toolchain de Buildroot,
# con el shim de framebuffer sintetico linkeado (LD_PRELOAD no sirve: se
# aplicaria a qemu, que es x86_64).
#
#   arm_det240 / arm_det358: -DBOOTANIM_TEST (reloj sintetico + limite de
#     cuadros) para comparar byte a byte contra el oraculo del host.
#   arm_prod: el binario de PRODUCCION (sin ese macro) con reloj acelerado
#     100x: se verifica que siga vivo y animando pasado los 30 s virtuales y
#     que salga limpio con SIGUSR1.
set -euo pipefail

WT="${WT:-/home/pablo/desa/asf/state/worktrees/x6200/bootanim-v2-204}"
TC="${TC:-/home/pablo/desa/x6200/rom-build/clone-buildroot/build/host}"
SYS="$TC/arm-buildroot-linux-gnueabihf/sysroot"
CC="$TC/bin/arm-buildroot-linux-gnueabihf-gcc"
QEMU="${QEMU:-qemu-arm-static}"
OUT="${OUT:-/tmp/opencode/ba-arm}"
SEAL="$(cd "$WT" && ./tools/fuente-md5.sh .)"

mkdir -p "$OUT"

echo "== toolchain: $($CC --version | head -1)"
echo "== sello de fuentes del arbol: $SEAL"
echo "== compilacion ARM (target Linux 6.1 armv7l, sysroot de Buildroot)"

CFLAGS_ARM="-std=c11 -Wall -Wextra -Werror --sysroot=$SYS -O2 -DX6200_FUENTE_MD5=\"$SEAL\" -I $WT/src/bootanim -I $WT/tests/bootanim"
SRCS="$WT/src/bootanim/renderer.c $WT/src/bootanim/fb.c $WT/src/bootanim/main.c"

$CC $CFLAGS_ARM -o "$OUT/arm_det240" $SRCS "$WT/tests/bootanim/fake_fb.c" -DBOOTANIM_TEST -DBA_TEST_FRAME_LIMIT=240 -ldl
$CC $CFLAGS_ARM -o "$OUT/arm_det358" $SRCS "$WT/tests/bootanim/fake_fb.c" -DBOOTANIM_TEST -DBA_TEST_FRAME_LIMIT=358 -ldl
$CC $CFLAGS_ARM -o "$OUT/arm_prod" $SRCS "$WT/tests/bootanim/fake_fb.c" -ldl

file "$OUT/arm_det240" "$OUT/arm_prod"

# Oracion del host: mismo geometry, mismo volcado independiente del gate.
LB=$(cd "$WT" && cc -std=c11 -Wall -Wextra -Werror -I src -I src/bootanim -I tests/bootanim \
	-o "$OUT/replay_dump_host" tests/bootanim/replay_dump.c src/bootanim/renderer.c src/bootanim/fb.c && echo ok)

make_fb() { # <path> <bytes>
	python3 - "$1" "$2" <<'PY'
import sys
open(sys.argv[1], 'wb').write(b'\x5a' * int(sys.argv[2]))
PY
}

echo
echo "== 1) cuadro 240 (20.160 ms: logo fijo + lluvia) con 16bpp y stride +64 =="
make_fb "$OUT/fb-arm-240.bin" $(( (480 * 2 + 64) * 800 ))
"$OUT/replay_dump_host" "$OUT/exp-host-240.bin" 16 64 0 0 rgb 240
env BA_TEST_FB="$OUT/fb-arm-240.bin" BA_TEST_BPP=16 BA_TEST_PAD=64 BA_TEST_BITFIELDS=rgb \
	"$QEMU" -L "$SYS" "$OUT/arm_det240"
echo "exit=$?"
if cmp -s "$OUT/fb-arm-240.bin" "$OUT/exp-host-240.bin"; then
	echo "IDENTICO al oraculo del host (819200 bytes)"
else
	echo "DIFIERE del oraculo del host"
	exit 1
fi

echo
echo "== 2) cuadro 358 (30.072 ms: STARTUP DELAYED y ciclo vivo) =="
make_fb "$OUT/fb-arm-358.bin" $(( (480 * 2 + 64) * 800 ))
"$OUT/replay_dump_host" "$OUT/exp-host-358.bin" 16 64 0 0 rgb 358
env BA_TEST_FB="$OUT/fb-arm-358.bin" BA_TEST_BPP=16 BA_TEST_PAD=64 BA_TEST_BITFIELDS=rgb \
	"$QEMU" -L "$SYS" "$OUT/arm_det358"
echo "exit=$?"
if cmp -s "$OUT/fb-arm-358.bin" "$OUT/exp-host-358.bin"; then
	echo "IDENTICO al oraculo del host (819200 bytes, >30 s y con el ciclo vivo)"
else
	echo "DIFIERE del oraculo del host"
	exit 1
fi

echo
echo "== 3) produccion ARM: vivo pasado 30 s virtuales y corte por SIGUSR1 =="
make_fb "$OUT/fb-arm-prod.bin" $(( 480 * 2 * 800 ))
env BA_TEST_FB="$OUT/fb-arm-prod.bin" BA_TEST_SPEED=100 "$QEMU" -L "$SYS" "$OUT/arm_prod" \
	>"$OUT/arm-prod.log" 2>&1 &
QPID=$!

marker() {
	python3 - "$1" <<'PY'
import sys
row = 480 * 2
bg = bytes([0x82, 0x08])
d = open(sys.argv[1], 'rb').read()

for lx in range(240, 267):
    for ly in range(380, 401):
        off = (799 - lx) * row + ly * 2

        if d[off:off + 2] and d[off:off + 2] != bg:
            sys.exit(0)

sys.exit(1)
PY
}

i=0
while [ "$i" -lt 200 ]; do
	if marker "$OUT/fb-arm-prod.bin"; then
		break
	fi

	kill -0 "$QPID" 2>/dev/null || { echo "el animador ARM se murio antes de los 30 s"; exit 1; }
	sleep 0.05
	i=$((i + 1))
done
[ "$i" -lt 200 ] || { echo "no llego a STARTUP DELAYED_"; exit 1; }
kill -0 "$QPID" 2>/dev/null || { echo "salio solo en los 30 s: el contrato #204 dice que sigue"; exit 1; }
M1="$(md5sum "$OUT/fb-arm-prod.bin" | cut -d' ' -f1)"
sleep 0.25
M2="$(md5sum "$OUT/fb-arm-prod.bin" | cut -d' ' -f1)"
[ "$M1" != "$M2" ] || { echo "pasados los 30 s la pantalla quedo congelada"; exit 1; }

T0=$(date +%s%N)
kill -USR1 "$QPID"
RC=0
wait "$QPID" || RC=$?
T1=$(date +%s%N)
DT=$(( (T1 - T0) / 1000000 ))
echo "vivo y animando pasado 30 s virtuales; SIGUSR1 -> exit=$RC en $DT ms reales"
[ "$RC" -eq 0 ] || exit 1
[ "$DT" -lt 150 ] || { echo "tardo $DT ms"; exit 1; }
cat "$OUT/arm-prod.log"

echo
echo "== 4) el cuadro final del ARM sigue siendo del ciclo (paleta + cajas) =="
python3 - "$OUT/fb-arm-prod.bin" <<'PY'
import sys
d = open(sys.argv[1], 'rb').read()
row = 480 * 2
bg, prim, sec, hi = bytes([0x82, 0x08]), bytes([0x56, 0x87]), bytes([0xb0, 0x74]), bytes([0xfc, 0xd7])
from collections import Counter
c = Counter(d[i:i + 2] for i in range(0, len(d), 2))
ajeno = [p for p in c if p not in (bg, prim, sec, hi)]

def centered(text, scale, y1, y2):
    w = scale * (6 * len(text) - 1)
    x = 400 - w // 2
    return (x, y1, x + w - 1, y2)

bxs = [
    centered("X6200 / SYSTEM STARTUP", 3, 49, 69),
    centered("X6200", 10, 168, 254),
    centered("HF TRANSCEIVER", 3, 287, 307),
    (240, 380, 560, 400),
    (116, 112, 125, 347), (182, 112, 191, 347), (608, 112, 617, 347), (676, 112, 685, 347),
]
fuera = 0
for y in range(800):
    for x in range(480):
        off = y * row + x * 2
        if d[off:off + 2] == bg:
            continue
        lx, ly = 799 - y, x
        if not any(x1 <= lx <= x2 and y1 <= ly <= y2 for x1, y1, x2, y2 in bxs):
            fuera += 1
if ajeno or fuera or c[prim] == 0 or c[sec] == 0:
    print("colores ajenos:", ajeno[:3], "fuera de cajas:", fuera, "cajas:", bxs)
    sys.exit(1)
print("ok: paleta del contrato (con destello), cajas del ciclo y cuadro final conservado")
PY

echo
echo "PASS: animador ARM (qemu-arm-static) identico al oraculo del host y vivo hasta la cesion"
