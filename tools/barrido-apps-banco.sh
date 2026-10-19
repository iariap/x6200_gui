#!/bin/bash
#
# Barrido de las 23 apps del banco: abre el banco UNA vez y maneja todas las apps contra la
# misma GUI por el socket de control, con permanencia (dwell) configurable, verificacion de que
# sigue viva y captura por app. Escribe un JSON con el resultado y las capturas/logs.
#
#   CHROOT=../.vfix3 tools/barrido-apps-banco.sh
#   CHROOT=../.vfix3 DWELL=20 tools/barrido-apps-banco.sh
#   DWELL=20 REINTENTAR=si tools/barrido-apps-banco.sh          # re-corre una vez lo que muera
#
# Cierra el pendiente "captura-banco.sh reinicia la GUI por cada llamada": el banco se levanta
# (y se reconstruye si hace falta) una sola vez con captura-banco.sh, y de ahi en mas las ordenes
# van por `docker exec` al contenedor que ya esta corriendo (`docker exec` no arranca nada).
#
# Garantias de evidencia (que el resultado no sea falso):
#   - Una app esta "viva" solo si: el contenedor sigue arriba, el socket contesta el handshake
#     `{"cmd":"get"}` y el framebuffer dibujo algo no uniforme. Ver tools/barrido_apps.py.
#   - El handshake se reintenta 3 veces antes de declarar muerta a la GUI (no confunde un socket
#     ocupado con un crash) y, si el contenedor no esta, es muerta sin discusion.
#   - El crash se corrobora por los dos lados: el proceso (contenedor/socket) y el log de la GUI
#     (`CRASHLOG` que escribe el handler de src/main.c). Los dos quedan en el JSON.
#   - La captura se toma con la GUI ya verificada viva y ANTES de cerrar la app: una captura de
#     una GUI muerta no se guarda como "viva".
#   - REINTENTAR=si re-corre una vez cada app que fallo: 2/2 es repro firme; 1/2 se ve en el JSON.
#
# Variables: CHROOT, IMAGEN, VOLUMEN, BUILDROOT, QEMU_CPU, RECONSTRUIR, DWELL, REINTENTAR, APPS,
# SALIDA (directorio de la evidencia), ESPERA_FB.
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -euo pipefail

AQUI=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$AQUI/.." && pwd)
DATOS="$AQUI/barrido_apps.py"

SALIDA=${SALIDA:-$REPO/tools/ab_testing/reports/BARRIDO-APPS-19}
DWELL=${DWELL:-20}
REINTENTAR=${REINTENTAR:-si}
CHROOT=${CHROOT:-$REPO/../.vfix3}
IMAGEN=${IMAGEN:-x6200-buildroot}
VOLUMEN=${VOLUMEN:-x6200-build-output}
BUILDROOT=${BUILDROOT:-$REPO/../buildroot_new/AetherX6200Buildroot}
QEMU_CPU=${QEMU_CPU:-cortex-a7}
RECONSTRUIR=${RECONSTRUIR:-auto}
# Las 24 apps del menu (src/remote_server.cpp app_names[] sin "off"): las 23 apps con boton en las
# paginas APP (21 make_app_btn + QTH/Callsign) mas el alias FT4, que reusa la accion de FT8.
APPS=${APPS:-"FT8 FT4 JS8 WSPR SSTV WeFax NavTex RTTY CW Field GPS Bands Settings BT Cluster CwTrainer Eq Status QTH Callsign Recorder WiFi SWRScan Propagation"}

BIN=/usr/sbin/x6200_gui
LOG=$CHROOT/out-banco.txt
FB=$CHROOT/tmp/fakefb0

CAPTURAS=$SALIDA/capturas
LOGS=$SALIDA/logs
TRABAJO=$(mktemp -d)
JSONL=$TRABAJO/intentos.jsonl
OBS=$TRABAJO/obs.txt
trap 'rm -rf "$TRABAJO"' EXIT

