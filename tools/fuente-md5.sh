#!/bin/sh
#
# Hash de identidad de las FUENTES de la GUI (issue #165). Es el mismo numero que
# tools/compila-gui.sh deja en el sello .fuente.md5 y que el binario embebe como
# X6200_FUENTE_MD5, para poder comparar el sello del build con el de la radio.
#
#   tools/fuente-md5.sh [dir-del-repo]     # default: el repo donde vive este script
#
# Por que rutas RELATIVAS: el hash se calcula sobre la lista `md5sum` de cada
# fuente, y esa salida incluye la ruta. Si se pasaran rutas absolutas, el arbol del
# repo (/.../x6200_gui/src/foo.c) y la copia que arma Buildroot
# (/workspace/build/build/x6200-gui-custom/src/foo.c) darian hashes distintos con
# el mismo codigo. Con `cd` al repo y rutas relativas (`src/foo.c`) los dos
# coinciden.
#
# `LC_ALL=C`: el orden de `sort` no tiene que depender del locale del host (el del
# contenedor de Buildroot es en_US.utf8 y el del host puede ser otro).
#
# SPDX-License-Identifier: GPL-2.0-or-later
set -e

AQUI=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO=${1:-$(CDPATH= cd -- "$AQUI/.." && pwd)}

[ -d "$REPO/src" ] || { echo "fuente-md5: no encuentro $REPO/src" >&2; exit 2; }

cd "$REPO"
export LC_ALL=C

{ find src lvgl/src -type f \( -name '*.c' -o -name '*.cpp' -o -name '*.h' \) -print0 2>/dev/null \
    | sort -z | xargs -0 md5sum 2>/dev/null; } | md5sum | cut -d' ' -f1
