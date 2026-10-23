#!/bin/bash
#
# Banco hermetico del camino de actualizacion de la ROM X6200
# (issue iariap/x6200_gui#215, epica #211).
#
# Corre SOLO en un contenedor privado y desechable (tests/release/Dockerfile:
# userland x86_64 con qemu-user-static) y ejecuta el S02update real dentro de un
# chroot sobre una COPIA del rootfs objetivo, con el BusyBox y el bash ARM de la
# imagen. Nunca toca la radio, la SD del owner, ni los contenedores o volumenes
# compartidos (banco-apps, banco-audio, x6200-build-output).
#
# Desde el checkout:
#
#   tests/release/test_safe_update.sh <paquete-publicado.tgz> [directorio-de-lab]
#
# Sin fixture imprime "SKIP:" y sale 77 (como las suites de codecs). El fixture es
# un paquete real de la ROM publicada: se usa como arbol de la "imagen" (sus
# archivos) y como contraste historico. Todo el trabajo pasa en <lab>, que es
# privado de esta corrida.
#
# Que cubre: paquete bueno, segunda aplicacion, rutas nuevas, backup verificado y
# su restauracion a mano, sidecar faltante / con hash malo / con otro nombre,
# tgz truncado, entradas absolutas / ".." / "./" / duplicadas / bajo /mnt /dev
# /tmp, nodos de dispositivo, setuid de mas, updater con uid 1000, tar que falla
# al extraer (y que el paquete quede retenido), reuso del backup identico, sin
# espacio, /mnt read-only, /mnt sin montar, symlinks con target absoluto o "..",
# y el primer update de una imagen historica (UNSUPPORTED/UNSAFE_LEGACY).
#
# Tambien la re-entrada del pase (issue #231): este archivo es el que la
# actualizacion reemplaza, asi que el shell que lo lee puede re-parsear la cola
# del script instalado y re-correr el despacho. La marca del pase
# (S02UPDATE_PASS) tiene que hacer esa re-entrada inerte.
#
# SPDX-License-Identifier: LGPL-2.1-or-later
#
set -u

# ---------------------------------------------------------------------------
# Fuera del contenedor: se re-ejecuta adentro.
# ---------------------------------------------------------------------------
if [ ! -e /.dockerenv ]; then
    FIXTURE=${1:-}
    if [ -z "$FIXTURE" ] || [ ! -f "$FIXTURE" ]; then
        echo "SKIP: pasá como argumento un paquete publicado de la ROM (x6200_r1cbu_*.tgz)" >&2
        echo "      p. ej. /home/pablo/desa/x6200/images/x6200_r1cbu_v0.1.2-lu2dsx.tgz" >&2
        exit 77
    fi
    LAB=${2:-}
    [ -n "$LAB" ] || LAB=$(mktemp -d /tmp/h215-safe-update.XXXXXX)
    HERE=$(cd "$(dirname "$0")" && pwd)
    GUI=$(cd "$HERE/../.." && pwd)
    if ! docker image inspect h215-x6200-update-lab >/dev/null 2>&1; then
        echo "==> construyendo la imagen privada h215-x6200-update-lab"
        docker build -t h215-x6200-update-lab "$HERE" || exit 2
    fi
    echo "==> banco privado: contenedor h215-x6200-update-lab, lab $LAB"
    # Cotas de recursos (issue iariap/x6200_gui#231): este banco corre el
    # S02update del target, que puede re-ejecutarse a sí mismo. Sin cota, un lazo
    # de procesos se lleva la RAM y el slab del HOST (medido en el issue: 70 GB y
    # 2502 procesos) y tumba la máquina. Con --pids-limit el lazo se corta por
    # fork (EAGAIN dentro del contenedor) y con --memory/--memory-swap la memoria
    # del cgroup queda acotada: el banco falla visible, no se lleva el host.
    # Mismos nombres de variables que el arnés A/B (tools/ab_testing).
    PIDS_LIMIT=${X6200_LAB_PIDS_LIMIT:-512}
    MEMORY=${X6200_LAB_DOCKER_MEMORY:-6g}
    echo "    cotas: pids-limit=$PIDS_LIMIT memory=$MEMORY (override: X6200_LAB_PIDS_LIMIT / X6200_LAB_DOCKER_MEMORY)"
    exec docker run --rm --privileged \
        --pids-limit "$PIDS_LIMIT" \
        --memory "$MEMORY" --memory-swap "$MEMORY" \
        -v "$GUI":/repo:ro \
        -v "$(cd "$(dirname "$FIXTURE")" && pwd)":/fixtures:ro \
        -v "$LAB":/lab \
        -e FIXTURE="/fixtures/$(basename "$FIXTURE")" \
        -e GUI_SHA="$(git -C "$GUI" rev-parse --short HEAD 2>/dev/null || echo desconocido)" \
        -e HOST_UID="$(id -u)" \
        -e HOST_GID="$(id -g)" \
        h215-x6200-update-lab bash /repo/tests/release/test_safe_update.sh --inside
fi

# ---------------------------------------------------------------------------
# Adentro del contenedor.
# ---------------------------------------------------------------------------
REPO=${REPO:-/repo}
LAB=${LAB:-/lab}
FIXTURE=${FIXTURE:?falta FIXTURE}
GUI_SHA=${GUI_SHA:-desconocido}
WORK=$LAB/work
TARGET=$LAB/target
PKG=$WORK/pkg
LOGS=$WORK/cases
RESULTS=$WORK/results.tsv
GOOD=x6200_r1cbu_v215.tgz
GOODEXT=x6200_r1cbu_v215ext.tgz