fallar() { echo "ERROR: $*" >&2; exit 1; }
nota()   { echo "  $*" >&2; }

# El cliente de control corre DENTRO del contenedor (el socket es de nobody modo 600): `docker
# exec` reusa la GUI ya arrancada en vez de levantar un contenedor por orden.
orden() { docker exec banco-apps chroot /t /tmp/x6200-remote "$1" 2>&1; }
vivo()  { docker ps --filter name=banco-apps --format '{{.Names}}' | grep -qx banco-apps; }
n_lineas() { wc -l < "$1" 2>/dev/null || echo 0; }

# Handshake con reintentos: el socket correcto es el que contesta {"t":"state"}.
handshake() {
    local i resp
    for i in 1 2 3; do
        resp=$(orden '{"cmd":"get"}' || true)
        if [[ "$resp" == *'"t":"state"'* ]]; then printf 'si'; return 0; fi
        sleep 1
    done
    printf 'no'
}

# La app que el estado dice que esta abierta ("" si no contesta o no hay ninguna). Es lo que
# convierte "la pantalla dibujo algo" en "abrio la app que pedi": la pantalla de inicio tambien
# dibuja, asi que sin esto una app que nunca abre se veria viva.
app_actual() {
    local resp
    resp=$(orden '{"cmd":"get"}' || true)
    printf '%s' "$resp" | sed -n 's/.*"app":"\([^"]*\)".*/\1/p'
}

# Deja el banco en la ultima version y con la GUI viva. Delega en captura-banco.sh, que ya
# garantiza el build fresco, el socket correcto y el handshake (y no reinicia si ya esta viva).
arrancar_banco() {
    nota "banco: reconstruyendo/verificando y arrancando (captura-banco.sh)"
    RECONSTRUIR="$RECONSTRUIR" CHROOT="$CHROOT" IMAGEN="$IMAGEN" VOLUMEN="$VOLUMEN" \
        BUILDROOT="$BUILDROOT" QEMU_CPU="$QEMU_CPU" ESPERA=1 \
        "$AQUI/captura-banco.sh" "$TRABAJO/base.png" '{"cmd":"app","text":"off"}' >&2 \
        || fallar "captura-banco.sh no pudo dejar el banco listo (ver arriba)"
}

