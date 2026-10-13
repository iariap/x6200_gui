#!/usr/bin/env python3
"""Comprueba que TODOS los widgets de Ajustes entren en el grupo de foco.

Un desplegable, switch o slider que se crea sin `dialog_item(&dialog, obj)` no entra en
`keyboard_group`: con las teclas y el MFK no se puede ni llegar ni cambiar. Paso dos veces (el
desplegable de temas y los dos switches del Bluetooth, ver docs/HALLAZGOS.md 5 y 18), asi que ahora
se comprueba solo.

SPDX-License-Identifier: GPL-2.0-or-later
"""

import re
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent / "src"
ARCHIVO = SRC / "dialog_settings.c"

CREA = re.compile(r"lv_(dropdown|switch|slider|spinbox)_create\(")
# Una definicion de funcion en la columna 0: "static lv_obj_t *make_lo_que_sea(...) {"
FUNC = re.compile(r"^(?:static\s+)?[A-Za-z_][A-Za-z0-9_ \*]*\([^;]*\)\s*\{\s*$")

lineas = ARCHIVO.read_text(errors="surrogateescape").split("\n")


def funcion_de(i):
    """La funcion que contiene la linea i (indice 0)."""
    for j in range(i, -1, -1):
        if FUNC.match(lineas[j]):
            return j, lineas[j].strip()
    return 0, "(fuera de una funcion)"


def fin_de_funcion(inicio):
    profundidad = 0
    for j in range(inicio, len(lineas)):
        profundidad += lineas[j].count("{") - lineas[j].count("}")
        if profundidad == 0 and j > inicio:
            return j
    return len(lineas) - 1


fallos = []
creados = 0

for i, linea in enumerate(lineas):
    if not CREA.search(linea):
        continue

    creados += 1
    inicio, nombre = funcion_de(i)
    cuerpo = "\n".join(lineas[inicio:fin_de_funcion(inicio) + 1])
    en_funcion = len(CREA.findall(cuerpo))
    registrados = cuerpo.count("dialog_item(&dialog")

    if registrados < en_funcion:
        fallos.append(
            f"linea {i + 1}: {nombre} crea {en_funcion} widget(s) y registra {registrados} "
            f"-> hay uno que no se puede alcanzar con las teclas"
        )

# El total tiene que cerrar: cada widget creado, registrado.
total_registrados = sum(linea.count("dialog_item(&dialog") for linea in lineas)
print(f"  widgets de foco creados: {creados} | dialog_item(): {total_registrados}")

if fallos:
    for f in fallos:
        print(f"  FALLA {f}")
    sys.exit(1)

print("  todos los widgets de Ajustes estan en el grupo de foco")
