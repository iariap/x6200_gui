#!/usr/bin/env bash
#
# Gate del arranque neo-retro con cesion segura al waterfall (issues #192 y
# #204: ciclo continuo de Decrypt/Beams/Rain hasta el handoff).
#
# Corre sin docker, sin buildroot y sin /dev/fb0:
#
#   1. tests nativos de host del renderer (modelo: intro, cursor, lluvia, BEAMS,
#      DECRYPT, ciclo de 6 s y los 30 s como cambio de texto; layout; geometria
#      de las cuatro lanes; higiene de los rectangulos sucios; determinismo) y
#      del backend de framebuffer (rotacion 90 grados contra un oraculo
#      independiente de LVGL, formatos 16/32 bpp, bitfields, stride con padding,
#      offsets, casos negativos de geometry, mmap y permisos, guard bytes
#      centinela, fixture de las lanes de lluvia) y de la cesion del framebuffer
#      (PIDFILE, identidad por /proc/<pid>/exe, 100 ms + 20 ms, SIGTERM/SIGKILL),
#      en build normal y bajo ASan/UBSan;
#   2. el binario REAL x6200_bootanim corriendo en el host con un shim
#      (LD_PRELOAD, tests/bootanim/fake_fb.c) que le da un /dev/fb0 sintetico:
#      a) el binario determinista del gate (-DBOOTANIM_TEST, reloj sintetico y
#         limite de cuadros) tiene que dejar, byte a byte, el mismo framebuffer
#         que el volcado independiente (tests/bootanim/replay_dump.c) en la
#         intro, en BEAMS (cuadro 32), en DECRYPT (cuadro 45), en el umbral de
#         los 30 s (357/358) y despues (500);
#      b) la version de PRODUCCION no puede terminar sola: se la para con
#         SIGUSR1/SIGTERM (en el arranque y despues de los 30 s virtuales, con
#         reloj acelerado), sale en <150 ms, conserva el ultimo cuadro y sigue
#         animando pasado el umbral; ademas se mide que no haga busy-wait;
#      c) los caminos degradados salen 0 sin escribir nada;
#   3. el script de init S01bootanim (sintaxis, singleton, PIDFILE, identidad,
#      stop con un start-stop-daemon doble) y los chequeos de integracion (prune
#      del S95gui, orden del handoff en src/main.c, callback screen_rec_flush
#      intacto, target en CMake, macro del banco solo en el gate);
#   4. mediciones reales en host (tests/bootanim/render_bench.c): pixeles sucios
#      y bytes por cuadro de cada fase, sin umbrales.
#
# Uso: bash tests/check_bootanim.sh
#   CC=<compilador>   (default cc)
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/src/bootanim"
T="$ROOT/tests/bootanim"
CC="${CC:-cc}"
WORK="$(mktemp -d)"
SAN_OK=0
ANIM_PID=""
OTHER_PID=""

cleanup() {
	for p in "$ANIM_PID" "$OTHER_PID" "${PID_HOST:-}"; do
		[ -n "$p" ] || continue
		kill -9 "$p" 2>/dev/null || true
	done

	if [ -n "$OTHER_PID" ]; then
		wait "$OTHER_PID" 2>/dev/null || true
	fi

	rm -rf "$WORK"
}
trap cleanup EXIT

fail() { echo "FAIL: $*" >&2; exit 1; }
step() { echo; echo "== $* =="; }

pid_alive() { # un zombi ya no cuenta como vivo
	[ -e "/proc/$1" ] || return 1

	case "$(cat "/proc/$1/stat" 2>/dev/null)" in
		*") Z "*) return 1 ;;
	esac

	return 0
}

wait_gone() { # <pid> : hasta-condicion con techo (2 s)
	i=0
	while [ "$i" -lt 20 ] && pid_alive "$1"; do
		sleep 0.1
		i=$((i + 1))
	done

	! pid_alive "$1"
}

CFLAGS_COMMON="-std=c11 -Wall -Wextra -Werror -I $ROOT/src -I $SRC -I $T"

# ---------------------------------------------------------------------------
step "1) compilacion host (-std=c11 -Wall -Wextra -Werror)"
# ---------------------------------------------------------------------------
$CC $CFLAGS_COMMON -o "$WORK/renderer_test" "$T/renderer_test.c" "$SRC/renderer.c"
$CC $CFLAGS_COMMON -o "$WORK/fb_test" "$T/fb_test.c" "$SRC/fb.c" "$SRC/renderer.c"
$CC $CFLAGS_COMMON -o "$WORK/handoff_test" "$T/handoff_test.c" "$ROOT/src/boot_splash.c"
$CC $CFLAGS_COMMON -o "$WORK/fake_animator" "$T/fake_animator.c"
$CC $CFLAGS_COMMON -o "$WORK/x6200_bootanim" "$SRC/renderer.c" "$SRC/fb.c" "$SRC/main.c"
$CC $CFLAGS_COMMON -o "$WORK/replay_dump" "$T/replay_dump.c" "$SRC/renderer.c" "$SRC/fb.c"
$CC $CFLAGS_COMMON -o "$WORK/render_bench" "$T/render_bench.c" "$SRC/renderer.c" "$SRC/fb.c"
$CC -std=c11 -Wall -Wextra -Werror -shared -fPIC -o "$WORK/fake_fb.so" "$T/fake_fb.c" -ldl