PASS=0
FAIL=0
LEGACY=0

trap cleanup_lab EXIT

say() { printf '%s\n' "$*"; }
ok() { printf 'ok   %s\n' "$*" >> "$CASE_LOG/steps.txt"; }
bad() { printf 'FAIL %s\n' "$*" >> "$CASE_LOG/steps.txt"; printf '    FAIL: %s\n' "$*"; CASE_FAIL=1; }
# Un chequeo que no puede correr queda visible como SKIP, nunca como pase.
skip_check() { printf 'SKIP %s\n' "$*" >> "$CASE_LOG/steps.txt"; printf '    SKIP: %s\n' "$*"; }

begin() {
    CASE=$1
    CASE_DESC=$2
    CASE_FAIL=0
    CASE_LOG=$LOGS/$CASE
    rm -rf "$CASE_LOG"
    mkdir -p "$CASE_LOG"
    say ""
    say "==> [$CASE] $CASE_DESC"
}

finish() {
    local verdict
    if [ "$CASE_FAIL" = "0" ]; then
        verdict=PASS
        PASS=$((PASS + 1))
    else
        verdict=FAIL
        FAIL=$((FAIL + 1))
    fi
    printf '%s\t%s\t%s\n' "$CASE" "$CASE_DESC" "$verdict" >> "$RESULTS"
}

finish_legacy() {
    LEGACY=$((LEGACY + 1))
    printf '%s\t%s\t%s\n' "$CASE" "$CASE_DESC" UNSUPPORTED/UNSAFE_LEGACY >> "$RESULTS"
}

# ---------------------------------------------------------------------------
# utilidades del banco
# ---------------------------------------------------------------------------
umount_tree() {
    local d
    for d in mnt proc dev; do
        if [ -d "$TARGET/$d" ] && mountpoint -q "$TARGET/$d"; then
            umount "$TARGET/$d"
        fi
    done
    return 0
}

# El banco corre como root dentro del contenedor sobre un directorio del host:
# al terminar saca el scratch pesado y le deja el lab al usuario que lo lanzo,
# para que pueda leer la evidencia y borrarlo.
cleanup_lab() {
    umount_tree
    rm -rf "$WORK/raw" "$WORK/stock" "$WORK/ext" "$WORK/restore" \
        "$WORK/image-new.tar" "$WORK/image-stock.tar" "$WORK/image-ext.tar" "$TARGET"
    if [ -n "${HOST_UID:-}" ] && [ -n "${HOST_GID:-}" ]; then
        chown -R "$HOST_UID:$HOST_GID" "$LAB"
    fi
    return 0
}

# Estado del arbol instalado (sin /mnt, /tmp, /proc, /sys, /dev, /run, que son
# scratch o montajes): nombres, tipo, modo, dueño y tamaño.
tree_state() {
    ( cd "$TARGET" && find . \
        \( -path ./mnt -o -path ./tmp -o -path ./proc -o -path ./sys -o -path ./dev -o -path ./run \) -prune \
        -o -printf '%P %y %m %U %G %s\n' ) | LC_ALL=C sort | sha256sum | awk '{ print $1 }'
}

reset_target() {
    umount_tree
    rm -rf "$TARGET"
    mkdir -p "$TARGET"
    tar -xf "$1" -C "$TARGET"
    install -m 0755 /usr/bin/qemu-arm-static "$TARGET/usr/bin/qemu-arm-static"
    mkdir -p "$TARGET/mnt"
    mount -t proc proc "$TARGET/proc"
    # /dev propio del target (tmpfs + los nodos que necesitan bash y las
    # herramientas): el paquete tambien escribe dev/* (la rootfs trae dev/fd,
    # dev/stdin, ...), asi que no se comparte el /dev del contenedor.
    mount -t tmpfs -o mode=0755 tmpfs "$TARGET/dev"
    while read -r node node_type major minor node_mode; do
        mknod -m "$node_mode" "$TARGET/dev/$node" "$node_type" "$major" "$minor"
    done <<'NODES'
null c 1 3 666
zero c 1 5 666
full c 1 7 666
random c 1 8 666
urandom c 1 9 666
tty c 5 0 666
NODES
    # El arranque real deja estos symlinks en /dev (inittab) y syslogd crea
    # /dev/log: el target de la prueba tiene que parecerse al de la radio, o el
    # paquete veria como "rutas nuevas" esas cinco entradas de la rootfs.
    ln -sf ../proc/self/fd "$TARGET/dev/fd"
    ln -sf ../proc/self/fd/0 "$TARGET/dev/stdin"
    ln -sf ../proc/self/fd/1 "$TARGET/dev/stdout"
    ln -sf ../proc/self/fd/2 "$TARGET/dev/stderr"
    ln -sf ../tmp/log "$TARGET/dev/log"
}

# $1 = tamaño del tmpfs de /mnt en MiB, "ro" o "none" (directorio sin montar).
mount_mnt() {
    case "$1" in
        none) return 0 ;;
        ro)
            mount -t tmpfs -o size=400m,mode=0755 tmpfs "$TARGET/mnt"
            ;;
        *)
            mount -t tmpfs -o "size=$1m,mode=0755" tmpfs "$TARGET/mnt"
            ;;
    esac
}

put_pkg() {
    local f
    for f in "$@"; do
        cp "$f" "$TARGET/mnt/"
        [ -f "$f.sha256" ] && cp "$f.sha256" "$TARGET/mnt/"
    done
    return 0
}

