#!/bin/sh
#
# Suite hermética del supervisor de la GUI (rootfs/usr/sbin/x6200_daemon,
# issue #214, epic #211).
#
#   tests/daemon/run_daemon_tests.sh [out-dir]
#
# Corre el WRAPPER REAL (sin editarlo) dentro de un chroot privado en un
# namespace de usuario/montaje/PID propio (unshare -Ur -m -p), con:
#   - una GUI sustituta controlada (guion por corrida: exit/exit78/live),
#   - dobles de las utilidades que el wrapper usa (sleep, date, mountpoint,
#     mount, killall) por PATH, para reloj virtual y condiciones de /mnt sin
#     tocar el /mnt del host ni la radio,
#   - el /bin/sh y las libs del host (bash) como intérprete; la ROM del target
#     también trae bash como /bin/sh, así que el shell es el del target,
#   - el start-stop-daemon doble de tests/bootanim para el caso S20gui.
#
# Un `sleep`/`date` real de 5 min no se usa: el reloj se simula con el doble de
# `date` (fallback del wrapper cuando no hay /proc/uptime), y los dos casos que
# necesitan tiempo real (crash instantáneo 10 s, backoff interrumpido) son de
# segundos. El wrapper NO se modifica para inyectar delays.
#
# Si el host no permite user namespaces (p. ej. el contenedor del CI) el script
# declara SKIP con el motivo y sale 77: no corre a medias ni declara PASS.
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -u

DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SELF=$DIR/$(basename -- "$0")
REPO=$(CDPATH= cd -- "$DIR/../.." && pwd)
DAEMON=${X6200_DAEMON:-$REPO/rootfs/usr/sbin/x6200_daemon}
S20GUI=$REPO/rootfs/etc/init.d/S20gui
SSD_STUB=$REPO/tests/bootanim/start-stop-daemon.sh
OUT=${1:-$DIR/build}