# Corre una app: abre, espera el dwell, verifica viva y captura; escribe un intento al JSONL.
# Devuelve 0 si la app quedo viva, 1 si no.
correr_app() {
    local app=$1 intento=$2
    local abre contenedor hs app_estado colores segundos inicio fin l0 l1 c0 c1 crash crash_senal resultado json falla

    nota "[intento $intento] $app: abriendo"
    orden '{"cmd":"app","text":"off"}' >/dev/null 2>&1 || true      # nada abierto, de arranque

    l0=$(n_lineas "$LOG")
    c0=$(grep -c CRASHLOG "$LOG" 2>/dev/null || true)
    inicio=$(date +%s.%N)
    abre=$(orden "{\"cmd\":\"app\",\"text\":\"$app\"}" || true)
    sleep "$DWELL"
    fin=$(date +%s.%N)
    segundos=$(awk -v a="$inicio" -v b="$fin" 'BEGIN{printf "%.1f", b-a}')
    abre=${abre//$'\n'/ }

    if vivo; then contenedor=si; else contenedor=no; fi
    hs=$(handshake)
    app_estado=$(app_actual)

    colores=$(python3 "$DATOS" captura "$FB" "$CAPTURAS/$app-$intento.png" 2>/dev/null \
        | sed -n 's/^colores=\([0-9]*\).*/\1/p' || true)
    colores=${colores:-0}

    l1=$(n_lineas "$LOG")
    c1=$(grep -c CRASHLOG "$LOG" 2>/dev/null || true)
    crash=no
    crash_senal=""
    if [ "${c1:-0}" -gt "${c0:-0}" ]; then
        crash=si
        crash_senal=$(grep CRASHLOG "$LOG" | tail -1)
    fi

    orden '{"cmd":"app","text":"off"}' >/dev/null 2>&1 || true      # cierra (vuelve al inicio)

    # El log de la GUI durante el dwell (sin el ruido del fondo), como evidencia por app.
    tail -n +"$((l0 + 1))" "$LOG" 2>/dev/null \
        | grep -v "draw_bg_img: Couldn't read the background image" | tail -200 \
        > "$LOGS/$app-$intento.txt" || true

    {
        echo "app=$app"
        echo "app_estado=$app_estado"
        echo "intento=$intento"
        echo "dwell_s=$DWELL"
        echo "segundos=$segundos"
        echo "abre=$abre"
        echo "contenedor=$contenedor"
        echo "handshake=$hs"
        echo "colores=$colores"
        echo "crash=$crash"
        echo "crash_senal=$crash_senal"
        echo "log_lineas=$((l1 - l0))"
        echo "captura=capturas/$app-$intento.png"
        echo "log_archivo=logs/$app-$intento.txt"
    } > "$OBS"

    falla=0
    if json=$(python3 "$DATOS" clasificar "$OBS"); then falla=0; else falla=1; fi
    printf '%s\n' "$json" >> "$JSONL"
    resultado=$(printf '%s' "$json" | sed -n 's/.*"resultado": "\([a-z-]*\)".*/\1/p')
    nota "[intento $intento] $app: $resultado (app_estado=${app_estado:-vacio}, colores=$colores, handshake=$hs, contenedor=$contenedor, crash=$crash)"
    return "$falla"
}

if [ "${1:-}" = "--check" ]; then
    CHROOT="$CHROOT" IMAGEN="$IMAGEN" VOLUMEN="$VOLUMEN" BUILDROOT="$BUILDROOT" \
        "$AQUI/captura-banco.sh" --check
    exit 0
fi

command -v python3 >/dev/null || fallar "falta python3 (para tools/barrido_apps.py)"
[ -f "$DATOS" ] || fallar "falta $DATOS"

mkdir -p "$CAPTURAS" "$LOGS"
: > "$JSONL"

arrancar_banco

echo "== barrido de apps ($(echo "$APPS" | wc -w) apps, dwell ${DWELL}s) ==" >&2
FALLADAS=()

for app in $APPS; do
    if correr_app "$app" 1; then :; else FALLADAS+=("$app"); fi
done

# Reintento: una app que fallo se corre otra vez (tras re-verificar el banco) para separar un
# crash reproducible de un hipo. El JSON guarda los dos intentos.
if [ "${#FALLADAS[@]}" -gt 0 ] && [ "$REINTENTAR" = "si" ]; then
    echo "== reintento de las falladas: ${FALLADAS[*]} ==" >&2
    arrancar_banco
    for app in "${FALLADAS[@]}"; do
        if correr_app "$app" 2; then :; fi
    done
fi

python3 "$DATOS" cerrar "$JSONL" "$SALIDA/barrido.json" \
    --meta "commit=$(git -C "$REPO" rev-parse HEAD 2>/dev/null || echo desconcido)" \
    --meta "commit_corto=$(git -C "$REPO" log -1 --format='%h %s' 2>/dev/null || echo desconcido)" \
    --meta "binario_md5=$(md5sum "$CHROOT$BIN" 2>/dev/null | cut -c1-12 || echo desconcido)" \
    --meta "dwell_s=$DWELL" \
    --meta "qemu_cpu=$QEMU_CPU" \
    --meta "imagen=$IMAGEN" \
    --meta "volumen=$VOLUMEN" \
    --meta "apps_solicitadas=$(echo "$APPS" | wc -w)" \
    --meta "reintentar=$REINTENTAR"

echo "== evidencia en $SALIDA/barrido.json ==" >&2