run_updater() {
    GUI_BEFORE=$(sha256sum "$TARGET/usr/sbin/x6200_gui" | awk '{ print $1 }')
    TREE_BEFORE=$(tree_state)
    chroot "$TARGET" /bin/sh /etc/init.d/S02update start >"$CASE_LOG/stdout.txt" 2>"$CASE_LOG/stderr.txt"
    RC=$?
    cp "$CASE_LOG/stdout.txt" "$CASE_LOG/stdout.raw"
    say "    rc=$RC"
    say "    stdout: $(tr '\n' '|' < "$CASE_LOG/stdout.txt")"
    say "    stderr: $(tr '\n' '|' < "$CASE_LOG/stderr.txt")"
}

gui_hash() { sha256sum "$1/usr/sbin/x6200_gui" | awk '{ print $1 }'; }

# Como run_updater, pero con la marca de un pase que ya corrió en el mismo árbol
# de procesos: es la re-entrada (issue iariap/x6200_gui#231, ver el caso
# updater-reentry).
reentry_updater() {
    GUI_BEFORE=$(sha256sum "$TARGET/usr/sbin/x6200_gui" | awk '{ print $1 }')
    TREE_BEFORE=$(tree_state)
    S02UPDATE_PASS=0 chroot "$TARGET" /bin/sh /etc/init.d/S02update start >"$CASE_LOG/stdout.txt" 2>"$CASE_LOG/stderr.txt"
    RC=$?
    cp "$CASE_LOG/stdout.txt" "$CASE_LOG/stdout.raw"
    say "    rc=$RC"
    say "    stdout: $(tr '\n' '|' < "$CASE_LOG/stdout.txt")"
    say "    stderr: $(tr '\n' '|' < "$CASE_LOG/stderr.txt")"
}

assert_rc() {
    [ "$RC" = "$1" ] && ok "rc=$RC" || bad "rc=$RC (esperaba $1)"
}

assert_out() {
    grep -qxF -- "$1" "$CASE_LOG/stdout.txt" && ok "stdout dice '$1'" || bad "stdout no dice '$1'"
}

assert_out_has() {
    grep -qF -- "$1" "$CASE_LOG/stdout.txt" && ok "stdout tiene '$1'" || bad "stdout no tiene '$1'"
}

assert_err_has() {
    grep -qF -- "$1" "$CASE_LOG/stderr.txt" && ok "stderr tiene '$1'" || bad "stderr no tiene '$1'"
}

assert_out_lacks() {
    grep -qF -- "$1" "$CASE_LOG/stdout.txt" && bad "stdout dice '$1' y no debía" || ok "stdout no dice '$1'"
}

assert_unchanged() {
    local now
    now=$(tree_state)
    [ "$now" = "$TREE_BEFORE" ] && ok "nada bajo / cambió" || bad "algo bajo / cambió"
    [ "$(gui_hash "$TARGET")" = "$GUI_BEFORE" ] && ok "la GUI quedó la de antes" || bad "la GUI cambió"
}

assert_pkg_kept() {
    [ -f "$TARGET/mnt/$1" ] && ok "el paquete $1 sigue en /mnt" || bad "el paquete $1 desapareció"
    [ -f "$TARGET/mnt/$1.sha256" ] && ok "el sidecar de $1 sigue en /mnt" || bad "el sidecar de $1 desapareció"
}

assert_pkg_gone() {
    [ -f "$TARGET/mnt/$1" ] && bad "el paquete $1 sigue en /mnt" || ok "el paquete $1 se consumió"
    [ -f "$TARGET/mnt/$1.sha256" ] && bad "el sidecar de $1 sigue en /mnt" || ok "el sidecar de $1 se consumió"
}

assert_no_backup() {
    local n
    n=$(find "$TARGET/mnt" -maxdepth 1 -name 'backup_*' | wc -l)
    [ "$n" = "0" ] && ok "no quedó backup ni .partial" || bad "quedó $n archivo(s) backup_* en /mnt"
}

assert_gui_pkg() {
    local want
    want=$(tar -xOf "$1" usr/sbin/x6200_gui | sha256sum | awk '{ print $1 }')
    [ "$(gui_hash "$TARGET")" = "$want" ] && ok "la GUI instalada es la del paquete" || bad "la GUI instalada no es la del paquete"
}

assert_setuid_ok() {
    local got
    got=$(cd "$TARGET" && find . \
        \( -path ./proc -o -path ./sys -o -path ./dev \) -prune \
        -o -perm -4000 -printf '%P %m %U %G\n' | LC_ALL=C sort | tr '\n' ';')
    [ "$got" = "bin/busybox 4755 0 0;usr/libexec/dbus-daemon-launch-helper 4750 0 101;usr/libexec/ssh-keysign 4711 0 0;" ] &&
        ok "setuid: los 3 auditados" || bad "setuid: '$got'"
}

assert_updater_owner() {
    local line
    line=$(cd "$TARGET" && find etc/init.d/S02update -printf '%m %U %G')
    [ "$line" = "755 0 0" ] && ok "el updater quedó root:root 0755" || bad "el updater quedó '$line'"
}

