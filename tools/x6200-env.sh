# Entorno de build comun (se SOURCEA, no se ejecuta): un solo lugar para el cache por usuario y
# para el arbol de AetherX6200Buildroot, con la MISMA respuesta desde el checkout base y desde
# cualquier worktree (state/worktrees/x6200/<slug>): los worktrees son descartables y nada pesado
# vive adentro de ellos.
#
#   REPO=<raiz del checkout> . tools/x6200-env.sh
#
# Deja:
#   X6200_CACHE_DIR  cache por usuario (${XDG_CACHE_HOME:-~/.cache}/x6200 por defecto).
#   X6200_BASE       checkout base del repo (el dueno de .git; en un clon comun es REPO).
#   BUILDROOT        el BUILDROOT del entorno si esta; si no, el primer arbol que exista de:
#                      $X6200_CACHE_DIR/AetherX6200Buildroot   (canonico: clon o symlink)
#                      $X6200_BASE/../buildroot_new/AetherX6200Buildroot
#                      $X6200_BASE/../AetherX6200Buildroot
#                      ~/desa/x6200/buildroot_new/AetherX6200Buildroot
#                      ~/desa/x6200/AetherX6200Buildroot
#                    vacio si no hay ninguno (solo hace falta para compilar).
#   x6200_requiere_buildroot  falla con un mensaje claro si no hay arbol.
#
# Los volumenes de Docker del build (x6200-build-output, ...) no cambian.
#
# SPDX-License-Identifier: LGPL-2.1-or-later

X6200_CACHE_DIR="${X6200_CACHE_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}/x6200}"

# El checkout base se saca del directorio .git comun: un worktree comparte el .git del base, asi
# que la respuesta no depende de la profundidad del worktree.
_x6200_comun=$(git -C "$REPO" rev-parse --path-format=absolute --git-common-dir 2>/dev/null || true)
case "$_x6200_comun" in
    */.git) X6200_BASE=${_x6200_comun%/.git} ;;
    *) X6200_BASE=$REPO ;;
esac
unset _x6200_comun

_x6200_candidatos() {
    printf '%s\n' \
        "$X6200_CACHE_DIR/AetherX6200Buildroot" \
        "$X6200_BASE/../buildroot_new/AetherX6200Buildroot" \
        "$X6200_BASE/../AetherX6200Buildroot" \
        "$HOME/desa/x6200/buildroot_new/AetherX6200Buildroot" \
        "$HOME/desa/x6200/AetherX6200Buildroot"
}

if [ -z "${BUILDROOT:-}" ]; then
    BUILDROOT=$(_x6200_candidatos | while IFS= read -r _c; do
        if [ -d "$_c/buildroot" ]; then (cd "$_c" && pwd); break; fi
    done)
fi

x6200_requiere_buildroot() {
    [ -n "$BUILDROOT" ] && [ -d "$BUILDROOT" ] && return 0
    {
        echo "no encuentro el arbol de AetherX6200Buildroot${BUILDROOT:+ (BUILDROOT=$BUILDROOT no existe)}."
        echo "Busque (en orden):"
        _x6200_candidatos | sed 's/^/  /'
        echo "Clonalo (o un symlink) en $X6200_CACHE_DIR/AetherX6200Buildroot, o exporta BUILDROOT=<clon>."
    } >&2
    return 1
}
