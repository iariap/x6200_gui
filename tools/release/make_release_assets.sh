#!/bin/bash
#
# Build the release assets for a built image and check them:
#
#   tools/release/make_release_assets.sh <image.img> <version> [out_dir]
#
# Produces, in <out_dir> (default: the image's directory):
#
#   x6200_<date>.img              the SD image, as built
#   x6200_r1cbu_<version>.tgz     the in-place update: what S02update() looks for
#                                 on the DATA partition (/mnt) and unpacks over /
#                                 (built by make_update_tgz.sh, which needs root)
#   x6200_r1cbu_<version>.tgz.sha256  the sidecar the updater demands next to the
#                                 package before touching anything (issue #215)
#   SHA256SUMS                    both of the above
#
# and verifies: the image's root filesystem is clean (e2fsck), the update tarball
# carries the same GUI binary as the image, the boot scripts are inside, the
# sidecar matches the package, and (with BUILDROOT_S02UPDATE=<checkout>) that the
# S02update is the same file in this repo and in AetherX6200Buildroot.
#
# SPDX-License-Identifier: LGPL-2.1-or-later
#
set -e

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

IMG=${1:?usage: make_release_assets.sh <image.img> <version> [out_dir]}
VER=${2:?usage: make_release_assets.sh <image.img> <version> [out_dir]}
OUT=${3:-$(dirname "$IMG")}

IMG=$(readlink -f "$IMG")
OUT=$(readlink -f "$OUT")
mkdir -p "$OUT"

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

echo "==> imagen: $IMG"
echo "==> versión: $VER"
echo "==> salida:  $OUT"

# The firmware boots from the SD card: the rootfs partition starts at 33 MiB and
# is 512 MiB (see the genimage configuration).
echo "==> extrayendo la partición raíz para verificarla"
dd if="$IMG" of="$TMP/rootfs.ext4" bs=1M skip=33 count=512 status=none
e2fsck -fn "$TMP/rootfs.ext4" | tail -1

echo "==> GUI en la imagen"
7z e -y -o"$TMP/img" "$TMP/rootfs.ext4" usr/sbin/x6200_gui >/dev/null
ls -l "$TMP/img/x6200_gui" | awk '{print "    " $5 " bytes"}'

echo "==> armando la actualización en el lugar (x6200_r1cbu_$VER.tgz)"
# The image's own rootfs archive: same content, paths starting with ./
ROOTFS=$(ls -t "$(dirname "$IMG")"/rootfs_*.tar 2>/dev/null | head -1)
if [ -z "$ROOTFS" ]; then
    echo "no rootfs tar next to the image" >&2
    exit 1
fi
# make_update_tgz.sh arma el tgz en la forma que el S02update del firmware necesita
# (solo archivos: ver el comentario de ese script) y hay que correrlo como root para
# preservar los bits setuid del rootfs.
"$HERE/make_update_tgz.sh" "$ROOTFS" "$OUT/x6200_r1cbu_$VER.tgz"

echo "==> verificando la actualización"
# El tgz lleva solo archivos (ver make_update_tgz.sh), asi que se extraen por nombre
tar -tzf "$OUT/x6200_r1cbu_$VER.tgz" | grep -qx "usr/sbin/x6200_gui"
tar -tzf "$OUT/x6200_r1cbu_$VER.tgz" | grep -q "^etc/init.d/S02update$"
tar -xzf "$OUT/x6200_r1cbu_$VER.tgz" -C "$TMP" usr/sbin/x6200_gui etc/init.d/S02update
if ! cmp -s "$TMP/img/x6200_gui" "$TMP/usr/sbin/x6200_gui"; then
    echo "the update tarball does not carry the same GUI binary as the image" >&2
    exit 1
fi
echo "    mismo binario GUI que la imagen"
echo "    S02update (el que aplica la actualización) está dentro"

echo "==> verificando el sidecar del paquete"
[ -f "$OUT/x6200_r1cbu_$VER.tgz.sha256" ] || {
    echo "falta $OUT/x6200_r1cbu_$VER.tgz.sha256 (lo arma make_update_tgz.sh)" >&2
    exit 1
}
( cd "$OUT" && sha256sum -c "x6200_r1cbu_$VER.tgz.sha256" ) >/dev/null || {
    echo "el sidecar no coincide con el tgz" >&2
    exit 1
}
echo "    $(cut -d' ' -f1 < "$OUT/x6200_r1cbu_$VER.tgz.sha256")  el S02update exige este sidecar junto al tgz"

cp -n "$IMG" "$OUT/$(basename "$IMG")" 2>/dev/null || true
( cd "$OUT" && sha256sum "$(basename "$IMG")" "x6200_r1cbu_$VER.tgz" > SHA256SUMS )

echo "==> paridad del updater (mismo archivo en GUI y Buildroot)"
# El gate de release: el S02update del overlay y el de la imagen de Buildroot
# tienen que ser el mismo archivo. Sin el checkout de Buildroot a mano se saltea
# acá (lo corre `check_updater_parity.sh` en la integración), pero el release no
# se publica sin la paridad: ver docs/TESTING.md.
if [ -n "${BUILDROOT_S02UPDATE:-}" ]; then
    "$HERE/check_updater_parity.sh" "$BUILDROOT_S02UPDATE"
else
    echo "    SKIP: falta BUILDROOT_S02UPDATE con el checkout (o la ruta) del S02update de Buildroot." >&2
    echo "    Un SKIP visible no es la paridad: la integración corre el gate con la ruta explícita." >&2
fi

echo "==> listo"
ls -lh "$OUT"/*.img "$OUT"/x6200_r1cbu_*.tgz "$OUT"/x6200_r1cbu_*.tgz.sha256 "$OUT"/SHA256SUMS |
    awk '{print "    " $5 "\t" $9}'
cat "$OUT/SHA256SUMS" | sed 's/^/    /'