# ---------------------------------------------------------------------------
# armado de las imagenes y de los paquetes
# ---------------------------------------------------------------------------
prepare() {
    mkdir -p "$WORK" "$LOGS" "$PKG"
    rm -rf "$WORK/raw" "$WORK/stock" "$WORK/ext"
    say "==> extrayendo el fixture $FIXTURE"
    mkdir -p "$WORK/raw"
    tar -xf "$FIXTURE" -C "$WORK/raw"

    # La imagen auditada trae 3 setuid (bin/mount y bin/umount son 0755); el
    # fixture publicado es anterior y los trae 4755.
    chmod 0755 "$WORK/raw/bin/mount" "$WORK/raw/bin/umount"
    mkdir -p "$WORK/raw/mnt" "$WORK/raw/proc" "$WORK/raw/sys" "$WORK/raw/tmp" \
        "$WORK/raw/dev" "$WORK/raw/run" "$WORK/raw/opt"
    chmod 1777 "$WORK/raw/tmp"
    # El updater de la imagen: el nuevo, root:root 0755.
    install -m 0755 -o 0 -g 0 "$REPO/tools/release/update-overlay/etc/init.d/S02update" \
        "$WORK/raw/etc/init.d/S02update"
    tar -cf "$WORK/image-new.tar" --numeric-owner -C "$WORK/raw" .

    # Imagen historica: el S02update stock de la imagen (xargs, sin sidecar).
    cp -a "$WORK/raw" "$WORK/stock"
    install -m 0755 -o 0 -g 0 "$REPO/tests/release/fixtures/S02update.image-stock" \
        "$WORK/stock/etc/init.d/S02update"
    tar -cf "$WORK/image-stock.tar" --numeric-owner -C "$WORK/stock" .

    # Imagen con dos archivos que el paquete agrega (rutas nuevas).
    cp -a "$WORK/raw" "$WORK/ext"
    mkdir -p "$WORK/ext/opt/x6200"
    printf 'h215 fixture\n' > "$WORK/ext/opt/x6200/h215-added.txt"
    printf '{}\n' > "$WORK/ext/usr/share/x6200/h215-added.json"
    tar -cf "$WORK/image-ext.tar" --numeric-owner -C "$WORK/ext" .

    say "==> paquetes buenos (con el empaquetador arreglado)"
    if ! bash "$REPO/tools/release/make_update_tgz.sh" "$WORK/image-new.tar" "$PKG/$GOOD"; then
        echo "FALLA: el empaquetador no pudo armar $GOOD (sin fixture no hay banco)" >&2
        exit 2
    fi
    if ! bash "$REPO/tools/release/make_update_tgz.sh" "$WORK/image-ext.tar" "$PKG/$GOODEXT"; then
        echo "FALLA: el empaquetador no pudo armar $GOODEXT (sin fixture no hay banco)" >&2
        exit 2
    fi
    [ -f "$PKG/$GOOD.sha256" ] && [ -f "$PKG/$GOODEXT.sha256" ] || {
        echo "FALLA: falta el sidecar de un paquete bueno" >&2
        exit 2
    }
}

mkfixture() { # mkfixture <salida.tgz> [--mutate X]
    python3 "$REPO/tests/release/mkpkg.py" minimal "$@" >>"$CASE_LOG/steps.txt" 2>&1
}

mkdir -p "$WORK" "$LOGS" "$PKG"
: > "$RESULTS"
say "==> banco de actualizacion verificada (issue #215), GUI $GUI_SHA"
say "==> fixture: $FIXTURE"
say "==> qemu-arm-static: $(/usr/bin/qemu-arm-static --version 2>&1 | head -n 1)"
prepare

# ---------------------------------------------------------------------------
# R1: la regla de tipos y de targets de enlaces del preflight (sobre el awk real)
# ---------------------------------------------------------------------------
begin types-rule "preflight: la regla de tipos y de targets de enlaces (19 casos de tabla)"
if python3 "$REPO/tests/release/check_types_rule.py" \
        "$REPO/tools/release/update-overlay/etc/init.d/S02update" >"$CASE_LOG/steps.txt" 2>&1; then
    ok "19/19 casos de la tabla"
else
    bad "la tabla de tipos/enlaces no coincide con la regla (19 casos, ver steps.txt)"
fi
finish

# ---------------------------------------------------------------------------
# P1: paquete bueno, primera aplicacion
# ---------------------------------------------------------------------------
begin good-first-apply "paquete bueno: se aplica, deja backup verificado y consume paquete y sidecar"
reset_target "$WORK/image-new.tar"
mount_mnt 1024
put_pkg "$PKG/$GOOD"
say "    entradas del paquete: $(tar -tzf "$TARGET/mnt/$GOOD" | wc -l)"
n_img=$(find "$WORK/raw" \( -type f -o -type l \) | wc -l)
n_pkg=$(tar -tzf "$PKG/$GOOD" | wc -l)
[ "$n_img" = "$n_pkg" ] && ok "el paquete lleva todas las entradas de la imagen ($n_pkg)" ||
    bad "el paquete lleva $n_pkg entradas y la imagen tiene $n_img"
