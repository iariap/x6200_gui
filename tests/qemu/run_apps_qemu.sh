#!/bin/bash
#
# Barrido QEMU de las 25 apps reales con recursos PRIVADOS (issue #222, epica #216).
#
#   tests/qemu/run_apps_qemu.sh --check          # preflight: dice que falta, no toca nada
#   tests/qemu/run_apps_qemu.sh                  # 3 rondas x 25 apps (contrato del issue)
#   RONDAS=1 DWELL=2 tests/qemu/run_apps_qemu.sh # corrida corta de humo
#
# Corre el binario ARM REAL de la GUI (el de la rootfs construida, no un sustituto)
# dentro de un chroot propio con el framebuffer falso (libfakefb) y lo comanda por su
# socket de control, igual que `tools/captura-banco.sh` / `tools/barrido-apps-banco.sh`
# del owner. NO reimplementa el harness: reusa el binario, `libfakefb`, `x6200-remote`,
# el socket y las primitivas de captura del banco. Lo que cambia es el aislamiento:
#
#   - contenedor, volumen de build y laboratorio PROPIOS de esta corrida
#     (CONT/VOL/LAB), nunca `banco-apps`, `banco-audio` ni `x6200-build-output`;
#   - los scripts del banco del owner tienen nombre fijo de contenedor y volumen
#     (`docker exec banco-apps`, `-v x6200-build-output:/b`), asi que aca se ejecutan
#     sus PRIMITIVAS compatibles en copia privada, no los scripts tal cual;
#   - el guard #86 (`tools/verifica-firmware-port.py`) corre sobre el binario copiado
#     antes de arrancar nada: un build que no es el firmware del port no se corre.
#
# Capa: QEMU_USER (userland armv7 por binfmt_misc/qemu-arm, kernel del host). No hay
# kernel/init real, no hay radio, no hay RF, no se toca la SD del owner. Un PASS de
# este barrido NO es un PASS del dispositivo: GPS/red/Bluetooth quedan PARTIAL.
#
# Contrato que implementa (issue #222 §2): por app, `get` del estado inicial,
# `app <nombre>`, confirmacion de que la app quedo abierta, PNG real 800x480,
# permanencia (DWELL), `app off`, confirmacion de la home, y estabilidad de PID,
# boot_id, logrev, freq/mode/band y de la cuenta de fds/hilos. Una corrida fallida no
# se descarta ni se repite para obtener verde: las 3 rondas quedan en el JSON.
#
# Variables: WT, LAB, SALIDA, CONT, VOL, IMAGEN, BUILDROOT, ROOTFS, QEMU_CPU, RONDAS,
#            DWELL, APPS, CONSTRUIR, ESPERA_SOCKET.
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -u

AQUI=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
WT=${WT:-$(CDPATH= cd -- "$AQUI/../.." && pwd)}
DATOS="$AQUI/barrido_qemu.py"

LAB=${LAB:-/tmp/x6200-next222-qemu}
SALIDA=${SALIDA:-$LAB/out}
CONT=${CONT:-x6200-next222-qemu}
VOL=${VOL:-x6200-next222-build}
IMAGEN=${IMAGEN:-x6200-buildroot}
BUILDROOT=${BUILDROOT:-}
ROOTFS=${ROOTFS:-}
QEMU_CPU=${QEMU_CPU:-cortex-a7}
RONDAS=${RONDAS:-3}
DWELL=${DWELL:-2}
CONSTRUIR=${CONSTRUIR:-no}
ESPERA_SOCKET=${ESPERA_SOCKET:-90}
BIN=/usr/sbin/x6200_gui
CHROOT=$LAB/chroot
FB=$CHROOT/tmp/fakefb0
LOG=$CHROOT/out-banco.txt
TECHO_CLIENTE=${TECHO_CLIENTE:-5}   # techo de un paso del barrido, en segundos (issue #222 §2)

APPS_DEFECTO="FT8 FT4 JS8 WSPR SSTV WeFax NavTex RTTY CW Field GPS Bands Settings BT Cluster CwTrainer Eq Status QTH Callsign Recorder WiFi SWRScan Propagation DFLEditor"
APPS=${APPS:-$APPS_DEFECTO}

