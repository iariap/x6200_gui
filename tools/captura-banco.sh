#!/bin/bash
#
# Banco de pruebas SIN radio: corre el x6200_gui real (el binario ARM de la imagen) con
# qemu-arm-static, dentro de un chroot del rootfs y con framebuffer falso, y lo comanda por su
# socket de control. Saca una captura de la pantalla.
#
#   tools/captura-banco.sh --check                     # verifica todo y dice que falta (no toca nada)
#   CHROOT=../.vfix3 tools/captura-banco.sh salida.png
#   CHROOT=../.vfix3 tools/captura-banco.sh salida.png '{"cmd":"app","text":"Status"}'
#   CHROOT=../.vfix3 tools/captura-banco.sh salida.png '{"cmd":"freq","val":14074000}'
#
# Audio (modo CAP_AUDIO, ver docs/banco-audio.md): graba lo que la GUI escribe a su sink y mide el
# nivel. La fuente de senal es la prueba de voz de la propia GUI (X6200_VOICE_TEST).
#   CHROOT=../.vfix3 CAP_AUDIO=/tmp/audio.wav  tools/captura-banco.sh    # dice la frase y mide RMS
#   CHROOT=../.vfix3 CAP_AUDIO=/tmp/vacio.wav CAP_VOZ= tools/captura-banco.sh   # control sin voz
#   CHROOT=../.vfix3 CAP_AUDIO=/tmp/vol.wav CAP_LOOPBACK=si tools/captura-banco.sh  # Parte B (sudo)
#
# Garantias (ver .agents/pruebas-sin-radio/SKILL.md):
#   - Prueba la ULTIMA version: si el hash de los fuentes cambio, hay cambios sin commitear o un fuente
#     es mas nuevo que el binario (RECONSTRUIR=auto|si|no), limpia con `make x6200-gui-dirclean` y
#     recien despues reconstruye. El rebuild compila en una copia propia del repo y el mtime no es
#     confiable (hallazgo #65: un binario viejo pasaba el check), asi que ese camino no reusa nada.
#     Imprime commit, md5 de la fuente y md5 del binario que corrio.
#   - Usa el socket CORRECTO: borra socket/framebuffer viejos, espera el socket, hace handshake
#     {"cmd":"get"}, comprueba que la GUI este viva y reintenta reiniciando el banco una vez.
#   - No deja evidencia falsa: si la captura sale vacia (o el framebuffer es uniforme) falla.
#   - No corre un binario que no es el firmware: antes de arrancar la GUI, el binario COPIADO pasa
#     por tools/verifica-firmware-port.py (gate fail-closed del issue #86: un build desde `main`
#     sale con exit 0 y deja un x6200_gui que no sirve). Si no es el firmware del port, falla.
#   - EJEMPLO CANONICO de uso con theme + guard #86: docs/evidencia/2026-10-15-wf92-banco/ (issue #92): rebuild con el guard, /mnt/themes sincronizado con los 6 temas v2 del rootfs (la DATA vieja miente: sin la key waterfall_signal_width la linea midio 1 px), medicion determinista del ancho con PIL.
#
# QEMU_CPU (por defecto cortex-a7, el core de la R16) es necesario: con el modelo por defecto la GUI
# muere a los pocos segundos con "qemu-arm-static: QEMU internal SIGBUS {code=ADERR}".
#
# Deja la GUI corriendo entre llamadas (contenedor banco-apps) para encadenar capturas; se termina
# con: docker rm -f banco-apps
#
# SPDX-License-Identifier: GPL-2.0-or-later
set -u

AQUI=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$AQUI/.." && pwd)

SALIDA=${1:-banco.png}
ORDEN=${2:-}
CHROOT=${CHROOT:-$REPO/../.vfix3}
VOLUMEN=${VOLUMEN:-x6200-build-output}
IMAGEN=${IMAGEN:-x6200-buildroot}
BUILDROOT=${BUILDROOT:-$REPO/../buildroot_new/AetherX6200Buildroot}
ESPERA=${ESPERA:-6}
RECONSTRUIR=${RECONSTRUIR:-auto}
QEMU_CPU=${QEMU_CPU:-cortex-a7}
MINIMO_PNG=${MINIMO_PNG:-4000}          # menos que esto = captura vacia
BIN=/usr/sbin/x6200_gui
CAP_AUDIO=${CAP_AUDIO:-}                # modo audio: ruta del WAV de salida (ver docs/banco-audio.md)
CAP_VOZ=${CAP_VOZ-prueba de audio del banco}   # frase de la prueba de voz; vacio = control de silencio
CAP_SEGUNDOS=${CAP_SEGUNDOS:-8}         # ventana de captura del modo audio, en segundos
CAP_LOOPBACK=${CAP_LOOPBACK:-no}        # Parte B: captura por el loopback del host (necesita sudo)
MODO_CHECK=no
[ "${1:-}" = "--check" ] && MODO_CHECK=si

