#!/usr/bin/env python3
"""Fija la regla de tipos y de targets de enlaces del preflight del S02update.

Extrae el programa awk de `check_types()` del S02update y lo corre contra una
tabla de entradas (nombre, modo, tamaño, dueño, target) con el veredicto
esperado. Es la parte del preflight que decide si un paquete puede escribir
fuera del rootfs, y la que mas facil es romper sin darse cuenta: los casos de la
rootfs auditada (dev/fd -> ../proc/self/fd, que queda adentro) y los que se
salen (../../etc/...) tienen que dar distinto.

    check_types_rule.py <ruta al S02update>
"""

import pathlib
import subprocess
import sys
import tempfile

# nombre, modo, tamaño, dueño, target, ¿se admite?
CASES = [
    # La rootfs auditada trae estos symlinks bajo /dev: un ".." que queda
    # adentro del rootfs se admite.
    ("dev/fd", "lrwxrwxrwx", "0", "0/0", "../proc/self/fd", True),
    ("dev/stdin", "lrwxrwxrwx", "0", "0/0", "../proc/self/fd/0", True),
    ("dev/log", "lrwxrwxrwx", "0", "0/0", "../tmp/log", True),
    ("x2", "lrwxrwxrwx", "0", "0/0", "etc/x", True),
    ("deeper/link3", "lrwxrwxrwx", "0", "0/0", "../etc", True),
    # ... y estos se salen o son absolutos.
    # ... y los absolutos se admiten: la rootfs auditada los trae
    # (etc/alsa/conf.d/50-arcam-av-ctl.conf -> /usr/share/alsa/...).
    ("opt/abs", "lrwxrwxrwx", "0", "0/0", "/etc/x", True),
    ("etc/alsa/conf.d/50-arcam-av-ctl.conf", "lrwxrwxrwx", "0", "0/0",
     "/usr/share/alsa/alsa.conf.d/50-arcam-av-ctl.conf", True),
    # ... y estos se salen.
    ("opt/h215-link", "lrwxrwxrwx", "0", "0/0", "../../etc/h215", False),
    ("x", "lrwxrwxrwx", "0", "0/0", "../etc", False),
    ("deeper/link", "lrwxrwxrwx", "0", "0/0", "../../../etc", False),
    ("deeper/link2", "lrwxrwxrwx", "0", "0/0", "../../etc", False),
    # Archivos y enlaces normales.
    ("bin/busybox", "-rwsr-xr-x", "8", "0/0", "", True),
    ("opt/x", "-rw-r--r--", "2", "0/0", "", True),
    # Puntos de montaje, nodos y directorios.
    ("dev", "-rw-r--r--", "2", "0/0", "", False),
    ("mnt", "-rw-r--r--", "2", "0/0", "", False),
    ("tmp", "-rw-r--r--", "2", "0/0", "", False),
    ("opt/fifo", "prw-r--r--", "0", "0/0", "", False),
    ("opt/dev", "crw-rw-rw-", "0", "0/0", "", False),
    ("opt/dir", "drwxr-xr-x", "0", "0/0", "", False),
]


def extract_awk(path):
    src = pathlib.Path(path).read_text()
    start = src.index("check_types()")
    a = src.index("awk -F'\\t' '", start) + len("awk -F'\\t' '")
    b = src.index("' \"$WORK/meta\")", a)
    return src[a:b]


def main(argv):
    if len(argv) != 2:
        raise SystemExit(__doc__)
    prog = extract_awk(argv[1])
    with tempfile.TemporaryDirectory() as tmp:
        awkf = pathlib.Path(tmp) / "check_types.awk"
        awkf.write_text(prog + "\n")
        bad = 0
        for name, mode, size, owner, target, allowed in CASES:
            one = pathlib.Path(tmp) / "one.tsv"
            one.write_text("\t".join([name, mode, size, owner, target]) + "\n")
            res = subprocess.run(
                ["awk", "-F", "\t", "-f", str(awkf), str(one)],
                capture_output=True, text=True, check=False,
            )
            if res.returncode != 0:
                print(f"MAL {name}: awk falló: {res.stderr.strip()}")
                bad += 1
                continue
            got = res.stdout.strip()
            if bool(got) == allowed:
                mark = "MAL"
                bad += 1
            else:
                mark = "ok "
            print(f"{mark} {name:16} target={target or '-':20} veredicto={got or 'se admite'}")
    if bad:
        print(f"\n{bad} caso(s) de la tabla no coinciden con la regla")
        return 1
    print(f"\n{len(CASES)}/{len(CASES)} casos de la tabla de tipos/enlaces OK")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