img_gui=$(sha256sum "$WORK/raw/usr/sbin/x6200_gui" | awk '{ print $1 }')
pkg_gui=$(tar -xOf "$PKG/$GOOD" usr/sbin/x6200_gui | sha256sum | awk '{ print $1 }')
[ "$img_gui" = "$pkg_gui" ] && ok "la GUI del paquete es la de la imagen" || bad "la GUI del paquete no es la de la imagen"
run_updater
assert_rc 0
assert_out "Update applied"
assert_pkg_gone "$GOOD"
assert_gui_pkg "$PKG/$GOOD"
assert_setuid_ok
assert_updater_owner
[ -f "$TARGET/mnt/backup_$GOOD" ] && ok "quedó backup_$GOOD" || bad "no quedó backup_$GOOD"
[ -f "$TARGET/mnt/new_$GOOD.list" ] && ok "quedó la lista de rutas nuevas" || bad "no quedó new_$GOOD.list"
[ -s "$TARGET/mnt/new_$GOOD.list" ] && bad "la lista de rutas nuevas no debería tener entradas" || ok "sin rutas nuevas (paquete de reemplazo puro)"
npartial=$(find "$TARGET/mnt" -maxdepth 1 -name '*.partial' | wc -l)
[ "$npartial" = "0" ] && ok "no quedó .partial" || bad "quedaron $npartial .partial"
# El backup tiene que listar exactamente las rutas del paquete que existían, y
# traer el binario viejo adentro (es de lo que se restaura a mano).
mkdir -p "$CASE_LOG/backup"
if tar -xf "$TARGET/mnt/backup_$GOOD" -C "$CASE_LOG/backup" usr/sbin/x6200_gui 2>>"$CASE_LOG/steps.txt"; then
    [ "$(sha256sum "$CASE_LOG/backup/usr/sbin/x6200_gui" | awk '{ print $1 }')" = "$GUI_BEFORE" ] &&
        ok "el backup trae la GUI vieja ($(printf '%.8s' "$GUI_BEFORE"))" ||
        bad "el backup no trae la GUI vieja"
else
    bad "no pude extraer la GUI del backup"
fi
bk_names=$(tar -tzf "$TARGET/mnt/backup_$GOOD" | LC_ALL=C sort | sha256sum | awk '{ print $1 }')
pk_names=$(tar -tzf "$PKG/$GOOD" | LC_ALL=C sort | sha256sum | awk '{ print $1 }')
[ "$bk_names" = "$pk_names" ] && ok "el backup lista las mismas rutas que el paquete" || bad "el backup no lista las mismas rutas"
# Restauracion a mano: el backup aplicado sobre una copia devuelve el arbol previo.
rm -rf "$WORK/restore"
cp -a "$WORK/raw" "$WORK/restore"
tar -xf "$TARGET/mnt/backup_$GOOD" -C "$WORK/restore"
[ "$(gui_hash "$WORK/restore")" = "$GUI_BEFORE" ] && ok "restaurar el backup a mano devuelve la GUI previa" ||
    bad "el backup no restaura la GUI previa"
finish

# ---------------------------------------------------------------------------
# P2: paquete que agrega rutas nuevas
# ---------------------------------------------------------------------------
begin good-added-paths "paquete con archivos nuevos: se instalan y quedan en /mnt/new_*.list, no en el backup"
reset_target "$WORK/image-new.tar"
mount_mnt 1024
put_pkg "$PKG/$GOODEXT"
run_updater
assert_rc 0
assert_out "Update applied"
assert_pkg_gone "$GOODEXT"
[ -f "$TARGET/opt/x6200/h215-added.txt" ] && ok "el archivo nuevo se instaló" || bad "el archivo nuevo no está"
assert_out_lacks "Update verification failed"
if [ -f "$TARGET/mnt/new_$GOODEXT.list" ]; then
    got=$(LC_ALL=C sort "$TARGET/mnt/new_$GOODEXT.list" | tr '\n' ';')
    [ "$got" = "opt/x6200/h215-added.txt;usr/share/x6200/h215-added.json;" ] &&
        ok "la lista de rutas nuevas es la esperada" || bad "la lista de rutas nuevas es '$got'"
else
    bad "no quedó new_$GOODEXT.list"
fi
if tar -tzf "$TARGET/mnt/backup_$GOODEXT" | grep -qx 'opt/x6200/h215-added.txt'; then
    bad "el backup incluye una ruta nueva (no existía antes)"
else
    ok "el backup no incluye rutas nuevas"
fi
finish

# ---------------------------------------------------------------------------
# P3: segunda aplicacion (dos paquetes en /mnt)
# ---------------------------------------------------------------------------
begin subsequent-apply "segunda aplicacion: los dos paquetes se aplican en orden y cada uno deja su backup"
reset_target "$WORK/image-new.tar"
mount_mnt 1024
put_pkg "$PKG/$GOOD" "$PKG/$GOODEXT"
run_updater
assert_rc 0
assert_out "Update applied"
assert_pkg_gone "$GOOD"
assert_pkg_gone "$GOODEXT"
assert_gui_pkg "$PKG/$GOOD"
assert_setuid_ok
[ -f "$TARGET/mnt/backup_$GOOD" ] && ok "backup del primer paquete" || bad "falta el backup del primer paquete"
[ -f "$TARGET/mnt/backup_$GOODEXT" ] && ok "backup del segundo paquete" || bad "falta el backup del segundo paquete"
finish

# ---------------------------------------------------------------------------
# N1..N3: sidecar
# ---------------------------------------------------------------------------
begin sidecar-missing "sin sidecar: no se toca nada y el paquete queda retenido"
reset_target "$WORK/image-new.tar"
mount_mnt 1024
cp "$PKG/$GOOD" "$TARGET/mnt/"
run_updater
assert_rc 1
assert_out "Update verification failed"
[ -f "$TARGET/mnt/$GOOD" ] && ok "el paquete sigue en /mnt" || bad "el paquete desapareció"
assert_unchanged
assert_no_backup
finish

begin sidecar-bad-hash "sidecar con hash malo: no se toca nada"
reset_target "$WORK/image-new.tar"
mount_mnt 1024
put_pkg "$PKG/$GOOD"
printf '%s  %s\n' "$(printf '0%.0s' $(seq 1 64))" "$GOOD" > "$TARGET/mnt/$GOOD.sha256"
run_updater
assert_rc 1
assert_out "Update verification failed"
assert_err_has "sidecar says"
assert_pkg_kept "$GOOD"
assert_unchanged
finish

