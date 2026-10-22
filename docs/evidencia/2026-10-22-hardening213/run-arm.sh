#!/bin/sh
#
# QA QEMU del binario ARM real para el issue #213 (evidencia).
#
#   docs/evidencia/2026-10-22-hardening213/run-arm.sh
#
# Mide el binario ARM del port (compilado de ESTE arbol por el build privado) en
# un chroot del rootfs del target, con /mnt montado de verdad en un contenedor
# PRIVADO por caso (tmpfs rw, bind RO, tmpfs llena, o directorio sin montar). No
# usa la radio, ni la SD del owner, ni el /mnt del host, ni volumenes/rootfs de
# otras corridas.
#
# Requiere: docker, la imagen x6200-buildroot, qemu-arm en binfmt_misc (el host ya
# lo tiene registrado), y el volumen PRIVADO del build de esta corrida con el
# target del rootfs.
#
#   VOL=h213-x6200-out  IMG=x6200-buildroot  WT=<worktree>  run-arm.sh
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -u

DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
WT=${WT:-/home/pablo/desa/asf/state/worktrees/x6200/hardening-213}
VOL=${VOL:-h213-x6200-out}
IMG=${IMG:-x6200-buildroot}
SANDBOX=${SANDBOX:-/tmp/opencode/h213-arm}
CHROOT="$SANDBOX/chroot"
FIX="$SANDBOX/fixtures"
EVID="$DIR"
ARM="$EVID/arm"

fail() { echo "FALLO: $*" >&2; exit 1; }

[ -f "$DIR/arm-inner.sh" ] || fail "no encuentro arm-inner.sh"
docker image inspect "$IMG" >/dev/null 2>&1 || fail "no existe la imagen $IMG"
docker volume inspect "$VOL" >/dev/null 2>&1 || fail "no existe el volumen $VOL"

mkdir -p "$ARM/logs" "$FIX" "$FIX/ro-mnt"

# --- 1. el binario del port esta en el volumen -------------------------------
docker run --rm --entrypoint "" -v "$VOL":/b:ro "$IMG" \
    sh -c 'test -x /b/target/usr/sbin/x6200_gui' || fail "no hay x6200_gui en $VOL/target"
docker run --rm --entrypoint "" -v "$VOL":/b:ro -v "$FIX":/f "$IMG" \
    sh -c 'cp /b/target/usr/sbin/x6200_gui /f/x6200_gui.arm && cp /b/target/usr/share/x6200/params.default.db /f/template.db'
echo "$(sha256sum "$FIX/x6200_gui.arm" | cut -d' ' -f1)  x6200_gui.arm" > "$ARM/SHA256SUMS"
if [ -x "$WT/tools/verifica-firmware-port.py" ]; then
    python3 "$WT/tools/verifica-firmware-port.py" "$FIX/x6200_gui.arm" || fail "el binario no es el firmware del port"
fi

# --- 2. chroot privado del rootfs del target ---------------------------------
# La copia del rootfs la hace el contenedor (root): los archivos del target son
# de root y desde el host no se pueden ni reescribir ni borrar.
docker run --rm --entrypoint "" -v "$SANDBOX":/s "$IMG" sh -c 'rm -rf /s/chroot; mkdir -p /s/chroot'
mkdir -p "$CHROOT"
docker run --rm --entrypoint "" -v "$VOL":/b:ro -v "$CHROOT":/t "$IMG" \
    sh -c 'cp -a /b/target/. /t/ && chmod 755 /t/usr/sbin/x6200_gui && mkdir -p /t/dev/shm /t/tmp /t/mnt'
# Libs del banco (ARM) dentro del chroot, tambien como root.
docker run --rm --entrypoint "" -v "$CHROOT":/t -v "$WT/tools/bench":/b:ro "$IMG" \
    sh -c 'cp /b/libfakefb.so /b/libcrash.so /b/libx6200trace.so /t/lib/' \
    || fail "no se pudieron copiar las libs del banco al chroot"

# Cliente de control remoto ARM (para el handshake de los casos que arrancan).
docker run --rm --entrypoint "" -v "$VOL":/b:ro -v "$WT":/w:ro -v "$CHROOT":/t "$IMG" \
    sh -c '/b/host/bin/arm-buildroot-linux-gnueabihf-gcc -O2 -o /t/tmp/x6200-remote /w/tools/remote/x6200-remote.c' \
    || fail "no se pudo compilar x6200-remote para ARM"

