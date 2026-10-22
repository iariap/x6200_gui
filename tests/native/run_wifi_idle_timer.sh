#!/bin/sh
#
# Stage de WiFi ocioso: el pump de GLib de src/wifi.cpp (issue #218, F-06).
#
# El modulo real (etapa de WiFi/BT de la GUI) crea un loop_timer de LVGL cuyo callback corre
# g_main_context_iteration() para despachar las callbacks async de NetworkManager. Ese timer
# es el UNICO pump de GLib del proceso y tiene que seguir existiendo con la radio apagada
# (las desconexiones tardias lo necesitan), asi que lo que se mide -y lo que el issue fija-
# es su periodo: 30 ms con la WiFi encendida y 250 ms con la WiFi apagada.
#
# Este stage compila el src/wifi.cpp REAL (el del arbol, o el de un ref con --ab) contra:
#   - tests/native/stubs_wifi/: stubs minimos de LVGL/GLib/libnm/params/GPIO (solo lo que el
#     modulo usa) y un motor de timers que espeja lvgl/src/misc/lv_timer.c;
#   - tests/native/wifi_idle_stubs.cpp: el pump de GLib con reloj virtual, que cuenta sus
#     vueltas y despacha lo que el doble de NetworkManager encola;
#   - tests/native/test_wifi_idle_timer.cpp: el contrato + la medicion.
#
# Uso:
#   tests/native/run_wifi_idle_timer.sh [out-dir]        # gate: el contrato de #218
#   tests/native/run_wifi_idle_timer.sh --ab <ref> [out-dir]
#       modo instrumento (NO es un gate): compila el MISMO test contra
#       `git show <ref>:src/wifi.cpp` y contra el arbol de trabajo, e imprime los dos
#       numeros. El codigo anterior a #218 no puede cumplir el contrato (el periodo era 30 ms
#       siempre), asi que su corrida queda roja a proposito: lo que se compara es el numero
#       que mide el instrumento.
#
# Sin radio, sin red y sin docker: solo compilador de C/C++ y el reloj virtual del fixture.
# La radio real (pin fisico, energia medida) queda PENDING_HARDWARE.
#
# SPDX-License-Identifier: LGPL-2.1-or-later
#
set -e

DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO=$(CDPATH= cd -- "$DIR/../.." && pwd)
SRC="$REPO/src"
STUBS="$DIR/stubs_wifi"
CXX="${CXX:-g++}"
CC="${CC:-gcc}"

CXXFLAGS="-std=gnu++17 -O1 -g -Wall -Wextra -Wno-unused-parameter -pthread"
CFLAGS="-std=gnu11 -O1 -g -Wall -Wextra -Wno-unused-parameter -pthread"
SAN="-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover"

MODE=gate
REF=""
if [ "${1:-}" = "--ab" ]; then
    MODE=ab
    REF="${2:?uso: $0 --ab <ref> [out-dir]}"
    shift 2
fi
OUT="${1:-$DIR/build/wifi-idle}"
mkdir -p "$OUT"

# El runtime de los sanitizers no esta en todos los toolchains (el runner de CI no lo trae):
# se prueba y, si falta, el stage corre sin ellos y lo dice.
if echo 'int main(void){return 0;}' | $CXX -x c++ - $SAN -o "$OUT/.san_probe" >/dev/null 2>&1; then
    SAN_FLAGS="$SAN"
else
    SAN_FLAGS=""
fi

