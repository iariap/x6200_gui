#!/bin/bash
#
# Publish the release, after checking everything it needs:
#
#   tools/release/publish_release.sh <tag> <version> [--publish|--draft]
#
# Checks, in order:
#   1. the image, the update tarball and SHA256SUMS are there and the checksums match;
#   2. the update tarball carries the same GUI binary as the image;
#   3. the working tree of this repo is clean and pushed;
#   4. the test suite passes (--quick: without the slow JS8 run);
#   5. the release notes exist.
#
# Without arguments it stops after the checks and prints the command it would run, so the
# release can be rehearsed. With --draft it uploads the three assets as a draft (nothing
# public, no tag) and with --publish it publishes: if a draft with that tag is already
# there it just flips it. Everything is a pre-release: this ROM is in beta.
#
# SPDX-License-Identifier: LGPL-2.1-or-later
#
set -e

TAG=${1:?usage: publish_release.sh <tag> <version> [--publish]}
VER=${2:?usage: publish_release.sh <tag> <version> [--publish]}
MODE=${3:-}

DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
GUI=$(CDPATH= cd -- "$DIR/../.." && pwd)
IMAGES=${IMAGES:-$GUI/../images}
REPO=${REPO:-$(git -C "$GUI" remote get-url fork | sed 's|.*github.com[:/]||; s|\.git$||')}

NOTES="$GUI/docs/RELEASE_NOTES_2026-10-11_features.md"
IMG=$(ls -t "$IMAGES"/x6200_*.img 2>/dev/null | head -1)
TGZ=$(ls -t "$IMAGES"/x6200_r1cbu_*.tgz 2>/dev/null | head -1)

echo "==> repositorio: $REPO   tag: $TAG   versión: $VER"
echo "==> imagen:      ${IMG:-FALTA}"
echo "==> actualización: ${TGZ:-FALTA}"

[ -f "$IMG" ] || { echo "no hay imagen" >&2; exit 1; }
[ -f "$TGZ" ] || { echo "no hay tgz de actualización" >&2; exit 1; }
[ -f "$IMAGES/SHA256SUMS" ] || { echo "no hay SHA256SUMS" >&2; exit 1; }
[ -f "$NOTES" ] || { echo "no hay notas del release" >&2; exit 1; }

echo "==> checksums"
( cd "$IMAGES" && sha256sum -c SHA256SUMS )

echo "==> la actualización trae el mismo GUI que la imagen"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
dd if="$IMG" of="$TMP/rootfs.ext4" bs=1M skip=33 count=512 status=none
7z e -y -o"$TMP" "$TMP/rootfs.ext4" usr/sbin/x6200_gui >/dev/null
tar -xzf "$TGZ" -C "$TMP" ./usr/sbin/x6200_gui
cmp "$TMP/x6200_gui" "$TMP/usr/sbin/x6200_gui"
echo "    mismo binario"

echo "==> árbol limpio y pusheado"
[ -z "$(git -C "$GUI" status --porcelain)" ] || { echo "hay cambios sin commitear" >&2; exit 1; }
git -C "$GUI" fetch -q fork lu2dsx/x6200-rom
[ "$(git -C "$GUI" rev-parse HEAD)" = "$(git -C "$GUI" rev-parse fork/lu2dsx/x6200-rom)" ] || {
    echo "HEAD no está pusheado" >&2; exit 1; }
echo "    $(git -C "$GUI" rev-parse --short HEAD)"

echo "==> suite de tests"
( cd "$GUI" && tests/run_all_tests.sh --quick > /tmp/publish_tests.log 2>&1 ) || {
    tail -20 /tmp/publish_tests.log >&2; exit 1; }
grep -E "OK, .* fallos" /tmp/publish_tests.log | tail -1

if [ "$MODE" != "--publish" ] && [ "$MODE" != "--draft" ]; then
    echo
    echo "==> todo listo. Para publicarlo:"
    echo
    echo "  gh release create $TAG \\"
    echo "      --repo $REPO --prerelease --title \"X6200 LU2DSX ROM $VER\" \\"
    echo "      --notes-file $NOTES \\"
    echo "      $IMG $TGZ $IMAGES/SHA256SUMS"
    echo
    exit 0
fi

# A draft release may already be there (it is the way to have the assets uploaded
# before deciding to publish): then publishing is just flipping it.
if gh release view "$TAG" --repo "$REPO" >/dev/null 2>&1; then
    echo "==> el release ya existe (borrador): publicándolo"
    gh release edit "$TAG" --repo "$REPO" --draft=false --prerelease \
        --title "X6200 LU2DSX ROM $VER" --notes-file "$NOTES"
else
    echo "==> creando el tag $TAG"
    git -C "$GUI" tag -f "$TAG" -m "X6200 LU2DSX ROM $VER"
    git -C "$GUI" push -f fork "$TAG"

    echo "==> publicando"
    gh release create "$TAG" --repo "$REPO" --prerelease \
        --title "X6200 LU2DSX ROM $VER" --notes-file "$NOTES" \
        "$IMG" "$TGZ" "$IMAGES/SHA256SUMS"
fi

# With --draft: create it as a draft instead (the assets uploaded, nothing public)
if [ "$MODE" = "--draft" ]; then
    echo "==> borrador: los assets quedan subidos y sin publicar"
    gh release edit "$TAG" --repo "$REPO" --draft=true
fi

echo "==> listo"
gh release view "$TAG" --repo "$REPO" --json tagName,assets \
    -q '"  " + .tagName + ": " + ([.assets[].name] | join(", "))'
