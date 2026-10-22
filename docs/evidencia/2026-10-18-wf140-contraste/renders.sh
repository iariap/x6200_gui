#!/usr/bin/env bash
# Renders del harness host para la evidencia de contraste del #140.
#
#   renders.sh <worktree> <salida> <pasada>
#
# pasada:
#   antes                  el src/ de HEAD (sin el fix), via stash temporal
#   despues                el src/ del worktree, tal cual (con el fix)
#   variante-panel-border  LV_OPA_COVER + TC(panel_border) + blending normal
#   variante-additive      LV_OPA_COVER + TC(panel_border) + LV_BLEND_MODE_ADDITIVE
#
# Deja en <salida>/on los 7 fixtures con la linea (main-espectro-agua es Aurora y
# los seis main-tema-<tema>-agua) y en <salida>/off los mismos 7 con el parametro
# `waterfall_center_line` apagado (el fondo del waterfall). Los .scn "off" se
# generan en tools/ui_harness/build/off-fixtures/ (ignorado por git), no en
# fixtures/.
#
# Las dos variantes se renderizan con el screens.c de HEAD: el assert nuevo del
# harness rechaza la variante panel-border a proposito (ver assert-panel-border.log).
# El worktree se restaura siempre (trap), incluso si algo falla.
set -euo pipefail

WT=${1:?worktree de la corrida}
OUT=${2:?directorio de salida}
PASADA=${3:?pasada: antes | despues | variante-panel-border | variante-additive}

THEMES="aurora cathode redshift sprint sunlit trail"
FIXTURES="main-espectro-agua $(for t in $THEMES; do echo -n "main-tema-$t-agua "; done)"
REV=$(git -C "$WT" rev-parse HEAD)
TMP=$(mktemp -d)

restaurar() {
    [ -n "${WATERFALL_BACKUP:-}" ] && cp "$WATERFALL_BACKUP" "$WT/src/waterfall.c"
    [ -n "${SCREENS_BACKUP:-}" ] && cp "$SCREENS_BACKUP" "$WT/tools/ui_harness/src/screens.c"
    [ "${STASHED:-no}" = si ] && git -C "$WT" stash pop >/dev/null
    rm -rf "$TMP"
    echo "-- worktree restaurado:"
    git -C "$WT" status --short
    # El build/ quedo con el src/ de la pasada; se recompila con el src/ restaurado
    # para no dejar el binario del harness mintiendo.
    [ "${TOCADO:-no}" = si ] && build
}
trap restaurar EXIT

build() {
    docker run --rm --entrypoint bash --user "$(id -u):$(id -g)" \
        -v "$WT:/src" -w /src/tools/ui_harness x6200-uiharness \
        -lc "mkdir -p build && cd build && cmake .. -DHARNESS_REVISION=$REV >/dev/null && make -j\$(nproc) 2>&1 | tail -1"
}

# --- el estado del src/ que corresponde a la pasada ---------------------------
case "$PASADA" in
    antes)
        git -C "$WT" stash push -m "wf140-renders-$PASADA" >/dev/null
        STASHED=si
        TOCADO=si
        ;;
    despues) ;;
    variante-panel-border|variante-additive)
        TOCADO=si
        WATERFALL_BACKUP=$TMP/waterfall.c
        SCREENS_BACKUP=$TMP/screens.c
        cp "$WT/src/waterfall.c" "$WATERFALL_BACKUP"
        cp "$WT/tools/ui_harness/src/screens.c" "$SCREENS_BACKUP"
        sed -i 's/theme_hex(TC_text)/theme_hex(TC_panel_border)/' "$WT/src/waterfall.c"
        git -C "$WT" show HEAD:tools/ui_harness/src/screens.c > "$WT/tools/ui_harness/src/screens.c"
        if [ "$PASADA" = variante-additive ]; then
            sed -i 's/    lv_style_set_line_opa(&middle_line_style, LV_OPA_COVER);/    lv_style_set_line_opa(\&middle_line_style, LV_OPA_COVER);\n    lv_style_set_blend_mode(\&middle_line_style, LV_BLEND_MODE_ADDITIVE);/' \
                "$WT/src/waterfall.c"
        fi
        ;;
    *) echo "pasada desconocida: $PASADA" >&2; exit 2 ;;
esac
echo "-- pasada $PASADA (revision $REV)"
grep -n "line_color\|line_opa\|blend_mode" "$WT/src/waterfall.c" || true

# --- los .scn "off" (el mismo fixture con el parametro apagado) ---------------
mkdir -p "$WT/tools/ui_harness/build/off-fixtures"
for f in $FIXTURES; do
    t=${f#main-tema-}
    sed -e "s/^name .*/name off-$t/" -e 's/^waterfall_center_line true$/waterfall_center_line false/' \
        "$WT/tools/ui_harness/fixtures/$f.scn" > "$WT/tools/ui_harness/build/off-fixtures/off-$t.scn"
    grep -q '^waterfall_center_line false$' "$WT/tools/ui_harness/build/off-fixtures/off-$t.scn"
done

# --- build + renders ---------------------------------------------------------
build

mkdir -p "$OUT/on" "$OUT/off"
render() { # render <scenario-relativo-al-build> <nombre>
    docker run --rm --entrypoint bash --user "$(id -u):$(id -g)" \
        -v "$WT:/src" -w /src/tools/ui_harness/build x6200-uiharness \
        -lc "./ui_harness --scenario $1 --png out/$2.png --json out/$2.json >/dev/null 2>&1"
    cp "$WT/tools/ui_harness/build/out/$2.png" "$OUT/$2.png"
}
for f in $FIXTURES; do
    render "../fixtures/$f.scn" "$f"
    mv "$OUT/$f.png" "$OUT/on/$f.png"
done
render "off-fixtures/off-espectro.scn" "off-espectro"
for t in $THEMES; do
    render "off-fixtures/off-$t.scn" "off-$t"
    mv "$OUT/off-$t.png" "$OUT/off/"
done
mv "$OUT/off-espectro.png" "$OUT/off/"

md5sum "$OUT"/on/*.png "$OUT"/off/*.png
echo "-- listo: $OUT"
