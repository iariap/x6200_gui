#!/bin/sh
#
# Compila la GUI de forma LIMPIA y deja el binario en el volumen del build, listo para desplegar.
#
#   tools/compila-gui.sh [nproc]
#
# Por que limpia: el build incremental se quedo con un binario viejo despues de cambios en headers
# (medido: dos md5 distintos, `30bef0dbec2a` incremental contra `5a6e561e502f` limpio, y la radio
# mostraba el color viejo). Borrar los objetos de la GUI antes de compilar cuesta unos minutos y
# evita desplegar algo que no es la ultima version. Para no hacerlo siempre, usar RECONSTRUIR=auto
# (compara el hash de los fuentes con el sello del ultimo build).
#
# SPDX-License-Identifier: GPL-2.0-or-later
set -e
AQUI=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO=$(CDPATH= cd -- "$AQUI/.." && pwd)
BUILDROOT=${BUILDROOT:-$REPO/../buildroot_new/AetherX6200Buildroot}
VOLUMEN=${VOLUMEN:-x6200-build-output}
IMAGEN=${IMAGEN:-x6200-buildroot}
JOBS=${1:-$(nproc)}
SELLO="$VOLUMEN:.fuente.md5"

hash_fuentes() {
    { find "$REPO/src" "$REPO/lvgl/src" -type f \( -name '*.c' -o -name '*.cpp' -o -name '*.h' \) -print0 2>/dev/null \
        | sort -z | xargs -0 md5sum 2>/dev/null; } | md5sum | cut -d' ' -f1
}
HASH=$(hash_fuentes)
ANTERIOR=$(docker run --rm --entrypoint "" -v "$VOLUMEN":/b "$IMAGEN" sh -c "cat /b/.fuente.md5 2>/dev/null || true" 2>/dev/null | tr -d '\r')
SUCIO=$(git -C "$REPO" status --porcelain -- src lvgl/src 2>/dev/null | head -1)

if [ "${RECONSTRUIR:-si}" = "auto" ] && [ "$HASH" = "$ANTERIOR" ] && [ -z "$SUCIO" ]; then
    echo "  sin cambios en los fuentes: se deja el binario que ya esta"
else
    echo "  compilando limpio (objetos de la GUI borrados)..."
    docker run --rm --entrypoint "" \
        -v "$BUILDROOT":/workspace -v "$VOLUMEN":/workspace/build -v "$REPO":/workspace_gui:ro \
        -e FORCE_UNSAFE_CONFIGURE=1 -w /workspace/build "$IMAGEN" sh -c "
            rm -f /workspace/build/target/usr/sbin/x6200_gui
            find /workspace/build -path '*x6200_gui*' -name '*.o' -delete 2>/dev/null || true
            make x6200-gui-rebuild -j$JOBS" >/dev/null 2>&1 || { echo "FALLO la compilacion" >&2; exit 1; }
    echo "$HASH" | docker run --rm -i --entrypoint "" -v "$VOLUMEN":/b "$IMAGEN" sh -c 'cat > /b/.fuente.md5'
fi

MD5=$(docker run --rm --entrypoint "" -v "$VOLUMEN":/b "$IMAGEN" sh -c 'md5sum /b/target/usr/sbin/x6200_gui | cut -c1-12') || true
echo "  binario: md5 $MD5   (commit $(git -C "$REPO" log -1 --format=%h) - $(git -C "$REPO" log -1 --format=%s | cut -c1-50))"
