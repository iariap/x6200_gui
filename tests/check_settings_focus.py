#!/usr/bin/env python3
"""Comprueba que TODOS los widgets de Ajustes entren en el grupo de foco.

Un desplegable, switch o slider que se crea sin `dialog_item(&dialog, obj)` no entra en
`keyboard_group`: con las teclas y el MFK no se puede ni llegar ni cambiar. Paso dos veces (el
desplegable de temas y los dos switches del Bluetooth, ver docs/HALLAZGOS.md 5 y 18), asi que ahora
se comprueba solo.

Ademas: cada slider de Ajustes tiene que mover el parametro que muestra. El callback de un slider
puede llamar a cualquier cosa (son acciones cortas y parecidas), asi que se comprueba por nombre:
el cuerpo del callback tiene que mencionar el parametro que el slider muestra. El de "Rec gain"
salio copiado del de "Play gain" y llamaba a audio_set_play_vol, asi que arrastrarlo movia el
volumen de salida y el gain de entrada no se aplicaba nunca (issue #26).

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

# --- Cada slider mueve el parametro que muestra ----------------------------------------------
# El segundo argumento de slider_with_text() es el parametro que el slider muestra; el ultimo es
# su callback. El callback tiene que tocar ESE parametro: un copiar/pegar cruzado no se ve en la
# pantalla (los dos sliders se ven iguales) y solo se nota al usarlos (issue #26).
SLIDER = re.compile(r"\bslider_with_text\(([^;]*)\)\s*;")


def cuerpos_de_funciones():
    """nombre de la funcion -> cuerpo, para las definiciones de este archivo."""
    out = {}
    for j, linea in enumerate(lineas):
        if FUNC.match(linea):
            out[linea.split("(")[0].split()[-1]] = "\n".join(lineas[j : fin_de_funcion(j) + 1])
    return out


# Lo que el callback *aplica* (la funcion que mueve el audio, la linea, etc.). Dos sliders
# distintos no pueden aplicar la misma: en el bug del #26 los dos sliders de ganancia
# llamaban a audio_set_play_vol, o sea que uno de los dos movia la ganancia del otro.
APLICA = re.compile(r"\b((?:audio_set|radio_set)_[a-z0-9_]+)\s*\(")

cuerpos = cuerpos_de_funciones()
sliders = 0
aplicadas = {}  # funcion aplicada -> (parametro, callback, linea)

for i, linea in enumerate(lineas):
    for args in SLIDER.findall(linea):
        partes = [a.strip() for a in args.split(",")]
        if len(partes) < 3 or not partes[1].startswith("params."):
            continue

        sliders += 1
        parametro = re.match(r"params\.([a-z0-9_]+)", partes[1]).group(1)
        callback = partes[-1]
        cuerpo = cuerpos.get(callback)

        if cuerpo is None:
            fallos.append(
                f"linea {i + 1}: el slider de params.{parametro} usa {callback}(), que no "
                f"esta definido en este archivo: no se puede comprobar que lo mueva"
            )
            continue

        if parametro not in cuerpo:
            fallos.append(
                f"linea {i + 1}: el slider de params.{parametro} llama a {callback}(), que no "
                f"toca ese parametro -> mueve otra cosa (copiar/pegar cruzado)"
            )

        for funcion in set(APLICA.findall(cuerpo)):
            antes = aplicadas.get(funcion)

            if antes is not None and antes[1] != callback:
                fallos.append(
                    f"linea {i + 1}: el slider de params.{parametro} ({callback}) y el de "
                    f"params.{antes[0]} ({antes[1]}, linea {antes[2]}) aplican los dos "
                    f"{funcion}() -> uno de los dos mueve lo del otro"
                )
            aplicadas.setdefault(funcion, (parametro, callback, i + 1))

# El total tiene que cerrar: cada widget creado, registrado.
total_registrados = sum(linea.count("dialog_item(&dialog") for linea in lineas)
print(f"  widgets de foco creados: {creados} | dialog_item(): {total_registrados}")
print(f"  sliders con parametro propio: {sliders}")

if fallos:
    for f in fallos:
        print(f"  FALLA {f}")
    sys.exit(1)

print("  todos los widgets de Ajustes estan en el grupo de foco")
print("  cada slider mueve el parametro que muestra")
