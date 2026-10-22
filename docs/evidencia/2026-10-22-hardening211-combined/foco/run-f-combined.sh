#!/bin/sh
# run-f-combined.sh — pasada COMBINADA F-01..F-05 sobre el par integrado
# (GUI b798f13b + Buildroot e5a6393), lado del host.
#
#   WT=<worktree GUI> OUT=<salida Buildroot privada (target/ + host/)> \
#   foco/run-f-combined.sh [caso...]
#
# Arma un chroot privado con el rootfs del target (binario ARM real, scripts de
# init instalados) y corre cada caso en un contenedor privado y desechable. No
# usa la radio, ni la SD del owner, ni volumenes/rootfs/contenedores de otras
# corridas; el /mnt del host no se toca.
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -u

DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
WT=${WT:-/home/pablo/desa/asf/state/worktrees/x6200/next-211-combined}
OUT=${OUT:-/home/pablo/desa/asf/state/worktrees/x6200/next-211-combined-lab/out}
IMG=${IMG:-x6200-buildroot}
SB=${SB:-/home/pablo/desa/asf/state/worktrees/x6200/next-211-combined-lab/f-combined}
E="$DIR"
CHROOT="$SB/chroot"
FIX="$SB/fixtures"

CASES=${*:-"f02-corrupt f02-nomount f03-absent f03-writefail f04-daemon-exit78 f04-daemon-nomnt f01f03-chain"}

fail() { echo "FALLO: $*" >&2; exit 1; }

[ -x "$OUT/target/usr/sbin/x6200_gui" ] || fail "no hay $OUT/target/usr/sbin/x6200_gui (build privado incompleto)"
[ -x "$OUT/host/bin/arm-buildroot-linux-gnueabihf-gcc" ] || fail "no está el toolchain del target en $OUT/host/bin"
docker image inspect "$IMG" >/dev/null 2>&1 || fail "no existe la imagen $IMG"

mkdir -p "$SB" "$E/logs" "$FIX"
EVID="$E/logs"

echo "== armando el chroot privado del target (rootfs del par integrado) =="
docker run --rm --entrypoint "" -v "$SB":/s "$IMG" sh -c 'rm -rf /s/chroot; mkdir -p /s/chroot'
docker run --rm --entrypoint "" -v "$OUT/target":/src:ro -v "$CHROOT":/t "$IMG" \
    sh -c 'cp -a /src/. /t/ && chmod 755 /t/usr/sbin/x6200_gui && mkdir -p /t/dev/shm /t/tmp /t/mnt /t/proc'
docker run --rm --entrypoint "" -v "$CHROOT":/t -v "$WT/tools/bench":/b:ro "$IMG" \
    sh -c 'cp /b/libfakefb.so /b/libcrash.so /b/libx6200trace.so /t/lib/' \
    || fail "no se pudieron copiar las libs del banco al chroot"

echo "== compilando el cliente de control remoto para ARM (dentro del chroot) =="
docker run --rm --entrypoint "" -v "$CHROOT":/t -v "$WT/tools/remote":/w:ro -v "$OUT/host/bin":/hb:ro "$IMG" \
    sh -c '/hb/arm-buildroot-linux-gnueabihf-gcc -O2 -o /t/tmp/x6200-remote /w/x6200-remote.c' \
    || fail "no se pudo compilar x6200-remote para ARM"

echo "== fixtures (a partir de la plantilla del propio rootfs) =="
cp "$CHROOT/usr/share/x6200/params.default.db" "$FIX/template.db" || fail "no hay plantilla en el rootfs"
sqlite3 "$FIX/template.db" "SELECT count(*) FROM sqlite_master" >/dev/null || fail "la plantilla no es sqlite"
cp "$FIX/template.db" "$FIX/params-threshold-invalid.db"
# El umbral fuera de rango se sanea al arrancar y queda dirty -> fuerza un
# guardado que en el caso `full` (tmpfs lleno) tiene que fallar y reportarse.
sqlite3 "$FIX/params-threshold-invalid.db" "INSERT OR REPLACE INTO params(name,val) VALUES('freq_accel_threshold',0);" >/dev/null

BINSHA=$(sha256sum "$OUT/target/usr/sbin/x6200_gui" | cut -d' ' -f1)
SELLO=$(sh "$WT/tools/fuente-md5.sh" "$WT")
{
    echo "== pasada combinada F-01..F-05 (capa QEMU_USER) =="
    echo "GUI HEAD:     $(git -C "$WT" rev-parse HEAD)"
    echo "sello fuente: $SELLO"
    echo "binario:      $BINSHA ($(stat -c%s "$OUT/target/usr/sbin/x6200_gui") B)"
    echo "init.d instalado: $(ls "$CHROOT/etc/init.d" | tr '\n' ' ')"
    echo "s01create_data instalado: $(sha256sum "$CHROOT/etc/init.d/S01create_data" 2>/dev/null | cut -d' ' -f1)"
    echo "s02update instalado:      $(sha256sum "$CHROOT/etc/init.d/S02update" 2>/dev/null | cut -d' ' -f1)"
    echo "fstab instalado:          $(sha256sum "$CHROOT/etc/fstab" 2>/dev/null | cut -d' ' -f1)"
} | tee "$E/README-f-combined.txt"

FAILED=0
: > "$E/matrix.txt"
for c in $CASES; do
    docker run --rm --cap-add SYS_ADMIN --device /dev/loop-control --entrypoint "" \
        -v "$CHROOT":/t -v "$FIX":/f -v "$EVID/..":/e \
        "$IMG" sh /e/foco/f-inner.sh "$c" > "$EVID/$c.stdout" 2>&1
    rc=$?
    cat "$EVID/$c.stdout"
    if [ "$rc" -eq 0 ]; then
        printf '%-18s ok\n' "$c" >> "$E/matrix.txt"
    else
        printf '%-18s FALLO (exit %s)\n' "$c" "$rc" >> "$E/matrix.txt"
        FAILED=$((FAILED + 1))
    fi
done

echo
echo "== matriz =="
cat "$E/matrix.txt"
echo "== casos con FALLO: $FAILED =="
[ "$FAILED" = 0 ]
