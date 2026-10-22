#!/bin/sh
# f-inner.sh <caso> — pasada COMBINADA F-01..F-05 del par integrado
# (GUI b798f13b + Buildroot e5a6393), lado de adentro del contenedor.
#
# Corre DENTRO de un contenedor privado y desechable (imagen x6200-buildroot,
# --cap-add SYS_ADMIN) con:
#   /t = chroot del rootfs del target construido del par integrado (binario ARM real)
#   /f = fixtures (bases sqlite armadas en el host a partir de la plantilla del rootfs)
#   /e = evidencia de esta corrida (logs crudos por caso)
#
# Capa: QEMU_USER (userland armv7 por binfmt_misc/qemu-arm, kernel del host).
# NO es boot de kernel/init, no hay radio, no hay RF, no se toca la SD del owner
# ni el /mnt del host (el /mnt que ve la GUI es un tmpfs o una imagen ext4 propia
# del caso).
#
# Casos: f02-corrupt f02-nomount f03-absent f03-writefail f04-daemon-exit78
#        f04-daemon-nomnt f01f03-chain
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -u

CASE=${1:?falta el caso}
T=/t
E=/e
F=/f
PRELOAD=/lib/libfakefb.so:/lib/libcrash.so:/lib/libx6200trace.so
LOGDIR="$E/logs"
LOG="$LOGDIR/$CASE.log"
OUTDIR="$LOGDIR/raw"
mkdir -p "$LOGDIR" "$OUTDIR"
: > "$LOG"
FAILS=0

note() { printf '%s\n' "$*" | tee -a "$LOG"; }
ok()   { note "  ok: $*"; }
bad()  { note "  FALLO: $*"; FAILS=$((FAILS + 1)); }
skip() { note "  SKIP: $*"; }
check() { if [ "$1" = 0 ]; then ok "$2"; else bad "$2"; fi; }
sha()  { sha256sum "$1" 2>/dev/null | cut -d' ' -f1; }
en_out() { if grep -q "$1" "$2"; then ok "salida contiene: $1"; else bad "salida NO contiene: $1"; fi; }
no_en_out() { if grep -q "$1" "$2"; then bad "salida NO debe contener: $1"; else ok "salida sin: $1"; fi; }

# --- entorno del chroot (nodos mínimos; el /dev del host no se toca) ----------
dev_nodes() {
    mkdir -p "$T/dev" "$T/dev/input" "$T/dev/shm" "$T/tmp" "$T/mnt" "$T/proc"
    [ -c "$T/dev/null" ]    || mknod -m 666 "$T/dev/null" c 1 3
    [ -c "$T/dev/zero" ]    || mknod -m 666 "$T/dev/zero" c 1 5
    [ -c "$T/dev/random" ]  || mknod -m 666 "$T/dev/random" c 1 8
    [ -c "$T/dev/urandom" ] || mknod -m 666 "$T/dev/urandom" c 1 9
    [ -c "$T/dev/tty" ]     || mknod -m 666 "$T/dev/tty" c 5 0
}
mount_proc() { mountpoint -q "$T/proc" 2>/dev/null || mount --bind /proc "$T/proc" || true; }

# --- GUI ----------------------------------------------------------------------
gui_bg() {   # $1 = archivo de salida
    rm -f "$T/tmp/x6200_remote.sock" "$T/tmp/crash.txt"
    ( cd "$T/tmp" && chroot "$T" /bin/busybox env LD_PRELOAD="$PRELOAD" QEMU_CPU=cortex-a7 /usr/sbin/x6200_gui ) >> "$1" 2>&1 &
    GUI=$!
}
gui_fg() {   # $1 = timeout, $2 = salida ; imprime el rc
    rm -f "$T/tmp/x6200_remote.sock" "$T/tmp/crash.txt"
    ( cd "$T/tmp" && timeout "$1" chroot "$T" /bin/busybox env LD_PRELOAD="$PRELOAD" QEMU_CPU=cortex-a7 /usr/sbin/x6200_gui ) > "$2" 2>&1
    echo $?
}
wait_socket() {
    i=0
    while [ "$i" -lt 60 ] && [ ! -S "$T/tmp/x6200_remote.sock" ]; do
        sleep 1
        i=$((i + 1))
    done
    if [ -S "$T/tmp/x6200_remote.sock" ]; then echo "$i"; fi
}
stop_gui() {
    [ -n "${GUI:-}" ] || return 0
    kill -TERM "$GUI" 2>/dev/null
    wait "$GUI" 2>/dev/null
    GUI=
}
gui_procs() { ps -eo args 2>/dev/null | grep -c '[x]6200_gui'; }
estado() { chroot "$T" /tmp/x6200-remote "$1" 2>&1; }
freq_of() { sed -n 's/.*"freq":\([0-9]*\).*/\1/p' "$1" | head -1; }

