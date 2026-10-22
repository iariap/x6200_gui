#!/bin/sh
#
# Compila x6200-remote (el cliente del socket de control de la GUI) para la radio, con el
# toolchain de buildroot. Queda estatico, asi que corre en la radio sin dependencias.
#
#   tools/remote/compila.sh
#   sshpass -p 123 scp <opciones> tools/remote/x6200-remote root@192.168.0.236:/tmp/
#   ssh root@... '/tmp/x6200-remote "{\"cmd\":\"app\",\"text\":\"Settings\"}"'
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -e

DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO=$(CDPATH= cd -- "$DIR/../.." && pwd)

docker run --rm --entrypoint "" -v "$REPO:/w" -v x6200-build-output:/b x6200-buildroot sh -c '
    GCC=/b/host/bin/arm-buildroot-linux-gnueabihf-gcc
    [ -x "$GCC" ] || { echo "falta el toolchain de buildroot ($GCC)" >&2; exit 1; }
    "$GCC" -static -O2 -o /w/tools/remote/x6200-remote /w/tools/remote/x6200-remote.c
'
echo "listo: tools/remote/x6200-remote"