# Binarios deterministas del gate: reloj sintetico + limite de cuadros, uno por
# limite (el macro es de compilacion: la ROM nunca lo define, y el gate lo
# verifica sobre el preprocesado en el paso 7).
for lim in 240 32 45 357 358 500; do
	$CC $CFLAGS_COMMON -DBOOTANIM_TEST -DBA_TEST_FRAME_LIMIT=$lim -o "$WORK/x6200_bootanim_det_$lim" \
		"$SRC/renderer.c" "$SRC/fb.c" "$SRC/main.c"
done

echo "ok: renderer, backend, handoff, animador, volcado, mediciones, shim y 6 binarios deterministas compilan sin avisos"

step "2) tests unitarios (modelo, layout, raster, framebuffer)"
"$WORK/renderer_test"
"$WORK/fb_test"

step "3) cesion del framebuffer (procesos reales, identidad por /proc/<pid>/exe)"
"$WORK/handoff_test" "$WORK/fake_animator"

# ---------------------------------------------------------------------------
step "4) sanitizers (ASan + UBSan)"
# ---------------------------------------------------------------------------
SAN_ERR=""
if SAN_ERR="$(echo 'int main(void){return 0;}' | $CC -x c -fsanitize=address,undefined -o "$WORK/san_probe" - 2>&1)"; then
	SAN_OK=1
	$CC $CFLAGS_COMMON -fsanitize=address,undefined -fno-omit-frame-pointer -o "$WORK/renderer_test_asan" \
		"$T/renderer_test.c" "$SRC/renderer.c"
	$CC $CFLAGS_COMMON -fsanitize=address,undefined -fno-omit-frame-pointer -o "$WORK/fb_test_asan" \
		"$T/fb_test.c" "$SRC/fb.c" "$SRC/renderer.c"
	$CC $CFLAGS_COMMON -fsanitize=address,undefined -fno-omit-frame-pointer -o "$WORK/handoff_test_asan" \
		"$T/handoff_test.c" "$ROOT/src/boot_splash.c"
	ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$WORK/renderer_test_asan" ||
		fail "renderer_test fallo bajo ASan/UBSan"
	ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$WORK/fb_test_asan" ||
		fail "fb_test fallo bajo ASan/UBSan"
	ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$WORK/handoff_test_asan" "$WORK/fake_animator" ||
		fail "handoff_test fallo bajo ASan/UBSan"
	echo "ok: los tres tests pasan bajo ASan + UBSan"
else
	echo "SKIP: este entorno no puede linkear los sanitizers con $CC:"
	echo "      $SAN_ERR"
fi

# ---------------------------------------------------------------------------
step "5) el binario REAL contra un /dev/fb0 sintetico (shim LD_PRELOAD)"
# ---------------------------------------------------------------------------
FB_DIR="$WORK/fb"
mkdir -p "$FB_DIR"

# El archivo que hace de framebuffer arranca con el MISMO patron que el volcado
# esperado (0x5A): el padding tiene que quedar intacto en los dos y, ademas,
# cualquier byte escrito fuera de lugar rompe la comparacion.
make_fb_file() { # <path> <bytes>
	python3 - "$1" "$2" <<'PY'
import sys
open(sys.argv[1], 'wb').write(b'\x5a' * int(sys.argv[2]))
PY
}

# Camino determinista: el binario compilado con -DBOOTANIM_TEST y
# -DBA_TEST_FRAME_LIMIT usa un reloj sintetico que avanza un cuadro por
# iteracion y termina SOLO en el cuadro pedido; su framebuffer tiene que ser
# byte a byte el del volcado independiente. Es el unico camino que puede
# compararse byte a byte: la version de PRODUCCION no tiene ese limite (corta
# solo con SIGUSR1/SIGTERM) y se prueba aparte.
run_det() { # <fb> <log> <bpp> <pad> <bitfields> <limite>
	local fb="$1" log="$2" bpp="$3" pad="$4" bf="$5" lim="$6" rc=0

	timeout 30 env LD_PRELOAD="$WORK/fake_fb.so" BA_TEST_FB="$fb" BA_TEST_BPP="$bpp" \
		BA_TEST_PAD="$pad" BA_TEST_BITFIELDS="$bf" "$WORK/x6200_bootanim_det_$lim" >"$log" 2>&1 || rc=$?
	[ "$rc" -eq 0 ] || fail "el binario de test (limite=$lim) no salio solo: rc=$rc (124 = colgado)"
}

# El binario de produccion no puede terminar solo: se lo arranca en segundo
# plano y se lo para con una senal. El oraculo es la senal (y el tiempo de
# salida), nunca un timeout.
start_animator() { # <fb> <log> [VAR=valor...]
	local fb="$1" log="$2"

	shift 2
	env LD_PRELOAD="$WORK/fake_fb.so" BA_TEST_FB="$fb" "$@" "$WORK/x6200_bootanim" >"$log" 2>&1 &
	ANIM_PID=$!
}

