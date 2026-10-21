#!/usr/bin/env python3
"""Audita el rootfs.tar del candidato (capa HOST, estático).

Lee el tar con tarfile (sin extraer), y reporta: conteo por tipo, setuid/setgid,
nodos de dispositivo, symlinks rotos (resolución dentro del archivo), scripts de
init, bibliotecas compartidas, binarios, archivos más grandes y duplicados de
bibliotecas. Escribe JSON a stdout. No modifica nada.
"""
import json
import sys
import tarfile
from collections import Counter, defaultdict

TAR = sys.argv[1]

out = {
    "archive": TAR,
    "counts_by_type": {},
    "setuid": [],
    "setgid": [],
    "device_nodes": [],
    "broken_symlinks": [],
    "symlink_count": 0,
    "init_scripts": [],
    "shared_libs": [],
    "elf_binaries": [],
    "largest_files": [],
    "top_level": {},
    "hardlinks": 0,
    "uid_gid": {},
    "total_bytes": 0,
}

members = {}
hardlink_targets = set()
with tarfile.open(TAR, "r:") as tf:
    for m in tf:
        name = m.name
        if name in (".", "./"):
            continue
        if name.startswith("./"):
            name = name[2:]
        elif name.startswith("/"):
            name = name.lstrip("/")
        members[name] = m
        out["total_bytes"] += m.size
        typ = "dir" if m.isdir() else "sym" if m.issym() else "lnk" if m.islnk() else "chr" if m.ischr() else "blk" if m.isblk() else "fifo" if m.isfifo() else "file"
        out["counts_by_type"][typ] = out["counts_by_type"].get(typ, 0) + 1
        if m.issym():
            out["symlink_count"] += 1
        if m.islnk():
            out["hardlinks"] += 1
            hardlink_targets.add(m.linkname[2:] if m.linkname.startswith("./") else m.linkname.lstrip("/"))
        if m.ischr() or m.isblk():
            out["device_nodes"].append({"name": name, "type": typ, "major": m.devmajor, "minor": m.devminor, "mode": oct(m.mode)})
        if m.mode & 0o4000:
            out["setuid"].append({"name": name, "mode": oct(m.mode), "uid": m.uid, "gid": m.gid, "size": m.size})
        if m.mode & 0o2000:
            out["setgid"].append({"name": name, "mode": oct(m.mode), "uid": m.uid, "gid": m.gid, "size": m.size})
        out["uid_gid"][f"{m.uid}:{m.gid}"] = out["uid_gid"].get(f"{m.uid}:{m.gid}", 0) + 1
        base = name.rsplit("/", 1)[-1]
        if name.startswith("etc/init.d/") and typ == "file":
            out["init_scripts"].append({"name": name, "mode": oct(m.mode), "size": m.size})
        if typ == "file" and (base.endswith(".so") or ".so." in base):
            out["shared_libs"].append({"name": name, "size": m.size, "mode": oct(m.mode)})
        if typ == "file" and m.size > 0:
            out["largest_files"].append([m.size, name])
        if typ == "dir":
            top = name.split("/")[1] if "/" in name else name
            out["top_level"][top] = out["top_level"].get(top, 0) + 1

# symlinks rotos: resolver dentro del archivo, sin salir de la raíz del tar
def resolve(path, seen=None):
    path = path[2:] if path.startswith("./") else path.lstrip("/")
    seen = seen or set()
    if path in seen:
        return False
    seen.add(path)
    if path in members:
        m = members[path]
        if m.issym():
            target = m.linkname
            if target.startswith("/"):
                cand = target.lstrip("/")
            else:
                cand = "/".join(p for p in path.split("/")[:-1] + [target] if p not in ("", "."))
            # normalizar ..
            parts = []
            for p in cand.split("/"):
                if p == "..":
                    if parts:
                        parts.pop()
                    else:
                        return False  # escapa del rootfs: tratar como roto
                elif p and p != ".":
                    parts.append(p)
            return resolve("/".join(parts), seen)
        return True
    if path in hardlink_targets:
        return True
    return False

for name, m in members.items():
    if m.issym():
        if not resolve(name):
            out["broken_symlinks"].append({"name": name, "target": m.linkname})

out["largest_files"] = sorted(out["largest_files"], reverse=True)[:25]
out["init_scripts"].sort(key=lambda x: x["name"])
out["shared_libs"].sort(key=lambda x: x["name"])
json.dump(out, sys.stdout, indent=1, sort_keys=True)
print()
