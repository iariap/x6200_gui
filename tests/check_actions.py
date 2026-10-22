#!/usr/bin/env python3
"""Comprueba que toda accion que ofrece Ajustes tenga su `case` en algun lado.

Una accion sin `case` es un boton que no hace nada: el usuario la asigna y no pasa nada. Ya
paso con ACTION_WF_MODE, que quedo en la lista de Ajustes sin manejador.

SPDX-License-Identifier: GPL-2.0-or-later
"""

import re
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent / "src"
fallos = []

# Acciones que ofrece Ajustes (la lista de la que el usuario elige)
settings = SRC / "dialog_settings.c"
ofrecidas = set(re.findall(r"\.action = (ACTION_[A-Z0-9_]+)", settings.read_text(errors="surrogateescape")))

# Todas las acciones con manejador, en cualquier archivo
manejadas = set()
for f in list(SRC.glob("*.c")) + list(SRC.glob("*.cpp")):
    manejadas |= set(re.findall(r"case (ACTION_[A-Z0-9_]+)\s*:", f.read_text(errors="surrogateescape")))

for a in sorted(ofrecidas - manejadas - {"ACTION_NONE"}):
    fallos.append(f"{a} esta en la lista de Ajustes y no tiene case")

for f in sorted(manejadas - ofrecidas):
    # no es un fallo: hay acciones que no se ofrecen (por ejemplo las de la propia lista)
    pass

print(f"  acciones ofrecidas: {len(ofrecidas)} | con manejador: {len(manejadas)}")

if fallos:
    for f in fallos:
        print("  FALLO:", f)
    sys.exit(1)

print("  todas las acciones ofrecidas tienen manejador")