fallar() { echo "ERROR: $*" >&2; exit 1; }
nota()   { echo "  $*"; }
cola_log() { [ -f "$CHROOT/out-banco.txt" ] && tail -5 "$CHROOT/out-banco.txt" | sed 's/^/    | /'; }
docker_ok() { docker "$1" >/dev/null 2>&1; }

# --- 0. comprobaciones previas: que exista todo lo que hace falta, con mensajes claros -----------
FALTAN=0
falta() { echo "  FALTA: $*"; FALTAN=$((FALTAN+1)); }
echo "== comprobando el banco =="

docker_ok "info"                       && nota "docker: ok" || falta "docker no responde"
docker image inspect "$IMAGEN" >/dev/null 2>&1      && nota "imagen $IMAGEN: ok"    || falta "falta la imagen $IMAGEN"
docker volume inspect "$VOLUMEN" >/dev/null 2>&1    && nota "volumen $VOLUMEN: ok"  || falta "falta el volumen $VOLUMEN (build del GUI)"
[ -d "$CHROOT" ]                    && nota "chroot $CHROOT: ok"          || falta "falta el chroot ($CHROOT): extraer el rootfs de la imagen, ver docs/TESTING.md nivel 4"
[ -x "$CHROOT/bin/busybox" ]        || [ -x "$CHROOT/bin/sh" ]            || falta "el chroot no parece un rootfs (sin /bin/busybox)"
[ -d "$CHROOT/lib" ]                                                       || falta "el chroot no tiene /lib"
command -v python3 >/dev/null       && nota "python3: ok"                 || falta "falta python3 (para rotar el framebuffer)"
python3 -c "import PIL" 2>/dev/null && nota "PIL: ok"                     || falta "falta Pillow (pip install pillow)"
[ -f "$AQUI/remote/x6200-remote" ]  && nota "cliente de control: ok"      || falta "falta $AQUI/remote/x6200-remote (compilar con $AQUI/remote/compila.sh)"
if [ "$RECONSTRUIR" != "no" ]; then
    [ -d "$BUILDROOT" ] && nota "buildroot: ok" || nota "aviso: sin $BUILDROOT no se puede reconstruir (RECONSTRUIR=no para saltearlo)"
fi
BENCH_FALTA=
for l in libfakefb.so libcrash.so libx6200trace.so; do [ -f "$AQUI/bench/$l" ] || BENCH_FALTA="$BENCH_FALTA $l"; done
if [ -n "$BENCH_FALTA" ]; then
    if [ -x "$AQUI/bench/compila.sh" ] && docker_ok "info"; then
        nota "compilando libs del banco que faltan:$BENCH_FALTA"
        "$AQUI/bench/compila.sh" >/dev/null 2>&1 || falta "no se pudieron compilar las libs del banco ($AQUI/bench/compila.sh)"
    else
        falta "faltan las libs del banco:$BENCH_FALTA (correr $AQUI/bench/compila.sh)"
    fi
fi
[ -f "$AQUI/bench/libfakefb.so" ] && nota "libs del banco: ok"
# modo audio (CAP_AUDIO): lo que necesita esta adentro del chroot/volumen; se informa, no se exige
if [ -n "$CAP_AUDIO" ]; then
    [ -f "$CHROOT/usr/bin/pulseaudio" ] && nota "audio: pulseaudio en el chroot: ok" || nota "aviso: sin $CHROOT/usr/bin/pulseaudio no hay modo CAP_AUDIO"
    [ -f "$CHROOT/usr/bin/parec" ]      && nota "audio: parec en el chroot: ok"      || nota "aviso: sin $CHROOT/usr/bin/parec no hay captura"
fi

if [ "$FALTAN" -gt 0 ]; then
    echo "== $FALTAN cosa(s) faltan: el banco no puede correr ==" >&2
    exit 1
