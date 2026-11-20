#!/bin/sh
# QA-LINUX-DSP-REPRO-BUILD — build reproducible branch-local, sin deploy.
# Aplica el parche X6200Control en una copia temporal (NUNCA modifica refs in place),
# compila libaether parcheada, GUI+servicio feature-ON ARM y GUI normal OFF; empaqueta
# artefactos QA con SHA256. No instala en la radio ni toca firmware.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
UUID=linux-dsp-repro
PATCH=${PATCH:-$ROOT/docs/evidencia/2026-11-04-linux-dsp-integration/patches/x6200-control-snapshot-bulk.patch}
PATCH_SHA=${PATCH_SHA:-$PATCH.sha256}
REFS_SRC=${REFS_SRC:-$ROOT/../refs/X6200Control}
TC=${TOOLCHAIN_FILE:-$ROOT/../rom-build/clone-buildroot/build/host/share/buildroot/toolchainfile.cmake}
SYSROOT=${SYSROOT:-$ROOT/../rom-build/clone-buildroot/build/host/arm-buildroot-linux-gnueabihf/sysroot}
WORK=${WORK:-/tmp/$UUID}
JOBS=${JOBS:-8}
CC_REF_INC=${CC_REF_INC:-$ROOT/../refs/X6200Control/include}
MODE=${1:-full}

say() { printf '%s %s\n' "==" "$*"; }
fail() { printf 'FAIL: %s\n' "$*" >&2; exit 1; }
skip() { printf 'SKIP: %s\n' "$*"; exit 0; }
verify_sha() {
    [ -f "$PATCH" ] || fail "falta patch $PATCH"
    expected=$(awk '{print $1}' "$PATCH_SHA" 2>/dev/null || true)
    actual=$(sha256sum "$PATCH" | awk '{print $1}')
    [ -n "$expected" ] || fail "falta $PATCH_SHA"
    [ "$expected" = "$actual" ] || fail "sha256 del patch no coincide (esperado $expected, actual $actual)"
    say "patch sha256 OK: $actual"
}
patch_self_test() {
    verify_sha
    TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
    cp -a "$REFS_SRC/include" "$REFS_SRC/src" "$TMP/"
    ( cd "$TMP" && patch -p1 --batch --forward < "$PATCH" >/dev/null ) || fail "patch no aplica"
    grep -q 'x6200_control_shadow_update' "$TMP/include/aether_radio/x6200_control/low/control.h" || fail "símbolo sombra no aplicado"
    grep -q 'x6200_control_bulk_write' "$TMP/src/low/control.c" || fail "símbolo bulk no aplicado"
    if ( cd "$TMP" && patch -p1 --batch --forward --dry-run < "$PATCH" >/dev/null 2>&1 ); then fail "patch no es idempotente (re-aplicó)"; fi
    say "patch aplica 1 vez, 2da aplicación rechazada (idempotente)"
}
preflight() {
    [ -f "$PATCH" ] || fail "falta patch $PATCH"
    [ -d "$REFS_SRC" ] || skip "falta fuente X6200Control en $REFS_SRC (REFS_SRC=...)"
    [ -f "$TC" ] || skip "falta toolchain Buildroot en $TC (TOOLCHAIN_FILE=...)"
    [ -d "$SYSROOT/usr/lib/pkgconfig" ] || skip "falta sysroot/pkg-config en $SYSROOT"
    [ -d "$CC_REF_INC" ] || skip "faltan headers de referencia en $CC_REF_INC"
    command -v arm-buildroot-linux-gnueabihf-gcc >/dev/null 2>&1 || \
        [ -x "$(dirname "$TC")/../../bin/arm-buildroot-linux-gnueabihf-gcc" ] || true
}
preflight
patch_self_test
[ "$MODE" = "patch-only" ] && { say "MODE=patch-only: OK"; exit 0; }

say "WORK=$WORK"
rm -rf "$WORK"; mkdir -p "$WORK/src"
( cd "$REFS_SRC" && tar -cf - --exclude=.git . ) | ( cd "$WORK/src" && tar -xf - )
( cd "$WORK/src" && patch -p1 --batch --forward < "$PATCH" >/dev/null )
say "parche aplicado en copia temporal (refs intacto)"
PKG="$SYSROOT/usr/lib/pkgconfig"
export PKG_CONFIG_PATH="$PKG" PKG_CONFIG_SYSROOT_DIR="$SYSROOT"
say "build libaether parcheada"
cmake -S "$WORK/src" -B "$WORK/build-ctrl" -DCMAKE_TOOLCHAIN_FILE="$TC" \
      -DCMAKE_INSTALL_PREFIX="$WORK/install" -DBUILD_SHARED_LIBS=ON > "$WORK/build-ctrl.log" 2>&1 \
  || { tail -20 "$WORK/build-ctrl.log"; fail "configure libaether"; }
cmake --build "$WORK/build-ctrl" -j"$JOBS" >> "$WORK/build-ctrl.log" 2>&1 \
  || { tail -20 "$WORK/build-ctrl.log"; fail "build libaether"; }
