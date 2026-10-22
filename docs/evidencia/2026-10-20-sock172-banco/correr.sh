#!/bin/bash
#
# Evidencia del banco QEMU para el issue #172 (socket de control robado por una
# segunda instancia de la GUI).
#
#   CHROOT=/home/pablo/desa/x6200/.vsock172 VOLUMEN=x6200-build-output-dfl173 \
#       docs/evidencia/2026-10-20-sock172-banco/correr.sh
#
# Corre el binario ARM real del worktree en un chroot PRIVADO (reflink de .vfix3)
# y con un volumen de build PRIVADO (x6200-build-output-dfl173, no el compartido).
#
# 1) Arranca la GUI (instancia 1) y hace el handshake {"cmd":"get"}.
# 2) Lanza una SEGUNDA instancia de la GUI en el mismo chroot (simula el
#    relanzador duplicado: el daemon arrancando mientras otra GUI ya vive).
# 3) Verifica que el path del socket NO se le robe a la instancia 1: mismo inodo
#    de filesystem y mismo listener en /proc/net/unix, y el cliente sigue
#    conectando y contestando.
#
# El binario es el del commit del fix (sello de fuentes verificado, gate #86).
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -euo pipefail

AQUI=$(cd "$(dirname -- "$0")" && pwd)
REPO=$(cd "$AQUI/../../.." && pwd)

CHROOT=${CHROOT:-/home/pablo/desa/x6200/.vsock172}
VOLUMEN=${VOLUMEN:-x6200-build-output-dfl173}
IMAGEN=${IMAGEN:-x6200-buildroot}
QEMU_CPU=${QEMU_CPU:-cortex-a7}
ESPERA_BOOT=${ESPERA_BOOT:-90}
SALIDA=${SALIDA:-$AQUI}
CONT=${CONT:-sock172-banco}

BIN=/usr/sbin/x6200_gui
LOG=$CHROOT/out-banco.txt
LOG2=$CHROOT/out-banco-2.txt
PRELOAD=/lib/libfakefb.so:/lib/libcrash.so:/lib/libx6200trace.so

fallar() { echo "ERROR: $*" >&2; docker logs "$CONT" --tail 20 2>/dev/null || true; exit 1; }
nota() { echo "  $*"; }

orden() { docker exec "$CONT" chroot /t /tmp/x6200-remote "$1"; }
get() { orden '{"cmd":"get"}'; }

inodo_path() { docker exec "$CONT" sh -c 'ls -i /t/tmp/x6200_remote.sock 2>/dev/null | cut -d" " -f1'; }
listener() { docker exec "$CONT" sh -c 'grep x6200_remote /proc/net/unix | tr -s " " | cut -d" " -f7,8'; }
# Dentro del chroot la GUI corre via binfmt (qemu-arm-static): pidof x6200_gui no
# la ve. Se cuenta por /proc/<pid>/cmdline (el mismo criterio del banco de #145).
guis() {
    docker exec "$CONT" sh -c '
        for p in /proc/[0-9]*; do
            c=$(tr "\0" " " < "$p/cmdline" 2>/dev/null)
            case "$c" in
                "/usr/bin/qemu-arm-static "*) echo "${p##*/}";;
            esac
        done'
}

