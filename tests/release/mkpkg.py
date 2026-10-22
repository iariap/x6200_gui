#!/usr/bin/env python3
"""Arma paquetes de actualizacion de fixture para el banco tests/release.

Uso:

    mkpkg.py minimal  <salida.tgz> [--mutate NOMBRE]
    mkpkg.py from     <base.tgz> <salida.tgz> --mutate NOMBRE

Escribe siempre <salida.tgz> y <salida.tgz>.sha256 ("<hash>  <basename>", el
formato de `sha256sum -c`).

`minimal` arma un paquete chico que pasa el preflight del S02update: lleva el
inventario setuid auditado (bin/busybox, usr/libexec/dbus-daemon-launch-helper,
usr/libexec/ssh-keysign), el propio etc/init.d/S02update root:root 0755 y un
archivo mas. `from` copia un paquete real y le aplica la mutacion.

Las entradas se escriben a mano sobre el formato tar: es la unica forma de armar
las que un empaquetador decente no deja crear (nombre absoluto, ".." como
componente, duplicados, nodos de dispositivo, setuid de mas). Los casos de
preflight se rechazan antes de tocar /, asi que su contenido no importa.
"""

import gzip
import hashlib
import os
import sys

BLOCK = 512


def entry(name, data=b"", mode=0o644, uid=0, gid=0, typeflag="0", linkname="", mtime=0):
    """Un bloque de cabecera tar (+ datos), con name sin normalizar."""
    name_b = name.encode("utf-8")
    if len(name_b) > 100:
        raise SystemExit(f"nombre demasiado largo para el fixture: {name}")
    h = bytearray(BLOCK)

    def octal(off, length, value):
        s = ("%0*o" % (length - 1, value)).encode()
        h[off:off + len(s)] = s

    h[0:len(name_b)] = name_b
    octal(100, 8, mode)
    octal(108, 8, uid)
    octal(116, 8, gid)
    octal(124, 12, len(data))
    octal(136, 12, mtime)
    h[148:156] = b" " * 8
    h[156] = ord(typeflag)
    link_b = linkname.encode("utf-8")
    if len(link_b) > 100:
        raise SystemExit(f"target demasiado largo para el fixture: {linkname}")
    h[157:157 + len(link_b)] = link_b
    h[257:263] = b"ustar\x00"
    h[263:265] = b"00"
    h[265:269] = b"root"
    h[297:301] = b"root"
    h[148:156] = ("%06o\0 " % sum(h)).encode()
    out = bytes(h)
    if data:
        out += data + b"\0" * (-len(data) % BLOCK)
    return out


def inventory(mutate=None):
    """Las entradas que el preflight del S02update exige."""
    # El bug del empaquetador viejo: el S02update salia del checkout (uid 1000).
    uid = 1000 if mutate == "updater-uid1000" else 0
    gid = 1000 if mutate == "updater-uid1000" else 0
    return [
        entry("bin/busybox", b"busybox\n", mode=0o4755),
        entry("usr/libexec/dbus-daemon-launch-helper", b"helper\n", mode=0o4750, gid=101),
        entry("usr/libexec/ssh-keysign", b"keysign\n", mode=0o4711),
        entry("etc/init.d/S02update", b"#!/bin/sh\n", mode=0o755, uid=uid, gid=gid),
        entry("usr/share/x6200/minimal.txt", b"fixture\n"),
    ]


