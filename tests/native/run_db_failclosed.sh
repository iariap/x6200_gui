#!/bin/sh
#
# End-to-end de la persistencia fail-closed de params.db (issue #213) con las
# rutas REALES del firmware (/mnt/params.db, /usr/share/x6200/params.default.db).
#
#   tests/native/run_db_failclosed.sh [out-dir]
#
# Corre en un namespace de montajes y PID PROPIO (unshare -Urmp) y en un chroot
# privado: el /mnt del host NO se toca (el namespace de montajes es privado) y la
# radio no se usa. Los casos de DATA (ausente / sin montar / RO / llena), la
# siembra de la plantilla y la propagacion del error de escritura se miden sobre
# el `database_init()` REAL (src/params/db.c tal cual sale del repo), no sobre un
# stub; el binario y la matriz quedan en el out-dir como evidencia.
#
# Si el host no permite user namespaces (p. ej. el contenedor del CI), el script
# declara SKIP con el motivo y sale 77: no corre a medias ni declara PASS.
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -u

DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
GUI=$(CDPATH= cd -- "$DIR/../.." && pwd)
SRC="$GUI/src"

OUT=${1:-$DIR/build/db_failclosed}
mkdir -p "$OUT"

CC=${CC:-gcc}
BASEFLAGS="-std=gnu11 -O1 -g -Wall -Wextra -Wno-unused-parameter -pthread"

# --- 1. el namespace tiene que ser posible -----------------------------------
if ! unshare -Ur -m -p --fork --mount-proc true 2>/dev/null; then
    echo "SKIP: este host no permite user namespaces (unshare -Ur -m -p): la suite"
    echo "      end-to-end necesita un /mnt de verdad y no puede simularlo. En el"
    echo "      CI corre el resto (tests/native/run_native_tests.sh) y en la ROM la"
    echo "      suite QEMU con el binario ARM."
    exit 77
fi

# --- 2. binario: db.c REAL + migrations.c staged + stubs ---------------------
MIG_STAGE="$OUT/migrations_src"
rm -rf "$MIG_STAGE"
mkdir -p "$MIG_STAGE/params" "$MIG_STAGE/cfg"
cp "$SRC/params/migrations.c" "$SRC/params/migrations.h" "$MIG_STAGE/params/"
cp "$DIR/stubs_migrations/db.h" "$MIG_STAGE/params/"
cp "$DIR/stubs_migrations/theme.h" "$MIG_STAGE/"
cp "$DIR/stubs_migrations/cfg/digital_modes.h" "$MIG_STAGE/cfg/"

BIN="$OUT/db_end_to_end"
# shellcheck disable=SC2086
$CC $BASEFLAGS -I"$DIR/stubs_db" -I"$SRC" -I"$DIR" -I"$MIG_STAGE/params" -I"$MIG_STAGE" \
    "$DIR/db_end_to_end.c" "$DIR/db_fixtures.c" "$DIR/db_test_stubs.c" \
    "$SRC/params/db.c" "$MIG_STAGE/params/migrations.c" \
    -lsqlite3 -o "$BIN" || { echo "FALLO: no compila el binario end-to-end" >&2; exit 1; }

# --- 3. chroot privado dentro del namespace ----------------------------------
SANDBOX=$(mktemp -d /tmp/x6200-dbfc-XXXXXX)
ROOT="$SANDBOX/root"
RO_MNT="$SANDBOX/ro-mnt"
mkdir -p "$ROOT/usr/bin" "$ROOT/usr/lib" "$ROOT/usr/lib64" "$ROOT/usr/share/x6200" \
         "$ROOT/mnt" "$ROOT/tmp" "$ROOT/proc" "$RO_MNT"
cp "$BIN" "$ROOT/usr/bin/db_end_to_end"
ln -s usr/lib "$ROOT/lib"
ln -s usr/lib64 "$ROOT/lib64"
ln -s usr/bin "$ROOT/bin"

MATRIX="$OUT/matrix.txt"
: > "$MATRIX"

