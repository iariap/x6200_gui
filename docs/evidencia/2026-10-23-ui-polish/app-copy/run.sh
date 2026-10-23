#!/bin/sh
#
# Reproduces the structural verification of issue #288 (copy-only change).
#
#   sh docs/evidencia/2026-10-23-ui-polish/app-copy/run.sh
#
# 1. Placeholder preservation: every format literal of the touched sources keeps
#    the same `%` specifier sequence as HEAD~ (the commit before the copy change).
# 2. Truncation: the changed `snprintf` calls, with the real destination sizes and
#    worst-case data, never truncate.
# 3. ARM compile: the three touched translation units compile with the firmware
#    flags (-mcpu=cortex-a7 ...) and strict format warnings, without new warnings.
#
# The cross toolchain comes from the Buildroot tree of the host; override it with
# TC=<path to arm-buildroot-linux-gnueabihf-gcc>. Without a toolchain, step 3 is
# reported as SKIP (never as a pass) and the first two steps still run.
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -eu

AQUI=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO=$(CDPATH= cd -- "$AQUI/../../../.." && pwd)
OUT="${OUT:-/tmp/x6200-app-copy-288}"
# Base del cambio (el `origin/main` sobre el que se aplico el mapeo). Pinneada
# para que el check siga comparando contra la version SIN el cambio despues de
# commitear; en el rebase de la Fase 2 se pasa la base nueva con BASE=<sha>.
BASE="${BASE:-eea3ed2022fc0f3be8c0afa1c318e32e42455ce8}"
TC="${TC:-/home/pablo/desa/x6200/AetherX6200Buildroot/build/host/bin/arm-buildroot-linux-gnueabihf-gcc}"
LVGL="${LVGL:-$REPO/lvgl}"

mkdir -p "$OUT"
cd "$REPO"

echo "== 1. placeholders (base: $BASE) =="
python3 "$AQUI/verify_placeholders.py" --base "$BASE" | tee "$OUT/placeholders.txt"

echo "== 2. snprintf truncation (worst-case data) =="
${CC:-cc} -std=gnu11 -O2 -Wall -Wextra -Werror -o "$OUT/truncation_check" \
    "$AQUI/truncation_check.c"
"$OUT/truncation_check" | tee "$OUT/truncation.txt"

echo "== 3. ARM compile of the touched TUs (firmware flags) =="
if [ ! -x "$TC" ]; then
    echo "SKIP: no cross toolchain at $TC (override with TC=...)" | tee "$OUT/arm_compile.txt"
    exit 0
fi
if [ ! -f "$LVGL/lvgl.h" ]; then
    echo "SKIP: lvgl/ submodule not initialized (git submodule update --init lvgl)" \
        | tee "$OUT/arm_compile.txt"
    exit 0
fi

# Same arch flags as CMakeLists.txt (the firmware target) plus the strict format
# warnings this change is verified against.
INC="-I src -I . -I $LVGL -I $LVGL/src -I tools/ui_harness/third_party"
FLAGS="-std=gnu11 -O3 -g0 -DNDEBUG -D_FORTIFY_SOURCE=1 -mtune=cortex-a7 -mcpu=cortex-a7 -mfloat-abi=hard -mfpu=neon-vfpv4 -Wall -Wextra -Wformat=2 -Wformat-security -Wformat-truncation=2"

for f in dialog_propagation dialog_field dialog_swrscan; do
    # shellcheck disable=SC2086
    "$TC" $FLAGS -c "src/$f.c" -o "$OUT/$f.o" $INC 2>"$OUT/$f.log"
    echo "$f: exit $? ($(grep -c 'warning:' "$OUT/$f.log") warning(s))" \
        | tee -a "$OUT/arm_compile.txt"
done
file "$OUT/dialog_propagation.o" | tee -a "$OUT/arm_compile.txt"

echo "== output in $OUT =="
