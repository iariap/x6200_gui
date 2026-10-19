#!/usr/bin/env bash
#
# Gate del rasterizado incremental del waterfall (OPT-2, issue #148).
#
# Corre los fixtures deterministas del waterfall (tools/ui_harness/CMakeLists.txt,
# target `waterfall_test`): el src/waterfall.c REAL contra LVGL 8.3.11 real y un
# stub de radio. Cubre:
#   - 20 vueltas de anillo con asserts de imagen (oraculo independiente del
#     mapeo) y del offset (render_head);
#   - cambio de frecuencia central durante el scroll y cambios de zoom;
#   - cambio de paleta, RX<->TX, ECO (refresh_period=4) y pantalla OFF/ON;
#   - limites: alto 0/1/512, size > WATERFALL_NFFT, ancho en Hz estrecho,
#     reserva fallida del frame;
#   - el instrumento de rasterizado (800 px por fila nueva en reposo).
# Y lo vuelve a correr bajo ASan + UBSan.
#
# El perfil natural (area invalidada/flusheada, filas, display-off) lo cubre
# tests/check_harness_natural.sh, que ahora tambien lee el instrumento
# waterfall_rasterized_px.
#
# Usage: tests/check_waterfall_incremental.sh
#   UI_HARNESS_IMAGE=<name>  override the Docker image (default x6200-uiharness)
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HARNESS_DIR="$ROOT/tools/ui_harness"
IMAGE="${UI_HARNESS_IMAGE:-x6200-uiharness}"

fail() { echo "FAIL: $*" >&2; exit 1; }

command -v docker >/dev/null 2>&1 || fail "docker not found"
docker image inspect "$IMAGE" >/dev/null 2>&1 || \
    fail "image '$IMAGE' not found (build it: docker build -t $IMAGE tools/ui_harness)"

run() {
    docker run --rm --entrypoint bash --user "$(id -u):$(id -g)" \
        -v "$ROOT:/src" -w /src/tools/ui_harness "$IMAGE" "$@"
}

echo "== waterfall incremental fixtures (OPT-2, #148) =="
run -lc "mkdir -p build && cd build && cmake .. >/dev/null && make -j\$(nproc) waterfall_test >/dev/null && ./waterfall_test" \
    || fail "the deterministic fixtures failed"

echo "== waterfall incremental fixtures under ASan + UBSan =="
# Bajo build/ (ignorado por git) para no dejar directorios de build sueltos.
run -lc "mkdir -p build/asan && cd build/asan && \
    cmake ../.. -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
                -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined' >/dev/null && \
    make -j\$(nproc) waterfall_test >/dev/null && \
    ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./waterfall_test" \
    || fail "the fixtures failed under ASan/UBSan"

echo "PASS: waterfall incremental fixtures + ASan/UBSan (#148)"
