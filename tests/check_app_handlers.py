#!/usr/bin/env python3
"""Comprueba que TODOS los botones de las apps (y de la pantalla principal) tengan handler.

Un boton sin `.press` es un boton muerto: al apretarlo `buttons_press()` avisa "press action is
NULL" en el log y no hace nada. Aparecieron tres asi en las paginas de Field Ops (los titulos
FIELD OPS, BANDS y LOGBOOK) y salian en el log de la radio cada vez que la app los apretaba.
Ver docs/HALLAZGOS.md 33.

SPDX-License-Identifier: GPL-2.0-or-later
"""

import re
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent / "src"

# definiciones de botones y de paginas, por archivo (los nombres son locales a cada archivo)
definiciones = {}      # (archivo, nombre) -> texto del inicializador
paginas = []           # (archivo, nombre de pagina, [items])

# helpers de buttons.cpp que arman el boton completo (page/app/action siempre ponen .press)
HELPERS_OK = ("make_btn", "make_page_btn", "make_app_btn", "make_action_btn", "make_mem_btn")

for f in sorted(list(SRC.glob("*.c")) + list(SRC.glob("*.cpp")) + list(SRC.glob("cfg/*.c"))):
    texto = f.read_text(errors="surrogateescape")
    for m in re.finditer(r"button_(?:data|item)_t\s+(\w+)\s*=\s*\{([^}]*)\}", texto):
        definiciones[(f.name, m.group(1))] = m.group(2)
    for m in re.finditer(r"button_(?:data|item)_t\s+(\w+)\s*=\s*(make_\w*btn\w*)\s*\(", texto):
        definiciones[(f.name, m.group(1))] = ".press por " + m.group(2)
    for m in re.finditer(r"buttons_page_t\s+(\w+)\s*=\s*\{\s*(?:\.items\s*=\s*)?\{([^}]*)\}", texto):
        items = re.findall(r"&(\w+)", m.group(2))
        paginas.append((f.name, m.group(1), items))

fallos = []
avisos = []
chequeados = 0

for archivo, pagina, items in paginas:
    vistos = {}
    for it in items:
        chequeados += 1
        inicializador = definiciones.get((archivo, it))

        if inicializador is None:
            fallos.append(f"{archivo}: {pagina} usa {it}, que no esta definido en el archivo")
            continue

        if ".press" not in inicializador:
            etiqueta = re.search(r'\.label\s*=\s*"([^"]*)"', inicializador)
            nombre = etiqueta.group(1).replace("\\n", " ") if etiqueta else "?"
            fallos.append(f"{archivo}: {pagina}: el boton {it} ('{nombre}') NO tiene handler (.press)")

        # un item en dos paginas del mismo archivo puede quedar con label_obj NULL al descargar una
        if it in vistos and vistos[it] != pagina:
            avisos.append(f"{archivo}: {it} esta en {vistos[it]} y en {pagina}")

        vistos.setdefault(it, pagina)

print(f"  botones en paginas: {chequeados} | paginas: {len(paginas)} | sin handler: {len(fallos)}")
for a in sorted(set(avisos)):
    print(f"  (aviso) item compartido entre paginas: {a}")

if fallos:
    for f in fallos:
        print(f"  FALLA {f}")
    sys.exit(1)

print("  todos los botones de las apps tienen handler")
