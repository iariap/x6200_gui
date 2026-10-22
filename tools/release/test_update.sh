#!/bin/bash
#
# Prueba el camino de actualización en el lugar DE VERDAD, sobre una copia del
# rootfs de la imagen, en un chroot con el bash y el BusyBox del target.
#
#   sudo tools/release/test_update.sh <rootfs.tar> <paquete.tgz> [directorio de trabajo]
#
# Escenarios (los dos en copias separadas, nada de la máquina se toca):
#
#   A) imagen ya compatible: el S02update de la imagen es el nuevo (el que dejó
#      una actualización anterior; acá se lo instala desde el propio paquete). Se
#      exige el contrato completo: sidecar verificado, backup completo y
#      verificado, extracción comprobada por hash, setuid intactos, y paquete y
#      sidecar consumidos sólo al final.
#
#   B) imagen histórica: el primer update lo corre el S02update VIEJO que ya tiene
#      la imagen (el stock de Buildroot, el que hace el backup con xargs). Ese
#      script no verifica sidecar ni hash, deja el backup truncado y borra el tgz
#      sin comprobarlo: se reporta UNSUPPORTED/UNSAFE_LEGACY, nunca como un pase
#      de seguridad. Se comprueba además que recién después de ese update la
#      imagen queda con el updater nuevo, o sea que la PRÓXIMA queda protegida.
#
# El paquete tiene que llamarse x6200_r1cbu_*.tgz (es el nombre que busca el
# updater en /mnt) y tiene que viajar con su <paquete>.sha256 al lado.
#
# No borra nada por su cuenta: el directorio de trabajo, si se pasa, tiene que
# estar vacío o no existir; si no se pasa, se crea con mktemp.
#
# SPDX-License-Identifier: LGPL-2.1-or-later
#
set -u

ROOTFS=${1:-}
TGZ=${2:-}
T=${3:-}

if [ -z "$ROOTFS" ] || [ -z "$TGZ" ]; then
    echo "uso: $0 <rootfs.tar> <paquete.tgz> [directorio de trabajo]" >&2
    exit 1
fi
[ -f "$ROOTFS" ] || { echo "no encuentro el rootfs $ROOTFS" >&2; exit 1; }
[ -f "$TGZ" ] || { echo "no encuentro el paquete $TGZ" >&2; exit 1; }

if [ "$(id -u)" != "0" ]; then
    echo "Hace falta root (chroot con los permisos reales del rootfs): sudo $0 $*" >&2
    exit 1
fi

BASE=$(basename "$TGZ")
case "$BASE" in
    x6200_r1cbu_*.tgz) ;;
    *) echo "el paquete tiene que llamarse x6200_r1cbu_*.tgz (el updater busca ese patrón en /mnt); este es '$BASE'" >&2; exit 1 ;;
esac
SIDE=$TGZ.sha256
[ -f "$SIDE" ] || {
    echo "no encuentro $SIDE: el paquete viaja con su sidecar (tools/release/make_update_tgz.sh lo arma)" >&2
    exit 1
}

QEMU=/usr/bin/qemu-arm-static
[ -x "$QEMU" ] || { echo "BLOCKED: no encuentro $QEMU (qemu-user-static); esta prueba corre el rootfs ARM real" >&2; exit 77; }

if [ -z "$T" ]; then
    T=$(mktemp -d /tmp/prueba-actualizacion.XXXXXX)
else
    if [ -e "$T" ]; then
        if [ -n "$(ls -A "$T" 2>/dev/null)" ]; then
            echo "el directorio de trabajo $T no está vacío: no lo borro (pasá uno vacío o ninguno)" >&2
            echo "  si es de una corrida vieja de esta prueba, borralo a mano" >&2
            exit 1
        fi
    else
        mkdir -p "$T"
    fi
fi
echo "==> directorio de trabajo: $T"

FAIL=0
pass() { printf '    ok    %s\n' "$*"; }
fail() { printf '    FALLA %s\n' "$*"; FAIL=1; }
# Una comprobación que no puede correr queda como SKIP visible, nunca como pase.
skip() { printf '    SKIP  %s\n' "$*"; }