# --- /mnt de cada caso --------------------------------------------------------
mnt_tmpfs() {
    umount "$T/mnt" 2>/dev/null || true
    mount -t tmpfs -o size=32M tmpfs "$T/mnt" || return 1
    rm -f "$T/mnt/params.db"
    mkdir -p "$T/mnt/app_logs"
    return 0
}
mnt_none() {
    umount "$T/mnt" 2>/dev/null || true
    rm -rf "$T/mnt"/* 2>/dev/null || true
}
mnt_ext4_img() {   # ext4 creado por el mkfs.ext4 DEL TARGET sobre una imagen propia
    umount "$T/mnt" 2>/dev/null || true
    rm -f "$T$1"
    dd if=/dev/zero of="$T$1" bs=1M count=32 > /dev/null 2>&1
    chroot "$T" /sbin/mkfs.ext4 -F -m 0 -L DATA "$1" > "$OUTDIR/data-mkfs.txt" 2>&1 || return 1
    mount -t ext4 -o loop "$T$1" "$T/mnt" || return 1
    mkdir -p "$T/mnt/app_logs"
    return 0
}
db_query() { sqlite3 "$1" "$2" 2>/dev/null; }

dev_nodes
mount_proc
note "== caso: $CASE (capa QEMU_USER)"
note "   binario medido: $(chroot "$T" /bin/busybox sha256sum /usr/sbin/x6200_gui 2>/dev/null | cut -d' ' -f1)"
note "   init.d en la rootfs: $(ls "$T/etc/init.d" 2>/dev/null | tr '\n' ' ')"
note "   herramientas del target: mountpoint=$(ls "$T/bin/mountpoint" "$T/usr/bin/mountpoint" 2>/dev/null | tr '\n' ',') mkfs.ext4=$(ls "$T/sbin/mkfs.ext4" 2>/dev/null) parted=$(ls "$T/usr/sbin/parted" 2>/dev/null)"
note "   dispositivos de bloque visibles en el contenedor: $(ls /dev/mmcblk* /dev/sd[a-z] /dev/nvme* 2>/dev/null | tr '\n' ' ')(ninguno es usado por este caso)"

case "$CASE" in

f02-corrupt)
    mnt_tmpfs || bad "no se pudo montar el tmpfs de /mnt"
    printf 'esto no es una base sqlite' > "$T/mnt/params.db"
    ANTES=$(sha "$T/mnt/params.db")
    OUTF="$OUTDIR/f02-corrupt.out"
    RC=$(gui_fg 60 "$OUTF")
    note "   rc=$RC"
    check "$([ "$RC" = 78 ]; echo $?)" "la GUI sale 78 (config error) con la base corrupta, rc medido=$RC"
    en_out "Persistence unavailable: check DATA and params.db" "$OUTF"
    check "$([ "$(sha "$T/mnt/params.db")" = "$ANTES" ]; echo $?)" "los bytes de la base corrupta quedan idénticos"
    check "$([ ! -S "$T/tmp/x6200_remote.sock" ]; echo $?)" "no abrió el socket de control (no inicializó radio/CAT)"
    check "$([ ! -f "$T/tmp/crash.txt" ]; echo $?)" "sin archivo de crash (no hubo SIGSEGV)"
    ;;

f02-nomount)
    mnt_none
    OUTF="$OUTDIR/f02-nomount.out"
    RC=$(gui_fg 60 "$OUTF")
    note "   rc=$RC"
    check "$([ "$RC" = 78 ]; echo $?)" "la GUI sale 78 con DATA sin montar, rc medido=$RC"
    en_out "DATA is not mounted" "$OUTF"
    check "$([ ! -e "$T/mnt/params.db" ]; echo $?)" "no creó /mnt/params.db sobre el rootfs"
    ;;

f03-absent)
    mnt_tmpfs || bad "no se pudo montar el tmpfs de /mnt"
    [ -f "$T/usr/share/x6200/params.default.db" ] || bad "el rootfs no trae la plantilla params.default.db"
    OUTF="$OUTDIR/f03-absent.out"
    gui_bg "$OUTF"
    SEG=$(wait_socket)
    note "   socket en ${SEG:-nunca}s"
    check "$([ -n "$SEG" ]; echo $?)" "la GUI arranca con DATA rw y base ausente (socket abierto)"
    en_out "seeding from /usr/share/x6200/params.default.db" "$OUTF"
    check "$([ -f "$T/mnt/params.db" ]; echo $?)" "sembró /mnt/params.db"
    MODE=$(stat -c %a "$T/mnt/params.db" 2>/dev/null)
    check "$([ "$MODE" = 644 ]; echo $?)" "modo de la base sembrada 0644 (medido $MODE)"
    VER=$(db_query "$T/mnt/params.db" "SELECT id FROM version;")
    check "$([ "$VER" = 5 ]; echo $?)" "versión de esquema = 5 (medido ${VER:-?})"
    INTEG=$(db_query "$T/mnt/params.db" "PRAGMA integrity_check;")
    check "$([ "$INTEG" = ok ]; echo $?)" "PRAGMA integrity_check = ${INTEG:-?}"
    stop_gui
    ;;

f03-writefail)
    mnt_tmpfs || bad "no se pudo montar el tmpfs de /mnt"
    cp "$F/params-threshold-invalid.db" "$T/mnt/params.db" || bad "falta el fixture de umbral inválido"
    DBK=$(du -k "$T/mnt/params.db" | cut -f1)
    FILL=$((32768 - DBK - 64))
    dd if=/dev/zero of="$T/mnt/filler" bs=1024 count="$FILL" > /dev/null 2>&1 || true
    note "   /mnt: base ${DBK}K + relleno ${FILL}K sobre un tmpfs de 32M"
    OUTF="$OUTDIR/f03-writefail.out"
    gui_bg "$OUTF"
    SEG=$(wait_socket)
    note "   socket en ${SEG:-nunca}s"
    check "$([ -n "$SEG" ]; echo $?)" "la GUI arranca aunque la escritura falle"
    sleep 3
    en_out "Persistence write failed" "$OUTF"
    INTEG=$(db_query "$T/mnt/params.db" "PRAGMA integrity_check;")
    check "$([ "$INTEG" = ok ]; echo $?)" "la base existente sigue íntegra (${INTEG:-?})"
    check "$([ "$(gui_procs)" -ge 1 ]; echo $?)" "la GUI sigue viva (no aborta por la escritura fallida)"
    stop_gui
    ;;

f04-daemon-exit78)
    # INTERACCIÓN #213 x #214: el exit 78 de la base inválida cae en el backoff
    # máximo del supervisor (no en el lazo rápido del daemon viejo).
    mnt_tmpfs || bad "no se pudo montar el tmpfs de /mnt"
    rm -f "$T/mnt/app_logs"/* 2>/dev/null || true
    printf 'esto no es una base sqlite' > "$T/mnt/params.db"
    DAEMONOUT="$OUTDIR/f04-daemon-exit78.out"
    ( cd "$T/tmp" && timeout 12 chroot "$T" /bin/busybox env QEMU_CPU=cortex-a7 /usr/sbin/x6200_daemon ) > "$DAEMONOUT" 2>&1 &
    WGUI=$!
    sleep 11
    NEXIT=$(grep -c 'GUI exited: code 78' "$DAEMONOUT" 2>/dev/null)
    note "   'GUI exited: code 78' en ~11 s: $NEXIT"
    check "$([ "$NEXIT" -ge 1 ]; echo $?)" "el supervisor informa el exit 78 del hijo SQLite"
    check "$([ "$NEXIT" -le 2 ]; echo $?)" "sin lazo rápido: ≤2 arranques en 11 s (repo pre-fix: 34 en 10 s)"
    en_out "GUI restart in 60s" "$DAEMONOUT"
    LOGS=$(ls "$T/mnt/app_logs" 2>/dev/null | wc -l)
    note "   logs en /mnt/app_logs: $LOGS [$(ls "$T/mnt/app_logs" 2>/dev/null | tr '\n' ' ')]"
    check "$([ "$LOGS" -ge 1 ]; echo $?)" "el supervisor dejó el log de la corrida en DATA"
    UNIQ=$(ls "$T/mnt/app_logs" 2>/dev/null | grep -c '^x6200_log_[0-9_-]*_p[0-9]*_r[0-9]*\.txt$')
    check "$([ "$LOGS" -ge 1 ] && [ "$UNIQ" = "$LOGS" ]; echo $?)" "nombre único por corrida (timestamp+pid+nº ejecución)"
    kill -TERM "$WGUI" 2>/dev/null
    sleep 2
    VIVAS=$(gui_procs)
    note "   procesos x6200_gui vivos tras SIGTERM: $VIVAS"
    check "$([ "$VIVAS" = 0 ]; echo $?)" "tras SIGTERM no queda ninguna GUI viva (no relanza)"
    wait "$WGUI" 2>/dev/null
    ;;

f04-daemon-nomnt)
    mnt_none
    DAEMONOUT="$OUTDIR/f04-daemon-nomnt.out"
    rm -f "$T/tmp"/x6200_log_*_p*_r*.txt 2>/dev/null || true
    ( cd "$T/tmp" && timeout 8 chroot "$T" /bin/busybox env QEMU_CPU=cortex-a7 /usr/sbin/x6200_daemon ) > "$DAEMONOUT" 2>&1 &
    WGUI=$!
    sleep 7
    en_out "not writable, keeping log at" "$DAEMONOUT"
    ENMNT=$(find "$T/mnt" -type f 2>/dev/null | wc -l)
    note "   archivos bajo /mnt (rootfs): $ENMNT"
    check "$([ "$ENMNT" = 0 ]; echo $?)" "0 archivos escritos sobre el rootfs cuando DATA no está"
    TMPLOGS=$(ls "$T/tmp"/x6200_log_*_p*_r*.txt 2>/dev/null | wc -l)
    note "   logs retenidos en /tmp: $TMPLOGS"
    check "$([ "$TMPLOGS" -ge 1 ]; echo $?)" "queda el log retenido en /tmp (diagnóstico disponible)"
    kill -TERM "$WGUI" 2>/dev/null
    wait "$WGUI" 2>/dev/null
    ;;

f01f03-chain)
    # F-01 -> F-03 -> persistencia: cadena que ningún hijo probó por separado.
    if mnt_ext4_img /tmp/data.img; then
        EXT4=si
    else
        EXT4=no
        skip "no se pudo montar una imagen ext4 con loop: se cae a tmpfs (sin persistencia entre arranques)"
        mnt_tmpfs || bad "no se pudo montar /mnt"
    fi
    check "$(mountpoint -q "$T/mnt"; echo $?)" "DATA (ext4=${EXT4}) montada en /mnt"
    check "$([ ! -e "$T/mnt/params.db" ]; echo $?)" "DATA recién creada, sin params.db"

    OUTF1="$OUTDIR/chain-boot1.out"
    gui_bg "$OUTF1"
    SEG1=$(wait_socket)
    note "   boot 1: socket en ${SEG1:-nunca}s"
    check "$([ -n "$SEG1" ]; echo $?)" "boot 1: la GUI arranca sobre la DATA nueva"
    en_out "seeding from /usr/share/x6200/params.default.db" "$OUTF1"
    estado '{"cmd":"freq","val":14074000}' > "$OUTDIR/chain-set.txt" 2>&1
    sleep 1
    estado '{"cmd":"get"}' > "$OUTDIR/chain-boot1.state" 2>&1
    F1=$(freq_of "$OUTDIR/chain-boot1.state")
    note "   boot 1: freq por socket = ${F1:-?}"
    check "$([ "$F1" = 14074000 ]; echo $?)" "boot 1: la GUI acepta el cambio de frecuencia"
    sha1db=$(sha "$T/mnt/params.db")
    stop_gui
    sync

    umount "$T/mnt" || bad "no se pudo desmontar la DATA"
    mount -t ext4 -o loop "$T/tmp/data.img" "$T/mnt" 2>/dev/null || mount -t tmpfs tmpfs "$T/mnt" || true
    OTRA=$(sha "$T/mnt/params.db")
    note "   sha de params.db: boot1=$sha1db reboot=$OTRA"
    check "$([ "$sha1db" = "$OTRA" ]; echo $?)" "la base persiste byte a byte tras el remontaje"

    OUTF2="$OUTDIR/chain-boot2.out"
    gui_bg "$OUTF2"
    SEG2=$(wait_socket)
    note "   boot 2: socket en ${SEG2:-nunca}s"
    check "$([ -n "$SEG2" ]; echo $?)" "boot 2: la GUI arranca desde la DATA existente"
    no_en_out "seeding from" "$OUTF2"
    sleep 1
    estado '{"cmd":"get"}' > "$OUTDIR/chain-boot2.state" 2>&1
    F2=$(freq_of "$OUTDIR/chain-boot2.state")
    note "   boot 2: freq por socket = ${F2:-?}"
    check "$([ -n "$F2" ] && [ "$F1" = "$F2" ]; echo $?)" "boot 2: la frecuencia persistida sobrevive al reinicio"
    stop_gui
    ;;

*)
    bad "caso desconocido: $CASE"
    ;;
esac

note "== resultado: $CASE FAILS=$FAILS"
[ "$FAILS" = 0 ]
