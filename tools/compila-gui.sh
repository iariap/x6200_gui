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
# Por que ADEMAS fuerza la copia de los fuentes (issue #165): Buildroot copia el arbol a su copia
# propia con `rsync -au` (pkg-generic.mk), que SALTEA un archivo modificado si en la copia tiene
# un timestamp igual o mas nuevo. Medido: un commit nuevo compilaba 20 s y daba el MISMO md5 que el
# anterior porque `src/info.c` (y los otros 6 archivos del commit) no se habian copiado, mientras
# que `src/ui_set.h` (archivo NUEVO, que rsync si copia) si estaba: el QA miraba justo el nuevo y
# no veia que los modificados eran viejos. Aca se hace un `rsync -a --checksum` (sin `-u`, compara
# contenido) antes del build, para que la copia sea siempre la del repo.
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
DEST=build/x6200-gui-custom                 # $(@D) del paquete, relativo a /workspace/build

# El hash de identidad de las fuentes lo calcula tools/fuente-md5.sh (mismo algoritmo que CMake y
# que tools/captura-banco.sh): rutas relativas, para que el repo y la copia del buildroot coincidan.
HASH=$(sh "$AQUI/fuente-md5.sh" "$REPO")
ANTERIOR=$(docker run --rm --entrypoint "" -v "$VOLUMEN":/b "$IMAGEN" sh -c "cat /b/.fuente.md5 2>/dev/null || true" 2>/dev/null | tr -d '\r')
SUCIO=$(git -C "$REPO" status --porcelain -- src lvgl/src 2>/dev/null | head -1)

if [ "${RECONSTRUIR:-si}" = "auto" ] && [ "$HASH" = "$ANTERIOR" ] && [ -z "$SUCIO" ]; then
    echo "  sin cambios en los fuentes: se deja el binario que ya esta"
else
    echo "  compilando limpio (objetos de la GUI borrados)..."
    # `-e X6200_FUENTE_MD5`: el binario lo embebe como sello (ver CMakeLists.txt y src/main.c).
    docker run --rm --entrypoint "" \
        -v "$BUILDROOT":/workspace -v "$VOLUMEN":/workspace/build -v "$REPO":/workspace_gui:ro \
        -e FORCE_UNSAFE_CONFIGURE=1 -e X6200_FUENTE_MD5="$HASH" -w /workspace/build "$IMAGEN" sh -c "
            DEST=/workspace/build/$DEST
            rm -f /workspace/build/target/usr/sbin/x6200_gui
            find /workspace/build -path '*x6200_gui*' -name '*.o' -delete 2>/dev/null || true
            mkdir -p \"\$DEST\"
            # Copia AUTORITATIVA (por contenido, no por timestamp): repone lo que el
            # rsync -au de Buildroot saltearia. Sin --delete, para no borrar los
            # artefactos de CMake de la copia.
            rsync -a --checksum --chmod=u=rwX,go=rX \
                --exclude .svn --exclude .git --exclude .hg --exclude .bzr --exclude CVS \
                /workspace_gui/ \"\$DEST/\"
            printf '%s\n' '$HASH' > \"\$DEST/.fuente.md5\"
            make x6200-gui-reconfigure -j$JOBS" >/dev/null 2>&1 || { echo "FALLO la compilacion" >&2; exit 1; }
    echo "$HASH" | docker run --rm -i --entrypoint "" -v "$VOLUMEN":/b "$IMAGEN" sh -c 'cat > /b/.fuente.md5'
fi

MD5=$(docker run --rm --entrypoint "" -v "$VOLUMEN":/b "$IMAGEN" sh -c 'md5sum /b/target/usr/sbin/x6200_gui | cut -c1-12') || true
echo "  binario: md5 $MD5   (commit $(git -C "$REPO" log -1 --format=%h) - $(git -C "$REPO" log -1 --format=%s | cut -c1-50))"
echo "  fuente:  md5 $(printf '%s' "$HASH" | cut -c1-12)   (sello .fuente.md5 del volumen)"
