#!/bin/bash
# Verificaciones reproducibles de la unidad #199 (fase A, sin radio).
# Genera logs/provenance/*.log y commands.jsonl. No toca la radio ni otros checkouts.
set -u

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
OUT=$(CDPATH= cd -- "$HERE/.." && pwd)
WT=$(CDPATH= cd -- "$OUT/../../../.." && pwd)
CG=$(CDPATH= cd -- "$WT/../clone-gui" && pwd)
BR=$(CDPATH= cd -- "$WT/../clone-buildroot" && pwd)
IMG="$BR/build/images"
L="$OUT/logs/provenance"
mkdir -p "$L"
: > "$OUT/commands.jsonl"

run() {
    id=$1; cwd=$2; log=$3; cmdstr=$4; shift 4
    start=$(date -u +%FT%TZ)
    ( cd "$cwd" && "$@" ) > "$log" 2>&1
    rc=$?
    end=$(date -u +%FT%TZ)
    log_rel=${log#"$OUT"/}
    python3 - "$id" "$cmdstr" "$cwd" "$log_rel" "$rc" "$start" "$end" "$log" <<'PY' >> "$OUT/commands.jsonl"
import json, sys
id, cmd, cwd, log, rc, start, end, logpath = sys.argv[1:9]
try:
    tail = open(logpath, encoding="utf-8", errors="replace").read().splitlines()[-2:]
except OSError:
    tail = []
print(json.dumps({
    "id": id,
    "command": cmd,
    "cwd": cwd,
    "env": {"CANDIDATE": "c888308eede71205605c06383432ee7e165f202e",
            "TZ": "UTC", "LANG": "C", "RADIO_TOUCHED": "false"},
    "start_utc": start,
    "end_utc": end,
    "exit_code": int(rc),
    "log": log,
    "summary": " | ".join(x[:220] for x in tail),
}, ensure_ascii=False))
PY
    echo "  $id exit=$rc"
}

run git-refs "$CG" "$L/git-refs.log" \
  "git rev-parse c888308e origin/assessment/rom-194; git ls-remote origin refs/heads/lu2dsx/x6200-rom; git ls-tree HEAD lvgl; tools/fuente-md5.sh" \
  bash -c 'echo "UTC=$(date -u +%FT%TZ)"; echo "candidate=$(git rev-parse c888308e)"; echo "integration=$(git rev-parse origin/assessment/rom-194)"; echo "tip=$(git ls-remote origin refs/heads/lu2dsx/x6200-rom | cut -f1)"; echo "buildroot_frozen=d084e2870a00b64d9c57474e7eceeef80f43f25b"; echo "buildroot_tip=$(git -C ../clone-buildroot rev-parse origin/lu2dsx/x6200-rom)"; echo "lvgl=$(git ls-tree HEAD lvgl | awk "{print \$3}")"; echo "fuente_md5=$(bash tools/fuente-md5.sh 2>/dev/null | tail -1)"'

run merge-counts "$CG" "$L/merge-counts.log" \
  "for m in 230b2063 ca1698d1 c41f36b2 05020942; do git diff --name-only \${m}^1 \${m} | wc -l; done; git merge-base --is-ancestor c888308e origin/assessment/rom-194" \
  bash -c 'echo "merge file counts (diff parent1..merge):"; for m in 230b2063 ca1698d1 c41f36b2 05020942; do printf "%s " "$m"; git diff --name-only ${m}^1 ${m} | wc -l; done; echo "ancestry c888308e..05020942:"; git merge-base --is-ancestor c888308e origin/assessment/rom-194 && echo OK'

run production-diff "$CG" "$L/production-diff.log" \
  "git diff --name-only c888308e origin/assessment/rom-194 | grep -v '^docs/'" \
  bash -c 'echo "production diff candidate..integration (expect 0):"; git diff --name-only c888308e origin/assessment/rom-194 | grep -v "^docs/" || true; echo "count=$(git diff --name-only c888308e origin/assessment/rom-194 | grep -v "^docs/" | wc -l)"'

run tip-diff "$CG" "$L/tip-diff.log" \
  "git diff --name-only c888308e d4fb57f8 | grep -v '^docs/'" \
  bash -c 'echo "production diff candidate..tip:"; git diff --name-only c888308e d4fb57f8 | grep -v "^docs/" | sort; echo "count=$(git diff --name-only c888308e d4fb57f8 | grep -v "^docs/" | wc -l)"'

run artifact-hashes "$IMG" "$L/artifact-hashes.log" \
  "sha256sum sdcard.img rootfs.tar rootfs.ext2 boot.vfat zImage sun8i-r16-x6200.dtb u-boot-sunxi-with-spl.bin; stat -c '%n %s' ..." \
  bash -c 'sha256sum sdcard.img rootfs.tar rootfs.ext2 boot.vfat zImage sun8i-r16-x6200.dtb u-boot-sunxi-with-spl.bin; stat -c "%n %s" sdcard.img rootfs.tar rootfs.ext2 boot.vfat zImage sun8i-r16-x6200.dtb u-boot-sunxi-with-spl.bin'

run gui-hash "$IMG" "$L/gui-hash.log" \
  "tar -xOf rootfs.tar ./usr/sbin/x6200_gui | sha256sum; ... | md5sum; tar -xOf rootfs.tar ./etc/fstab | grep mmcblk0p3" \
  bash -c 'echo "GUI from rootfs.tar:"; tar -xOf rootfs.tar ./usr/sbin/x6200_gui | sha256sum; tar -xOf rootfs.tar ./usr/sbin/x6200_gui | md5sum; echo "fstab:"; tar -xOf rootfs.tar ./etc/fstab | grep mmcblk0p3'

run gate86 "$CG" "$L/gate86.log" \
  "python3 tools/verifica-firmware-port.py --tar $IMG/rootfs.tar" \
  python3 tools/verifica-firmware-port.py --tar "$IMG/rootfs.tar"

run sha256sums "$OUT/.." "$L/sha256sums.log" \
  "sha256sum -c SHA256SUMS en 01-baseline, 02-rom, 03-ui-ux, 04-performance" \
  bash -c 'for d in 01-baseline 02-rom 03-ui-ux 04-performance; do echo "=== $d"; (cd "$d" && sha256sum -c SHA256SUMS >/dev/null 2>&1; echo "sha256sum_c_exit=$? entries=$(wc -l < SHA256SUMS) files=$(find . -type f | wc -l)"); done'

run file-counts "$OUT/.." "$L/file-counts.log" \
  "find <unidad> -type f | wc -l; wc -l < SHA256SUMS" \
  bash -c 'for d in 01-baseline 02-rom 03-ui-ux 04-performance; do printf "%s files=%s sums=%s\n" "$d" "$(find "$d" -type f | wc -l)" "$(wc -l < "$d/SHA256SUMS")"; done'

run audita-pruebas "$WT" "$L/audita-pruebas.log" \
  "python3 tests/audita-pruebas.py" \
  python3 tests/audita-pruebas.py

run check-links "$OUT" "$L/check-links.log" \
  "python3 scripts/check_links.py" \
  python3 scripts/check_links.py

echo "commands.jsonl lines: $(wc -l < "$OUT/commands.jsonl")"
