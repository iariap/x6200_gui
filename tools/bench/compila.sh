#!/bin/bash
# Compila las libs del banco (ARM, para el chroot) con el toolchain de la imagen de build.
#
#   tools/bench/compila.sh
#
# Deja en tools/bench/: libfakefb.so (framebuffer falso), libcrash.so (backtrace en crash.txt) y
# libx6200trace.so (traza de open/pthread_create). Las versiones ya compiladas viven versionadas en
# este directorio, asi que solo hace falta si se tocan los .c.
set -e
cd "$(dirname "$0")"
IMAGEN=${IMAGEN:-x6200-buildroot}
CC=arm-buildroot-linux-gnueabihf-gcc
docker run --rm --entrypoint "" -v "$PWD:/b" "$IMAGEN" sh -c "
    cd /b
    \$CC -shared -fPIC -O2 -o libfakefb.so    fakefb.c -ldl
    \$CC -shared -fPIC -O2 -o libcrash.so     crash.c  -ldl -lpthread
    \$CC -shared -fPIC -O2 -o libx6200trace.so nobuf.c -ldl -lpthread
    ls -la lib*.so"
