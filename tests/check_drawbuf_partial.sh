#!/usr/bin/env bash
#
# Gate del draw buffer parcial con rotación 90° (issue #147).
#
# El firmware reserva el draw buffer en *pixeles* (size_in_px_cnt), no en bytes,
# y la pantalla corre con sw_rotate + LV_DISP_ROT_90 (lógica 800x480, física
# 480x800). Achicar el buffer solo es seguro si LVGL 8.3.11 produce el MISMO
# framebuffer físico con cualquier tamaño de buffer. Este gate lo prueba con el
# probe del arnés (tools/ui_harness/probe/disp_buf_probe.c), que linkea el LVGL
# real del submódulo con la configuración de display del firmware y reconstruye
# el framebuffer físico desde los flush callbacks.
#
# Para cada tamaño de buffer corre una secuencia de escenas (pantalla completa,
# invalidaciones parciales, rects cortados en límites, más de un flush por
# frame, áreas no alineadas, cambios de pantalla) y exige:
#   1. framebuffer físico idéntico byte a byte al del buffer actual;
#   2. contrato de flush: lv_disp_flush_is_last verdadero exactamente una vez
#      por ciclo, ningún rect fuera del panel, cada píxel flusheado una sola vez.
#
# Usage: tests/check_drawbuf_partial.sh
#   SKIP_BUILD=1             reusa tools/ui_harness/build
#   UI_HARNESS_IMAGE=<name>  override de la imagen Docker (default x6200-uiharness)
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HARNESS_DIR="$ROOT/tools/ui_harness"
BUILD_DIR="$HARNESS_DIR/build"
OUT_DIR="$BUILD_DIR/out/drawbuf"
IMAGE="${UI_HARNESS_IMAGE:-x6200-uiharness}"

# px = size_in_px_cnt (lv_color_t), lines = px / 800. The first is the current
# firmware buffer (the reference); the rest are the experiment's candidates.
SIZES="1536000 384000 102400 51200 25600"
REF=1536000

fail() { echo "FAIL: $*" >&2; exit 1; }

command -v docker >/dev/null 2>&1 || fail "docker not found"
docker image inspect "$IMAGE" >/dev/null 2>&1 || \
    fail "image '$IMAGE' not found (build it: docker build -t $IMAGE tools/ui_harness)"

run() {
    docker run --rm --entrypoint bash --user "$(id -u):$(id -g)" \
        -v "$ROOT:/src" -w /src/tools/ui_harness "$IMAGE" "$@"
}

if [ "${SKIP_BUILD:-0}" != "1" ]; then
    REV="$(git -C "$ROOT" rev-parse HEAD 2>/dev/null || echo unknown)"
    echo "building disp_buf_probe (revision $REV)..."
    run -lc "mkdir -p build && cd build && \
        cmake .. -DHARNESS_REVISION=$REV >/dev/null && make disp_buf_probe -j\$(nproc) >/dev/null"
fi

[ -x "$BUILD_DIR/disp_buf_probe" ] || fail "disp_buf_probe not built ($BUILD_DIR/disp_buf_probe)"

echo "running the probe for every draw-buffer size..."
run -lc "cd build && rm -rf out/drawbuf && mkdir -p out/drawbuf && \
    for px in $SIZES; do ./disp_buf_probe --px \$px --out out/drawbuf/\$px >/dev/null; done"

status=0

# 1. Every candidate's physical framebuffer must match the reference byte by byte.
echo "framebuffer comparison (vs px=$REF, the current firmware buffer):"
for px in $SIZES; do
    [ -f "$OUT_DIR/$px.fb" ] || fail "missing $OUT_DIR/$px.fb"
done
for px in $SIZES; do
    [ "$px" = "$REF" ] && continue
    if cmp -s "$OUT_DIR/$REF.fb" "$OUT_DIR/$px.fb"; then
        echo "  px=$px (lines=$((px / 800))): IDENTICAL"
    else
        echo "  px=$px (lines=$((px / 800))): DIFFERS" >&2
        status=1
    fi
done

# 2. Flush contract, per phase, per size.
echo "flush contract (last==1, out_of_area==0, dup_px==0, covered_px==flushed_px):"
for px in $SIZES; do
    [ -f "$OUT_DIR/$px.json" ] || fail "missing $OUT_DIR/$px.json"
    python3 - "$OUT_DIR/$px.json" <<'PY' || status=1
import json, sys
path = sys.argv[1]
data = json.loads(open(path).read())
bad = []
for ph in data["phases"]:
    if ph["last"] != 1:
        bad.append(f"phase {ph['i']}: last={ph['last']} (must be 1)")
    if ph["out_of_area"] != 0:
        bad.append(f"phase {ph['i']}: out_of_area={ph['out_of_area']} (rect outside the panel)")
    if ph["dup_px"] != 0:
        bad.append(f"phase {ph['i']}: dup_px={ph['dup_px']} (pixel flushed twice)")
    if ph["covered_px"] != ph["flushed_px"]:
        bad.append(f"phase {ph['i']}: covered={ph['covered_px']} != flushed={ph['flushed_px']}")
if bad:
    print(f"  px={data['px']} (lines={data['lines']}): FAIL")
    for b in bad:
        print(f"    {b}")
    raise SystemExit(1)
print(f"  px={data['px']} (lines={data['lines']}): ok "
      f"({len(data['phases'])} phases, {sum(p['flushes'] for p in data['phases'])} flushes)")
PY
done

[ "$status" -eq 0 ] || fail "draw-buffer partial gate"

echo "PASS: partial draw buffer renders the identical physical framebuffer (sw-rotate 90)"
