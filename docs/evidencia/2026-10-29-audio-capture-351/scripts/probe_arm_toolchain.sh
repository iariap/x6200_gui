#!/bin/sh
#
# Sonda del toolchain ARM del Buildroot para el build del modulo de captura (#351,
# parte 3b). No compila nada si no hay toolchain: informa exactamente que falta.
#
#   sh docs/evidencia/2026-10-29-audio-capture-351/scripts/probe_arm_toolchain.sh
#
# Contrato que verifica (punto 5 de «Tests y regresion obligatorios» del issue):
# compilar `src/audio.c` y `src/audio_enhancement.c` con el toolchain del Buildroot
# del proyecto, `readelf -h` sobre el objeto (ELF32 ARM, EABI5) y el linking contra
# la `libspeexdsp` del target. **No** se presenta un build de host como si fuera ARM.
#
# Que se comprueba, en orden:
#   1. la imagen del build del proyecto existe (`x6200-buildroot`);
#   2. su salida de build (`/workspace/build`, que es el bind del clon del owner) tiene
#      el toolchain (`host/bin/*-gcc`) — sin eso la imagen son solo las fuentes de
#      Buildroot y el cache de descargas;
#   3. el volumen de salida del build (`x6200-build-output`) tiene `target/` con la
#      `libspeexdsp` y su header;
#   4. hay un cross-compiler armhf en el host (no sirve uno soft-float: la rootfs del
#      ROM es armhf).
#
# Si cualquiera falta: imprime `BLOCKED` con la causa y sale 77 (el codigo de "omitido"
# del proyecto), nunca 0.
#
# Variables: IMAGEN (default x6200-buildroot), VOL (default x6200-build-output),
#            BUILDROOT_OUT (clon del buildroot en el host, opcional: si esta, se mira
#            ahi el toolchain en vez de dentro de la imagen).
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -u

IMAGEN=${IMAGEN:-x6200-buildroot}
VOL=${VOL:-x6200-build-output}
BUILDROOT_OUT=${BUILDROOT_OUT:-}
fallas=""

echo "== sonda del toolchain ARM del Buildroot (#351) =="

if docker image inspect "$IMAGEN" >/dev/null 2>&1; then
    echo "  imagen $IMAGEN: ok"
else
    echo "  imagen $IMAGEN: FALTA (docker build -t $IMAGEN <clon del buildroot>)"
    fallas="$fallas imagen"
fi

if [ -n "$BUILDROOT_OUT" ] && [ -d "$BUILDROOT_OUT/host/bin" ]; then
    echo "  toolchain en $BUILDROOT_OUT/host/bin: ok"
    ls "$BUILDROOT_OUT"/host/bin/*-gcc 2>/dev/null | head -3 | sed 's/^/    /'
else
    salida=$(docker run --rm --entrypoint "" "$IMAGEN" sh -c \
        'ls -A /workspace/build 2>/dev/null | head -3; echo "[fin]"' 2>/dev/null)
    if [ "$salida" = "[fin]" ]; then
        echo "  /workspace/build (salida del build del proyecto): VACIA"
        echo "    -> la imagen trae las fuentes de Buildroot y el cache de descargas,"
        echo "       pero el toolchain se construye en runtime (make toolchain)."
        fallas="$fallas toolchain"
    else
        echo "  /workspace/build: tiene contenido"
        echo "$salida" | sed 's/^/    /'
        if docker run --rm --entrypoint "" "$IMAGEN" sh -c \
            'ls /workspace/build/host/bin/*-gcc >/dev/null 2>&1'; then
            echo "  host/bin/*-gcc: ok"
        else
            echo "  host/bin/*-gcc: FALTA"
            fallas="$fallas toolchain"
        fi
    fi
fi

if docker run --rm --entrypoint "" -v "$VOL":/b "$IMAGEN" sh -c \
    'ls /b/target/usr/lib/libspeexdsp.so* >/dev/null 2>&1 && ls /b/target/usr/include/speex/speex_preprocess.h >/dev/null 2>&1'; then
    echo "  $VOL: target/ con libspeexdsp y header: ok"
else
    echo "  $VOL: target/ con libspeexdsp y header: FALTA"
    fallas="$fallas target-libspeexdsp"
fi

if command -v arm-buildroot-linux-gnueabihf-gcc >/dev/null 2>&1; then
    echo "  cross-compiler armhf en el host: ok"
else
    hf=$(ls /usr/bin 2>/dev/null | grep -c 'gnueabihf')
    echo "  cross-compiler armhf en el host: no (binarios *gnueabihf* en /usr/bin: $hf)"
    fallas="$fallas cross-host"
fi

echo
if [ -n "$fallas" ]; then
    echo "BLOCKED: no hay toolchain ARM del Buildroot disponible ($(echo "$fallas" | sed 's/^ //; s/ /, /g'))"
    echo "  comando que lo construiria (no ejecutado en esta corrida):"
    echo "    docker run --rm --entrypoint \"\" -v <clon>:/workspace $IMAGEN \\"
    echo "      bash -c 'cd /workspace && ./br_config.sh && make -C buildroot O=../build toolchain -j\$(nproc)'"
    echo "  el build ARM del modulo queda PENDING; no se sustituye por un build de host."
    exit 77
fi

echo "OK: toolchain ARM del Buildroot presente; correr el build del modulo con el."
exit 0