umount_target() {
    local d
    for d in mnt proc dev; do
        if [ -d "$1/$d" ] && mountpoint -q "$1/$d"; then
            umount "$1/$d"
        fi
    done
    return 0
}

cleanup() {
    umount_target "$T/targetA"
    umount_target "$T/targetB"
    return 0
}
trap cleanup EXIT

# Arma un "sistema destino" con el rootfs de la imagen, y le pone /mnt: el
# updater nuevo exige que /mnt sea un montaje propio y escribible (si no, el
# backup caería en el rootfs).
prepare_target() {
    local t=$1
    rm -rf "$t"
    mkdir -p "$t"
    tar -xf "$ROOTFS" -C "$t"
    mkdir -p "$t/mnt" "$t/proc" "$t/sys" "$t/dev" "$t/tmp"
    install -m 0755 "$QEMU" "$t/usr/bin/qemu-arm-static"
    mount -t tmpfs -o size=420m,mode=0755 tmpfs "$t/mnt"
    mount -t proc proc "$t/proc"
    mount --bind /dev "$t/dev"
}

run_updater() { # run_updater <target> <logbase>
    local t=$1 log=$2
    PATH=/sbin:/usr/sbin:/bin:/usr/bin chroot "$t" /bin/sh /etc/init.d/S02update start \
        >"$log.out" 2>"$log.err"
    RC=$?
    echo "    rc=$RC"
    echo "    stdout: $(tr '\n' '|' < "$log.out")"
    echo "    stderr: $(tr '\n' '|' < "$log.err")"
}

gui_marker() { # deja una "versión vieja" reconocible en el binario de la GUI
    printf 'SOY LA VERSION VIEJA\n' > "$1/usr/sbin/x6200_gui"
    chmod 755 "$1/usr/sbin/x6200_gui"
    sha256sum "$1/usr/sbin/x6200_gui" | awk '{ print $1 }'
}

setuid_count() { # cuenta los setuid fuera de los montajes
    ( cd "$1" && find . \( -path ./proc -o -path ./sys -o -path ./dev -o -path ./mnt \) -prune \
        -o -perm -4000 -print ) | wc -l
}

# ---------------------------------------------------------------------------
echo
echo "==> A) imagen ya compatible (updater nuevo en la imagen)"
AT=$T/targetA
prepare_target "$AT"
# El updater de una imagen ya actualizada: el que trae el paquete. El paquete se
# instala a / y /mnt ya está montado, así que se extrae sólo ese archivo.
if tar -xzf "$TGZ" -C "$AT" etc/init.d/S02update; then
    chmod 755 "$AT/etc/init.d/S02update"
    pass "instalé el S02update del paquete como el de la imagen (actualización previa)"
else
    fail "el paquete no trae etc/init.d/S02update: armalo con make_update_tgz.sh de este árbol"
fi
MARCADO=$(gui_marker "$AT")
cp "$TGZ" "$SIDE" "$AT/mnt/"
setuid_before=$(setuid_count "$AT")
run_updater "$AT" "$T/A"
[ "$RC" = "0" ] && pass "el update salió 0" || fail "el update salió $RC"
grep -qxF 'Update applied' "$T/A.out" && pass "dice 'Update applied'" || fail "no dijo 'Update applied'"
grep -q 'Update verification failed' "$T/A.out" "$T/A.err" && fail "dijo 'Update verification failed'" || pass "sin 'Update verification failed'"
[ -f "$AT/mnt/$BASE" ] && fail "el paquete sigue en /mnt" || pass "el paquete se consumió"
[ -f "$AT/mnt/$BASE.sha256" ] && fail "el sidecar sigue en /mnt" || pass "el sidecar se consumió"
if [ -f "$AT/mnt/backup_$BASE" ]; then
    pass "backup_$BASE presente"
    bk=$(tar -tzf "$AT/mnt/backup_$BASE" 2>/dev/null | wc -l)
    pk=$(tar -tzf "$TGZ" 2>/dev/null | wc -l)
    echo "    entradas del backup: $bk   entradas del paquete: $pk"
    [ "$bk" = "$pk" ] && pass "el backup tiene todas las entradas que reemplaza" ||
        fail "el backup tiene $bk entradas y el paquete $pk (backup incompleto)"
    mkdir -p "$T/backupA"
    tar -xzf "$AT/mnt/backup_$BASE" -C "$T/backupA" usr/sbin/x6200_gui 2>/dev/null &&
        [ "$(sha256sum "$T/backupA/usr/sbin/x6200_gui" | awk '{ print $1 }')" = "$MARCADO" ] &&
        pass "el backup trae la GUI vieja marcada" || fail "el backup no trae la GUI vieja marcada"