MUTATIONS = {
    # nombre absoluto: el tar no debe poder escribir fuera del rootfs
    "absolute": lambda: entry("/etc/h215-absolute", b"x\n"),
    # ".." como componente
    "dotdot": lambda: entry("etc/../../h215-escape", b"x\n"),
    # "./" como prefijo de un nombre
    "dot": lambda: entry("./h215-dot", b"x\n"),
    # entrada duplicada (mismo nombre dos veces)
    "duplicate": lambda: entry("usr/share/x6200/minimal.txt", b"y\n"),
    # la entrada del punto de montaje mismo: reemplazarla seria quedarse sin /mnt
    # (o sin /dev, /tmp, ...). Los hijos si viajan (dev/fd de la rootfs auditada).
    "bare-mnt": lambda: entry("mnt", b"x\n"),
    "bare-dev": lambda: entry("dev", b"x\n"),
    "bare-tmp": lambda: entry("tmp", b"x\n"),
    # nodos que el paquete no debe llevar
    "fifo": lambda: entry("opt/h215-fifo", b"", mode=0o644, typeflag="6"),
    "chardev": lambda: entry("opt/h215-node", b"", mode=0o666, typeflag="3"),
    # setuid de mas
    "setuid-extra": lambda: entry("usr/libexec/h215-extra", b"x\n", mode=0o4755),
    # symlink con target absoluto / con ..
    "symlink-abs": lambda: entry("opt/h215-link", b"", mode=0o777, typeflag="2", linkname="/etc/h215"),
    "symlink-dotdot": lambda: entry("opt/h215-link", b"", mode=0o777, typeflag="2", linkname="../../etc/h215"),
    # un archivo regular donde el target tiene un directorio. El backup lo
    # intercepta (el tar lista el directorio con barra: "usr/sbin/") y el updater
    # corta ahi con "Update backup failed", sin tocar /: es el caso
    # `entry-over-directory` del banco.
    "usr-sbin-file": lambda: entry("usr/sbin", b"not a directory\n"),
    # un archivo cuyo DIRECTORIO PADRE es un archivo en el target: el preflight y
    # el backup pasan (la ruta no existe) y el tar de la extraccion falla
    # ("Not a directory"), que es lo que pide el contrato (`extract-fails`).
    "nested-under-file": lambda: entry("etc/passwd/h215-nested", b"x\n"),
}


def read_members(blob):
    """Los bloques de un tar sin sus bloques de fin, para poder agregar mas."""
    end = len(blob)
    while end >= BLOCK and blob[end - BLOCK:end] == b"\0" * BLOCK:
        end -= BLOCK
    return blob[:end]


def build_minimal(mutate):
    blob = b"".join(inventory(mutate))
    if mutate and mutate != "updater-uid1000":
        blob += MUTATIONS[mutate]()
    return blob + b"\0" * (2 * BLOCK)


def build_from(base_path, mutate, first=False):
    if mutate not in MUTATIONS:
        raise SystemExit(f"mutacion desconocida: {mutate}")
    with gzip.open(base_path, "rb") as fh:
        blob = fh.read()
    body = read_members(blob)
    if first:
        # La mutacion va primero: el tar de la extraccion corta ahi y no escribe
        # nada, que es lo que hace determinista al caso (BusyBox tar aborta en el
        # primer error en vez de seguir con las entradas siguientes).
        return MUTATIONS[mutate]() + body + b"\0" * (2 * BLOCK)
    return body + MUTATIONS[mutate]() + b"\0" * (2 * BLOCK)


def main(argv):
    if len(argv) < 3:
        raise SystemExit(__doc__)
    mode = argv[1]
    mutate = None
    first = False
    if "--first" in argv:
        argv.remove("--first")
        first = True
    if "--mutate" in argv:
        i = argv.index("--mutate")
        mutate = argv[i + 1]
        del argv[i:i + 2]
    if mode == "minimal":
        out = argv[2]
        blob = build_minimal(mutate)
    elif mode == "from":
        base, out = argv[2], argv[3]
        blob = build_from(base, mutate, first)
    else:
        raise SystemExit(__doc__)
    with gzip.open(out, "wb", compresslevel=6) as fh:
        fh.write(blob)
    with open(out, "rb") as fh:
        digest = hashlib.sha256(fh.read()).hexdigest()
    with open(out + ".sha256", "w", encoding="utf-8") as fh:
        fh.write(f"{digest}  {os.path.basename(out)}\n")
    print(f"{out}: {len(blob)} bytes, sha256 {digest}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
