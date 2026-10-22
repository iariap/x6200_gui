#!/bin/sh
#
# Caso del QA QEMU del binario ARM real (issue #213).
#
# Corre DENTRO del contenedor privado del build (imagen x6200-buildroot, con
# --cap-add SYS_ADMIN para poder montar un /mnt propio), con:
#   /t = chroot del rootfs del target (el binario que se mide)
#   /f = fixtures (bases preparadas en el host)
#   /e = evidencia (los logs crudos de este caso, y la base final copiada)
#
#   arm-inner.sh <caso>
#
# No toca la radio, ni la SD del owner, ni el /mnt del host: el /mnt que ve la GUI
# es un tmpfs (o un bind RO) de este contenedor. Sale 0 solo si el caso cumple lo
# que se espera de el.
#
# Casos: valid corrupt no-mount absent ro schema-incomplete version-missing v1-atu
#        v1-atu-mixed full cycles reboot
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -u

CASE=$1
BIN=/usr/sbin/x6200_gui
PRELOAD=/lib/libfakefb.so:/lib/libcrash.so:/lib/libx6200trace.so
LOGDIR=/e/arm/logs
OUT="$LOGDIR/$CASE.out"
LOG="$LOGDIR/$CASE.log"
FAILS=0
GUI=""

mkdir -p "$LOGDIR"

note() {
    echo "$*" >> "$LOG"
}

check() {
    if [ "$1" = 0 ]; then
        note "    ok: $2"
    else
        note "    FALLO: $2"
        FAILS=$((FAILS + 1))
    fi
}

sha() {
    sha256sum "$1" 2>/dev/null | cut -d' ' -f1
}

# --- arranque / parada de la GUI ---------------------------------------------
start_gui() {
    rm -f /t/tmp/x6200_remote.sock
    chroot /t /bin/busybox env LD_PRELOAD="$PRELOAD" QEMU_CPU=cortex-a7 "$BIN" >> "$OUT" 2>&1 &
    GUI=$!
    I=0
    while [ "$I" -lt 60 ] && [ ! -S /t/tmp/x6200_remote.sock ]; do
        sleep 1
        I=$((I + 1))
    done
    note "    socket de control remoto: I=${I}s"
    [ -S /t/tmp/x6200_remote.sock ]
}

stop_gui() {
    if [ -n "$GUI" ]; then
        kill "$GUI" 2>/dev/null
        wait "$GUI" 2>/dev/null
        GUI=""
    fi
}

estado() {
    # El socket es de nobody modo 600: el cliente se corre dentro del chroot.
    chroot /t /tmp/x6200-remote "$1"
}

# /proc DENTRO del chroot: la guarda de DATA lee /proc/self/mountinfo (sin esto
# la GUI aborta por "cannot read /proc/self/mountinfo" y los casos no medirian lo
# que tienen que medir).
mkdir -p /t/proc
mount --bind /proc /t/proc

# --- preparacion de /mnt y de la base ----------------------------------------
prepare_mnt() {
    case "$CASE" in
        no-mount)   : ;;                                   # /mnt es un directorio del rootfs
        ro)         mount --bind /f/ro-mnt /t/mnt ;;
        *)          mount -t tmpfs -o size=16M tmpfs /t/mnt ;;
    esac
}

prepare_db() {
    rm -f /t/mnt/params.db
    case "$CASE" in
        no-mount|absent)    : ;;
        corrupt)            printf 'esto no es una base sqlite' > /t/mnt/params.db ;;
        valid|ro|cycles|reboot) cp /f/params-valid.db /t/mnt/params.db ;;
        schema-incomplete)  cp /f/params-no-bands.db /t/mnt/params.db ;;
        version-missing)    cp /f/params-no-version-row.db /t/mnt/params.db ;;
        v1-atu)             cp /f/params-v1-atu-index.db /t/mnt/params.db ;;
        v1-atu-mixed)       cp /f/params-v1-atu-mixed.db /t/mnt/params.db ;;
        full)               cp /f/params-threshold-invalid.db /t/mnt/params.db ;;
    esac

    # DATA llena: se deja libre menos que el journal de la base (que pesa como la
    # base), para que la ESCRITURA falle, pero alcanza para los logs de arranque.
    if [ "$CASE" = full ]; then
        DBK=$(du -k /t/mnt/params.db | cut -f1)
        FILL=$((16 * 1024 - DBK - 16))
        dd if=/dev/zero of=/t/mnt/filler bs=1024 count="$FILL" >/dev/null 2>&1 || true
        sync
        note "    /mnt: base ${DBK}K + relleno ${FILL}K en un tmpfs de 16M (libre ~16K)"
    fi

    if [ "$CASE" = ro ]; then
        mount -o remount,ro,bind /t/mnt
        note "    /mnt remontado RO"
    fi
}