else
    fail "no quedó backup_$BASE"
fi
pkg_gui=$(tar -xOzf "$TGZ" usr/sbin/x6200_gui | sha256sum | awk '{ print $1 }')
[ "$(sha256sum "$AT/usr/sbin/x6200_gui" | awk '{ print $1 }')" = "$pkg_gui" ] &&
    pass "la GUI quedó la del paquete (el tar se aplicó)" || fail "la GUI no quedó la del paquete"
[ "$(setuid_count "$AT")" = "$setuid_before" ] && pass "setuid intactos ($setuid_before)" ||
    fail "cambió la cantidad de setuid ($setuid_before -> $(setuid_count "$AT"))"
[ -f "$AT/mnt/new_$BASE.list" ] && pass "quedó la lista de rutas nuevas" || fail "no quedó new_$BASE.list"

# ---------------------------------------------------------------------------
echo
echo "==> B) imagen histórica (la corre el updater VIEJO de la imagen)"
BT=$T/targetB
prepare_target "$BT"
MARCADO_B=$(gui_marker "$BT")
cp "$TGZ" "$SIDE" "$BT/mnt/"
STOCK_SHA=$(sha256sum "$BT/etc/init.d/S02update" | awk '{ print substr($1, 1, 12) }')
echo "    updater de la imagen: $STOCK_SHA  ($(wc -l < "$BT/etc/init.d/S02update") líneas)"
run_updater "$BT" "$T/B"
[ "$RC" = "0" ] && pass "el updater viejo salió 0 (no verifica nada)" || fail "el updater viejo salió $RC"
[ -f "$BT/mnt/$BASE.sha256" ] && pass "el sidecar quedó en /mnt: el updater viejo no lo usa" ||
    fail "el sidecar desapareció: el updater viejo no debía tocarlo"
[ -f "$BT/mnt/$BASE" ] && fail "el updater viejo no borró el paquete" ||
    pass "el updater viejo borró el paquete sin verificar nada"
if [ -f "$BT/mnt/backup_$BASE" ]; then
    bk=$(tar -tzf "$BT/mnt/backup_$BASE" 2>/dev/null | wc -l)
    pk=$(tar -tzf "$TGZ" 2>/dev/null | wc -l)
    echo "    entradas del backup del stock: $bk   entradas del paquete: $pk"
    [ "$bk" -lt "$pk" ] && pass "el backup quedó truncado ($bk de $pk): no sirve para volver atrás" ||
        fail "el backup del stock no quedó truncado ($bk de $pk): revisar la imagen de partida"
else
    fail "el updater viejo no dejó backup"
fi
PKG_UPD=$(tar -xOzf "$TGZ" etc/init.d/S02update | sha256sum | awk '{ print $1 }')
BT_UPD=$(sha256sum "$BT/etc/init.d/S02update" | awk '{ print $1 }')
[ "$BT_UPD" = "$PKG_UPD" ] &&
    pass "la imagen quedó con el updater nuevo: la PRÓXIMA actualización sí queda protegida" ||
    fail "la imagen quedó con el updater $BT_UPD y el paquete trae $PKG_UPD"
echo "    UNSUPPORTED/UNSAFE_LEGACY: el primer update de una imagen histórica no queda cubierto por las verificaciones nuevas."

# ---------------------------------------------------------------------------
echo
if [ "$FAIL" = "0" ]; then
    echo "==> A PASS, B UNSUPPORTED/UNSAFE_LEGACY (reportado, no es un pase de seguridad)"
    exit 0
fi
echo "==> FALLÓ alguna comprobación (ver arriba)" >&2
exit 1