# --- 3. fixtures (bases armadas en el host a partir de la plantilla de la ROM) -
sqlite3 "$FIX/template.db" "SELECT 'plantilla ok', count(*) FROM sqlite_master" >/dev/null \
    || fail "la plantilla extraida no es una base sqlite"

fixture() {
    name=$1
    sql=$2
    rm -f "$FIX/params-$name.db"
    cp "$FIX/template.db" "$FIX/params-$name.db"
    [ -n "$sql" ] && sqlite3 "$FIX/params-$name.db" "$sql" >/dev/null
}

fixture valid ""
fixture no-bands "DROP TABLE bands;"
fixture no-version-row "DELETE FROM version;"
fixture v1-atu-index "UPDATE version SET id=1; DELETE FROM atu; INSERT INTO atu VALUES(1,3,100),(1,5,200);"
fixture v1-atu-mixed "UPDATE version SET id=1; DELETE FROM atu; INSERT INTO atu VALUES(1,3,100),(1,7025000,200);"
# El umbral fuera de rango se sanea al arrancar y queda dirty -> fuerza un guardado
# (que en el caso `full` tiene que fallar y reportarse). Ojo: la plantilla NO trae
# la clave (params.csv tiene cuatro filas), asi que hay que insertarla.
fixture threshold-invalid "INSERT OR REPLACE INTO params(name,val) VALUES('freq_accel_threshold',0);"

# Bytes de la base corrupta (el caso la escribe a mano en el chroot).
printf 'esto no es una base sqlite' > "$FIX/corrupt.expected"

# /mnt del caso RO: el bind sale de $FIX/ro-mnt y el fixture lo escribe el script
# interno (root, dentro del contenedor).

# --- 4. casos ----------------------------------------------------------------
FAILED=0

run_case() {
    case=$1
    docker run --rm --cap-add SYS_ADMIN --entrypoint "" \
        -v "$CHROOT":/t -v "$FIX":/f -v "$EVID":/e \
        "$IMG" sh /e/arm-inner.sh "$case" > "$ARM/$case.stdout" 2>&1
    rc=$?
    cat "$ARM/$case.stdout"
    if [ "$rc" -eq 0 ]; then
        printf '%-18s ok\n' "$case" | tee -a "$ARM/matrix.txt"
    else
        printf '%-18s FALLO (exit %s)\n' "$case" "$rc" | tee -a "$ARM/matrix.txt"
        FAILED=$((FAILED + 1))
    fi
}

: > "$ARM/matrix.txt"
{
    echo "== #213: QA QEMU del binario ARM real (chroot + /mnt real) =="
    echo "binario: $(cat "$ARM/SHA256SUMS")"
    echo "sello de fuentes del arbol medido: $(sh "$WT/tools/fuente-md5.sh" "$WT")"
    echo "volumen privado: $VOL   imagen: $IMG"
} | tee "$ARM/README.txt"

check_db() {
    case_name=$1
    sql=$2
    want=$3
    got=$(sqlite3 "$ARM/logs/$case_name.params.db" "$sql" 2>/dev/null)
    if [ "$got" = "$want" ]; then
        printf '    ok  %-14s %s = %s\n' "$case_name" "$sql" "$got" >> "$ARM/matrix.txt"
    else
        printf '    FALLO %-12s %s = %s (esperado %s)\n' "$case_name" "$sql" "$got" "$want" >> "$ARM/matrix.txt"
        FAILED=$((FAILED + 1))
    fi
}

run_case valid
run_case corrupt
run_case no-mount
run_case absent
run_case ro
run_case schema-incomplete
run_case version-missing
run_case v1-atu
run_case v1-atu-mixed
run_case full
run_case cycles
run_case reboot