fi

# qemu utilizable: se prueba con el busybox del chroot, adentro del contenedor
if ! docker run --rm --entrypoint "" -v "$CHROOT:/t" "$IMAGEN" sh -c 'chroot /t /bin/busybox true' >/dev/null 2>&1; then
    falta "qemu-arm-static no puede ejecutar el chroot (¿binfmt_misc cargado?)"
    echo "== no se puede ejecutar ARM: probar 'ls /proc/sys/fs/binfmt_misc' en el host ==" >&2
    exit 1
fi
nota "qemu-arm-static ejecutando ARM: ok"

if [ "$MODO_CHECK" = "si" ]; then
    echo "== todo listo para el banco (no se toco nada) =="
    docker image inspect "$IMAGEN" --format '  imagen: {{.Id}}' 2>/dev/null | cut -c1-40
    echo "  version del repo: $(git -C "$REPO" log -1 --format='%h %s' 2>/dev/null | cut -c1-60)"
    [ -n "$(git -C "$REPO" status --porcelain -- src lvgl/src 2>/dev/null)" ] && echo "  aviso: hay cambios sin commitear en src/ (se reconstruye antes de probar)"
    echo "  rebuild: si corresponde, dirclean + rebuild (el rebuild compila en una copia propia del repo)"
    echo "  capturas mas chicas que $MINIMO_PNG bytes se consideran vacias"
    exit 0
fi

mkdir -p "$CHROOT/mnt" "$CHROOT/dev/shm" "$CHROOT/tmp"

