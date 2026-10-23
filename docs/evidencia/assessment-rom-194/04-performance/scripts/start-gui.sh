#!/usr/bin/env bash
# start-gui.sh — #198 helper: start the private QEMU_USER GUI and record a
# boot-to-socket / boot-to-first-answer wall time. QEMU_USER is NOT the radio's
# kernel/init: this is a proxy, never a boot-to-ready claim for the device.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
SCR=${SCR:-/home/pablo/desa/x6200/assessment-194/scratch-198}
CHROOT=${CHROOT:-$SCR/rootfs-res}
OUT=${OUT:-$HERE/../capturas}
LOGS=${LOGS:-$HERE/../logs/step4-resources}
CT=${CT:-assessment198-res}
IMG=${IMG:-x6200-buildroot}
mkdir -p "$OUT" "$LOGS" "$CHROOT/tmp/res"

docker rm -f "$CT" >/dev/null 2>&1
rm -f "$CHROOT/tmp/x6200_remote.sock" "$CHROOT/out-banco.txt"
start_ns=$(date +%s%N)
docker run -d --name "$CT" --entrypoint "" -e QEMU_CPU=cortex-a7 -v "$CHROOT:/t" "$IMG" \
    sh -c 'exec chroot /t /bin/busybox env LD_PRELOAD=/lib/libfakefb.so:/lib/libcrash.so:/lib/libx6200trace.so /usr/sbin/x6200_gui > /t/out-banco.txt 2>&1' >/dev/null
socket_ns=""
first_ns=""
for _ in $(seq 1 300); do
    if [ -z "$socket_ns" ] && [ -S "$CHROOT/tmp/x6200_remote.sock" ]; then
        socket_ns=$(date +%s%N)
    fi
    if [ -n "$socket_ns" ]; then
        if docker exec "$CT" chroot /t /tmp/x6200-remote '{"cmd":"get"}' > "$CHROOT/tmp/first-get.json" 2>/dev/null; then
            first_ns=$(date +%s%N)
            break
        fi
    fi
    sleep 0.1
done
python3 - "$start_ns" "$socket_ns" "$first_ns" "$CHROOT/tmp/first-get.json" > "$OUT/boot-to-socket-qemu.json" <<'PY'
import json, sys
start, sock, first, path = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
try:
    state = json.load(open(path))
except Exception:
    state = None
d = {
    "layer": "QEMU_USER", "coverage": "PARTIAL",
    "start_ns": int(start),
    "socket_ns": int(sock) if sock else None,
    "first_get_ns": int(first) if first else None,
    "socket_ms": (int(sock)-int(start))/1e6 if sock else None,
    "first_get_ms": (int(first)-int(start))/1e6 if first else None,
    "first_get": state,
    "limitation": "QEMU_USER (no kernel/init/hardware); wall time of the container, not boot-to-ready of the radio"
}
print(json.dumps(d, indent=2))
PY
echo "started $CT: $(cat "$OUT/boot-to-socket-qemu.json" | tr '\n' ' ')" | tee "$LOGS/boot.txt"