MODO_CHECK=no
[ "${1:-}" = "--check" ] && MODO_CHECK=si

FALTAN=0
falta()  { echo "  FALTA: $*" >&2; FALTAN=$((FALTAN + 1)); }
nota()   { echo "  $*" >&2; }
fallar() { echo "ERROR: $*" >&2; exit 1; }
docker_ok() { docker "$1" >/dev/null 2>&1; }
en_volumen() { docker run --rm --entrypoint "" -v "$VOL":/b "$IMAGEN" sh -c "$1" 2>/dev/null; }

# --- 0. el aislamiento es una restriccion, no un comentario --------------------------------
# La politica de laboratorio de la epica prohibe tocar recursos compartidos: si alguien
# apunta CONT/VOL/LAB a los del owner, el script se frena antes de arrancar nada.
aislamiento() {
    local c v
    for c in banco-apps banco-audio; do
        [ "$CONT" = "$c" ] && fallar "CONT=$CONT es un contenedor compartido del owner: usá un nombre privado"
    done
    for v in x6200-build-output x6200-build-cache; do
        [ "$VOL" = "$v" ] && fallar "VOL=$VOL es un volumen compartido del owner: usá un volumen privado"
    done
    case "$LAB" in
        */state/worktrees/x6200/*) fallar "LAB=$LAB cae en un worktree de otra corrida: usá un directorio privado fuera de los worktrees" ;;
    esac
    case "$CONT" in
        x6200-*) : ;;
        *) fallar "CONT=$CONT no es un nombre privado de esta app (tiene que empezar con x6200-)" ;;
    esac
}

# --- 1. preflight --------------------------------------------------------------------------
comprobar() {
    echo "== comprobando el entorno del barrido QEMU (privado) ==" >&2
    docker_ok info                    && nota "docker: ok"                      || falta "docker no responde"
    docker image inspect "$IMAGEN" >/dev/null 2>&1 && nota "imagen $IMAGEN: ok"  || falta "falta la imagen $IMAGEN"
    command -v python3 >/dev/null     && nota "python3: ok"                      || falta "falta python3"
    python3 -c "import PIL" 2>/dev/null && nota "PIL: ok"                        || falta "falta Pillow (pip install pillow)"
    [ -f "$DATOS" ]                   && nota "reporteador: ok"                  || falta "falta $DATOS"
    [ -f "$WT/tools/verifica-firmware-port.py" ] && nota "guard #86: ok"         || falta "falta tools/verifica-firmware-port.py"
    [ -f "$WT/tools/bench/libfakefb.so" ] && nota "libs del banco: ok"           || falta "faltan las libs del banco (tools/bench/compila.sh)"
    [ -f "$WT/tools/remote/x6200-remote.c" ] && nota "cliente de control (fuente): ok" \
        || falta "falta tools/remote/x6200-remote.c"

    if ! docker volume inspect "$VOL" >/dev/null 2>&1; then
        if [ "$CONSTRUIR" = "si" ] && [ -n "$BUILDROOT" ] && [ -d "$BUILDROOT" ]; then
            nota "volumen privado $VOL: no existe (CONSTRUIR=si: se crea y se construye; horas)"
        else
            falta "falta el volumen privado $VOL (un build de la GUI; CONSTRUIR=si + BUILDROOT=<clon> para construirlo)"
        fi
    elif [ "$(en_volumen "test -x /b/target$BIN && echo si")" = "si" ]; then
        nota "volumen $VOL: tiene la GUI construida"
    elif [ "$CONSTRUIR" = "si" ] && [ -n "$BUILDROOT" ] && [ -d "$BUILDROOT" ]; then
        nota "volumen $VOL: sin GUI construida (CONSTRUIR=si: se construye)"
    else
        falta "el volumen $VOL no tiene /b/target$BIN (CONSTRUIR=si + BUILDROOT=<clon>)"
    fi

    if [ -n "$ROOTFS" ]; then
        [ -f "$ROOTFS" ] && nota "rootfs privada: ok ($ROOTFS)" || falta "ROOTFS=$ROOTFS no existe"
    elif [ -x "$CHROOT$BIN" ] || [ -x "$CHROOT/bin/busybox" ] || [ -x "$CHROOT/bin/sh" ]; then
        nota "chroot privado: ok ($CHROOT)"
    elif [ "$(en_volumen "test -x /b/target$BIN && echo si")" = "si" ]; then
        # El chroot no esta armado todavia, pero el volumen tiene la GUI: `armar_chroot`
        # lo extrae de $VOL/target, que es exactamente lo que promete su mensaje. No es
        # un requisito faltante (antes se declaraba FALTA y el barrido salia 77 aunque
        # el propio script supiera armarlo).
        nota "chroot privado: se arma desde $VOL/target al arrancar"
    else
        falta "falta el chroot privado ($CHROOT): pasar ROOTFS=<rootfs.tar privada> o dejar el volumen con target/ (se arma solo)"
    fi

    if ls /proc/sys/fs/binfmt_misc/qemu-arm >/dev/null 2>&1; then
        nota "binfmt qemu-arm: ok"
    else
        falta "binfmt_misc/qemu-arm no está cargado (qemu-arm-static no puede ejecutar ARM)"
    fi

    if [ "$FALTAN" -gt 0 ]; then
        echo "== $FALTAN cosa(s) faltan: el barrido QEMU no puede correr ==" >&2
        echo "SKIP: barrido QEMU sin entorno privado ($FALTAN requisito(s) faltante(s))" >&2
        return 77
    fi
    echo "== todo listo para el barrido QEMU (no se tocó nada) ==" >&2
    echo "  contenedor privado: $CONT" >&2
    echo "  volumen privado:    $VOL" >&2
    echo "  laboratorio:        $LAB" >&2
    echo "  version del repo:   $(git -C "$WT" log -1 --format='%h %s' 2>/dev/null | cut -c1-60)" >&2
    return 0
}

aislamiento
if [ "$MODO_CHECK" = "si" ]; then
    comprobar
    exit $?
fi

comprobar || exit $?

# --- 2. el binario ARM real: se construye (si hace falta) y se copia al chroot --------------
mkdir -p "$LAB" "$SALIDA"

construir_gui() {
    # Primitiva del banco del owner (`tools/compila-gui.sh`), con el volumen PRIVADO:
    # ese script ya toma VOLUMEN y BUILDROOT del entorno, asi que se reusa tal cual.
    local antes despues
    antes=$(en_volumen "md5sum /b/target$BIN 2>/dev/null | cut -d' ' -f1")
    echo "== construyendo la GUI en el volumen privado $VOL ==" >&2
    VOLUMEN="$VOL" BUILDROOT="$BUILDROOT" RECONSTRUIR=auto sh "$WT/tools/compila-gui.sh" >&2 \
        || fallar "falló la construcción de la GUI (tools/compila-gui.sh con VOLUMEN=$VOL)"
    despues=$(en_volumen "md5sum /b/target$BIN 2>/dev/null | cut -d' ' -f1")
    [ -n "$despues" ] || fallar "el volumen $VOL no quedó con /b/target$BIN"
    nota "binario md5 ${despues:0:12}${antes:+ (antes ${antes:0:12})}"
}

construir_volumen() {
    # Volumen vacío: hay que configurar y construir TODO el árbol de Buildroot en el
    # volumen privado (toolchain + rootfs + GUI). Es el camino largo (horas): se hace
    # una sola vez por volumen y después se reusa. La receta es la del propio entorno
    # de build de la imagen (`/workspace/br_config.sh` + `make`), con O= al volumen
    # PRIVADO. Se verifica el resultado antes de seguir (fail-closed).
    if [ "$(en_volumen "test -x /b/host/bin/arm-buildroot-linux-gnueabihf-gcc && echo si")" = "si" ] \
       && [ "$(en_volumen "test -f /b/Makefile -o -f /b/build/Makefile && echo si")" = "si" ]; then
        nota "volumen $VOL: ya tiene el árbol de Buildroot configurado (no se reconstruye)"
        return 0
    fi
    [ -n "$BUILDROOT" ] && [ -d "$BUILDROOT" ] || fallar "CONSTRUIR=si necesita BUILDROOT=<clon de Buildroot>"
    echo "== configurando y construyendo Buildroot en el volumen privado $VOL (camino largo) ==" >&2
    docker run --rm --entrypoint "" \
        -v "$BUILDROOT":/workspace -v "$VOL":/workspace/build \
        -e FORCE_UNSAFE_CONFIGURE=1 -w /workspace "$IMAGEN" \
        sh -c 'sh br_config.sh && make -C buildroot BR2_EXTERNAL=../br2_external O=../build -j"$(nproc)"' \
        || fallar "falló el build de Buildroot en el volumen $VOL"
    [ "$(en_volumen "test -x /b/host/bin/arm-buildroot-linux-gnueabihf-gcc && echo si")" = "si" ] \
        || fallar "el volumen $VOL quedó sin toolchain (build incompleto)"
}

armar_chroot() {
    if [ -x "$CHROOT$BIN" ] && [ -d "$CHROOT/lib" ]; then
        nota "chroot ya armado ($CHROOT): se refresca el binario y las libs"
    else
        echo "== armando el chroot privado en $CHROOT ==" >&2
        rm -rf "$CHROOT"
        mkdir -p "$CHROOT"
        if [ -n "$ROOTFS" ]; then
            docker run --rm --entrypoint "" -v "$ROOTFS":/r:ro -v "$LAB":/lab "$IMAGEN" \
                sh -c 'tar -xf /r -C /lab/chroot' \
                || fallar "no pude extraer la rootfs privada ($ROOTFS)"
        else
            docker run --rm --entrypoint "" -v "$VOL":/b -v "$LAB":/lab "$IMAGEN" \
                sh -c 'tar -C /b/target -cf - . | tar -C /lab/chroot -xf -' \
                || fallar "no pude copiar la rootfs del volumen privado ($VOL/target)"
        fi
        [ -x "$CHROOT/bin/busybox" ] || [ -x "$CHROOT/bin/sh" ] || fallar "el chroot no parece una rootfs (sin /bin/busybox)"
    fi

    docker run --rm --entrypoint "" -v "$CHROOT":/t -v "$VOL":/b "$IMAGEN" \
        sh -c "cp /b/target$BIN /t$BIN" \
        || fallar "no pude copiar el binario desde el volumen $VOL"
    chmod 755 "$CHROOT$BIN" 2>/dev/null || true

    # Guard #86 fail-closed: el binario COPIADO tiene que ser el firmware del port.
    python3 "$WT/tools/verifica-firmware-port.py" "$CHROOT$BIN" \
        || fallar "el binario copiado no es el firmware del port (issue #86): NO se corre la GUI"

    cp "$WT/tools/bench/libfakefb.so" "$WT/tools/bench/libcrash.so" "$WT/tools/bench/libx6200trace.so" "$CHROOT/lib/" \
        || fallar "no pude copiar las libs del banco al chroot"
    mkdir -p "$CHROOT/mnt" "$CHROOT/dev/shm" "$CHROOT/tmp"

    # x6200-remote: primitiva de `tools/remote/compila.sh`, con el volumen privado
    # (ese script tiene `-v x6200-build-output:/b` fijo: no se puede reusar tal cual).
    docker run --rm --entrypoint "" -v "$WT":/w -v "$VOL":/b "$IMAGEN" sh -c '
        GCC=/b/host/bin/arm-buildroot-linux-gnueabihf-gcc
        [ -x "$GCC" ] || { echo "falta el toolchain de buildroot ($GCC)" >&2; exit 1; }
        "$GCC" -static -O2 -o /w/tools/remote/x6200-remote /w/tools/remote/x6200-remote.c
    ' || fallar "no pude compilar tools/remote/x6200-remote con el volumen $VOL"
    cp "$WT/tools/remote/x6200-remote" "$CHROOT/tmp/x6200-remote" || fallar "no pude copiar el cliente de control"
    chmod 755 "$CHROOT/tmp/x6200-remote"
}

if [ "$CONSTRUIR" = "si" ]; then
    construir_volumen
    construir_gui
fi
[ "$(en_volumen "test -x /b/target$BIN && echo si")" = "si" ] \
    || fallar "el volumen $VOL no tiene /b/target$BIN (CONSTRUIR=si + BUILDROOT=<clon> para construirlo)"
armar_chroot

# --- 3. arrancar el banco en un contenedor PRIVADO -----------------------------------------
cola_log() { [ -f "$LOG" ] && tail -5 "$LOG" | sed 's/^/    | /'; }

arrancar() {
    docker rm -f "$CONT" >/dev/null 2>&1 || true
    rm -f "$FB" "$CHROOT/crash.txt" "$CHROOT/tmp/x6200_remote.sock"
    # Montajes que el binario ENDURECIDO exige (issues #211-#215, r3): `src/params/db.c`
    # aborta con EXIT_PERSISTENCE_UNAVAILABLE (78) si /mnt no es un montaje real rw de un
    # filesystem distinto de la raiz, y `data_mount_ok()` lee `/proc/self/mountinfo`, asi que
    # /proc tiene que estar montado dentro del chroot. El contenedor NO trae /proc ni una DATA
    # real por defecto: se montan aca (receta canonica del QA de #213). El tmpfs de /mnt se
    # siembra con la plantilla de la ROM (`params.default.db`) para que la GUI arranque como en
    # la radio. Se aclara con `--cap-add SYS_ADMIN` porque `mount` lo necesita; el aislamiento
    # (contenedor, volumen, chroot, red y espacio de montaje PROPIOS) no cambia.
    docker run -d --name "$CONT" --rm --entrypoint "" --cap-add SYS_ADMIN -e "QEMU_CPU=$QEMU_CPU" -v "$CHROOT:/t" "$IMAGEN" \
        sh -c "mkdir -p /t/proc /t/mnt && mount --bind /proc /t/proc && mount -t tmpfs -o size=64M tmpfs /t/mnt && cp /t/usr/share/x6200/params.default.db /t/mnt/params.db && chroot /t /bin/busybox env LD_PRELOAD=/lib/libfakefb.so:/lib/libcrash.so:/lib/libx6200trace.so $BIN > /t/out-banco.txt 2>&1" >/dev/null \
        || fallar "no pude arrancar el contenedor privado $CONT"
    local i
    for i in $(seq 1 "$ESPERA_SOCKET"); do [ -S "$CHROOT/tmp/x6200_remote.sock" ] && break; sleep 1; done
    [ -S "$CHROOT/tmp/x6200_remote.sock" ] || { echo "la GUI no creó el socket en ${ESPERA_SOCKET}s:" >&2; cola_log >&2; return 1; }
}

vivo() { docker ps --filter "name=$CONT" --format '{{.Names}}' 2>/dev/null | grep -qx "$CONT"; }

# La orden va por `docker exec` al contenedor ya arriba (no levanta uno por orden). El
# cliente corre DENTRO del chroot, que es donde vive el socket (de nobody, modo 600).
orden() { timeout "$TECHO_CLIENTE" docker exec "$CONT" chroot /t /tmp/x6200-remote "$1" 2>&1; }

campo() {
    python3 -c '
import json, sys
try:
    datos = json.loads(sys.argv[1])
except Exception:
    datos = {}
print(datos.get(sys.argv[2], "") if isinstance(datos, dict) else "")
' "$1" "$2"
}

pid_gui() {
    docker exec "$CONT" sh -c 'for p in /proc/[0-9]*; do [ "$(cat $p/comm 2>/dev/null)" = x6200_gui ] && basename $p; done' 2>/dev/null | head -1
}
cuenta() { docker exec "$CONT" sh -c "ls /proc/$1/$2 2>/dev/null | wc -l" 2>/dev/null | tr -d ' '; }

handshake() {
    local i resp
    for i in 1 2 3; do
        resp=$(orden '{"cmd":"get"}' || true)
        if echo "$resp" | grep -q '"t":"state"'; then printf 'si'; return 0; fi
        sleep 1
    done
    printf 'no'
}

# Una app en una ronda. Escribe la observacion en $OBS y clasifica con el reporteador.
correr_app() {
    local app=$1 ronda=$2
    local obs="$LAB/obs.txt" t0 t1
    local st_antes st_despues hs pid0 pid1 fds0 fds1 hilos0 hilos1
    local crash0 crash1 segundos paso_lento=no colores=0
    local huella_antes huella_despues app_cerrada lineas_desde

    t0=$(date +%s.%N)
    hs=$(handshake)
    st_antes=$(orden '{"cmd":"get"}' 2>/dev/null || true)
    pid0=$(pid_gui)
    fds0=$(cuenta "$pid0" fd)
    hilos0=$(cuenta "$pid0" task)
    crash0=$(grep -c CRASHLOG "$LOG" 2>/dev/null || true)
    huella_antes=$(md5sum "$FB" 2>/dev/null | cut -d' ' -f1)

    # 1. abrir la app (el techo del cliente marca el timeout del paso)
    timeout "$TECHO_CLIENTE" docker exec "$CONT" chroot /t /tmp/x6200-remote \
        "{\"cmd\":\"app\",\"text\":\"$app\"}" >/dev/null 2>&1 || paso_lento=si
    sleep "$DWELL"

    # 2. estado + captura + estabilidad durante la permanencia
    st_despues=$(orden '{"cmd":"get"}' 2>/dev/null || true)
    colores=$(python3 "$DATOS" captura "$FB" "$SALIDA/capturas/$app-r$ronda.png" 2>/dev/null \
        | sed -n 's/^colores=\([0-9]*\).*/\1/p' || true)
    colores=${colores:-0}
    pid1=$(pid_gui)
    fds1=$(cuenta "$pid1" fd)
    hilos1=$(cuenta "$pid1" task)

    # 3. cerrar la app y confirmar la home (el estado dice si no quedo ninguna app abierta)
    orden '{"cmd":"app","text":"off"}' >/dev/null 2>&1 || true
    sleep 1
    app_cerrada=$(campo "$(orden '{"cmd":"get"}' 2>/dev/null || true)" app)
    huella_despues=$(md5sum "$FB" 2>/dev/null | cut -d' ' -f1)
    crash1=$(grep -c CRASHLOG "$LOG" 2>/dev/null || true)

    t1=$(date +%s.%N)
    segundos=$(python3 -c 'import sys; print("%.2f" % (float(sys.argv[2]) - float(sys.argv[1])))' "$t0" "$t1")

    # log de la GUI durante la app (evidencia por app, sin el ruido de fondo)
    lineas_desde=$(wc -l < "$LOG" 2>/dev/null || echo 0)
    lineas_desde=$(( lineas_desde > 200 ? lineas_desde - 200 : 1 ))
    tail -n +"$lineas_desde" "$LOG" 2>/dev/null \
        | grep -v "draw_bg_img: Couldn't read the background image" > "$SALIDA/logs/$app-r$ronda.txt" 2>/dev/null || true

    {
        echo "app=$app"
        echo "ronda=$ronda"
        echo "app_esperada=$app"
        echo "app_estado=$(campo "$st_despues" app)"
        echo "contenedor=$(vivo && echo si || echo no)"
        echo "handshake=$hs"
        echo "colores=$colores"
        echo "segundos=$segundos"
        echo "timeout=$paso_lento"
        echo "crash=$([ "${crash1:-0}" -gt "${crash0:-0}" ] && echo si || echo no)"
        echo "crash_senal=$(grep CRASHLOG "$LOG" 2>/dev/null | tail -1)"
        echo "sigsegv=$(grep -q SIGSEGV "$LOG" 2>/dev/null && echo si || echo no)"
        echo "tx=$(campo "$st_despues" tx)"
        echo "pid_antes=$pid0"
        echo "pid_despues=$pid1"
        echo "fds_crecidos=$(( ${fds1:-0} - ${fds0:-0} ))"
        echo "hilos_crecidos=$(( ${hilos1:-0} - ${hilos0:-0} ))"
        echo "did_antes=$(campo "$st_antes" did)"
        echo "did_despues=$(campo "$st_despues" did)"
        echo "logrev_antes=$(campo "$st_antes" logrev)"
        echo "logrev_despues=$(campo "$st_despues" logrev)"
        echo "freq_antes=$(campo "$st_antes" freq)"
        echo "freq_despues=$(campo "$st_despues" freq)"
        echo "mode_antes=$(campo "$st_antes" mode)"
        echo "mode_despues=$(campo "$st_despues" mode)"
        echo "band_antes=$(campo "$st_antes" band)"
        echo "band_despues=$(campo "$st_despues" band)"
        echo "home_px=$([ "$huella_antes" = "$huella_despues" ] && echo igual || echo distinto)"
        echo "app_cerrada=$app_cerrada"
        echo "captura=capturas/$app-r$ronda.png"
        echo "log_archivo=logs/$app-r$ronda.txt"
    } > "$obs"

    local json resultado
    json=$(python3 "$DATOS" clasificar "$obs") \
        || json='{"app":"'"$app"'","ronda":'"$ronda"',"resultado":"BLOCKED","motivo":"BLOCKED: el reporteador no pudo clasificar la observación"}'
    printf '%s\n' "$json" >> "$JSONL"
    resultado=$(campo "$json" resultado)
    nota "ronda $ronda  $app: $resultado"
    [ "$resultado" = "PASS" ] || nota "        $(campo "$json" motivo)"
}