# --- 1. ultima version: hash de los fuentes + cambios sin commitear + timestamps ----------------
hash_fuentes() {
    { find "$REPO/src" "$REPO/lvgl/src" -type f \( -name '*.c' -o -name '*.cpp' -o -name '*.h' \) -print0 2>/dev/null \
        | sort -z | xargs -0 md5sum 2>/dev/null; } | md5sum | cut -d' ' -f1
}
SELLO="$CHROOT/.banco-fuente.md5"
HASH=$(hash_fuentes)
SUCIO=$(git -C "$REPO" status --porcelain -- src lvgl/src 2>/dev/null | head -1)
MAS_NUEVO=$(docker run --rm --entrypoint "" -v "$VOLUMEN":/b -v "$REPO":/g:ro "$IMAGEN" sh -c "
    [ -f /b/target$BIN ] || { echo FALTA; exit 0; }
    find /g/src /g/lvgl/src -newer /b/target$BIN \\( -name '*.c' -o -name '*.cpp' -o -name '*.h' \\) 2>/dev/null | head -1" 2>/dev/null || true)
SELLO_VIEJO=$(cat "$SELLO" 2>/dev/null || echo "")
MOTIVO=
[ "$SELLO_VIEJO" != "$HASH" ] && MOTIVO="el hash de los fuentes cambio"
[ -n "$SUCIO" ] && MOTIVO="${MOTIVO:+$MOTIVO y }hay cambios sin commitear"
[ -n "$MAS_NUEVO" ] && MOTIVO="${MOTIVO:+$MOTIVO y }un fuente es mas nuevo que el binario"
if [ "$RECONSTRUIR" = "si" ]; then MOTIVO="RECONSTRUIR=si"; fi

# Corre un target de buildroot del GUI en el volumen del build (mismo entorno para dirclean y rebuild).
make_gui() {
    docker run --rm --entrypoint "" \
        -v "$BUILDROOT":/workspace -v "$VOLUMEN":/workspace/build -v "$REPO":/workspace_gui:ro \
        -e FORCE_UNSAFE_CONFIGURE=1 -w /workspace/build "$IMAGEN" \
        make "$@" >/dev/null 2>&1
}

if [ -n "$MOTIVO" ] && [ "$RECONSTRUIR" != "no" ]; then
    [ -d "$BUILDROOT" ] || fallar "hay que reconstruir ($MOTIVO) y no esta $BUILDROOT (usar RECONSTRUIR=no para probar el binario que ya esta)"
    # dirclean SIEMPRE en este camino: el rebuild compila en una copia propia del repo y su mtime no
    # es confiable (hallazgo #65: el check-matrix daba ok con un binario viejo), asi que no se reusa
    # nada. Borra la copia y los .o del GUI y el rebuild los rehace del arbol fuente. Cuesta un
    # rebuild completo, aceptable en el banco.
    echo "  limpiando la GUI (dirclean): el rebuild reusa una copia propia del repo"
    make_gui x6200-gui-dirclean || fallar "fallo la limpieza (make x6200-gui-dirclean)"
    echo "  reconstruyendo la GUI: $MOTIVO"
    make_gui x6200-gui-rebuild -j"$(nproc)" || fallar "fallo la reconstruccion (make x6200-gui-rebuild)"
    echo "$HASH" > "$SELLO"
elif [ -n "$MOTIVO" ]; then
    echo "  aviso: $MOTIVO, pero RECONSTRUIR=no: se prueba el binario que ya esta"
fi

# --- 2. el binario y las libs del banco --------------------------------------------------------
docker run --rm --entrypoint "" -v "$CHROOT:/t" -v "$VOLUMEN":/b "$IMAGEN" \
    sh -c "cp /b/target$BIN /t$BIN" 2>/dev/null || fallar "no pude copiar el binario desde el volumen $VOLUMEN (¿corriste make?)"
[ -x "$CHROOT$BIN" ] || fallar "quedo sin permiso de ejecucion $CHROOT$BIN"

# --- 2a. GATE fail-closed: el binario COPIADO tiene que ser el firmware del port ------------------
# Antes de correr nada: un build desde el `main` del remoto (issue #86) sale con exit 0 y deja un
# binario de 776.884 B que NO es el firmware del port (el del port pesa 13.737.784 B). El guard
# compara con los dos binarios reales (tamano + marcas del port) y, si no es el firmware, frena
# aca en vez de correr una GUI que no es la del port. Senales y calibracion: ver el encabezado de
# tools/verifica-firmware-port.py.
python3 "$AQUI/verifica-firmware-port.py" "$CHROOT$BIN" \
    || fallar "el binario copiado ($CHROOT$BIN) no es el firmware del port (issue #86): NO se corre la GUI"

cp "$AQUI/bench/libfakefb.so" "$AQUI/bench/libcrash.so" "$AQUI/bench/libx6200trace.so" "$CHROOT/lib/" || fallar "no pude copiar las libs del banco al chroot"
cp "$AQUI/remote/x6200-remote" "$CHROOT/tmp/x6200-remote" || fallar "no pude copiar el cliente de control al chroot"
chmod 755 "$CHROOT/tmp/x6200-remote"

# --- 2b. modo audio (CAP_AUDIO=<out.wav>): lo que la GUI escribe a su sink, medido ---------------
#
# Que se puede medir sin root y sin /dev/snd (el chroot no tiene placa de sonido) y que no:
#
#   - La GUI manda su audio a PulseAudio (src/audio.c: pa_stream_connect_playback al sink
#     "alsa_output.platform-sound.stereo-fallback"), asi que la unica via de captura es correr
#     PulseAudio ADENTRO del chroot con un sink que lleve ESE nombre y grabar su monitor con
#     `parec`: es exactamente lo que la GUI manda al sink, a ritmo real (176 KB/s).
#     El reloj sin hardware lo pone el sink nulo de PulseAudio (ver abajo).
#   - El plugin de ALSA `type file` NO sirve aca como captura: sin un device que consuma a ritmo
#     real (nulo, en la config del banco) escribe a velocidad de CPU — medido 3,5 GB en 6 s — y no
#     hay forma de acotarlo: ninguna primitiva de ALSA sin hardware pone el reloj (se probaron null,
#     plug, rate, dmix y un loopback de PulseAudio: todos escriben a velocidad de CPU;
#     ver docs/banco-audio.md, que ademas valida el plugin con un tono conocido).
#   - El volumen de la GUI (`audio_set_play_vol` -> el elemento "AIF1 DA0" de la tarjeta "default")
#     NO se puede medir aca: sin /dev/snd no hay tarjeta, el mixer no abre y la GUI lo avisa
#     ("No se pudo preparar el mixer de 'default'"). Esa parte necesita el loopback del host (Parte
#     B): `sudo modprobe snd-aloop` da una tarjeta de verdad, y ahi softvol crea el elemento que el
#     GUI busca y el volumen entra en la captura. Runbook: docs/banco-audio.md.
#
# La senal de no-silencio es la prueba de voz de la propia GUI (X6200_VOICE_TEST): espeak manda su
# WAV por el mismo camino de audio_play que la voz/FT8/JS8 (ver docs/HALLAZGOS.md 2), sin depender
# de la radio. espeak no esta en este chroot y se copia del volumen del build, como $BIN.
#
# Genera todo lo que hace falta (config de PulseAudio, config de ALSA de la Parte B y el arranque
# interno) y lo deja en $CHROOT/tmp/banco-audio/ para poder mirarlo. No edita el chroot a mano: solo
# copia espeak/libespeak/espeak-data si faltan (igual que hace con el binario) y deja esos archivos.
modo_audio() {
    local sink=alsa_output.platform-sound.stereo-fallback
    local adir="$CHROOT/tmp/banco-audio"
    local wav="$adir/capture.wav"
    local voz=$CAP_VOZ
    local loop=$CAP_LOOPBACK

    # el directorio lo crea el host (y tiene que quedar escribible por el: el contenedor escribe
    # adentro como root y si un archivo queda de root no se puede limpiar desde aca; el arranque
    # interno limpia rt/home/state como root)
    if ! mkdir -p "$adir" 2>/dev/null || ! : > "$adir/.escribible" 2>/dev/null; then
        fallar "$adir no se puede escribir (¿quedo de root? borralo con: docker run --rm -v \"$CHROOT:/t\" $IMAGEN rm -rf /t/tmp/banco-audio)"
    fi

    cat > "$adir/pulse-banco.pa" <<'FIN'
# lo genera tools/captura-banco.sh: el sink nulo lleva el nombre que la GUI busca por nombre fijo
# (src/audio.c), y es el unico reloj disponible sin hardware (el sink nulo de PulseAudio consume a
# ritmo real; un slave `null` de ALSA no).
load-module module-native-protocol-unix auth-anonymous=1
load-module module-null-sink sink_name=alsa_output.platform-sound.stereo-fallback
FIN

    cat > "$adir/arranca-audio.sh" <<'FIN'
# lo genera tools/captura-banco.sh y corre ADENTRO del chroot (ver ese script para el porque)
set -u
B=/tmp/banco-audio
# lo que dejo la corrida anterior lo limpia el contenedor (es root aca): rt con el pid de pulse y
# la captura vieja. Los .pa/.conf los escribe el host y no se tocan.
rm -rf "$B/rt" "$B/home" "$B/state" "$B/capture.wav" "$B/pulse.log" "$B/gui.log" "$B/parec.log" "$B/capture.log"
mkdir -p "$B" /mnt/app_logs "$B/rt" "$B/home" "$B/state"
export HOME="$B/home"
export PULSE_RUNTIME_PATH="$B/rt"
export PULSE_STATE_PATH="$B/state"
export PULSE_SERVER="unix:$B/rt/native"

# 1. espeak de la prueba de voz: no esta en el chroot del banco, se copia del volumen del build
if [ ! -x /usr/bin/espeak ] && [ -f /b/target/usr/bin/espeak ]; then
    cp -a /b/target/usr/bin/espeak /usr/bin/espeak
    cp -a /b/target/usr/lib/libespeak.so* /usr/lib/ 2>/dev/null
    [ -d /usr/share/espeak-data ] || cp -a /b/target/usr/share/espeak-data /usr/share/ 2>/dev/null
fi
if [ -n "$A_VOZ" ] && [ ! -x /usr/bin/espeak ]; then
    echo "BANCO-AUDIO: no hay espeak en el chroot ni en el volumen: sin fuente de senal (usar CAP_VOZ= para el control)"
    exit 3
fi

# 2. la grabacion
GRABADOR=
if [ "$A_LOOPBACK" = "si" ]; then
    # Parte B: la GUI -> ALSA -> softvol -> lado playback del loopback, y del otro lado arecord
    # (que ademas es el que hace andar el loopback: sin consumidor, la escritura se traba)
    if [ ! -d /dev/snd ]; then
        echo "BANCO-AUDIO: PENDIENTE parte B: el chroot no tiene /dev/snd. En el host:"
        echo "  sudo modprobe snd-aloop"
        echo "  y montar /dev/snd en el chroot del contenedor (-v /dev/snd:/t/dev/snd; ver docs/banco-audio.md)"
        exit 2
    fi
    CARD=$(/usr/bin/aplay -l 2>/dev/null | sed -n 's/^card \([0-9]*\): Loopback.*/\1/p' | head -1)
    if [ -z "$CARD" ]; then
        echo "BANCO-AUDIO: hay /dev/snd pero no la tarjeta Loopback (sudo modprobe snd-aloop)"
        exit 2
    fi
    cat > "$B/alsa-banco.conf" <<CONF
# lo genera tools/captura-banco.sh: el elemento "AIF1 DA0" que audio_set_play_vol() busca en la
# tarjeta "default" lo crea softvol sobre la tarjeta del loopback (sin tarjeta, softvol no abre:
# ese es el motivo de fondo de la Parte B)
pcm.banco_loop { type plug; slave.pcm "hw:Loopback,1" }
pcm.banco_vol {
    type softvol
    slave.pcm "banco_loop"
    control { name "AIF1 DA0"; card $CARD }
    min_dB -51.0
    max_dB 0.0
}
pcm.!default { type plug; slave.pcm "banco_vol" }
ctl.!default { type hw; card $CARD }
CONF
    export ALSA_CONFIG_PATH="$B/alsa-banco.conf"
    echo "BANCO-AUDIO: parte B con la tarjeta Loopback $CARD (ALSA_CONFIG_PATH=$ALSA_CONFIG_PATH)"
    /usr/bin/arecord -D hw:Loopback,0 -f S16_LE -r 44100 -c 2 -d "$A_SEGUNDOS" "$B/capture.wav" > "$B/capture.log" 2>&1 &
    GRABADOR=$!
else
    /usr/bin/pulseaudio -nF "$B/pulse-banco.pa" --exit-idle-time=-1 --disallow-exit --disable-shm \
        --log-target=stderr > "$B/pulse.log" 2>&1 &
    # hasta-condicion: el socket aparece antes de que el demonio atienda, asi que se espera a que
    # conteste de verdad (con el socket solo, el primero en conectar se come un "Connection refused")
    i=0
    while [ "$i" -lt 20 ]; do
        /usr/bin/pactl info >/dev/null 2>&1 && break
        sleep 1
        i=$((i + 1))
    done
    if ! /usr/bin/pactl info >/dev/null 2>&1; then
        echo "BANCO-AUDIO: pulseaudio no atiende en $B/rt/native"
        tail -5 "$B/pulse.log"
        exit 1
    fi
    /usr/bin/pactl list short sinks
    /usr/bin/parec -d "$A_SINK.monitor" --file-format=wav "$B/capture.wav" > "$B/parec.log" 2>&1 &
    GRABADOR=$!
    sleep 2
fi

# 3. la GUI: con la prueba de voz dice la frase y termina sola; sin voz (control) corre normal y la
#    paramos nosotros. 1 s de latido para que la grabacion ya este prendida.
sleep 1
X6200_VOICE_TEST="$A_VOZ" LD_PRELOAD=/lib/libfakefb.so:/lib/libcrash.so:/lib/libx6200trace.so \
    "$A_BIN" > "$B/gui.log" 2>&1 &
GUI=$!
sleep "$A_SEGUNDOS"
kill -TERM "$GUI" 2>/dev/null
sleep 1
# SIGTERM: parec finaliza el WAV (escribe el tamano en la cabecera) y arecord cierra el suyo
kill -TERM "$GRABADOR" 2>/dev/null
sleep 1
kill -TERM "$GRABADOR" 2>/dev/null
wait "$GRABADOR" 2>/dev/null

# en cuantos dB esta el sink: si no fuera 0 dB, el nivel capturado ya vendria atenuado
/usr/bin/pactl list sinks 2>/dev/null | grep -i "Volume:" | head -1
echo "BANCO-AUDIO: captura lista"
FIN
    chmod 755 "$adir/arranca-audio.sh"

    echo "== audio del banco (modo CAP_AUDIO) =="
    nota "sink: $sink   ventana: ${CAP_SEGUNDOS}s   voz: ${voz:-(vacio: control de silencio)}   parte B: $loop"
    # el techo es de red de seguridad: el arranque interno ya termina solo
    timeout $((CAP_SEGUNDOS + 90)) docker run --rm --name banco-audio --entrypoint "" \
        -v "$CHROOT:/t" -v "$VOLUMEN:/t/b:ro" \
        -e "A_BIN=$BIN" -e "A_SINK=$sink" -e "A_VOZ=$voz" -e "A_SEGUNDOS=$CAP_SEGUNDOS" -e "A_LOOPBACK=$loop" \
        "$IMAGEN" sh -c "chroot /t /bin/busybox sh /tmp/banco-audio/arranca-audio.sh" \
        > "$adir/out.txt" 2>&1
    COD=$?
    docker rm -f banco-audio >/dev/null 2>&1
    sed 's/^/  /' "$adir/out.txt"
    if [ "$COD" = 2 ]; then
        echo "  PENDIENTE (Parte B): el paso de arriba es del owner (sudo); la Parte A no lo necesita" >&2
        exit 2
    fi
    [ "$COD" = 0 ] || fallar "el modo audio fallo (exit $COD): ver $adir/out.txt"
    [ -s "$wav" ] || fallar "no quedo la captura ($wav): ver $adir/out.txt"

    # que se probo (la garantia 1 del banco vale tambien para el audio)
    echo "  version: $(git -C "$REPO" log -1 --format='%h %s' 2>/dev/null | cut -c1-60)"
    echo "  fuente: md5 $(printf '%s' "$HASH" | cut -c1-12) (src/ + lvgl/src)"
    echo "  binario: md5 $(md5sum "$CHROOT$BIN" | cut -c1-12)   qemu-cpu: $QEMU_CPU"

    # la cuenta del RMS: numeros, no afirmaciones (y falla si pidieron voz y la captura es silencio)
    python3 - "$wav" "$CAP_AUDIO" "$voz" <<'PY' || exit 1
import math, sys, wave

ruta, salida, voz = sys.argv[1], sys.argv[2], sys.argv[3]
w = wave.open(ruta, 'rb')
n, ch, rate = w.getnframes(), w.getnchannels(), w.getframerate()
if ch != 1 and ch != 2:
    sys.exit("el WAV no es mono ni estereo (%d canales)" % ch)
buf = w.readframes(n)
w.close()
# se cuenta lo que de verdad hay en el archivo (puede ser menos que lo que dice la cabecera)
muestras = len(buf) // 2
datos = memoryview(buf).cast('h')[:muestras]
canales = [datos[c::ch] for c in range(ch)]
print("  wav: %s -> %s (%d muestras por canal, %d canal(es), %d Hz, %.2f s, %d bytes)"
      % (ruta, salida, muestras // ch, ch, rate, muestras / ch / rate, len(buf)))
if muestras == 0:
    sys.exit("ERROR: la captura no tiene ninguna muestra: no es una captura, es un grabador que no "
             "arranco (ver capture.log / parec.log en el mismo directorio)")


def a_db(nivel):
    return 20.0 * math.log10(nivel / 32767.0) if nivel > 0 else -math.inf


for c, canal in enumerate(canales):
    n_c = len(canal)
    rms = math.sqrt(sum(float(v) * v for v in canal) / n_c) if n_c else 0.0
    pico = max((abs(v) for v in canal), default=0)
    distintos = sum(1 for v in canal if v != 0)
    print("  canal %d: RMS %+.2f dBFS   pico %+.2f dBFS   distinto de 0: %d de %d (%.1f%%)"
          % (c, a_db(rms), a_db(pico), distintos, n_c, 100.0 * distintos / n_c if n_c else 0.0))

if all(max((abs(v) for v in c), default=0) == 0 for c in canales):
    print("  la captura es SILENCIO DIGITAL (todas las muestras en 0)")
    if voz:
        sys.exit("ERROR: se pidio la prueba de voz (%r) y la captura quedo en silencio digital: "
                 "esto no es evidencia de que el audio de la GUI llegue al sink" % voz)
PY
    cp "$wav" "$CAP_AUDIO" || fallar "no pude copiar la captura a $CAP_AUDIO"
    nota "captura util ($CAP_AUDIO)"

    # el camino del mixer (o su ausencia en el banco) tambien es evidencia
    grep -h -m2 "mixer de 'default'" "$adir/gui.log" 2>/dev/null | sed 's/^/  mixer del GUI: /' || true
    nota "config y logs en $adir/ (arranca-audio.sh, pulse-banco.pa, pulse.log, gui.log)"
}

if [ -n "$CAP_AUDIO" ]; then
    modo_audio
    exit 0
fi

# --- 3. arrancar la GUI (una sola vez por contenedor) ------------------------------------------
arrancar_banco() {
    docker rm -f banco-apps >/dev/null 2>&1 || true
    # socket y framebuffer viejos: si quedan, cualquier espera pasa de largo y la captura sale vacia
    rm -f "$CHROOT/tmp/fakefb0" "$CHROOT/crash.txt" "$CHROOT/tmp/x6200_remote.sock"
    docker run -d --name banco-apps --rm --entrypoint="" -e "QEMU_CPU=$QEMU_CPU" -v "$CHROOT:/t" "$IMAGEN" \
        sh -c "chroot /t /bin/busybox env LD_PRELOAD=/lib/libfakefb.so:/lib/libcrash.so:/lib/libx6200trace.so $BIN > /t/out-banco.txt 2>&1" >/dev/null \
        || fallar "no pude arrancar el contenedor banco-apps"
    for i in $(seq 1 90); do [ -S "$CHROOT/tmp/x6200_remote.sock" ] && break; sleep 1; done
    [ -S "$CHROOT/tmp/x6200_remote.sock" ] || { echo "la GUI no creo el socket en 90 s:"; cola_log; return 1; }
}
CLIENTE="chroot /t /tmp/x6200-remote"
preguntar() { docker run --rm --entrypoint "" -v "$CHROOT:/t" "$IMAGEN" sh -c "$CLIENTE '{\"cmd\":\"get\"}'" 2>&1 | head -c 200; }
# la GUI esta viva si contesta el handshake: no hace falta pidof (y en busybox no existe)

if ! docker ps --filter name=banco-apps --format '{{.Names}}' | grep -q banco-apps; then
    arrancar_banco || true
fi

# handshake + GUI viva, con un reinicio de reintento
RESP=$(preguntar || true)
if ! echo "$RESP" | grep -q '"t"'; then
    echo "  el banco no contesta o la GUI no esta viva; reiniciando..."
    arrancar_banco || true
    RESP=$(preguntar || true)
fi
if ! echo "$RESP" | grep -q '"t"'; then
    echo "el socket no responde {\"cmd\":\"get\"}: $RESP" >&2; cola_log; exit 1
fi

# --- 4. la orden -------------------------------------------------------------------------------
if [ -n "$ORDEN" ]; then
    R=$(docker run --rm --entrypoint "" -v "$CHROOT:/t" "$IMAGEN" sh -c "$CLIENTE '$ORDEN'" 2>&1)
    echo "$R" | sed 's/^/  respuesta: /'
    echo "$R" | grep -q '"t":"err"' && nota "aviso: la GUI contesto un error a la orden (arriba)"
    sleep "$ESPERA"
fi

# --- 5. captura: el framebuffer falso es 480x800x32 BGRA, se rota 270 como en la radio ---------
[ -s "$CHROOT/tmp/fakefb0" ] || { echo "no hay framebuffer ($CHROOT/tmp/fakefb0)" >&2; cola_log; exit 1; }
python3 - "$CHROOT/tmp/fakefb0" "$SALIDA" <<'PY' || exit 1
import sys
from PIL import Image
raw = open(sys.argv[1], 'rb').read()
W, H = 480, 800
if len(raw) < W*H*4:
    sys.exit("el framebuffer es mas chico de lo esperado (%d bytes)" % len(raw))
img = Image.new('RGB', (W, H)); px = img.load()
for y in range(H):
    for x in range(W):
        i = (y * W + x) * 4
        px[x, y] = (raw[i+2], raw[i+1], raw[i])
img.transpose(Image.ROTATE_270).save(sys.argv[2])
colores = img.getcolors(maxcolors=1 << 20) or []
print("  captura: %s (%d colores distintos)" % (sys.argv[2], len(colores)))
PY

# --- 6. que se probo, y que la captura no sea basura -------------------------------------------
TAM=$(stat -c %s "$SALIDA" 2>/dev/null || echo 0)
echo "  version: $(git -C "$REPO" log -1 --format='%h %s' 2>/dev/null | cut -c1-60)"
echo "  fuente: md5 $(printf '%s' "$HASH" | cut -c1-12) (src/ + lvgl/src)"
echo "  binario: md5 $(md5sum "$CHROOT$BIN" | cut -c1-12)   qemu-cpu: $QEMU_CPU"
if [ "$TAM" -lt "$MINIMO_PNG" ]; then
    echo "ERROR: la captura parece vacia ($TAM bytes < $MINIMO_PNG): la GUI no dibujo la pantalla" >&2
    echo "  (si es la primera captura, subir ESPERA; el banco muere solo despues de un rato)" >&2
    cola_log; exit 1
fi
nota "captura util ($TAM bytes)"