# wifi_idle_build <archivo-fuente> <tag> <allow-failure>
# Compila y corre el test con esa fuente como src/wifi.cpp. El exit del binario se devuelve.
wifi_idle_build() {
    src_file="$1"
    tag="$2"
    allow_failure="$3"
    stage="$OUT/$tag/stage"

    rm -rf "$OUT/$tag"
    mkdir -p "$stage"

    cp "$src_file" "$stage/wifi.cpp"
    cp "$SRC/wifi.h" "$stage/"

    # Todo include entre comillas del wifi.cpp staged tiene que resolver al stage o a los
    # stubs: si el modulo suma un header, el error tiene que nombrarlo en vez de morir en
    # medio del compile de C++.
    missing=""
    for inc in $(sed -n 's/^[[:space:]]*#[[:space:]]*include[[:space:]]*"\([^"]*\)".*/\1/p' "$stage/wifi.cpp"); do
        if [ ! -f "$stage/$inc" ] && [ ! -f "$STUBS/$inc" ]; then
            missing="$missing $inc"
        fi
    done
    if [ -n "$missing" ]; then
        echo "FALLO: wifi.cpp cita headers que no estan staged ni en $STUBS:" >&2
        for inc in $missing; do echo "  - $inc" >&2; done
        return 1
    fi

    # shellcheck disable=SC2086
    $CXX $CXXFLAGS $SAN_FLAGS -I"$stage" -I"$STUBS" -c "$DIR/test_wifi_idle_timer.cpp" \
        -o "$OUT/$tag/test.o"
    # shellcheck disable=SC2086
    $CXX $CXXFLAGS $SAN_FLAGS -I"$stage" -I"$STUBS" -c "$DIR/wifi_idle_stubs.cpp" \
        -o "$OUT/$tag/stubs.o"
    # shellcheck disable=SC2086
    $CC $CFLAGS $SAN_FLAGS -I"$DIR" -c "$DIR/test_framework.c" -o "$OUT/$tag/test_framework.o"
    # shellcheck disable=SC2086
    $CC $CFLAGS $SAN_FLAGS -I"$DIR" -c "$DIR/wifi_idle_timer_main.c" -o "$OUT/$tag/main.o"

    # shellcheck disable=SC2086
    $CXX "$OUT/$tag/test.o" "$OUT/$tag/stubs.o" "$OUT/$tag/test_framework.o" \
        "$OUT/$tag/main.o" -lm -pthread $SAN_FLAGS -o "$OUT/$tag/wifi_idle_tests"

    status=0
    "$OUT/$tag/wifi_idle_tests" > "$OUT/$tag/out.txt" 2>&1 || status=$?
    cat "$OUT/$tag/out.txt"

    if [ "$status" -ne 0 ] && [ "$allow_failure" != "yes" ]; then
        echo "FALLO: el contrato de #218 no se cumple (exit $status)" >&2
        return "$status"
    fi

    return 0
}

echo "== compilador: $($CXX --version | head -1) =="
if [ -n "$SAN_FLAGS" ]; then
    echo "== sanitizers: ASan + UBSan =="
else
    echo "== sanitizers: NO disponibles en este toolchain (se omite) =="
fi

echo "== WiFi ocioso (#218): contrato del pump de GLib y periodo por estado =="
wifi_idle_build "$SRC/wifi.cpp" head-no

if [ "$MODE" = "gate" ]; then
    echo "PASS: un loop_timer, periodo 30/250 ms, pump vivo y NetworkManager intacto"
    exit 0
fi

# --- modo instrumento: el antes y el despues con el MISMO test ----------------
# No se fetchea: un ref ausente es un fallo explicito.
if ! git -C "$REPO" cat-file -e "$REF:src/wifi.cpp" 2>/dev/null; then
    echo "FALLO: $REF no tiene src/wifi.cpp (ref ausente; no se fetchea)" >&2
    exit 2
fi
git -C "$REPO" show "$REF:src/wifi.cpp" > "$OUT/wifi-before.cpp"

echo ""
echo "== A/B del instrumento: el mismo test contra $REF:src/wifi.cpp =="
echo "   (el codigo de antes de #218 no cumple el contrato: su corrida queda roja a proposito)"
wifi_idle_build "$OUT/wifi-before.cpp" before yes

printf '\n== numeros (vueltas del pump de GLib en 10000 ms de WiFi ociosa) ==\n'
printf '  %-28s %s\n' "arbol de trabajo (HEAD):" "$(grep -E '^  (OFF|ON)  ?\(periodo' "$OUT/head-no/out.txt" | tr -s ' ' | tr '\n' ' ')"
printf '  %-28s %s\n' "$REF (antes):" "$(grep -E '^  (OFF|ON)  ?\(periodo' "$OUT/before/out.txt" | tr -s ' ' | tr '\n' ' ')"
echo "  (el test de $REF falla sus aserciones de contrato: se lee el numero, no el exit)"
exit 0