# --- 4. el barrido -------------------------------------------------------------------------
mkdir -p "$SALIDA/capturas" "$SALIDA/logs"
JSONL=$SALIDA/intentos.jsonl
: > "$JSONL"

arrancar || fallar "el banco privado no arrancó"
trap 'docker rm -f "$CONT" >/dev/null 2>&1 || true' EXIT

echo "== barrido QEMU: $(echo "$APPS" | wc -w) apps x $RONDAS rondas, dwell ${DWELL}s ==" >&2
R=1
while [ "$R" -le "$RONDAS" ]; do
    echo "-- ronda $R/$RONDAS --" >&2
    for app in $APPS; do
        vivo || { nota "el banco se cayó antes de $app (ronda $R)"; arrancar || fallar "el banco privado no volvió a arrancar"; }
        correr_app "$app" "$R"
    done
    R=$((R + 1))
done

python3 "$DATOS" cerrar "$JSONL" "$SALIDA" \
    --meta "commit=$(git -C "$WT" rev-parse HEAD 2>/dev/null || echo desconocido)" \
    --meta "commit_corto=$(git -C "$WT" log -1 --format='%h %s' 2>/dev/null | cut -c1-60 || echo desconocido)" \
    --meta "fuente_md5=$(sh "$WT/tools/fuente-md5.sh" "$WT" 2>/dev/null || echo desconocido)" \
    --meta "binario_md5=$(md5sum "$CHROOT$BIN" 2>/dev/null | cut -c1-12 || echo desconocido)" \
    --meta "dwell_s=$DWELL" \
    --meta "rondas=$RONDAS" \
    --meta "qemu_cpu=$QEMU_CPU" \
    --meta "imagen=$IMAGEN" \
    --meta "contenedor=$CONT" \
    --meta "volumen=$VOL" \
    --meta "apps_solicitadas=$(echo "$APPS" | wc -w)"

echo "== evidencia en $SALIDA (barrido.json, results.csv, index.txt, capturas/, logs/) ==" >&2

# Sin barrido.json no hay corrida: no se puede declarar nada (fail-closed).
[ -f "$SALIDA/barrido.json" ] || fallar "el barrido no dejó $SALIDA/barrido.json"

# El exit del runner: 0 solo si no hay FAIL. PARTIAL/BLOCKED no son PASS pero tampoco
# se esconden: quedan en el JSON y el runner sale 1/77 para que no parezca verde.
if grep -q '"resultado": "FAIL"' "$SALIDA/barrido.json" 2>/dev/null; then
    exit 1
fi
if grep -q '"resultado": "BLOCKED"' "$SALIDA/barrido.json" 2>/dev/null; then
    echo "SKIP: hubo apps BLOCKED (el banco no midió esas rondas): ver $SALIDA/barrido.json" >&2
    exit 77
fi
exit 0