arrancar_banco() {
    docker rm -f "$CONT" >/dev/null 2>&1 || true

    # El binario se copia SIN la GUI vieja corriendo (cp sobre un ejecutable en uso
    # falla con ETXTBSY y el banco correria la version anterior sin avisar).
    docker run --rm --entrypoint "" -v "$CHROOT:/t" -v "$VOLUMEN":/b "$IMAGEN" \
        sh -c "cp -f /b/target$BIN /t$BIN && chmod 755 /t$BIN" || fallar "no pude copiar el binario"

    python3 "$REPO/tools/verifica-firmware-port.py" "$CHROOT$BIN" || fallar "el binario no es el firmware del port"

    cp "$REPO/tools/bench/libfakefb.so" "$REPO/tools/bench/libcrash.so" \
       "$REPO/tools/bench/libx6200trace.so" "$CHROOT/lib/"
    cp "$REPO/tools/remote/x6200-remote" "$CHROOT/tmp/x6200-remote"
    chmod 755 "$CHROOT/tmp/x6200-remote"

    rm -f "$CHROOT/tmp/fakefb0" "$CHROOT/crash.txt" "$CHROOT/tmp/x6200_remote.sock" "$LOG" "$LOG2"
    docker run -d --name "$CONT" --rm --entrypoint= -e "QEMU_CPU=$QEMU_CPU" \
        -v "$CHROOT:/t" "$IMAGEN" \
        sh -c "chroot /t /bin/busybox env LD_PRELOAD=$PRELOAD $BIN > /t/out-banco.txt 2>&1"

    local i resp
    for i in $(seq 1 "$ESPERA_BOOT"); do
        [ -S "$CHROOT/tmp/x6200_remote.sock" ] || { sleep 1; continue; }
        resp=$(get 2>/dev/null || true)
        case "$resp" in
            *'"t":"state"'*) nota "banco listo (socket y handshake)"; return 0;;
        esac
        sleep 1
    done
    fallar "la GUI no contesto el handshake en ${ESPERA_BOOT}s"
}

mkdir -p "$SALIDA"

echo "== #172: banco QEMU, dos instancias de la GUI =="
echo "== binario =="
docker run --rm --entrypoint "" -v "$VOLUMEN":/b "$IMAGEN" sh -c "md5sum /b/target$BIN; stat -c '%s B' /b/target$BIN; echo sello:; cat /b/.fuente.md5"
echo "== arrancando la instancia 1 =="
arrancar_banco

pid1=$(guis)
inodo1=$(inodo_path)
listen1=$(listener)
echo "-- instancia 1 viva --"
echo "pidof x6200_gui : $pid1"
echo "inodo del path  : $inodo1"
echo "listener        : $listen1"
echo "get             : $(get)"

echo
echo "== lanzando la instancia 2 (simula el relanzador duplicado) =="
docker exec -d "$CONT" sh -c "chroot /t /bin/busybox env LD_PRELOAD=$PRELOAD $BIN > /t/out-banco-2.txt 2>&1"

# Espera hasta-condicion: la instancia 2 ya arranco y paso por remote_server_init
# (deja la linea del yield). Techo de 40 s.
yield2=no
for _i in $(seq 1 40); do
    if docker exec "$CONT" sh -c 'grep -aq "servidor vivo" /t/out-banco-2.txt'; then
        yield2=si
        break
    fi
    sleep 1
done

pid2=$(guis)
inodo2=$(inodo_path)
listen2=$(listener)
resp2=$(get 2>&1 || true)
echo "-- tras la instancia 2 --"
echo "pidof x6200_gui : $pid2"
echo "inodo del path  : $inodo2"
echo "listener        : $listen2"
echo "get             : $resp2"

echo
echo "== veredicto =="
n_guis=$(echo "$pid2" | wc -w)
[ "$n_guis" -ge 2 ] || fallar "no hay dos instancias de GUI ($pid2): la prueba no es la del issue"
echo "  dos instancias de GUI corriendo: si ($pid2)"
[ "$inodo2" = "$inodo1" ] || fallar "la instancia 2 le robo el path a la 1 (inodo $inodo1 -> $inodo2)"
echo "  el path NO cambio de inodo: si ($inodo1)"
[ "$listen2" = "$listen1" ] || fallar "el listener cambio ($listen1 -> $listen2): el path quedo desvinculado"
echo "  el listener de /proc/net/unix es el mismo: si"
case "$resp2" in
    *'"t":"state"'*) echo "  el cliente sigue conectando y contestando: si";;
    *) fallar "el cliente ya no contesta tras la segunda instancia";;
esac

echo
echo "== log de la instancia 2 (yield esperado) =="
[ "$yield2" = si ] || fallar "la instancia 2 no logueo el yield (no paso por el camino arreglado)"
docker exec "$CONT" sh -c "grep -a 'servidor vivo' /t/out-banco-2.txt"

cp "$LOG" "$SALIDA/out-banco.txt" 2>/dev/null || true
docker exec "$CONT" sh -c 'cat /t/out-banco-2.txt' > "$SALIDA/out-banco-2.txt" 2>/dev/null || true

echo
echo "== listo: $SALIDA =="