# ---------------------------------------------------------------------------
# Modo interno: corre DENTRO del namespace, sobre un chroot ya armado.
# ---------------------------------------------------------------------------
if [ "${1:-}" = "--inner" ]; then
    CASE=$2
    ROOT=$3

    FAILED=0
    fail() { echo "  FAIL: $*" >&2; FAILED=$((FAILED + 1)); }
    ok()   { echo "  ok: $*"; }

    mount --bind /usr/bin "$ROOT/usr/bin"
    mount --bind /usr/lib "$ROOT/usr/lib"
    mount --bind /usr/lib64 "$ROOT/usr/lib64"
    mount --bind /dev/null "$ROOT/dev/null"
    mount --bind /dev/zero "$ROOT/dev/zero"

    CASE_OUT=$ROOT/tmp/case.out
    CASE_ERR=$ROOT/tmp/case.err

    # Estado fresco del chroot (el daemon escribe en /tmp, /var/run, /mnt).
    fresh() {
        rm -rf "$ROOT/tmp" "$ROOT/var/run" "$ROOT/mnt" "$ROOT/dev/shm" "$ROOT/proc"
        mkdir -p "$ROOT/tmp" "$ROOT/var/run" "$ROOT/mnt" "$ROOT/dev/shm"
        : > "$ROOT/tmp/sleep.log"
        : > "$ROOT/tmp/gui.log"
        : > "$ROOT/tmp/date.mark"
        : > "$ROOT/tmp/mount.log"
        : > "$ROOT/tmp/date.n"
        echo 0 > "$ROOT/tmp/gui.count"
        case "${GUI_PLAN:-}" in
            '') : ;;
            *) : > "$ROOT/tmp/gui.plan" ;;
        esac
        : > "$CASE_OUT"
        : > "$CASE_ERR"
    }

    plan() { printf '%s\n' "$1" > "$ROOT/tmp/gui.plan"; }

    start_daemon() {
        PATH="/stubs:/usr/bin:/bin" chroot "$ROOT" /usr/sbin/x6200_daemon \
            >>"$CASE_OUT" 2>>"$CASE_ERR" &
        DAEMON_PID=$!
    }

    stop_daemon() {
        [ -n "${DAEMON_PID:-}" ] || return 0
        kill -TERM "$DAEMON_PID" 2>/dev/null || true
        wait "$DAEMON_PID" 2>/dev/null || true
        DAEMON_PID=
    }

    # hasta-condición con techo: espera a que un archivo tenga N líneas.
    wait_lines() { # <file> <n>
        i=0
        while [ "$(wc -l < "$1" 2>/dev/null || echo 0)" -lt "$2" ] && [ "$i" -lt 100 ]; do
            sleep 0.05
            i=$((i + 1))
        done
    }
    gui_runs() { cat "$ROOT/tmp/gui.count" 2>/dev/null || echo 0; }
    wait_gui_runs() { # <n>
        i=0
        while [ "$(gui_runs)" -lt "$1" ] && [ "$i" -lt 100 ]; do
            sleep 0.05
            i=$((i + 1))
        done
    }
    delays() { tr '\n' ' ' < "$ROOT/tmp/sleep.log"; }

    case "$CASE" in
    backoff)
        fresh; plan 'exit:1'; export MP_MNT=1; export DATE_STEP=0
        start_daemon
        wait_lines "$ROOT/tmp/sleep.log" 5
        stop_daemon
        d=$(head -n 5 "$ROOT/tmp/sleep.log" | tr '\n' ' ')
        [ "$d" = "2 5 15 60 60 " ] && ok "backoff determinista 2 5 15 60 60" \
            || fail "backoff fue: $d"
        grep -q 'GUI exited: code 1' "$CASE_ERR" && ok "informa 'GUI exited: code 1'" \
            || fail "no informó el código de salida"
        grep -q 'GUI restart in 2s' "$CASE_ERR" && ok "informa 'GUI restart in 2s'" \
            || fail "no informó el reinicio"
        grep -q '^+%s$' "$ROOT/tmp/date.mark" && ok "reloj por date +%s (fallback sin /proc)" \
            || fail "no usó el fallback date +%s"
        ;;
    exit78)
        fresh; plan 'exit:78'; export MP_MNT=1; export DATE_STEP=0
        start_daemon
        wait_lines "$ROOT/tmp/sleep.log" 3
        stop_daemon
        d=$(head -n 3 "$ROOT/tmp/sleep.log" | tr '\n' ' ')
        [ "$d" = "60 60 60 " ] && ok "exit 78 ⇒ 60 s desde el primer fallo" \
            || fail "delays de exit 78: $d"
        grep -q 'GUI exited: code 78' "$CASE_ERR" && ok "informa el 78" \
            || fail "no informó el 78"
        ;;
    instant10)
        # Falla instantánea: en los primeros 10 s NO puede haber > 3 arranques.
        # Sleep real: es el propio backoff el que tiene que acotar.
        fresh; plan 'exit:1'; export MP_MNT=1; export SLEEP_REAL=1; export DATE_STEP=0
        start_daemon
        sleep 10
        stop_daemon
        n=$(gui_runs)
        [ "$n" -le 3 ] && ok "≤3 invocaciones de la GUI en 10 s (n=$n)" \
            || fail "$n invocaciones en 10 s (>3)"
        d=$(head -n 2 "$ROOT/tmp/sleep.log" | tr '\n' ' ')
        [ "$d" = "2 5 " ] && ok "los dos primeros delays son 2 s y 5 s" \
            || fail "delays en la ventana: $d"
        ;;
    uniquelogs)
        # Dos salidas en el MISMO segundo: los nombres tienen que ser únicos.
        fresh; export MP_MNT=0; export DATE_STEP=0; export DATE_FIXED=2026-10-22_00-00-00
        printf 'exit:1\nexit:1\nlive:30\n' > "$ROOT/tmp/gui.plan"
        mount -t tmpfs -o size=16M tmpfs "$ROOT/mnt"
        start_daemon
        wait_lines "$ROOT/tmp/gui.log" 2
        # esperar a que los dos logs archivados existan
        i=0
        while [ "$(ls -1 "$ROOT/mnt/app_logs" 2>/dev/null | grep -c .)" -lt 2 ] && [ "$i" -lt 100 ]; do
            sleep 0.05; i=$((i + 1))
        done
        stop_daemon
        files=$(ls -1 "$ROOT/mnt/app_logs" 2>/dev/null)
        cnt=$(printf '%s\n' "$files" | grep -c .)
        [ "$cnt" -eq 2 ] && ok "2 logs archivados en /mnt/app_logs" \
            || fail "esperaba 2 logs, hay $cnt: $files"
        printf '%s\n' "$files" | grep -q '_r1.txt' || fail "falta el log de la ejecución 1"
        printf '%s\n' "$files" | grep -q '_r2.txt' || fail "falta el log de la ejecución 2"
        for f in $files; do
            case "$f" in
                *2026-10-22_00-00-00*) : ;;
                *) fail "el nombre no lleva el timestamp UTC: $f" ;;
            esac
        done
        ok "nombres únicos con el mismo segundo: $(printf '%s ' $files)"
        ;;
    single)
        # Una sola GUI viva: el segundo arranque no levanta otra.
        fresh; plan 'live:30'; export MP_MNT=1; export DATE_STEP=0
        start_daemon
        wait_gui_runs 1
        PATH="/stubs:/usr/bin:/bin" chroot "$ROOT" /usr/sbin/x6200_daemon \
            >"$ROOT/tmp/second.out" 2>"$ROOT/tmp/second.err" &
        SECOND=$!
        wait "$SECOND"; rc=$?
        [ "$rc" -eq 0 ] && ok "el segundo arranque sale 0" || fail "el segundo arranque salió $rc"
        grep -q 'another instance is alive' "$ROOT/tmp/second.err" \
            && ok "avisa que ya hay una instancia" || fail "no avisó de la instancia viva"
        [ "$(gui_runs)" -eq 1 ] && ok "una sola GUI (el segundo no relanzó)" \
            || fail "hubo $(gui_runs) arranques de GUI"
        stop_daemon
        ;;
    mntabsent)
        fresh; export MP_MNT=1; export DATE_STEP=0
        printf 'exit:1\nexit:1\nlive:30\n' > "$ROOT/tmp/gui.plan"
        start_daemon
        wait_lines "$ROOT/tmp/gui.log" 2
        stop_daemon
        f=$(find "$ROOT/mnt" -type f 2>/dev/null)
        [ -z "$f" ] && ok "0 archivos bajo /mnt (no escribió el rootfs)" \
            || fail "escribió bajo /mnt: $f"
        ls "$ROOT"/tmp/x6200_log_*_p*_r*.txt >/dev/null 2>&1 \
            && ok "log retenido en /tmp" || fail "no quedó log en /tmp"
        grep -q 'not writable' "$CASE_ERR" && ok "diagnóstico en stderr" \
            || fail "sin diagnóstico en stderr"
        ;;
    mntro)
        fresh; export MP_MNT=0; export DATE_STEP=0
        printf 'exit:1\nexit:1\nlive:30\n' > "$ROOT/tmp/gui.plan"
        mount --bind "$ROOT/ro-mnt" "$ROOT/mnt"
        mount -o remount,ro,bind "$ROOT/mnt"
        start_daemon
        wait_lines "$ROOT/tmp/gui.log" 2
        stop_daemon
        f=$(find "$ROOT/mnt" -type f 2>/dev/null)
        [ -z "$f" ] && ok "0 archivos bajo /mnt RO" || fail "escribió bajo /mnt RO: $f"
        ls "$ROOT"/tmp/x6200_log_*_p*_r*.txt >/dev/null 2>&1 \
            && ok "log retenido en /tmp" || fail "no quedó log en /tmp"
        ;;
    sigtermgui)
        fresh; plan 'live:30'; export MP_MNT=1; export DATE_STEP=0
        start_daemon
        wait_gui_runs 1
        t0=$(date +%s%N)
        stop_daemon
        t1=$(date +%s%N); ms=$(( (t1 - t0) / 1000000 ))
        [ "$ms" -lt 2000 ] && ok "sale en ${ms} ms al recibir SIGTERM" \
            || fail "tardó ${ms} ms en salir"
        [ "$(gui_runs)" -eq 1 ] && ok "no relanzó la GUI tras el SIGTERM" \
            || fail "relanzó la GUI ($(gui_runs) arranques)"
        grep -q 'GUI restart' "$CASE_ERR" && fail "anunció un reinicio tras el SIGTERM" \
            || ok "no anunció reinicio"
        ;;
    sigtermbackoff)
        fresh; plan 'exit:1'; export MP_MNT=1; export SLEEP_REAL=1; export DATE_STEP=0
        start_daemon
        wait_lines "$ROOT/tmp/sleep.log" 1
        t0=$(date +%s%N)
        stop_daemon
        t1=$(date +%s%N); ms=$(( (t1 - t0) / 1000000 ))
        [ "$ms" -lt 1500 ] && ok "cortó el backoff de 2 s en ${ms} ms" \
            || fail "esperó ${ms} ms (no interrumpió el backoff)"
        [ "$(gui_runs)" -eq 1 ] && ok "no relanzó tras cortar el backoff" \
            || fail "relanzó ($(gui_runs) arranques)"
        ;;
    longrun)
        # 6 min sanos: el contador tiene que resetearse (delay 2 s, no 5 s).
        fresh; plan 'exit:1'; export MP_MNT=1; export DATE_STEP=400; export SLEEP_REAL=0
        start_daemon
        wait_lines "$ROOT/tmp/sleep.log" 3
        stop_daemon
        d=$(head -n 3 "$ROOT/tmp/sleep.log" | tr '\n' ' ')
        [ "$d" = "2 2 2 " ] && ok "contador reseteado tras corrida ≥300 s (2 2 2)" \
            || fail "no reseteó: $d"
        ;;
    procuptime)
        # Con /proc/uptime presente el wrapper NO debe caer al fallback date.
        fresh; mkdir -p "$ROOT/proc"; printf '1000.00 1000.00\n' > "$ROOT/proc/uptime"
        plan 'exit:1'; export MP_MNT=1; export DATE_STEP=0
        start_daemon
        wait_lines "$ROOT/tmp/sleep.log" 2
        stop_daemon
        grep -q '^+%s$' "$ROOT/tmp/date.mark" \
            && fail "usó date +%s teniendo /proc/uptime" \
            || ok "usó el reloj de /proc/uptime"
        d=$(head -n 2 "$ROOT/tmp/sleep.log" | tr '\n' ' ')
        [ "$d" = "2 5 " ] && ok "backoff normal con /proc/uptime estático" \
            || fail "delays: $d"
        ;;
    imagesexisting)
        # /dev/shm/images ya es un montaje: no hay que tocarlo.
        fresh; plan 'exit:1'; export MP_MNT=1; export MP_SHM=0; export DATE_STEP=0
        start_daemon
        wait_lines "$ROOT/tmp/sleep.log" 1
        stop_daemon
        [ -s "$ROOT/tmp/mount.log" ] && fail "montó con /dev/shm/images ya montado" \
            || ok "no tocó /dev/shm/images ya montado"
        ;;
    imagesbind)
        fresh; plan 'exit:1'; export MP_MNT=1; export MP_SHM=1; export MOUNT_RC=0; export DATE_STEP=0
        start_daemon
        wait_lines "$ROOT/tmp/sleep.log" 1
        stop_daemon
        grep -q -- '--bind /usr/share/x6200/images /dev/shm/images' "$ROOT/tmp/mount.log" \
            && ok "bind de los assets a /dev/shm/images" \
            || fail "no hizo el bind: $(cat "$ROOT/tmp/mount.log")"
        ;;
    imagescopy)
        # bind no soportado ⇒ copia de antes, intacta.
        fresh; plan 'exit:1'; export MP_MNT=1; export MP_SHM=1; export MOUNT_RC=1; export DATE_STEP=0
        start_daemon
        wait_lines "$ROOT/tmp/sleep.log" 1
        stop_daemon
        [ -f "$ROOT/dev/shm/images/fixture.txt" ] && ok "fallback: copió los assets" \
            || fail "el fallback no copió los assets"
        ;;
    s20gui)
        # Retorno normal de S20gui start/stop con el wrapper real adentro.
        # /proc propio (namespace de PID) para el pid_alive del start-stop-daemon doble.
        fresh; plan 'live:30'; export MP_MNT=0; export DATE_STEP=0
        mkdir -p "$ROOT/proc"
        mount -t proc proc "$ROOT/proc"
        mount -t tmpfs -o size=16M tmpfs "$ROOT/mnt"
        PATH="/stubs:/usr/bin:/bin" chroot "$ROOT" /etc/init.d/S20gui start \
            >"$ROOT/tmp/s20.out" 2>"$ROOT/tmp/s20.err"; rc=$?
        [ "$rc" -eq 0 ] || fail "S20gui start salió $rc: $(cat "$ROOT/tmp/s20.err")"
        grep -q 'OK' "$ROOT/tmp/s20.out" && ok "S20gui start imprimió OK" \
            || fail "S20gui start no imprimió OK: $(cat "$ROOT/tmp/s20.out")"
        [ -s "$ROOT/var/run/x6200_daemon.pid" ] && ok "S20gui dejó el PIDFILE" \
            || fail "S20gui no dejó el PIDFILE"
        wait_gui_runs 1
        [ "$(gui_runs)" -eq 1 ] && ok "el daemon arrancó una GUI" \
            || fail "el daemon no arrancó la GUI"
        PATH="/stubs:/usr/bin:/bin" chroot "$ROOT" /etc/init.d/S20gui stop \
            >"$ROOT/tmp/s20stop.out" 2>"$ROOT/tmp/s20stop.err"; rc2=$?
        [ "$rc2" -eq 0 ] && ok "S20gui stop salió 0" || fail "S20gui stop salió $rc2"
        grep -q 'OK' "$ROOT/tmp/s20stop.out" && ok "S20gui stop imprimió OK" \
            || fail "S20gui stop no imprimió OK: $(cat "$ROOT/tmp/s20stop.out")"
        [ -e "$ROOT/var/run/x6200_daemon.pid" ] && fail "el PIDFILE quedó" \
            || ok "el PIDFILE se limpió"
        ;;
    *)
        fail "caso desconocido: $CASE"
        ;;
    esac

    exit "$FAILED"
