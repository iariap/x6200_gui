#!/usr/bin/env python3
#
# Enumera las bases de carga 0x1000-alineadas de liblvgl y se queda con las que
# explican TODA la evidencia del CRASHLOG #131:
#
#   - pc en una instruccion de acceso a memoria (la que fallo con 0xf363);
#   - lr y las palabras altas de la pila dentro de funciones;
#   - lr y esas palabras justo despues de un bl/blx (direcciones de retorno).
#
#   verifica-base-131.py <liblvgl.so> <objdump-arm> <nm-arm>
#
# SPDX-License-Identifier: LGPL-2.1-or-later
import bisect
import re
import subprocess
import sys

PC = 0xB6186AE8
LR = 0xB618D4F0
WORDS = [0xB618DB48, 0xB6196128, 0xB619605C]
MEM = ("ldr", "str", "ldrd", "strd", "ldm", "stm", "ldrex", "strex",
       "push", "pop", "ldrb", "strb", "ldrh", "strh")


def main():
    so, objdump, nm = sys.argv[1], sys.argv[2], sys.argv[3]

    text = subprocess.run([objdump, "-d", so], capture_output=True, text=True, check=True).stdout
    ins = []
    for line in text.splitlines():
        m = re.match(r"^\s*([0-9a-f]+):\s+[0-9a-f]+\s+(\S+)", line)
        if m:
            ins.append((int(m.group(1), 16), m.group(2)))

    calls = set()       # direccion que sigue a un bl/blx = retorno
    memory = set()      # instrucciones que pueden fallar por un puntero malo
    for i, (addr, mnem) in enumerate(ins):
        base = mnem.split(".")[0]
        if base in ("bl", "blx") and i + 1 < len(ins):
            calls.add(ins[i + 1][0])
        if base in MEM:
            memory.add(addr)

    starts = []
    for line in subprocess.run([nm, "-n", "--defined-only", so],
                               capture_output=True, text=True, check=True).stdout.splitlines():
        p = line.split()
        if len(p) >= 3:
            try:
                starts.append(int(p[0], 16))
            except ValueError:
                pass
    starts.sort()

    def in_function(off):
        return bisect.bisect_right(starts, off) > 0

    base_min = (PC - max(memory)) & ~0xFFF
    base_max = (PC - min(memory)) & ~0xFFF
    found = []
    for base in range(base_min, base_max + 1, 0x1000):
        opc, olr = PC - base, LR - base
        if opc not in memory or olr not in calls:
            continue
        if not in_function(opc) or not in_function(olr):
            continue
        if all(in_function(w - base) and (w - base) in calls for w in WORDS):
            found.append(base)

    print("pc=0x%x lr=0x%x palabras=%s" % (PC, LR, [hex(w) for w in WORDS]))
    print("candidatos 0x1000-alineados que cumplen TODO: %d" % len(found))
    for b in found:
        print("  base=0x%x  pc=+0x%x  lr=+0x%x" % (b, PC - b, LR - b))


if __name__ == "__main__":
    main()