begin sidecar-wrong-name "sidecar que nombra otro archivo: no se toca nada"
reset_target "$WORK/image-new.tar"
mount_mnt 1024
put_pkg "$PKG/$GOOD"
printf '%s  %s\n' "$(cut -d' ' -f1 < "$PKG/$GOOD.sha256")" "otro_paquete.tgz" > "$TARGET/mnt/$GOOD.sha256"
run_updater
assert_rc 1
assert_out "Update verification failed"
assert_pkg_kept "$GOOD"
assert_unchanged
finish

begin truncated-tgz "tgz truncado: gzip -t lo detecta y no se toca nada"
reset_target "$WORK/image-new.tar"
mount_mnt 1024
cp "$PKG/$GOOD" "$TARGET/mnt/"
head -c 200000 "$PKG/$GOOD" > "$TARGET/mnt/$GOOD.truncado"
mv "$TARGET/mnt/$GOOD.truncado" "$TARGET/mnt/$GOOD"
printf '%s  %s\n' "$(sha256sum "$TARGET/mnt/$GOOD" | awk '{ print $1 }')" "$GOOD" > "$TARGET/mnt/$GOOD.sha256"
run_updater
assert_rc 1
assert_out "Update verification failed"
assert_pkg_kept "$GOOD"
assert_unchanged
finish

# ---------------------------------------------------------------------------
# N4..N11: preflight del paquete (todo se rechaza antes de tocar /)
# ---------------------------------------------------------------------------
preflight_case() { # preflight_case <caso> <descripcion> <esperado en stderr> <mutate>
    begin "$1" "$2"
    reset_target "$WORK/image-new.tar"
    mount_mnt 1024
    pkg=x6200_r1cbu_$1.tgz
    mkfixture "$WORK/pkg/$pkg" --mutate "$4"
    put_pkg "$WORK/pkg/$pkg"
    run_updater
    assert_rc 1
    assert_out "Update verification failed"
    assert_err_has "$3"
    assert_pkg_kept "$pkg"
    assert_unchanged
    assert_no_backup
    finish
}

# El tar del target recorta un "/" inicial o un "../" adelante AL LISTAR (y lo
# avisa por stderr): el paquete tiene que listar limpio, asi que esos dos casos
# se rechazan por ese aviso.
preflight_case absolute-entry "entrada con nombre absoluto: rechazada" "does not list cleanly" absolute
preflight_case dotdot-entry "entrada con componente ..: rechazada" "does not list cleanly" dotdot
preflight_case dot-entry "entrada con ./: rechazada" ". component" dot
preflight_case duplicate-entry "entrada duplicada: rechazada" "duplicated entry" duplicate
preflight_case forbidden-mnt "entrada que reemplazaria el punto de montaje /mnt: rechazada" "would replace the mount point /mnt" bare-mnt
preflight_case forbidden-dev "entrada que reemplazaria el punto de montaje /dev: rechazada" "would replace the mount point /dev" bare-dev
preflight_case forbidden-tmp "entrada que reemplazaria el punto de montaje /tmp: rechazada" "would replace the mount point /tmp" bare-tmp
preflight_case fifo-entry "fifo en el paquete: rechazada" "fifo entry" fifo
preflight_case device-entry "nodo de dispositivo en el paquete: rechazado" "device entry" chardev
preflight_case unexpected-setuid "setuid de mas en el paquete: rechazado" "setuid inventory" setuid-extra
preflight_case symlink-dotdot "symlink con target ..: rechazado" ".. target" symlink-dotdot

# El updater tiene que viajar root:root 0755: el bug del empaquetador viejo
# (uid 1000 del checkout) se rechaza antes de tocar /.
begin updater-uid-1000 "S02update con uid 1000 en el paquete: rechazado antes de tocar /"
reset_target "$WORK/image-new.tar"
mount_mnt 1024
pkg=x6200_r1cbu_uid1000.tgz
mkfixture "$WORK/pkg/$pkg" --mutate updater-uid1000
put_pkg "$WORK/pkg/$pkg"
run_updater
assert_rc 1
assert_out "Update verification failed"
assert_err_has "travels as"
assert_pkg_kept "$pkg"
assert_unchanged
finish

# ---------------------------------------------------------------------------
# N16: re-entrada del pase de actualización (issue iariap/x6200_gui#231)
# ---------------------------------------------------------------------------
# Este archivo es el que la actualización reemplaza. El shell que lo está
# leyendo puede volver a parsear la cola del script recién instalado y re-correr
# el despacho: una re-entrada en el MISMO árbol de procesos (la misma marca de
# entorno, el mismo fork). Sin guardia esa re-entrada vuelve a correr la
# actualización entera -- y cada pasada vuelve a extraer el paquete y a leer el
# archivo, así que no tiene tope: es el fork bomb del issue. Con la marca, la
# re-entrada no toca nada y sale con el estado del pase que ya corrió (acá 0).
# El segundo tramo verifica lo otro: un arranque NUEVO (sin marca) sigue
# actualizando normal, o sea que la guardia no cambia la semántica del update.
begin updater-reentry "re-entrada del pase (misma marca de entorno): no aplica de nuevo y sale con el estado del pase"
reset_target "$WORK/image-new.tar"
mount_mnt 1024
put_pkg "$PKG/$GOOD"
reentry_updater
assert_rc 0
if [ -s "$CASE_LOG/stdout.txt" ]; then
    bad "la re-entrada no debe escribir ninguna línea de estado y dice '$(tr '\n' '|' < "$CASE_LOG/stdout.txt")'"