fi

# ---------------------------------------------------------------------------
# Modo externo: arma el chroot y corre cada caso en su propio namespace.
# ---------------------------------------------------------------------------

[ -x "$DAEMON" ] || { echo "FAIL: falta $DAEMON" >&2; exit 2; }
[ -f "$S20GUI" ] || { echo "FAIL: falta $S20GUI" >&2; exit 2; }
[ -x "$SSD_STUB" ] || { echo "FAIL: falta $SSD_STUB" >&2; exit 2; }
sh -n "$DAEMON" || { echo "FAIL: sh -n del wrapper" >&2; exit 2; }

if ! unshare -Ur -m -p --fork true 2>/dev/null; then
    echo "SKIP: este host no permite user namespaces (unshare -Ur -m -p); la suite"
    echo "      necesita un chroot privado con /mnt y PATH propios."
    exit 77
fi

WORK=$(mktemp -d "/tmp/x6200-daemon-XXXXXX")
trap 'rm -rf "$WORK"' EXIT INT HUP TERM
ROOT=$WORK/root
mkdir -p "$ROOT/usr/bin" "$ROOT/usr/lib" "$ROOT/usr/lib64" "$ROOT/usr/sbin" \
         "$ROOT/usr/share/x6200" "$ROOT/stubs" "$ROOT/mnt" "$ROOT/ro-mnt" \
         "$ROOT/tmp" "$ROOT/dev/shm" "$ROOT/var/run" "$ROOT/etc/init.d" \
         "$ROOT/sys/class/graphics/fbcon"