cmake --install "$WORK/build-ctrl" >> "$WORK/build-ctrl.log" 2>&1 \
  || { tail -20 "$WORK/build-ctrl.log"; fail "install libaether"; }
LIB=$(find "$WORK/install" -name 'libaether_x6200_control.so*' | head -1)
[ -n "$LIB" ] || fail "libaether parcheada no instalada"
REF_INC="$CC_REF_INC"
C_FLAGS="-I$WORK/install/include -I$REF_INC"
say "configure/build feature-ON GUI+servicio (ARM)"
cmake -S "$ROOT" -B "$WORK/build-on" -DX6200_LINUX_DSP_ROUTE=ON -DX6200_CTRL_HAVE_BULK_WRITE=ON \
      -DCMAKE_TOOLCHAIN_FILE="$TC" \
      -DCMAKE_C_FLAGS="$C_FLAGS" -DCMAKE_CXX_FLAGS="$C_FLAGS" \
      -DCMAKE_EXE_LINKER_FLAGS="-L$WORK/install/lib -Wl,-rpath-link,$WORK/install/lib" \
      -DAETHER_X6200_CONTROL_LIB="$(cd "$(dirname "$LIB")" && pwd)/libaether_x6200_control.so" \
      > "$WORK/build-on.log" 2>&1 || { tail -20 "$WORK/build-on.log"; fail "configure feature-ON"; }
cmake --build "$WORK/build-on" --target x6200_gui x6200_route_service -j"$JOBS" >> "$WORK/build-on.log" 2>&1 \
  || { tail -20 "$WORK/build-on.log"; fail "build feature-ON"; }
GUI="$WORK/build-on/src/x6200_gui"; SVC="$WORK/build-on/src/linux_dsp_route/x6200_route_service"
[ -x "$GUI" ] && [ -x "$SVC" ] || fail "faltan binarios feature-ON"
file "$GUI" | grep -q 'ARM, EABI5' || fail "GUI no es ARM EABI5"
file "$SVC" | grep -q 'ARM, EABI5' || fail "servicio no es ARM EABI5"
nm -D "$SVC" | grep -q 'x6200_control_bulk_write' || fail "servicio no referencia bulk_write"
say "feature-ON ARM OK (GUI+servicio, símbolo bulk)"
say "build normal OFF (BASE: sin ruta/servicio)"
cmake -S "$ROOT" -B "$WORK/build-off" -DX6200_LINUX_DSP_ROUTE=OFF -DCMAKE_TOOLCHAIN_FILE="$TC" \
      -DCMAKE_C_FLAGS="$C_FLAGS" -DCMAKE_CXX_FLAGS="$C_FLAGS" > "$WORK/build-off.log" 2>&1 \
  || { tail -20 "$WORK/build-off.log"; fail "configure OFF"; }
cmake --build "$WORK/build-off" --target x6200_gui -j"$JOBS" >> "$WORK/build-off.log" 2>&1 \
  || { tail -20 "$WORK/build-off.log"; fail "build OFF"; }
OFF_GUI="$WORK/build-off/src/x6200_gui"
[ -x "$OFF_GUI" ] || fail "falta GUI OFF"
[ ! -d "$WORK/build-off/src/linux_dsp_route" ] || fail "OFF construyó linux_dsp_route (no debe)"
[ ! -e "$WORK/build-off/src/linux_dsp_route/x6200_route_service" ] || fail "OFF construyó servicio (no debe)"
if nm -D "$OFF_GUI" | grep -q 'route_\|x6200_route'; then fail "GUI OFF referencia símbolos de ruta"; fi
say "OFF BASE OK (sin ruta/servicio/símbolos)"
say "tests host/ARM/QEMU"
OUT="$WORK/test-out" "$ROOT/tests/linux-dsp-integration/run_tests.sh" > "$WORK/tests.log" 2>&1 \
  || { tail -30 "$WORK/tests.log"; fail "tests"; }
grep -q '0 fallos' "$WORK/tests.log" || fail "tests con fallos"
say "tests OK (ver logs)"
say "empaquetar artefactos QA (sin deploy)"
mkdir -p "$WORK/pkg"
cp "$GUI" "$WORK/pkg/x6200_gui"; cp "$SVC" "$WORK/pkg/x6200_route_service"
{
    echo "QA-LINUX-DSP reproducible build"
    echo "base=$ROOT"
    echo "commit=$(git -C "$ROOT" rev-parse HEAD 2>/dev/null || echo unknown)"
    echo "patch_sha256=$(sha256sum "$PATCH" | awk '{print $1}')"
    echo "libaether_patched_sha256=$(sha256sum "$LIB" | awk '{print $1}')"
    echo "IMPORTANTE: NO instalar en la radio; requiere autorización del owner."
} > "$WORK/pkg/MANIFEST.txt"
( cd "$WORK/pkg" && sha256sum x6200_gui x6200_route_service MANIFEST.txt > SHA256SUMS )
say "artefactos: $WORK/pkg (GUI, servicio, MANIFEST, SHA256SUMS) — NO deploy"
say "PASS reproducible build completo"