# --- corrida -----------------------------------------------------------------
DB=/t/mnt/params.db

prepare_mnt
mkdir -p /t/mnt/app_logs 2>/dev/null || true
: > "$OUT"          # el log de este caso arranca limpio (los casos se re-corren)
: > "$LOG"
rm -f /t/tmp/fakefb0 /t/tmp/x6200_remote.sock /t/crash.txt
prepare_db
BEFORE=$(sha "$DB")

case "$CASE" in
    valid)
        start_gui && check 0 "arranco (socket presente)"
        check "$(estado '{"cmd":"get"}' | grep -q '"t":"state"'; echo $?)" "contesta el handshake"
        sleep 6
        stop_gui
        ;;
    absent)
        start_gui && check 0 "arranco (socket presente)"
        check "$(estado '{"cmd":"get"}' | grep -q '"t":"state"'; echo $?)" "contesta el handshake"
        sleep 6
        stop_gui
        ;;
    v1-atu)
        start_gui && check 0 "arranco despues de migrar (socket presente)"
        check "$(estado '{"cmd":"get"}' | grep -q '"t":"state"'; echo $?)" "contesta el handshake"
        sleep 6
        stop_gui
        ;;
    v1-atu-mixed|corrupt|ro|schema-incomplete|version-missing|no-mount)
        chroot /t /bin/busybox env LD_PRELOAD="$PRELOAD" QEMU_CPU=cortex-a7 "$BIN" > "$OUT" 2>&1
        RC=$?
        note "    exit=$RC"
        check "$([ "$RC" -eq 78 ]; echo $?)" "exit 78 (EX_CONFIG)"
        ;;
    full)
        start_gui && check 0 "arranco (socket presente)"
        # El umbral fuera de rango quedo dirty al arrancar: el saver intenta
        # guardarlo a los ~3s y la escritura tiene que fallar y reportarse.
        sleep 10
        stop_gui
        ;;
    cycles)
        # 100 ordenes de frecuencia por el socket: la GUI tiene que seguir viva,
        # contestando y con la base integra.
        start_gui && check 0 "arranco (socket presente)"
        I=1
        while [ "$I" -le 100 ]; do
            estado "{\"cmd\":\"freq\",\"val\":$((7000000 + I * 1000))}" >/dev/null 2>&1
            I=$((I + 1))
        done
        check "$(estado '{"cmd":"get"}' | grep -q '"t":"state"'; echo $?)" "sigue contestando despues de 100 ciclos"
        check "$(kill -0 "$GUI" 2>/dev/null; echo $?)" "el proceso sigue vivo"
        sleep 6
        stop_gui
        ;;
    reboot)
        # Dos arranques: el segundo tiene que conservar lo que guardo el primero.
        start_gui && check 0 "primer arranque"
        estado '{"cmd":"freq","val":14074000}' >/dev/null
        estado '{"cmd":"filter","val":500}' >/dev/null
        sleep 1
        estado '{"cmd":"get"}' > "$LOGDIR/reboot-1.state" 2>&1
        sleep 8
        stop_gui
        start_gui && check 0 "segundo arranque"
        sleep 1
        estado '{"cmd":"get"}' > "$LOGDIR/reboot-2.state" 2>&1
        stop_gui

        F1=$(sed -n 's/.*"freq":\([0-9]*\).*/\1/p' "$LOGDIR/reboot-1.state" | head -1)
        F2=$(sed -n 's/.*"freq":\([0-9]*\).*/\1/p' "$LOGDIR/reboot-2.state" | head -1)
        X1=$(sed -n 's/.*"filter":\([0-9]*\).*/\1/p' "$LOGDIR/reboot-1.state" | head -1)
        X2=$(sed -n 's/.*"filter":\([0-9]*\).*/\1/p' "$LOGDIR/reboot-2.state" | head -1)
        note "    arranque 1: freq=$F1 filtro=$X1"
        note "    arranque 2: freq=$F2 filtro=$X2"
        check "$([ -n "$F1" ] && [ "$F1" = "$F2" ]; echo $?)" "la frecuencia se conserva entre arranques"
        check "$([ -n "$X1" ] && [ "$X1" = "$X2" ]; echo $?)" "el filtro se conserva entre arranques"
        ;;