ln -sf usr/bin "$ROOT/bin"
ln -sf usr/lib "$ROOT/lib"
ln -sf usr/lib64 "$ROOT/lib64"
ln -sf usr/sbin "$ROOT/sbin"
: > "$ROOT/dev/null"
: > "$ROOT/dev/zero"
: > "$ROOT/sys/class/graphics/fbcon/cursor_blink"

cp "$DAEMON" "$ROOT/usr/sbin/x6200_daemon"; chmod +x "$ROOT/usr/sbin/x6200_daemon"
cp "$S20GUI" "$ROOT/etc/init.d/S20gui"; chmod +x "$ROOT/etc/init.d/S20gui"
cp "$SSD_STUB" "$ROOT/stubs/start-stop-daemon"; chmod +x "$ROOT/stubs/start-stop-daemon"

mkdir -p "$ROOT/usr/share/x6200/images"
: > "$ROOT/usr/share/x6200/images/fixture.txt"
: > "$ROOT/usr/share/x6200/images/fixture2.txt"
: > "$ROOT/usr/share/x6200/params.default.db"

# --- dobles de utilidades (controlan el reloj, /mnt y el bind) --------------
cat > "$ROOT/stubs/sleep" <<'EOF'
#!/bin/sh
printf '%s\n' "$1" >> /tmp/sleep.log
if [ "${SLEEP_REAL:-0}" = 1 ]; then exec /bin/sleep "$1"; fi
exit 0
EOF
cat > "$ROOT/stubs/date" <<'EOF'
#!/bin/sh
printf '%s\n' "$*" >> /tmp/date.mark
if [ "${1:-}" = "+%s" ]; then
    n=$(cat /tmp/date.n 2>/dev/null || echo 0)
    printf '%s\n' "$(( 1000000 + n * ${DATE_STEP:-0} ))"
    printf '%s\n' "$((n + 1))" > /tmp/date.n
    exit 0
