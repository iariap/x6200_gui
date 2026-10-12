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
#   SHA256SUMS                    both of the above
#
# and verifies: the image's root filesystem is clean (e2fsck), the update tarball
# carries the same GUI binary as the image, and the boot scripts are inside.
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

cp -n "$IMG" "$OUT/$(basename "$IMG")" 2>/dev/null || true
( cd "$OUT" && sha256sum "$(basename "$IMG")" "x6200_r1cbu_$VER.tgz" > SHA256SUMS )

echo "==> listo"
ls -lh "$OUT"/*.img "$OUT"/x6200_r1cbu_*.tgz "$OUT"/SHA256SUMS | awk '{print "    " $5 "\t" $9}'
cat "$OUT/SHA256SUMS" | sed 's/^/    /'
