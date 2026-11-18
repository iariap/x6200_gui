# Lo sourcea build.sh desde buildroot/. El arbol de AetherX6200Buildroot sale del entorno comun
# (tools/x6200-env.sh): la misma respuesta desde el checkout base y desde cualquier worktree.
REPO=$(cd .. && pwd)
. ../tools/x6200-env.sh
x6200_requiere_buildroot || exit 1
BUILDROOT_OUT="$BUILDROOT/build"