else
    ok "la re-entrada no escribe ninguna línea de estado"
fi
[ -s "$CASE_LOG/stderr.txt" ] && bad "la re-entrada no debe escribir nada en stderr" || ok "stderr vacío"
assert_pkg_kept "$GOOD"
assert_unchanged
assert_no_backup
# Sin marca (arranque nuevo): el pase corre normal y consume el paquete.
run_updater
assert_rc 0
assert_out "Update applied"
assert_pkg_gone "$GOOD"
finish

# ---------------------------------------------------------------------------
# R2: target absoluto de un symlink (la rootfs auditada los trae: se admiten)
# ---------------------------------------------------------------------------
begin symlink-absolute-admitted "symlink con target absoluto (como etc/alsa/conf.d/* de la rootfs): se admite y se aplica"
reset_target "$WORK/image-new.tar"
mount_mnt 1024
# Un paquete armado con el empaquetador real sobre la misma imagen + un symlink
# con target absoluto: tiene que aplicarse como cualquier otro.
cp -a "$WORK/raw" "$WORK/abs"
ln -sf /etc/h215-abs "$WORK/abs/opt/h215-link-abs"
tar -cf "$WORK/image-abs.tar" --numeric-owner -C "$WORK/abs" .
pkg=x6200_r1cbu_vabs.tgz
if bash "$REPO/tools/release/make_update_tgz.sh" "$WORK/image-abs.tar" "$WORK/pkg/$pkg" >>"$CASE_LOG/steps.txt" 2>&1; then
    put_pkg "$WORK/pkg/$pkg"
    run_updater
    grep -q "absolute target" "$CASE_LOG/stderr.txt" && \
        bad "rechazó un target absoluto, y la rootfs auditada los trae" ||
        ok "no rechaza targets absolutos"
    assert_rc 0
    assert_out "Update applied"
    [ -L "$TARGET/opt/h215-link-abs" ] && ok "el symlink quedó instalado" || bad "el symlink no quedó"
    [ "$(readlink "$TARGET/opt/h215-link-abs")" = "/etc/h215-abs" ] && ok "con su target absoluto" ||
        bad "target distinto"
else
    bad "el empaquetador no pudo armar el paquete del caso"
fi
finish

# ---------------------------------------------------------------------------
# N12: el tar de la extraccion falla despues del preflight y del backup
# ---------------------------------------------------------------------------
begin extract-fails "tar que falla al extraer: paquete y backup retenidos, mensaje de restauracion"
reset_target "$WORK/image-new.tar"
mount_mnt 1024
pkg=x6200_r1cbu_fails.tgz
# Un archivo cuyo directorio padre es un archivo en el target (etc/passwd): pasa
# el preflight, pasa el backup (la ruta no existe, no entra al backup) y el tar
# de la extraccion falla con "Not a directory".
python3 "$REPO/tests/release/mkpkg.py" from "$PKG/$GOOD" "$WORK/pkg/$pkg" --mutate nested-under-file --first >>"$CASE_LOG/steps.txt" 2>&1
put_pkg "$WORK/pkg/$pkg"
run_updater
assert_rc 1
assert_out "Update failed: package retained; restore backup manually"
assert_pkg_kept "$pkg"
# La entrada que falla va primero: BusyBox tar aborta ahi y no escribe nada.
assert_unchanged
if [ -f "$TARGET/mnt/backup_$pkg" ]; then
    ok "el backup quedó"
    BK_HASH=$(sha256sum "$TARGET/mnt/backup_$pkg" | awk '{ print $1 }')
else
    bad "no quedó el backup (sin backup no tiene sentido probar el reuso)"
    BK_HASH=""
fi
# Segunda corrida con el mismo paquete: el backup identico se reusa, no se
# sobrescribe ni se duplica.
run_updater
assert_rc 1
assert_out "Update failed: package retained; restore backup manually"
assert_pkg_kept "$pkg"
if [ -n "$BK_HASH" ]; then
    [ "$(sha256sum "$TARGET/mnt/backup_$pkg" | awk '{ print $1 }')" = "$BK_HASH" ] &&
        ok "el backup identico se reusó sin cambios" || bad "el backup cambió"
fi
npartial=$(find "$TARGET/mnt" -maxdepth 1 -name '*.partial' | wc -l)
[ "$npartial" = "0" ] && ok "no quedó .partial" || bad "quedaron $npartial .partial"
# Un backup preexistente con otro contenido: error cerrado, no se sobrescribe.
if [ -n "$BK_HASH" ]; then
    printf 'no soy un tar\n' > "$TARGET/mnt/backup_$pkg.otro"
    mv "$TARGET/mnt/backup_$pkg.otro" "$TARGET/mnt/backup_$pkg"
    run_updater
    assert_rc 1
    assert_out "Update backup failed"
    assert_err_has "different content"
    assert_pkg_kept "$pkg"
fi
finish