fi
printf '%s\n' "${DATE_FIXED:-2026-10-22_00-00-00}"
EOF
cat > "$ROOT/stubs/mountpoint" <<'EOF'
#!/bin/sh
p=
for a in "$@"; do case "$a" in -q) ;; *) p=$a ;; esac; done
case "$p" in
    /mnt) exit "${MP_MNT:-1}" ;;
    /dev/shm/images) exit "${MP_SHM:-1}" ;;
    *) exit 1 ;;
esac
EOF
cat > "$ROOT/stubs/mount" <<'EOF'
#!/bin/sh
printf 'mount %s\n' "$*" >> /tmp/mount.log
exit "${MOUNT_RC:-0}"
EOF
cat > "$ROOT/stubs/killall" <<'EOF'
#!/bin/sh
printf 'killall %s\n' "$*" >> /tmp/mount.log
exit 0
EOF
chmod +x "$ROOT"/stubs/*

# --- GUI sustituta: guion por corrida (exit:N, live:N) ----------------------
cat > "$ROOT/usr/sbin/x6200_gui" <<'EOF'
#!/bin/sh
n=$(cat /tmp/gui.count 2>/dev/null || echo 0)
n=$((n + 1))
printf '%s\n' "$n" > /tmp/gui.count
printf 'gui start %s %s\n' "$n" "$$" >> /tmp/gui.log
echo "gui stub run $n"
mode=$(sed -n "${n}p" /tmp/gui.plan 2>/dev/null)
[ -n "$mode" ] || mode=$(tail -n 1 /tmp/gui.plan 2>/dev/null)
[ -n "$mode" ] || mode="exit:0"
case "$mode" in
    live:*) exec /bin/sleep "${mode#live:}" ;;
    exit:*) exit "${mode#exit:}" ;;
    *) exit 0 ;;
esac
EOF
chmod +x "$ROOT/usr/sbin/x6200_gui"

# Variables que ven los dobles DENTRO del chroot (rutas relativas al chroot).
export GUI_PLAN=/tmp/gui.plan
export MP_MNT=1 MP_SHM=1 MOUNT_RC=0 SLEEP_REAL=0 DATE_STEP=0
export DATE_FIXED=2026-10-22_00-00-00

mkdir -p "$OUT"
MATRIX=$OUT/matrix.txt
: > "$MATRIX"
FAILED=0
TOTAL=0

run_case() { # <caso>
    TOTAL=$((TOTAL + 1))
    printf '\n== caso: %s ==\n' "$1" | tee -a "$MATRIX"
    case_out=$WORK/case-$1.out
    if unshare -Ur -m -p --fork sh "$SELF" --inner "$1" "$ROOT" >"$case_out" 2>&1; then
        st=0
    else
        st=$?
    fi
    tee -a "$MATRIX" < "$case_out"
    if [ "$st" -eq 0 ]; then
        printf -- '-- %s: PASS\n' "$1" | tee -a "$MATRIX"
    else
        printf -- '-- %s: FAIL\n' "$1" | tee -a "$MATRIX"
        FAILED=$((FAILED + 1))
    fi
}

for c in ${X6200_CASES:-backoff exit78 uniquelogs single mntabsent mntro sigtermgui sigtermbackoff longrun procuptime imagesexisting imagesbind imagescopy s20gui instant10}; do
    run_case "$c"
done

printf '\n============ resumen ============\n' | tee -a "$MATRIX"
if [ "$FAILED" -eq 0 ]; then
    printf '  all OK: %d casos, 0 fallos\n' "$TOTAL" | tee -a "$MATRIX"
else
    printf '  %d casos, %d fallos\n' "$TOTAL" "$FAILED" | tee -a "$MATRIX"
fi
[ "$FAILED" -eq 0 ]
