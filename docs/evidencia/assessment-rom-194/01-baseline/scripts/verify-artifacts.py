#!/usr/bin/env python3
"""Paso 7 de #195: identidad de los artefactos ROM construidos con el par congelado.

Uso:
  python3 verify_artifacts.py --img <dir build/images> --label <mine|orchestrator> \
      --log <build log> --gui <clone-gui> --br <clone-buildroot> \
      --out <dir de evidencia>/logs/step7

No modifica nada: hashea, corre el gate #86, extrae el binario y los archivos de
interes de rootfs.tar/rootfs.ext4 con debugfs (sin root) y escribe un JSON con los
hechos medidos. Conserva todos los comandos y su salida en el log del paso.
"""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile


def run(cmd, cwd=None, log=None):
    p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    if log is not None:
        log.write(f"\n$ {' '.join(cmd)}\n[cwd={cwd or os.getcwd()}]\n")
        log.write(p.stdout)
        if p.stderr:
            log.write("[stderr]\n" + p.stderr)
        log.write(f"[exit={p.returncode}]\n")
        log.flush()
    return p


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def md5(path):
    h = hashlib.md5()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--img", required=True)
    ap.add_argument("--label", required=True, help="mine | orchestrator")
    ap.add_argument("--log", required=True, help="log completo del make")
    ap.add_argument("--gui", required=True)
    ap.add_argument("--br", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()

    os.makedirs(a.out, exist_ok=True)
    facts = {"label": a.label, "img_dir": a.img, "artifacts": {}, "checks": {}}
    logpath = os.path.join(a.out, f"identity-{a.label}.log")
    with open(logpath, "w", encoding="utf-8") as log:
        log.write(f"# paso 7 #195 — artefactos {a.label}\n")
        log.write(f"# host: {os.uname().sysname} {os.uname().release}; python {sys.version.split()[0]}\n")

        # --- hashes/tamanos de los artefactos -----------------------------------
        for name in ("sdcard.img", "rootfs.tar", "rootfs.ext4", "boot.vfat",
                     "u-boot-sunxi-with-spl.bin", "zImage", "sun8i-r16-x6200.dtb"):
            p = os.path.join(a.img, name)
            if not os.path.isfile(p):
                facts["artifacts"][name] = {"present": False}
                continue
            st = os.stat(p)
            facts["artifacts"][name] = {
                "present": True,
                "path": p,
                "bytes": st.st_size,
                "sha256": sha256(p),
                "mtime_utc": subprocess.run(
                    ["date", "-u", "-d", f"@{int(st.st_mtime)}", "+%Y-%m-%dT%H:%M:%SZ"],
                    capture_output=True, text=True).stdout.strip(),
            }
            log.write(f"{name}: bytes={st.st_size} sha256={facts['artifacts'][name]['sha256']} "
                      f"mtime={facts['artifacts'][name]['mtime_utc']}\n")

        tar = os.path.join(a.img, "rootfs.tar")
        ext4 = os.path.join(a.img, "rootfs.ext4")
        if not os.path.isfile(tar):
            log.write("\nSIN rootfs.tar: no se puede correr el gate ni extraer la identidad\n")
            facts["checks"]["rootfs_tar"] = "MISSING"
            with open(os.path.join(a.out, f"identity-{a.label}.json"), "w") as f:
                json.dump(facts, f, indent=2)
            print(logpath)
            return 2

        # --- gate #86 ----------------------------------------------------------
        gate = os.path.join(a.gui, "tools", "verifica-firmware-port.py")
        g = run([sys.executable, gate, "--tar", tar], log=log)
        facts["checks"]["gate_86"] = {"exit": g.returncode,
                                      "stdout": g.stdout.strip(), "stderr": g.stderr.strip()}
        log.write(f"# gate #86 exit={g.returncode}\n")

        # --- fuente-md5 del checkout y del staged source -----------------------
        fm = run(["sh", os.path.join(a.gui, "tools", "fuente-md5.sh"), a.gui], log=log)
        fuente = fm.stdout.strip()
        facts["checks"]["fuente_md5_candidate"] = fuente
        staged = os.path.join(a.br, "build", "build", "x6200-gui-custom")
        staged_md5 = None
        stamp = os.path.join(staged, ".fuente.md5")
        if os.path.isfile(stamp):
            staged_md5 = open(stamp).read().strip()
        facts["checks"]["fuente_md5_staged"] = staged_md5
        log.write(f"# fuente-md5 checkout={fuente} staged={staged_md5}\n")

        # --- extraccion del binario --------------------------------------------
        work = tempfile.mkdtemp(prefix="art195-")
        gui_tar = os.path.join(work, "x6200_gui.from-tar")
        with open(gui_tar, "wb") as out:
            t = subprocess.run(["tar", "-xOf", tar, "./usr/sbin/x6200_gui"], stdout=out,
                               stderr=subprocess.PIPE)
        facts["checks"]["tar_extract_exit"] = t.returncode
        log.write(f"# tar -xOf rootfs.tar ./usr/sbin/x6200_gui exit={t.returncode} "
                  f"stderr={t.stderr.decode(errors='replace')[:400]}\n")

        gui_ext4 = os.path.join(work, "x6200_gui.from-ext4")
        if os.path.isfile(ext4):
            d = run(["debugfs", "-R", f"dump /usr/sbin/x6200_gui {gui_ext4}", ext4], log=log)
            facts["checks"]["debugfs_dump_exit"] = d.returncode
        for label, path in (("tar", gui_tar), ("ext4", gui_ext4)):
            if os.path.isfile(path) and os.path.getsize(path) > 0:
                st = os.stat(path)
                facts["checks"][f"gui_{label}"] = {
                    "bytes": st.st_size, "md5": md5(path), "sha256": sha256(path)}
                log.write(f"# x6200_gui ({label}): bytes={st.st_size} md5={md5(path)} "
                          f"sha256={sha256(path)}\n")
            else:
                facts["checks"][f"gui_{label}"] = {"missing": True}

        # mismo binario por las dos vias
        if "bytes" in facts["checks"].get("gui_tar", {}) and \
                "bytes" in facts["checks"].get("gui_ext4", {}):
            facts["checks"]["gui_tar_equals_ext4"] = (
                facts["checks"]["gui_tar"]["md5"] == facts["checks"]["gui_ext4"]["md5"])

        # ELF y dependencias
        if os.path.isfile(gui_tar) and os.path.getsize(gui_tar) > 0:
            f = run(["file", gui_tar], log=log)
            facts["checks"]["file"] = f.stdout.strip()
            h = run(["readelf", "-h", gui_tar], log=log)
            facts["checks"]["readelf_h"] = h.stdout.strip()
            d = run(["readelf", "-d", gui_tar], log=log)
            facts["checks"]["readelf_d"] = d.stdout.strip()
            # el binario embebe el fuente-md5 como texto: comprobarlo
            with open(gui_tar, "rb") as fh:
                blob = fh.read()
            facts["checks"]["fuente_md5_in_binary"] = fuente.encode() in blob
            import re as _re
            hexes = _re.findall(rb"(?<![0-9a-f])[0-9a-f]{32}(?![0-9a-f])", blob)
            facts["checks"]["hex32_in_binary"] = sorted({h.decode() for h in hexes})[:20]
            if staged_md5:
                facts["checks"]["staged_md5_in_binary"] = staged_md5.encode() in blob

        # --- fstab y S01create_data dentro del rootfs --------------------------
        def from_tar(member):
            p = subprocess.run(["tar", "-xOf", tar, member], capture_output=True, text=True)
            return p.returncode, p.stdout

        rc, fstab = from_tar("./etc/fstab")
        facts["checks"]["fstab"] = {"exit": rc, "content": fstab}
        facts["checks"]["fstab_ext4_mmcblk0p3"] = bool(
            [ln for ln in fstab.splitlines()
             if "/dev/mmcblk0p3" in ln and "/mnt" in ln and "ext4" in ln])
        log.write(f"# /etc/fstab exit={rc} ext4_mmcblk0p3="
                  f"{facts['checks']['fstab_ext4_mmcblk0p3']}\n{fstab}\n")

        rc2, s01 = from_tar("./etc/init.d/S01create_data")
        facts["checks"]["s01create_data"] = {"exit": rc2,
                                             "has_DATA_ya_existe": "DATA ya existe" in s01,
                                             "bytes": len(s01)}
        log.write(f"# S01create_data exit={rc2} 'DATA ya existe'="
                  f"{facts['checks']['s01create_data']['has_DATA_ya_existe']}\n")

        # init.d y binarios presentes en el artefacto
        listing = subprocess.run(["tar", "-tf", tar], capture_output=True, text=True)
        members = listing.stdout.splitlines()
        facts["checks"]["init_d_present"] = sorted(
            m for m in members if m.startswith("./etc/init.d/"))
        facts["checks"]["gui_member"] = "./usr/sbin/x6200_gui" in members
        facts["checks"]["libs_present"] = sorted(
            m for m in members if m.startswith("./usr/lib/lib") and m.endswith(".so"))[:40]

        # --- procedencia: log del build ---------------------------------------
        for needle in ("x6200-gui", "rsync", "fuente"):
            hits = []
            if os.path.isfile(a.log):
                with open(a.log, errors="replace") as f:
                    for i, line in enumerate(f):
                        if needle in line and ("x6200-gui" in line or needle != "x6200-gui"):
                            hits.append(line.rstrip()[:220])
                        if len(hits) >= 12:
                            break
            facts["checks"][f"log_{needle}"] = hits

        with open(os.path.join(a.out, f"identity-{a.label}.json"), "w") as f:
            json.dump(facts, f, indent=2)
        shutil.rmtree(work, ignore_errors=True)
    print(logpath)
    return 0


if __name__ == "__main__":
    sys.exit(main())
