#!/usr/bin/env python3
#
# Resuelve la pila del CRASHLOG del issue #116 con los artefactos EXACTOS que corre
# la radio (md5 verificados contra /usr/sbin/x6200_gui y /lib/libc.so.6 de la radio).
#
#   resuelve-pila.py <x6200_gui> <libc.so.6>
#
# El handler de crashes (src/main.c) imprime la base del binario (&main) y 160
# palabras de pila. Con `nm` se saca el offset de main en el ELF y con eso se pasa
# cada direccion de la pila a su direccion de enlace:
#
#   enlace = link(main) + (palabra - base)
#
# Para una direccion dentro de una libreria (el pc cae en libc) lo que importa es el
# offset relativo a la base de la libreria; la base de libc se fija con el par pc/lr
# (strlen/strdup: el `blx strlen` de __strdup queda 0x950 abajo del pc).
#
# SPDX-License-Identifier: LGPL-2.1-or-later
import subprocess
import sys

# Del CRASHLOG del issue #116:
PC = 0xB5DE0064
LR = 0xB5DDF714
BASE = 0x44EA74          # &main en runtime
LIBC_BASE = 0xB5D40000   # unica base 0x10000-alineada con pc en strlen y lr en strdup


def symbols(path, dynamic):
    args = ["nm", "-D" if dynamic else "-n", "--defined-only", path]
    out = subprocess.run(args, capture_output=True, text=True, check=True).stdout
    syms = []
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 3:
            try:
                v = int(parts[0], 16)
            except ValueError:
                continue
            # los marcadores de mapeo ARM ($a/$d/$t) no son funciones: no sirven
            # para atribuir una direccion.
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
    bin_path, libc_path = sys.argv[1], sys.argv[2]
    bin_syms = symbols(bin_path, dynamic=False)
    libc_syms = symbols(libc_path, dynamic=True)

    main_link = next(v for v, n in bin_syms if n == "main")
    print("main link=0x%x  base(runtime)=0x%x  base de carga del ELF=0x%x"
          % (main_link, BASE, BASE - main_link))
    print("libc base (fijada por pc/lr) = 0x%x" % LIBC_BASE)
    print()
    print("== pc/lr dentro de libc (offsets relativos a la base) ==")
    print("pc=0x%x -> libc+0x%x : %s" % (PC, PC - LIBC_BASE, enclosing(libc_syms, PC - LIBC_BASE)))
    print("lr=0x%x -> libc+0x%x : %s" % (LR, LR - LIBC_BASE, enclosing(libc_syms, LR - LIBC_BASE)))
    print()
    print("== cadenas de la pila dentro del binario (enlace = main + (palabra - base)) ==")
    # sp[5]  = 0047dacc  (lr guardado por strdup: vuelve a cw_encoder_send, el `bl strdup`)
    # sp[21] = 004773cc  (lr guardado por cw_encoder_send: vuelve a dialog_msg_cw_beacon_cb)
    for word in (0x0047DACC, 0x004773CC):
        link = main_link + word - BASE
        print("0x%08x -> enlace 0x%x : %s" % (word, link, enclosing(bin_syms, link)))


if __name__ == "__main__":
    main()