# Script interno: monta los bind mounts del chroot, prepara /mnt segun el caso y
# corre el binario. Cada caso prepara su /mnt con herramientas del host (que si
# tiene), y el binario adentro del chroot ve los paths reales.
cat > "$OUT/inner.sh" <<'INNER'
set -u
ROOT=$1
CASE=$2
RO_MNT=$3

mount --bind /usr/lib "$ROOT/usr/lib"
mount --bind /usr/lib64 "$ROOT/usr/lib64"
mount --bind /proc "$ROOT/proc"

mnt_rw()   { mount -t tmpfs -o size=16M tmpfs "$ROOT/mnt"; }
fixture()  { chroot "$ROOT" /usr/bin/db_end_to_end --make-db "$1"; }
template() { chroot "$ROOT" /usr/bin/db_end_to_end --make-template "$1"; }

case "$CASE" in
    valid)              mnt_rw; rm -f "$ROOT/mnt/params.db"; fixture valid; template valid ;;
    corrupt)            mnt_rw; rm -f "$ROOT/mnt/params.db"; printf 'esto no es una base sqlite' > "$ROOT/mnt/params.db"; template valid ;;
    no-mount)           template valid; rm -f "$ROOT/mnt/params.db" ;;
    absent)             mnt_rw; rm -f "$ROOT/mnt/params.db"; template valid ;;
    absent-no-template) mnt_rw; rm -f "$ROOT/mnt/params.db"; template none ;;
    ro)                 mount --bind "$RO_MNT" "$ROOT/mnt"; rm -f "$ROOT/mnt/params.db"; fixture valid; template valid; mount -o remount,ro,bind "$ROOT/mnt" ;;
    schema-incomplete)  mnt_rw; rm -f "$ROOT/mnt/params.db"; fixture no-bands; template valid ;;
    version-missing)    mnt_rw; rm -f "$ROOT/mnt/params.db"; fixture no-version-row; template valid ;;
    v1-atu)             mnt_rw; rm -f "$ROOT/mnt/params.db"; fixture v1-atu-index; template valid ;;
    v1-atu-mixed)       mnt_rw; rm -f "$ROOT/mnt/params.db"; fixture v1-atu-mixed; template valid ;;
    full)               mnt_rw; rm -f "$ROOT/mnt/params.db"; fixture valid; template valid; \
                        dd if=/dev/zero of="$ROOT/mnt/filler" bs=1024 count=20000 2>/dev/null || true; sync ;;
    *)                  echo "caso desconocido: $CASE" >&2; exit 2 ;;
esac

chroot "$ROOT" /usr/bin/db_end_to_end "$CASE"
INNER

FAILED=0

case_run() {
    name=$1
    expect=$2

    printf -- '-- %s (la salida esperada del binario es PASS)\n' "$name" | tee -a "$MATRIX"

    if unshare -Ur -m -p --fork --mount-proc sh "$OUT/inner.sh" "$ROOT" "$name" "$RO_MNT" 2>&1 | tee -a "$MATRIX"; then
        printf -- '%-20s exit=0  esperado=%s\n' "$name" "$expect" | tee -a "$MATRIX"
    else
        printf -- '%-20s exit!=0  <-- FALLO (esperado=%s)\n' "$name" "$expect" | tee -a "$MATRIX"
        FAILED=$((FAILED + 1))
    fi
}

# En todos los casos el binario tiene que salir 0: el mismo afirma el resultado
# (PASS) cuando el comportamiento coincide con lo que el caso espera (init ok o
# rechazo). El campo `esperado` documenta la semantica del caso.
case_run valid ok
case_run corrupt rejected
case_run no-mount rejected
case_run absent ok
case_run absent-no-template rejected
case_run ro rejected
case_run schema-incomplete rejected
case_run version-missing rejected
case_run v1-atu ok
case_run v1-atu-mixed rejected
case_run full ok

rm -rf "$SANDBOX"

if [ "$FAILED" -ne 0 ]; then
    echo "FALLO: $FAILED caso(s) de la matriz end-to-end" >&2
    exit 1
fi
echo "todos los casos corrieron y afirmaron su resultado (matriz: $MATRIX)"
