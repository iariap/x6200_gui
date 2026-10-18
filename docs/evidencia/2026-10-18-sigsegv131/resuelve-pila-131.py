#!/usr/bin/env python3
#
# Resuelve la pila del CRASHLOG del issue #131 con los artefactos EXACTOS que
# corria la radio:
#
#   x6200_gui md5 b1ba67520079  (rebuild aislado del commit 63dbf7a4)
#   liblvgl.so md5 cd30ca6c58ad (rootfs de la ROM c640645d810c)
#
#   resuelve-pila-131.py <x6200_gui> <liblvgl.so>
#
# La base de liblvgl se fija con el par pc/lr: es la unica base 0x1000-alineada
# en la que el pc cae dentro de _lv_event_mark_deleted, el lr queda exactamente
# 4 bytes despues del `bl _lv_event_mark_deleted@plt` de lv_obj_destructor, y
# las palabras de pila 0xb618db48/0xb6196128/0xb619605c caen despues de los
# `bl`/`blx` de _lv_obj_destruct y obj_del_core. Ese encadenamiento no es una
# coincidencia: es el arbol de borrado de objetos de LVGL.
#
# SPDX-License-Identifier: LGPL-2.1-or-later
import struct
import subprocess
import sys

PC = 0xB6186AE8
LR = 0xB618D4F0
BASE_MAIN = 0x47EA8C          # &main en runtime (CRASHLOG)
LVGL_BASE = 0xB6170000        # fijada por pc/lr (ver arriba)
WORDS = [0xB618DB48, 0xB6196128, 0xB619605C]
EXE_WORDS = [0x00510B7C, 0x00612848]


def symbols(path, dynamic):
    out = subprocess.run(["nm", "-D" if dynamic else "-n", "--defined-only", path],
                         capture_output=True, text=True, check=True).stdout
    syms = []
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 3:
            try:
                v = int(parts[0], 16)
            except ValueError:
                continue
            if parts[2].startswith("$"):
                continue
            syms.append((v, parts[2]))
    return syms


def enclosing(syms, off):
    best = None
    for v, name in syms:
        if v <= off and (best is None or v > best[0]):
            best = (v, name)
    return "%s+0x%x" % (best[1], off - best[0]) if best else "?"


def main():
    bin_path, lvgl_path = sys.argv[1], sys.argv[2]
    bin_syms = symbols(bin_path, False)
    lvgl_syms = symbols(lvgl_path, False)

    main_link = next(v for v, n in bin_syms if n == "main")
    exe_base = BASE_MAIN - main_link

    print("main link=0x%x  base(runtime)=0x%x  base de carga del ELF=0x%x"
          % (main_link, BASE_MAIN, exe_base))
    print("liblvgl base (fijada por pc/lr) = 0x%x" % LVGL_BASE)
    print()
    print("== pc/lr dentro de liblvgl ==")
    print("pc=0x%x -> liblvgl+0x%x : %s" % (PC, PC - LVGL_BASE, enclosing(lvgl_syms, PC - LVGL_BASE)))
    print("lr=0x%x -> liblvgl+0x%x : %s" % (LR, LR - LVGL_BASE, enclosing(lvgl_syms, LR - LVGL_BASE)))
    print()
    print("== palabras altas de la pila dentro de liblvgl ==")
    for w in WORDS:
        print("0x%08x -> liblvgl+0x%x : %s" % (w, w - LVGL_BASE, enclosing(lvgl_syms, w - LVGL_BASE)))
    print()
    print("== palabras de la pila dentro del ejecutable (enlace = main + (palabra - base)) ==")
    for w in EXE_WORDS:
        link = main_link + w - BASE_MAIN
        print("0x%08x -> enlace 0x%x : %s" % (w, link, enclosing(bin_syms, link)))


if __name__ == "__main__":
    main()