# Presets/identificadores IDENTICOS a la plantilla de la ROM (contrato #213): cada
# fila de la plantilla tiene que seguir existiendo con el mismo valor en la base
# del caso. El arranque agrega filas (los savers del firmware persisten defaults
# de otras rutas), por eso se compara "no falta ninguna", no "mismos bytes".
presets_identicos() {
    case_name=$1
    got=$(sqlite3 "$ARM/logs/$case_name.params.db" "
        ATTACH '$FIX/template.db' AS t;
        SELECT (SELECT count(*) FROM t.params p WHERE NOT EXISTS (SELECT 1 FROM params m WHERE m.name=p.name AND m.val=p.val))
             + (SELECT count(*) FROM t.band_params p WHERE NOT EXISTS (SELECT 1 FROM band_params m WHERE m.bands_id=p.bands_id AND m.name=p.name AND m.val=p.val))
             + (SELECT count(*) FROM t.mode_params p WHERE NOT EXISTS (SELECT 1 FROM mode_params m WHERE m.mode=p.mode AND m.name=p.name AND m.val=p.val))
             + (SELECT count(*) FROM t.bands p WHERE NOT EXISTS (SELECT 1 FROM bands m WHERE m.id=p.id AND m.name=p.name AND m.start_freq=p.start_freq AND m.stop_freq=p.stop_freq))
             + (SELECT count(*) FROM t.digital_modes p WHERE NOT EXISTS (SELECT 1 FROM digital_modes m WHERE m.freq=p.freq AND m.type=p.type))
             + (CASE WHEN (SELECT id FROM version)=5 THEN 0 ELSE 1 END);" 2>/dev/null)
    if [ "$got" = 0 ]; then
        printf '    ok  %-14s presets/identificadores identicos a la plantilla\n' "$case_name" >> "$ARM/matrix.txt"
    else
        printf '    FALLO %-12s %s preset(s) distinto(s) de la plantilla\n' "$case_name" "${got:-?}" >> "$ARM/matrix.txt"
        FAILED=$((FAILED + 1))
    fi
}

# Verificacion de la base final de cada caso (la corrió el binario ARM) con el
# sqlite3 del host: es la evidencia de que los presets y el ATU quedaron como
# tienen que quedar (y de que la migracion 2 no convirtio dos veces).
check_db valid "SELECT id FROM version" 5
presets_identicos valid
check_db absent "SELECT id FROM version" 5
presets_identicos absent
check_db v1-atu "SELECT id FROM version" 5
check_db v1-atu "SELECT freq FROM atu WHERE ant=1 AND val=100" 175000
check_db v1-atu "SELECT freq FROM atu WHERE ant=1 AND val=200" 275000
check_db v1-atu-mixed "SELECT id FROM version" 1
check_db v1-atu-mixed "SELECT freq FROM atu WHERE ant=1 AND val=100" 3
check_db v1-atu-mixed "SELECT freq FROM atu WHERE ant=1 AND val=200" 7025000
check_db full "SELECT count(*) FROM params WHERE name='probe_full'" 0
check_db full "SELECT val FROM params WHERE name='freq_accel_threshold'" 0
check_db cycles "PRAGMA integrity_check" ok
check_db full "PRAGMA integrity_check" ok


if [ "$(sha256sum "$ARM/logs/corrupt.params.db" | cut -d' ' -f1)" = "$(sha256sum "$FIX/corrupt.expected" | cut -d' ' -f1)" ]; then
    printf '    ok  corrupt        la base corrupta quedo con sus bytes\n' >> "$ARM/matrix.txt"
else
    printf '    FALLO corrupt      la base corrupta cambio\n' >> "$ARM/matrix.txt"
    FAILED=$((FAILED + 1))
fi

# Nota (no asercion): sha256 de la base final vs la plantilla. La base sembrada no
# es byte a byte la plantilla porque el arranque agrega filas (los savers del
# firmware persisten defaults de otras rutas); los presets se comparan arriba.
printf '    nota absente/valid: sha256 aqui vs plantilla de la ROM:\n' >> "$ARM/matrix.txt"
for c in absent valid; do
    printf '      %-8s caso=%s plantilla=%s\n' "$c" \
        "$(sha256sum "$ARM/logs/$c.params.db" 2>/dev/null | cut -d' ' -f1)" \
        "$(sha256sum "$FIX/template.db" | cut -d' ' -f1)" >> "$ARM/matrix.txt"
done

echo "-- matriz" | tee -a "$ARM/matrix.txt"
cat "$ARM/matrix.txt"

[ "$FAILED" -eq 0 ] || { echo "FALLO: $FAILED caso(s) ARM" >&2; exit 1; }
echo "todos los casos ARM corrieron y afirmaron su resultado (logs crudos en $ARM/logs)"
