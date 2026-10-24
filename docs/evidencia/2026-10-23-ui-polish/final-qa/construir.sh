#!/bin/bash
#
# QA final de la epica #284 (issue #287): construye el arnes de host de FUENTE
# REAL out-of-source, con el SHA candidato cableado en el binario.
#
#   sh docs/evidencia/2026-10-23-ui-polish/final-qa/construir.sh [WT] [SCRATCH] [SHA]
#
# El build va FUERA del worktree ($SCRATCH/build) para que el unico cambio en el
# arbol sea el directorio de evidencia. El SHA entra por -DHARNESS_REVISION: git
# no ve el worktree enlazado desde dentro del contenedor (el mount oculta .git),
# asi que sin ese flag el JSON diria "unknown" y no se podria probar que captura
# corresponde al candidato.
#
# Los DOS binarios salen del mismo arbol y del mismo LVGL:
#   ui_harness          los dialogos mockeados del corpus (invariante de la epica)
#   ui_harness_dialogs  las tres ventanas reales que el protocolo remoto no abre
#
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail

AQUI=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
WT=${1:-$(CDPATH= cd -- "$AQUI/../../../.." && pwd)}
SCRATCH=${2:-/tmp/opencode/ui-polish-qa-284}
SHA=${3:-1845cb77d36b93c4d8cd774fd130735f4d2f6f30}
IMAGEN=${UI_HARNESS_IMAGE:-x6200-uiharness}

mkdir -p "$SCRATCH/build" "$SCRATCH/out"

echo "== construyendo el arnes de host en $SCRATCH/build (SHA $SHA) =="
docker run --rm --name x6200-qa284-construir --entrypoint bash \
    --user "$(id -u):$(id -g)" \
    -v "$WT:/src" -v "$SCRATCH:/scratch" \
    -w /src/tools/ui_harness "$IMAGEN" -lc "
set -e
cmake -S /src/tools/ui_harness -B /scratch/build \
      -DHARNESS_REVISION=$SHA -DCMAKE_BUILD_TYPE=Release \
      > /scratch/build-configure.log 2>&1
cmake --build /scratch/build -j 8 > /scratch/build.log 2>&1
echo '== binarios =='
ls -l /scratch/build/ui_harness /scratch/build/ui_harness_dialogs
"
echo "== lvgl del arbol (host) =="
git -C "$WT/lvgl" rev-parse HEAD

echo "== revision que reporta el binario (una captura de humo) =="
docker run --rm --name x6200-qa284-smoke --entrypoint bash \
    --user "$(id -u):$(id -g)" \
    -v "$WT:/src" -v "$SCRATCH:/scratch" \
    -w /scratch/build "$IMAGEN" -lc "
./ui_harness --scenario /src/tools/ui_harness/fixtures/home-usb-20m.scn \
    --png /scratch/out/smoke.png --json /scratch/out/smoke.json >/dev/null 2>&1
grep -o '\"revision\": \"[^\"]*\"' /scratch/out/smoke.json
grep -o '\"requested\": \"[^\"]*\"' /scratch/out/smoke.json
"