# true si el pie ya muestra STARTUP DELAYED_: la zona logica x=240..266
# (y=380..400) solo puede tener pixeles de la variante demorada, porque
# > INITIALIZING_ empieza en x=267. Es la evidencia observable de que paso los
# 30 s SIN salir (el contrato viejo terminaba ahi).
delayed_marker() { # <fb>
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

# hasta-condicion con techo (5 s): espera el pie demorado y falla antes si el
# animador se murio (que es justamente el bug del contrato viejo).
wait_delayed() { # <fb>
	local i=0

	while [ "$i" -lt 100 ]; do
		if delayed_marker "$1"; then
			return 0
		fi

		pid_alive "$ANIM_PID" || return 1
		sleep 0.05
		i=$((i + 1))
	done

	return 1
}

# Paleta del contrato (incluido el destello del BEAMS), cajas de CUALQUIER
# cuadro del ciclo (logo, textos, pie y las cuatro lanes de lluvia) y cuadro no
# vacio: sirve tanto para el revelado como para los 30 s o los 120 s.
check_frame() { # <fb> <descripcion>
	python3 - "$1" "$2" <<'PY' || fail "el cuadro final '$2' no cumple los invariantes del contrato"
import sys
d = open(sys.argv[1], 'rb').read()
row = 480 * 2
bg, prim, sec, hi = bytes([0x82, 0x08]), bytes([0x56, 0x87]), bytes([0xb0, 0x74]), bytes([0xfc, 0xd7])
from collections import Counter
c = Counter(d[i:i + 2] for i in range(0, len(d), 2))
ajeno = [p for p in c if p not in (bg, prim, sec, hi)]
if ajeno:
    print("colores fuera de la paleta del contrato:", ajeno[:5])
    sys.exit(1)
if c[prim] == 0 or c[sec] == 0:
    print("el ultimo cuadro quedo vacio: se borro la pantalla al salir")
    sys.exit(1)

def width(text, scale):
    return scale * (6 * len(text) - 1)

def centered(text, scale, y1, y2):
    w = width(text, scale)
    x = 400 - w // 2
    return (x, y1, x + w - 1, y2)

bxs = [
    centered("X6200 / SYSTEM STARTUP", 3, 49, 69),
    centered("X6200", 10, 168, 254),
    centered("HF TRANSCEIVER", 3, 287, 307),
    (240, 380, 560, 400),   # union de las dos variantes del pie
    (116, 112, 125, 347),   # lanes de lluvia del contrato
    (182, 112, 191, 347),
    (608, 112, 617, 347),
    (676, 112, 685, 347),
]
fuera = 0
muestra = []
for y in range(800):
    for x in range(480):
        off = y * row + x * 2
        if d[off:off + 2] == bg:
            continue
        log_x, log_y = 799 - y, x
        if not any(x1 <= log_x <= x2 and y1 <= log_y <= y2 for x1, y1, x2, y2 in bxs):
            fuera += 1
            if len(muestra) < 5:
                muestra.append((log_x, log_y, d[off:off + 2].hex()))
if fuera:
    print(f"{fuera} pixeles fuera de las cajas del contrato")
    print("cajas (logico x1,y1,x2,y2):", bxs)
    print("muestra (log_x, log_y, color):", muestra)
    sys.exit(1)
print("ok: paleta del contrato (con destello), cajas del ciclo respetadas y ultimo cuadro conservado")
PY
}

# 5.1 los formatos/stride del contrato: el cuadro 240 (20.160 ms, logo fijo +
#     lluvia) del binario determinista tiene que ser identico, byte a byte, al
#     volcado independiente (renderer + fb reales).
for caso in "16 64 rgb" "16 0 rgb" "32 0 rgb" "32 0 bgr"; do
	set -- $caso
	CASE_BPP="$1"
	CASE_PAD="$2"
	CASE_BF="$3"
	LINE=$((480 * CASE_BPP / 8 + CASE_PAD))
	SIZE=$((LINE * 800))
	FB="$FB_DIR/fb-$CASE_BPP-$CASE_PAD-$CASE_BF.bin"
	EXP="$FB_DIR/exp-$CASE_BPP-$CASE_PAD-$CASE_BF.bin"

	make_fb_file "$FB" "$SIZE"
	run_det "$FB" "$WORK/log-$CASE_BPP-$CASE_PAD-$CASE_BF.txt" "$CASE_BPP" "$CASE_PAD" "$CASE_BF" 240
	"$WORK/replay_dump" "$EXP" "$CASE_BPP" "$CASE_PAD" 0 0 "$CASE_BF" 240

	cmp -s "$FB" "$EXP" ||
		fail "el framebuffer de ${CASE_BPP}bpp pad=$CASE_PAD $CASE_BF no coincide con el volcado esperado"
	echo "ok: ${CASE_BPP}bpp line_length=$LINE ($SIZE bytes) $CASE_BF: cuadro 240 identico byte a byte al volcado independiente"
done

# El binario deja su sello de fuentes (issue #165) cuando toma el framebuffer.
grep -q 'x6200_bootanim: fuente' "$WORK/log-16-64-rgb.txt" ||
	fail "el animador no loguea el sello de fuentes"
echo "ok: el animador loguea su sello de fuentes ($(cat "$WORK/log-16-64-rgb.txt"))"

# 5.2 fases BEAMS y DECRYPT en el proceso real: los cuadros 32 (2.688 ms) y 45
#     (3.780 ms) tambien tienen que coincidir byte a byte con el volcado.
for lim in 32 45; do
	FB="$FB_DIR/fb-fase-$lim.bin"
	EXP="$FB_DIR/exp-fase-$lim.bin"
	make_fb_file "$FB" "$((480 * 2 * 800))"
	run_det "$FB" "$WORK/log-fase-$lim.txt" 16 0 rgb "$lim"
	"$WORK/replay_dump" "$EXP" 16 0 0 0 rgb "$lim"
	cmp -s "$FB" "$EXP" || fail "el cuadro $lim (fase del ciclo) no coincide con el volcado"
	echo "ok: cuadro $lim ($((lim * 84)) ms) identico byte a byte al volcado independiente"
done

# 5.3 los 30 s son SOLO un cambio de texto: los cuadros 357 (29.988 ms),
#     358 (30.072 ms) y 500 (42.000 ms) coinciden byte a byte con el volcado, y
#     la diferencia entre 357 y 358 queda confinada al pie (texto) y a las lanes
#     de lluvia: nada de pantalla completa, logo ni fundido.
LINE=$((480 * 2 + 64))
SIZE=$((LINE * 800))

for lim in 357 358 500; do
	FB="$FB_DIR/fb-t-$lim.bin"
	EXP="$FB_DIR/exp-t-$lim.bin"
	make_fb_file "$FB" "$SIZE"
	run_det "$FB" "$WORK/log-t-$lim.txt" 16 64 rgb "$lim"
	"$WORK/replay_dump" "$EXP" 16 64 0 0 rgb "$lim"
	cmp -s "$FB" "$EXP" || fail "el cuadro $lim no coincide con el volcado independiente"
done

cmp -s "$FB_DIR/fb-t-357.bin" "$FB_DIR/fb-t-358.bin" &&
	fail "el pie no cambio en el umbral de los 30 s (cuadros 357 y 358 iguales)"

python3 - "$FB_DIR/fb-t-357.bin" "$FB_DIR/fb-t-358.bin" <<'PY' || fail "los 30 s cambiaron pixeles fuera del pie y de la lluvia"
import sys
row = 480 * 2 + 64
a = open(sys.argv[1], 'rb').read()
b = open(sys.argv[2], 'rb').read()
bxs = [(240, 380, 560, 400), (116, 112, 125, 347), (182, 112, 191, 347),
       (608, 112, 617, 347), (676, 112, 685, 347)]
fuera = 0
muestra = []
for Y in range(800):
    for X in range(480):
        off = Y * row + X * 2
        if a[off:off + 2] == b[off:off + 2]:
            continue
        log_x, log_y = 799 - Y, X
        if not any(x1 <= log_x <= x2 and y1 <= log_y <= y2 for x1, y1, x2, y2 in bxs):
            fuera += 1
            if len(muestra) < 5:
                muestra.append((log_x, log_y))
if fuera:
    print(f"{fuera} pixeles cambiaron fuera del pie/lluvia en el umbral de los 30 s:", muestra)
    sys.exit(1)
print("ok: entre 29.988 y 30.072 ms solo cambiaron el pie (texto) y la lluvia")
PY
echo "ok: los 30 s son solo texto (cuadros 357/358/500 identicos al volcado)"

# 5.4 el proceso de PRODUCCION vive mas alla de los 30 s virtuales (reloj
#     acelerado 100x) y sigue dibujando: se lo para con SIGUSR1 y sale al
#     instante (<150 ms reales) sin borrar el ultimo cuadro.
FB="$FB_DIR/fb-live.bin"
make_fb_file "$FB" "$((480 * 2 * 800))"
start_animator "$FB" "$WORK/log-live.txt" BA_TEST_SPEED=100
pid_alive "$ANIM_PID" || fail "el animador no arranco"

wait_delayed "$FB" || fail "no llego a STARTUP DELAYED_ (o se murio antes: el contrato dice que sigue)"
pid_alive "$ANIM_PID" || fail "el animador salio solo en los 30 s: el contrato #204 dice que sigue"
MD5_1="$(md5sum "$FB" | cut -d' ' -f1)"
sleep 0.25
MD5_2="$(md5sum "$FB" | cut -d' ' -f1)"
[ "$MD5_1" != "$MD5_2" ] ||
	fail "pasados los 30 s virtuales la pantalla quedo congelada (mismo framebuffer)"

T0=$(date +%s%N)
kill -USR1 "$ANIM_PID" || fail "no se pudo avisar al animador"
RC=0
wait "$ANIM_PID" || RC=$?
ANIM_PID=""
T1=$(date +%s%N)
DT_MS=$(( (T1 - T0) / 1000000 ))
[ "$RC" -eq 0 ] || fail "el animador salio con $RC ante SIGUSR1"
[ "$DT_MS" -lt 150 ] || fail "el animador tardo $DT_MS ms en salir ante SIGUSR1 (contrato: inmediato)"
check_frame "$FB" "SIGUSR1 despues de los 30 s"
echo "ok: vivo y animando pasado 30 s virtuales; SIGUSR1 -> salida en $DT_MS ms con codigo 0"

# 5.5 SIGTERM: sale limpio antes de los 30 s (arranque) y tambien despues.
FB="$FB_DIR/fb-term-intro.bin"
make_fb_file "$FB" "$((480 * 2 * 800))"
start_animator "$FB" "$WORK/log-term-intro.txt" BA_TEST_SPEED=100
pid_alive "$ANIM_PID" || fail "el animador no arranco (SIGTERM temprano)"
T0=$(date +%s%N)
kill -TERM "$ANIM_PID" || fail "no se pudo mandar SIGTERM"
RC=0
wait "$ANIM_PID" || RC=$?
ANIM_PID=""
T1=$(date +%s%N)
DT_MS=$(( (T1 - T0) / 1000000 ))
[ "$RC" -eq 0 ] || fail "el animador salio con $RC ante SIGTERM temprano"
[ "$DT_MS" -lt 150 ] || fail "el animador tardo $DT_MS ms en salir ante SIGTERM (contrato: inmediato)"
check_frame "$FB" "SIGTERM en el arranque"
echo "ok: SIGTERM en el arranque -> salida en $DT_MS ms con codigo 0, cuadro del revelado intacto"

FB="$FB_DIR/fb-term-late.bin"
make_fb_file "$FB" "$((480 * 2 * 800))"
start_animator "$FB" "$WORK/log-term-late.txt" BA_TEST_SPEED=100
wait_delayed "$FB" || fail "SIGTERM tardio: no llego a STARTUP DELAYED_"
T0=$(date +%s%N)
kill -TERM "$ANIM_PID" || fail "no se pudo mandar SIGTERM tardio"
RC=0
wait "$ANIM_PID" || RC=$?
ANIM_PID=""
T1=$(date +%s%N)
DT_MS=$(( (T1 - T0) / 1000000 ))
[ "$RC" -eq 0 ] || fail "el animador salio con $RC ante SIGTERM tardio"
[ "$DT_MS" -lt 150 ] || fail "el animador tardo $DT_MS ms en salir ante SIGTERM tardio"
check_frame "$FB" "SIGTERM despues de los 30 s"
echo "ok: SIGTERM despues de los 30 s -> salida en $DT_MS ms con codigo 0, cuadro intacto"

# 5.6 sin busy-wait: a reloj real el consumo de CPU queda muy por debajo del
#     tiempo de pared (una espera activa lo dejaria cerca del 100%), asi que el
#     proceso puede vivir indefinidamente sin comerse la CPU del arranque.
FB="$FB_DIR/fb-cpu.bin"
make_fb_file "$FB" "$((480 * 2 * 800))"
start_animator "$FB" "$WORK/log-cpu.txt"
pid_alive "$ANIM_PID" || fail "el animador no arranco (CPU)"
CPU_T0=$(date +%s%N)
sleep 3 # intervalo de muestreo (la condicion es el ratio, no el tiempo)
CPU_T1=$(date +%s%N)
read -r UTIME STIME <<<"$(awk '{print $14, $15}' "/proc/$ANIM_PID/stat")"
CLK="$(getconf CLK_TCK)"
CPU_MS=$(( (UTIME + STIME) * 1000 / CLK ))
WALL_MS=$(( (CPU_T1 - CPU_T0) / 1000000 ))
kill -TERM "$ANIM_PID" 2>/dev/null || true
wait "$ANIM_PID" 2>/dev/null || true
ANIM_PID=""
[ "$WALL_MS" -gt 2000 ] || fail "la ventana de medicion de CPU fue de $WALL_MS ms"
[ "$CPU_MS" -lt $((WALL_MS / 4)) ] ||
	fail "consumo de CPU ${CPU_MS} ms en ${WALL_MS} ms de pared (>=25%): parece busy-wait"
echo "ok: consumo real ${CPU_MS} ms de CPU en ${WALL_MS} ms de reloj ($((CPU_MS * 100 / WALL_MS))%): sin busy-wait"

# 5.7 caminos degradados: salida 0 y NADA escrito en el framebuffer. El timeout
#     del helper es solo una red de seguridad: el oraculo es el codigo de salida.
FB_DEG="$FB_DIR/fb-degraded.bin"
make_fb_file "$FB_DEG" "$((480 * 2 * 800))"

degraded() { # <descripcion> [VAR=valor...]
	local desc="$1"
	shift
	local before after rc=0
	before="$(md5sum "$FB_DEG" | cut -d' ' -f1)"
	timeout 30 env LD_PRELOAD="$WORK/fake_fb.so" BA_TEST_FB="$FB_DEG" "$@" "$WORK/x6200_bootanim" \
		>"$WORK/log-degraded.txt" 2>&1 || rc=$?
	[ "$rc" -eq 0 ] || fail "camino degradado '$desc' no salio con 0 (rc=$rc)"
	after="$(md5sum "$FB_DEG" | cut -d' ' -f1)"
	[ "$before" = "$after" ] || fail "camino degradado '$desc' ESCRIBIO en el framebuffer"
	echo "ok: $desc -> exit 0 sin escribir en el framebuffer"
}

degraded "resolucion que no es 480x800" BA_TEST_XRES=640
degraded "bpp no soportado (24)" BA_TEST_BPP=24
degraded "xoffset distinto de cero" BA_TEST_XOFF=4
degraded "yoffset distinto de cero" BA_TEST_YOFF=8
degraded "line_length demasiado corto" BA_TEST_PAD=-2
degraded "smem_len insuficiente" BA_TEST_SMEM_SHORT=1
degraded "mmap fallido" BA_TEST_MMAP_FAIL=1

# 5.8 dispositivo ausente: reintenta ~2 s y sale 0 EN SILENCIO (el boot normal
#     sigue igual), sin crear ni escribir nada.
T0=$(date +%s%N)
timeout 30 env LD_PRELOAD="$WORK/fake_fb.so" BA_TEST_FB="$FB_DIR/no-existe/fb0" "$WORK/x6200_bootanim" \
	>"$WORK/log-absent.txt" 2>&1 ||
	fail "sin /dev/fb0 el animador no salio con 0"
T1=$(date +%s%N)
TRIES_MS=$(( (T1 - T0) / 1000000 ))
if [ -s "$WORK/log-absent.txt" ]; then
	fail "sin /dev/fb0 el animador escribio en stderr: $(cat "$WORK/log-absent.txt")"
fi
[ "$TRIES_MS" -lt 4000 ] || fail "los reintentos tardaron ${TRIES_MS} ms (contrato: 20 x 100 ms)"
if [ -e "$FB_DIR/no-existe/fb0" ]; then
	fail "el animador creo el dispositivo"
fi
echo "ok: sin /dev/fb0 reintenta y sale 0 en silencio (${TRIES_MS} ms)"

# 5.9 sin permisos (root ignora los permisos: se informa el motivo).
if [ "$(id -u)" -eq 0 ]; then
	echo "SKIP: caso 'sin permisos' del animador omitido (uid 0 no aplica los permisos del archivo)"
else
	FB_PERM="$FB_DIR/fb-noperm.bin"
	make_fb_file "$FB_PERM" "$((480 * 2 * 800))"
	chmod 000 "$FB_PERM"
	timeout 30 env LD_PRELOAD="$WORK/fake_fb.so" BA_TEST_FB="$FB_PERM" "$WORK/x6200_bootanim" \
		>"$WORK/log-noperm.txt" 2>&1 || fail "sin permisos el animador no salio con 0"
	chmod 644 "$FB_PERM"
	grep -q 'sin animacion' "$WORK/log-noperm.txt" || fail "sin permisos no quedo el motivo en stderr"
	echo "ok: framebuffer sin permisos -> exit 0 con aviso en stderr"
fi

# 5.10 mediciones reales en host (no son umbrales: los valores van al reporte).
"$WORK/render_bench"

# ---------------------------------------------------------------------------
step "6) script de init S01bootanim"
# ---------------------------------------------------------------------------
INIT="$ROOT/rootfs/etc/init.d/S01bootanim"
bash -n "$INIT"
[ -x "$INIT" ] || fail "$INIT no es ejecutable"
head -1 "$INIT" | grep -q '^#!/bin/sh$' || fail "el shebang no es #!/bin/sh"
grep -q 'start-stop-daemon -S -b -q -m -p "\$PIDFILE" -x "\$BIN"' "$INIT" ||
	fail "no aparece la invocacion exacta de start-stop-daemon del contrato"
# Los chequeos de "no toca la GUI" y "no usa killall" miran el CODIGO (sin
# comentarios): la prosa del script puede nombrarlos, la ejecucion no.
INIT_CODE="$(sed 's/#.*//' "$INIT")"
if printf '%s\n' "$INIT_CODE" | grep -q 'killall'; then
	fail "el script invoca killall"
fi
if printf '%s\n' "$INIT_CODE" | grep -qE 'x6200_gui|x6200_daemon'; then
	fail "el script invoca la GUI (no tiene que tocarla)"
fi
echo "ok: sintaxis, permiso de ejecucion, invocacion del contrato, sin killall ni GUI"

# 6.1 comportamiento con un start-stop-daemon doble (el host no tiene el real):
#     singleton, PIDFILE, identidad y stop sin tocar procesos ajenos.
BIN_DIR="$WORK/bin"
mkdir -p "$BIN_DIR"
cp "$T/start-stop-daemon.sh" "$BIN_DIR/start-stop-daemon"
chmod +x "$BIN_DIR/start-stop-daemon"
SSD_LOG="$WORK/ssd.log"
export SSD_LOG
: > "$SSD_LOG"

# El script de init apunta a /usr/sbin/x6200_bootanim, que no existe en el host:
# se lo corre con BIN y PIDFILE sustituidos (el mismo codigo, otro path).
INIT_HOST="$WORK/S01bootanim.host"
sed -e "s#^BIN=\"/usr/sbin/\$DAEMON\"#BIN=\"$WORK/fake_animator\"#" \
	-e "s#^PIDFILE=\"/var/run/\$DAEMON.pid\"#PIDFILE=\"$WORK/host.pid\"#" "$INIT" > "$INIT_HOST"
grep -q "BIN=\"$WORK/fake_animator\"" "$INIT_HOST" || fail "no se pudo adaptar el script para el host"
grep -q "PIDFILE=\"$WORK/host.pid\"" "$INIT_HOST" || fail "no se pudo adaptar el PIDFILE para el host"
chmod +x "$INIT_HOST"

PID_HOST="$WORK/host.pid"
run_init() { PATH="$BIN_DIR:$PATH" sh "$INIT_HOST" "$1"; }

START_T0=$(date +%s%N)
run_init start
START_MS=$(( ($(date +%s%N) - START_T0) / 1000000 ))
[ -s "$PID_HOST" ] || fail "start no dejo PIDFILE"
PID1="$(cat "$PID_HOST")"
pid_alive "$PID1" || fail "start no dejo el animador corriendo"
[ "$(readlink "/proc/$PID1/exe")" = "$WORK/fake_animator" ] || fail "el PIDFILE no apunta al animador"
[ "$START_MS" -lt 100 ] || fail "start tardo ${START_MS} ms (contrato: <100 ms tras backgrounding)"
echo "ok: start vuelve en ${START_MS} ms (contrato: <100 ms, sin sleeps) y deja el PIDFILE"

run_init start
[ "$(cat "$PID_HOST")" = "$PID1" ] || fail "start arranco una SEGUNDA instancia"
[ "$(grep -c '^start ' "$SSD_LOG")" -eq 1 ] || fail "se invoco start-stop-daemon -S mas de una vez"
echo "ok: singleton (un solo animador, un solo start-stop-daemon -S)"

run_init stop
wait_gone "$PID1" || fail "stop no termino el animador"
if [ -e "$PID_HOST" ]; then
	fail "stop no limpio el PIDFILE"
fi
echo "ok: stop termina el animador, limpia el PIDFILE y no usa killall"

# PIDFILE con el PID de OTRO proceso (otro ejecutable): no se senaliza ni se
# mata, y se limpia el PIDFILE.
sleep 30 &
OTHER_PID=$!
printf '%s\n' "$OTHER_PID" > "$PID_HOST"
run_init stop
pid_alive "$OTHER_PID" || fail "stop mato a un proceso ajeno (PID reciclado)"
if [ -e "$PID_HOST" ]; then
	fail "stop no limpio un PIDFILE ajeno"
fi
echo "ok: un PIDFILE con un PID ajeno no se senaliza ni se mata"
kill -9 "$OTHER_PID" 2>/dev/null || true
wait "$OTHER_PID" 2>/dev/null || true
OTHER_PID=""

# PIDFILE corrupto, vacio o ausente: stop limpia y no rompe.
for corrupto in "basura" "" "0" "-3"; do
	printf '%s\n' "$corrupto" > "$PID_HOST"
	run_init stop
	if [ -e "$PID_HOST" ]; then
		fail "stop no limpio un PIDFILE corrupto ('$corrupto')"
	fi
done
run_init stop # sin PIDFILE
echo "ok: PIDFILE corrupto, vacio o ausente: stop limpia y no rompe"

# Ciclo completo otra vez, con liveness real.
run_init start
PID2="$(cat "$PID_HOST")"
pid_alive "$PID2" || fail "no se pudo rearrancar el animador"
run_init stop
wait_gone "$PID2" || fail "el segundo stop no termino el animador"
if [ -e "$PID_HOST" ]; then
	fail "el segundo stop no limpio el PIDFILE"
fi
if grep -qE 'x6200_gui|x6200_daemon' "$SSD_LOG"; then
	fail "se intento matar la GUI"
fi
echo "ok: arrancar/detener dos veces deja todo limpio y nunca toca la GUI"

# 6.2 sin el ejecutable: el boot sigue (exit 0 y aviso en stderr, sin bloquear).
INIT_MISSING="$WORK/S01bootanim.missing"
sed -e "s#^BIN=\"/usr/sbin/\$DAEMON\"#BIN=\"$WORK/no-existe/x6200_bootanim\"#" \
	-e "s#^PIDFILE=\"/var/run/\$DAEMON.pid\"#PIDFILE=\"$WORK/missing.pid\"#" "$INIT" > "$INIT_MISSING"
if PATH="$BIN_DIR:$PATH" sh "$INIT_MISSING" start 2>"$WORK/log-init-missing.txt"; then
	grep -q 'se arranca sin animacion' "$WORK/log-init-missing.txt" || fail "no aviso en stderr"
	echo "ok: sin ejecutable el init no bloquea el boot (exit 0 + aviso en stderr)"
else
	fail "start con el ejecutable ausente devolvio error (bloquearia rcS)"
fi
unset SSD_LOG

# ---------------------------------------------------------------------------
step "7) integracion (rootfs, prune, handoff en la GUI, CMake, workflow)"
# ---------------------------------------------------------------------------
if [ -f "$ROOT/rootfs/etc/init.d/S95gui" ]; then
	fail "rootfs/ volvio a traer S95gui"
fi
[ -f "$ROOT/rootfs/etc/init.d/S20gui" ] || fail "falta S20gui"
[ "$(ls "$ROOT"/rootfs/etc/init.d/ | grep -cE 'S[0-9]+gui$')" -eq 1 ] || fail "hay mas de un script de GUI"
[ -f "$ROOT/rootfs/etc/init.d/S01boot_time" ] || fail "falta S01boot_time (no se puede tocar el orden del boot)"
echo "ok: S95gui ausente del rootfs del port, S20gui unico y S01boot_time preservado"

PRUNE_ROOT="$WORK/prune"
mkdir -p "$PRUNE_ROOT/etc/init.d"
printf 'x\n' > "$PRUNE_ROOT/etc/init.d/S95gui"
printf 'y\n' > "$PRUNE_ROOT/etc/init.d/S99vendor"
printf 'z\n' > "$PRUNE_ROOT/etc/init.d/S01bootanim"
PORT_ROOTFS="$ROOT/rootfs" sh "$ROOT/buildroot/prune-vendor-rootfs.sh" "$PRUNE_ROOT" >/dev/null
if [ -e "$PRUNE_ROOT/etc/init.d/S95gui" ]; then
	fail "el prune ya no saca S95gui"
fi
[ -f "$PRUNE_ROOT/etc/init.d/S99vendor" ] || fail "el prune borro de mas"
[ -f "$PRUNE_ROOT/etc/init.d/S01bootanim" ] || fail "el prune borro el script nuevo"
echo "ok: el prune sigue sacando el S95gui del vendor y no toca S01bootanim"

# El handoff tiene que quedar ANTES de drm_init (si se pide DRM) y antes del
# primer lv_timer_handler, y el callback de la GUI no se toca.
MAIN="$ROOT/src/main.c"
grep -qE 'disp_drv\.flush_cb[[:space:]]*=[[:space:]]*screen_rec_flush;' "$MAIN" ||
	fail "cambio el callback screen_rec_flush"
LINE_SPLASH_DRM="$(grep -n 'boot_splash_stop()' "$MAIN" | head -1 | cut -d: -f1)"
LINE_DRM_INIT="$(grep -n 'drm_init()' "$MAIN" | head -1 | cut -d: -f1)"
LINE_SPLASH_GUI="$(grep -n 'boot_splash_stop()' "$MAIN" | tail -1 | cut -d: -f1)"
LINE_TIMER="$(grep -n 'next_loop_time = lv_timer_handler()' "$MAIN" | head -1 | cut -d: -f1)"
[ -n "$LINE_SPLASH_DRM" ] || fail "no hay boot_splash_stop() en main.c"
[ "$LINE_SPLASH_DRM" -lt "$LINE_DRM_INIT" ] || fail "el splash no se detiene antes de drm_init()"
[ "$LINE_SPLASH_GUI" -lt "$LINE_TIMER" ] || fail "el splash no se detiene antes del primer lv_timer_handler()"
[ "$(grep -c 'boot_splash_stop()' "$MAIN")" -ge 3 ] || fail "falta el stop en el camino de X6200_VOICE_TEST"
grep -q 'boot_mark("bootanim stop")' "$MAIN" || fail "falta la marca bootanim stop"
grep -q 'boot_mark("bootanim handoff")' "$MAIN" || fail "falta la marca bootanim handoff"
grep -q 'boot_mark("GUI inicio")' "$MAIN" || fail "se perdieron marcas existentes de boot_mark"
grep -q 'lv_scr_load(main_obj);' "$MAIN" || fail "cambio la carga de la pantalla principal"
echo "ok: handoff antes de drm_init y del primer lv_timer_handler, marcas nuevas, screen_rec_flush intacto"

grep -q 'install(TARGETS ${PROJECT_NAME} btcat x6200_bootanim DESTINATION sbin)' "$ROOT/CMakeLists.txt" ||
	fail "el target x6200_bootanim no se instala en sbin desde CMakeLists.txt raiz"
grep -q 'add_subdirectory(bootanim)' "$ROOT/src/CMakeLists.txt" || fail "src/CMakeLists.txt no agrega bootanim"
grep -q 'target_compile_options(x6200_bootanim PRIVATE -std=c11 -Wall -Wextra -Werror)' "$SRC/CMakeLists.txt" ||
	fail "el target no compila con -std=c11 -Wall -Wextra -Werror"
grep -q 'boot_splash.c' "$ROOT/src/CMakeLists.txt" || fail "boot_splash.c no entra en el build de la GUI"
echo "ok: CMake integra el animador y boot_splash, con -std=c11 -Wall -Wextra -Werror e install en sbin"

grep -q 'tests/check_bootanim.sh' "$ROOT/.github/workflows/boot-rootfs.yml" ||
	fail "el gate no esta integrado en .github/workflows/boot-rootfs.yml"
echo "ok: el gate esta integrado en .github/workflows/boot-rootfs.yml"

# Los limites del contrato (seccion 3) se codifican: el animador no puede tener
# LVGL, subprocesos, shell, red, radio, SQLite/params.db ni threads, y sus
# senales van por volatile sig_atomic_t. Se mira el CODIGO sin comentarios: los
# comentarios si pueden nombrar LVGL o el archivo de fbdev.
python3 - "$SRC" <<'PY' || fail "el animador usa algo prohibido por el contrato (issue #192)"
import re
import sys
from pathlib import Path

prohibidos = ["system(", "popen", "execl", "execv", "fork(", "pthread", "lv_", "LV_",
              "sqlite", "params.db", "socket(", "connect(", "gpio"]
violaciones = []
for path in sorted(Path(sys.argv[1]).glob("*.[ch]")):
    code = re.sub(r"/\*.*?\*/", " ", path.read_text(encoding="utf-8"), flags=re.S)
    code = re.sub(r"//[^\n]*", " ", code)
    for item in prohibidos:
        if item in code:
            violaciones.append(f"{path.name}: {item}")
if violaciones:
    print("  " + "\n  ".join(violaciones))
    sys.exit(1)
PY
grep -q 'volatile sig_atomic_t' "$SRC/main.c" || fail "el handler de senales no usa volatile sig_atomic_t"
echo "ok: sin LVGL, subprocesos, shell, red, radio, SQLite ni threads; banderas volatile sig_atomic_t"

# El reloj/flujo de produccion no tiene NINGUNA salida propia: el limite de
# cuadros del banco existe solo bajo -DBOOTANIM_TEST (se comprueba sobre el
# preprocesado, que es lo que realmente ve el compilador).
PROD_CPP="$WORK/main-prod.i"
TEST_CPP="$WORK/main-test.i"
$CC $CFLAGS_COMMON -E "$SRC/main.c" >"$PROD_CPP"
if grep -q 'BA_TEST_FRAME_LIMIT\|BOOTANIM_TEST' "$PROD_CPP"; then
	fail "el build de produccion sigue viendo el limite de cuadros del banco"
fi
grep -q 'BOOTANIM_TEST' "$SRC/main.c" || fail "falta el camino determinista del banco (-DBOOTANIM_TEST)"
$CC $CFLAGS_COMMON -DBOOTANIM_TEST -DBA_TEST_FRAME_LIMIT=BA_TEST_LIMIT_SENTINEL -E "$SRC/main.c" >"$TEST_CPP"
grep -q 'BA_TEST_LIMIT_SENTINEL' "$TEST_CPP" || fail "el macro del banco no llega al preprocesado del gate"
if grep -rq 'BOOTANIM_TEST' "$ROOT/CMakeLists.txt" "$ROOT/src/CMakeLists.txt" "$SRC/CMakeLists.txt"; then
	fail "CMake define el macro del banco: la ROM no puede tener esa salida"
fi
echo "ok: el limite de cuadros vive solo en -DBOOTANIM_TEST (el preprocesado de produccion no lo ve) y CMake no lo define"

echo
if [ "$SAN_OK" -eq 1 ]; then
	echo "PASS: boot anim neo-retro (#192) - unitarios + ASan/UBSan + binario real con fb sintetico + init + integracion"
else
	echo "PASS (parcial): boot anim neo-retro (#192) - unitarios + binario real con fb sintetico + init + integracion; ASan/UBSan SKIP por falta de soporte del compilador"
fi