# ---------------------------------------------------------------------------
# R3: entrada que es un directorio en el sistema (hallazgo del banco)
# ---------------------------------------------------------------------------
# El contrato pide que el preflight rechace lo que no se puede aplicar. Un
# paquete que declara un ARCHIVO en una ruta que en el sistema es un DIRECTORIO
# (usr/sbin) hoy no lo rechaza el preflight: la ruta existe, entra al backup, y
# el tar del target lista el directorio con barra ("usr/sbin/"), asi que la
# verificacion del backup no coincide y el updater corta con "Update backup
# failed". Es fail-closed (no se toca /, el paquete y el sidecar quedan), pero la
# diagnosis apunta al backup y no al paquete. Este caso fija ese comportamiento
# observado (y queda como hallazgo en la evidencia).
begin entry-over-directory "entrada que es un directorio en el sistema: la corta el backup (fail-closed, diagnosis mejorable)"
reset_target "$WORK/image-new.tar"
mount_mnt 1024
pkg=x6200_r1cbu_overdir.tgz
python3 "$REPO/tests/release/mkpkg.py" from "$PKG/$GOOD" "$WORK/pkg/$pkg" --mutate usr-sbin-file --first >>"$CASE_LOG/steps.txt" 2>&1
put_pkg "$WORK/pkg/$pkg"
[ -d "$TARGET/usr/sbin" ] && ok "en el sistema usr/sbin es un directorio" || bad "el fixture no tiene usr/sbin como directorio"
run_updater
assert_rc 1
assert_out "Update backup failed"
assert_err_has "does not list the same paths"
assert_pkg_kept "$pkg"
assert_unchanged
assert_no_backup
finish

# ---------------------------------------------------------------------------
# N13..N15: /mnt
# ---------------------------------------------------------------------------
begin no-space "backup sin espacio: no se extrae y el paquete queda"
reset_target "$WORK/image-new.tar"
# 150 MiB: el paquete (100 MiB) entra, el backup (250 MiB) no.
mount_mnt 150
put_pkg "$PKG/$GOOD"
run_updater
assert_rc 1
assert_out "Update backup failed"
assert_pkg_kept "$GOOD"
assert_unchanged
assert_no_backup
finish

begin readonly-mnt "/mnt read-only: no se toca nada"
reset_target "$WORK/image-new.tar"
mount_mnt 1024
put_pkg "$PKG/$GOOD"
mount -o remount,ro "$TARGET/mnt"
run_updater
assert_rc 1
assert_out "Update backup failed"
assert_err_has "read-only"
assert_pkg_kept "$GOOD"
assert_unchanged
finish

begin mnt-not-mounted "/mnt sin montar: se rechaza (el backup caeria en el rootfs)"
reset_target "$WORK/image-new.tar"
mount_mnt none
put_pkg "$PKG/$GOOD"
run_updater
assert_rc 1
assert_out "Update backup failed"
assert_err_has "not a separate mount point"
assert_pkg_kept "$GOOD"
assert_no_backup
assert_unchanged
finish

# ---------------------------------------------------------------------------
# L1: primer update de una imagen historica (NO es un pase de seguridad)
# ---------------------------------------------------------------------------
begin legacy-first-update "imagen historica: la corre SU script stock, el sidecar no protege nada"
reset_target "$WORK/image-stock.tar"
mount_mnt 1024
put_pkg "$PKG/$GOOD"
say "    (updater de la imagen, antes de correr: $(sha256sum "$TARGET/etc/init.d/S02update" | awk '{ print substr($1,1,12) }'))"
run_updater
legacy_ok=1
case "$RC" in
    0) ok "el script stock salió 0 (no valida nada)" ;;
    *) bad "el script stock salió $RC y no debía verificar nada"; legacy_ok=0 ;;
esac
assert_out_has "Creating backup from"
[ -f "$TARGET/mnt/$GOOD.sha256" ] && ok "el script stock dejó el sidecar en /mnt (no lo usa)" ||
    { bad "el sidecar desapareció: el script stock no debía tocarlo"; legacy_ok=0; }
[ -f "$TARGET/mnt/$GOOD" ] && bad "el script stock no borró el paquete" || ok "el script stock borró el paquete sin verificar"
# El backup del script stock es el que la epica marca como truncado: varios tar
# concatenados de los que solo se lee el primero.
[ -f "$TARGET/mnt/backup_$GOOD" ] && ok "dejó un backup (el de xargs)" || bad "no dejó backup"
if [ -f "$TARGET/mnt/backup_$GOOD" ]; then
    bk=$(tar -tzf "$TARGET/mnt/backup_$GOOD" | wc -l)
    pk=$(tar -tzf "$PKG/$GOOD" | wc -l)
    [ "$bk" -lt "$pk" ] && ok "el backup stock quedó truncado ($bk de $pk entradas)" ||
        bad "el backup stock no quedó truncado ($bk de $pk)"
    if tar -tzf "$TARGET/mnt/backup_$GOOD" >/dev/null 2>&1 && [ "$bk" -ge "$pk" ]; then
        bad "el backup stock es completo: revisar la epica"
    fi
fi
printf 'caso\t%s\tdesc\t%s\tveredicto\tUNSUPPORTED/UNSAFE_LEGACY\n' "$CASE" "$CASE_DESC" >> "$CASE_LOG/steps.txt"
[ "$legacy_ok" = "1" ] && finish_legacy || { CASE_FAIL=1; finish; }
say "    (UNSUPPORTED/UNSAFE_LEGACY: el primer update de una imagen historica NO queda protegido)"

# ---------------------------------------------------------------------------
say ""
say "================ resumen ================"
printf '  %-24s %s\n' CASO VEREDICTO
while IFS=$'\t' read -r c d v; do
    printf '  %-24s %s\n' "$c" "$v"
done < "$RESULTS"
say "  PASS: $PASS   FAIL: $FAIL   UNSUPPORTED/UNSAFE_LEGACY: $LEGACY"
say "  evidencia cruda: $LOGS/<caso>/{stdout.txt,stderr.txt,steps.txt} y $RESULTS"
[ "$FAIL" = "0" ] || exit 1
exit 0