esac

AFTER=$(sha "$DB")

# --- afirmaciones comunes ----------------------------------------------------
# SIGSEGV: 0 en TODOS los casos (el defecto del assessment: PC 0x42e12c, exit 139).
check "$([ "$(grep -c 'CRASHLOG sig=11' "$OUT")" -eq 0 ]; echo $?)" "sin CRASHLOG SIGSEGV"

case "$CASE" in
    valid)
        check "$(grep -q 'fuente:' /t/mnt/app_logs/boot.log; echo $?)" "boot.log tiene el sello de fuentes"
        check "$(grep -q 'radio' /t/mnt/app_logs/boot.log; echo $?)" "boot.log llega hasta radio"
        ;;
    absent)
        check "$([ -f "$DB" ]; echo $?)" "se creo la base"
        check "$(grep -q 'absent, seeding from /usr/share/x6200/params.default.db' "$OUT"; echo $?)" \
              "sembro la plantilla versionada (no creo un esquema vacio)"
        check "$([ "$(stat -c %a "$DB")" = 644 ]; echo $?)" "modo 0644"
        ;;
    corrupt|ro|schema-incomplete|version-missing|v1-atu-mixed)
        check "$([ "$BEFORE" = "$AFTER" ]; echo $?)" "la base quedo intacta (mismos bytes)"
        check "$(grep -q 'Persistence unavailable: check DATA and params.db' "$OUT"; echo $?)" \
              "el mensaje de persistencia esta en la salida"
        check "$([ ! -S /t/tmp/x6200_remote.sock ]; echo $?)" "no llego a inicializar el control remoto"
        # El motivo EXACTO del rechazo, para que un caso no pase por el motivo de otro.
        case "$CASE" in
            corrupt)            motivo='params.db: invalid database' ;;
            ro)                 motivo='is mounted read-only' ;;
            schema-incomplete)  motivo='incomplete schema' ;;
            version-missing)    motivo='The version table has no row' ;;
            v1-atu-mixed)       motivo='ambiguous provenance, refusing to convert' ;;
        esac
        check "$(grep -q "$motivo" "$OUT"; echo $?)" "el motivo del rechazo es el esperado ($motivo)"
        # No se siguio hacia la radio: el boot.log no tiene la marca (solo aplica
        # donde /mnt es escribible; en RO y sin montar no hay log).
        if [ "$CASE" != ro ]; then
            check "$(grep -q 'radio' /t/mnt/app_logs/boot.log 2>/dev/null; [ $? -ne 0 ] && echo 0 || echo 1)" \
                  "NO se inicializo la radio (boot.log sin 'radio')"
        fi
        ;;
    no-mount)
        check "$([ ! -f "$DB" ]; echo $?)" "no se creo /mnt/params.db sobre el rootfs"
        check "$(grep -q 'Persistence unavailable: check DATA and params.db' "$OUT"; echo $?)" \
              "el mensaje de persistencia esta en la salida"
        check "$(grep -q 'is not a mount point (DATA is not mounted)' "$OUT"; echo $?)" \
              "el motivo es DATA sin montar"
        ;;
    v1-atu)
        check "$(grep -q 'Apply migration: 2' "$OUT"; echo $?)" "corrio la migracion 2 (ATU)"
        check "$([ "$AFTER" != "$BEFORE" ]; echo $?)" "la migracion escribio (version y ATU)"
        ;;
    full)
        # No se afirma "la base no cambio": en un tmpfs lleno algunas escrituras
        # (las chicas, cuyo journal entra) pueden pasar y otras fallar; lo que el
        # contrato exige es que el fallo se DETECTE y se reporte. La integridad se
        # verifica aparte (host, PRAGMA integrity_check).
        check "$(grep -q 'Persistence write failed' "$OUT"; echo $?)" "la escritura fallida se reporto"
        ;;
esac

# La base final sale del tmpfs (que solo existe en este contenedor) a la
# evidencia, para poder verificarla en el host con sqlite3.
if [ -f "$DB" ]; then
    cp "$DB" "$LOGDIR/$CASE.params.db"
    note "    sha256 base: $BEFORE -> $AFTER"
fi
cp /t/usr/share/x6200/params.default.db "$LOGDIR/template.db" 2>/dev/null || true

echo "case=$CASE fails=$FAILS"
[ "$FAILS" -eq 0 ] || exit 1
exit 0
