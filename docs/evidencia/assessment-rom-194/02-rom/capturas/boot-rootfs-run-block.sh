set -euo pipefail

# El build invoca el script con el rootfs destino como $1:
#   install(CODE ...)  ->  sh <script> "$ENV{DESTDIR}"   (CMakeLists.txt)
# El test lo llama IGUAL: `sh <script> <rootfs>`. PORT_ROOTFS es el
# rootfs/ del port (el guard de "no borrar lo propio" lo lee).
SCRIPT=buildroot/prune-vendor-rootfs.sh
PORT_ROOTFS="$PWD/rootfs"

echo "== 1) sintaxis =="
bash -n "$SCRIPT"
echo "ok: $SCRIPT parsea"

echo "== 2) rootfs de prueba =="
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/etc/init.d"
printf 'S20gui!!\n'   > "$TMP/etc/init.d/S20gui"    # 9 bytes, del port
printf 'vendor-dup\n' > "$TMP/etc/init.d/S95gui"    # el duplicado del vendor
printf 'vendor-ok\n'  > "$TMP/etc/init.d/S99vendor" # otro init del vendor: NO se toca
ls -l "$TMP/etc/init.d/"

echo "== 3) prune (como lo llama el build: sh <script> <rootfs>) =="
sh "$SCRIPT" "$TMP"

echo "== 4) asserts before/after =="
[ -f "$TMP/etc/init.d/S20gui" ]    || { echo "FAIL: S20gui desaparecio"; exit 1; }
[ ! -e "$TMP/etc/init.d/S95gui" ]  || { echo "FAIL: S95gui sigue en el rootfs"; exit 1; }
[ -f "$TMP/etc/init.d/S99vendor" ] || { echo "FAIL: S99vendor desaparecio (la lista no es explicita)"; exit 1; }
echo "ok: S20gui queda, S95gui no, S99vendor queda (lista explicita, no patrones)"

echo "== 5) guards fail-closed (rc != 0 y NO borrar nada) =="
guard_must_fail() {
    local label="$1" bad="$2"
    local dir badscript rc=0
    dir=$(mktemp -d)
    mkdir -p "$dir/etc/init.d"
    printf 'S20gui!!\n'   > "$dir/etc/init.d/S20gui"
    printf 'vendor-dup\n' > "$dir/etc/init.d/S95gui"
    badscript=$(mktemp)
    sed "s#^etc/init.d/S95gui#${bad}#" "$SCRIPT" > "$badscript"
    PORT_ROOTFS="$PORT_ROOTFS" sh "$badscript" "$dir" >/dev/null 2>&1 || rc=$?
    if [ "$rc" -eq 0 ]; then
        echo "FAIL: el guard '$label' acepto '$bad' (rc=0)"; exit 1
    fi
    if [ ! -f "$dir/etc/init.d/S20gui" ] || [ ! -f "$dir/etc/init.d/S95gui" ]; then
        echo "FAIL: el guard '$label' borro algo antes de fallar"; exit 1
    fi
    echo "ok: '$label' rechazado (rc=$rc), no borro nada"
    rm -rf "$dir" "$badscript"
}
guard_must_fail "comodin"                  'etc/init.d/*gui'
guard_must_fail "path absoluto"            '/etc/init.d/S95gui'
guard_must_fail "traversal (..)"           '../etc/init.d/S95gui'
guard_must_fail "path que instala el port" 'etc/init.d/S20gui'

echo "== resumen =="
echo "ko: prune OK — S20gui queda, S95gui no, S99vendor queda; los 4 guards rechazados sin borrar"

if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
    {
        echo "## boot-rootfs — prune guards"
        echo ""
        echo "- \`bash -n\`: ok"
        echo "- prune: S20gui queda, S95gui no, S99vendor queda"
        echo "- guards fail-closed: comodin / absoluto / .. / path-del-port, rechazados sin borrar"
    } >> "$GITHUB_STEP_SUMMARY"
fi
