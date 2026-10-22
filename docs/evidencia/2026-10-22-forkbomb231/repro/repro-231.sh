#!/bin/bash
#
# Reproducción ACOTADA del fork bomb del lab safe-update (issue iariap/x6200_gui#231).
#
# Todo corre en contenedores PRIVADOS y desechables con cota de PIDs y de RAM
# (--pids-limit, --memory/--memory-swap): un lazo de procesos no puede llevarse
# la memoria del host. Cada bloque imprime lo que midió.
#
#   docs/evidencia/2026-10-22-forkbomb231/repro/repro-231.sh <worktree> <fixture.tgz> <base-sha>
#
# Lo que muestra:
#   0. las cotas del contenedor y que contienen un fork bomb (EAGAIN en fork)
#   1. el tar del target REEMPLAZA el inodo: por el camino tar el shell no puede
#      re-leer el archivo que la extracción pisa (mide el alcance real)
#   2. bash, en cambio, lee su script por OFFSET sobre el mismo fd: si el archivo
#      se reescribe en el lugar mientras corre, re-parsea la cola del script
#      recién instalado y RE-CORRE el despacho (el update reentrando en sí
#      mismo). Antes (base) / después (worktree) de la guardia S02UPDATE_PASS.
#
# SPDX-License-Identifier: LGPL-2.1-or-later
#
set -uo pipefail

WT=${1:?uso: repro-231.sh <worktree> <fixture.tgz> <base-sha>}
FIXTURE=${2:?uso: repro-231.sh <worktree> <fixture.tgz> <base-sha>}
BASE=${3:?uso: repro-231.sh <worktree> <fixture.tgz> <base-sha>}
IMG=h215-x6200-update-lab
PIDS=${X6200_LAB_PIDS_LIMIT:-64}
MEM=${X6200_LAB_DOCKER_MEMORY:-512m}
WORK=/tmp/opencode/f231/repro
mkdir -p "$WORK"

AFTER=$WT/tools/release/update-overlay/etc/init.d/S02update
# El updater de la base: el de antes de este fix (el que la radio tiene hoy).
git -C "$WT" show "$BASE":tools/release/update-overlay/etc/init.d/S02update > "$WORK/S02update.base"
# El updater que corría la radio antes del primer update in-place (stock publicado).
tar -xzf "$FIXTURE" -C "$WORK" etc/init.d/S02update
cp "$WORK/etc/init.d/S02update" "$WORK/S02update.stock"

say() { printf '%s\n' "$*"; }

say "== 0. cotas del contenedor y contención de un fork bomb"
docker run --rm --memory="$MEM" --memory-swap="$MEM" --pids-limit="$PIDS" "$IMG" bash -c '
echo "pids.max=$(cat /sys/fs/cgroup/pids.max)  memory.max=$(cat /sys/fs/cgroup/memory.max)"
n=0
for i in $(seq 1 200); do (sleep 5) & n=$((n+1)); done 2>/dev/null
sleep 2
echo "fork lanzados=$n  pids.current=$(cat /sys/fs/cgroup/pids.current)/$(cat /sys/fs/cgroup/pids.max)  memory.current=$(cat /sys/fs/cgroup/memory.current)/$(cat /sys/fs/cgroup/memory.max)"
echo "arriba: los forks que no entran en la cota: \"fork: Resource temporarily unavailable\" (se corta ahi, no en la RAM del host)"
wait 2>/dev/null'

say ""
say "== 1. el tar del target reemplaza el inodo"
docker run --rm --privileged --memory="$MEM" --memory-swap="$MEM" --pids-limit="$PIDS" \
    -v "$WORK/S02update.stock":/stock.sh:ro -v "$AFTER":/new.sh:ro -v "$FIXTURE":/fixtures/fixture.tgz:ro \
    "$IMG" bash -c '
set -u
mkdir -p /iso && tar -xf /fixtures/fixture.tgz -C /iso 2>/dev/null
install -m 0755 /usr/bin/qemu-arm-static /iso/usr/bin/qemu-arm-static
mkdir -p /iso/tmp/t/etc/init.d /iso/tmp/stage/etc/init.d
cp /stock.sh /iso/tmp/t/etc/init.d/S02update
cp /new.sh /iso/tmp/stage/etc/init.d/S02update
tar -cf /iso/tmp/pkg.tar -C /iso/tmp/stage etc/init.d/S02update
echo "antes:  $(stat -c "inode=%i size=%s" /iso/tmp/t/etc/init.d/S02update)"
chroot /iso /bin/sh -c "PATH=/sbin:/usr/sbin:/bin:/usr/bin; tar -xf /tmp/pkg.tar -C /tmp/t"
echo "después:$(stat -c "inode=%i size=%s" /iso/tmp/t/etc/init.d/S02update)  <- inodo nuevo = reemplazado"'

say ""
say "== 2. la re-entrada: el shell re-parsea [L, fin) del script recién instalado"
say "   (L = el largo del script que corría antes; acá el stock publicado; la marca del pase se fuerza)"
docker run --rm --memory="$MEM" --memory-swap="$MEM" --pids-limit="$PIDS" --tmpfs /mnt:size=4m \
    -v "$WORK/S02update.base":/base.sh:ro -v "$WORK/S02update.stock":/stock.sh:ro -v "$AFTER":/after.sh:ro \
    "$IMG" bash -c '
set -u
L=$(wc -c < /stock.sh)     # el offset donde terminaba el script viejo
printf "paquete sin sidecar: solo para que un pase se delate si corre\n" > /mnt/x6200_r1cbu_prueba.tgz
for v in base after; do
    case $v in base) src=/base.sh ;; after) src=/after.sh ;; esac
    tail -c +$((L + 1)) "$src" > /frag.sh
    echo "--- $v: cola de $(wc -c < /frag.sh) bytes, desde el offset $L"
    out=$(S02UPDATE_PASS=0 timeout 60 bash /frag.sh start 2>/tmp/err.txt); rc=$?
    echo "    rc=$rc"
    echo "    stdout=[$(printf "%s" "$out" | tr "\n" "|")]"
    echo "    stderr=[$(tr "\n" "|" < /tmp/err.txt | cut -c1-140)]"
done'