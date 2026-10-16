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

Y: los campos de la fila de fecha (`make_date`) declaran la MISMA caja. El campo del Año se
dimensionaba por contenido y quedaba mas angosto que Dia y Mes (issue #110: 70 px contra 93,
medido con `ui_harness --inspect` sobre settings-home), asi que la fila se veia desalineada.
Los tres son columnas de la misma grilla: mismo ancho y mismo alto declarados.

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

# --- Los campos de la fila de fecha comparten la caja -----------------------------------------
# `make_date()` crea tres spinboxes (Dia, Mes, Ano) que son columnas de la misma grilla: los tres
# tienen que declarar el mismo `lv_obj_set_size`. El Ano se dimensionaba por contenido (el helper
# `ancho_campo_cifras`) y quedaba mas angosto que Dia y Mes (issue #110: 70 px contra 93, medido
# con `ui_harness --inspect` sobre settings-home), asi que la fila se veia desalineada. Con el
# check, un ancho por contenido no vuelve sin que la corrida lo note.
SET_SIZE = re.compile(r"lv_obj_set_size\(\s*(.*?)\s*\)\s*;")
SPINBOX_CREA = "lv_spinbox_create("
CAMPOS_FECHA = 3  # Dia, Mes, Ano


def argumentos(llamada):
    """Los argumentos de una llamada, separados por las comas de primer nivel."""
    partes, nivel, actual = [], 0, ""
    for c in llamada:
        if c == "(":
            nivel += 1
        elif c == ")":
            nivel -= 1
        if c == "," and nivel == 0:
            partes.append(actual.strip())
            actual = ""
        else:
            actual += c
    partes.append(actual.strip())
    return partes


cuerpo_fecha = cuerpos.get("make_date")
cajas = []
if cuerpo_fecha is None:
    fallos.append("no esta make_date() en este archivo: no se puede comprobar la fila de fecha")
else:
    # Un bloque por spinbox: del `lv_spinbox_create(` al siguiente; cada bloque declara su caja.
    for i, bloque in enumerate(cuerpo_fecha.split(SPINBOX_CREA)[1:], start=1):
        tam = SET_SIZE.search(bloque)
        if tam is None:
            fallos.append(f"make_date(): el spinbox {i} no declara su caja con lv_obj_set_size()")
            continue
        cajas.append((i, *argumentos(tam.group(1))[1:3]))

    if len(cajas) != CAMPOS_FECHA:
        fallos.append(
            f"make_date(): {len(cajas)} spinbox(es) con caja declarada, se esperaban "
            f"{CAMPOS_FECHA} (Dia, Mes, Ano)"
        )
    elif len({ancho for _, ancho, _ in cajas}) > 1 or len({alto for _, _, alto in cajas}) > 1:
        detalle = ", ".join(f"#{i} {ancho} x {alto}" for i, ancho, alto in cajas)
        fallos.append(
            f"make_date(): los campos de la fila no comparten la caja ({detalle}) -> uno queda "
            f"mas angosto que el resto y la fila se ve desalineada"
        )

# El total tiene que cerrar: cada widget creado, registrado.
total_registrados = sum(linea.count("dialog_item(&dialog") for linea in lineas)
print(f"  widgets de foco creados: {creados} | dialog_item(): {total_registrados}")
print(f"  sliders con parametro propio: {sliders}")
print(f"  campos de la fila de fecha: {len(cajas) if cuerpo_fecha is not None else 0}")

if fallos:
    for f in fallos:
        print(f"  FALLA {f}")
    sys.exit(1)

print("  todos los widgets de Ajustes estan en el grupo de foco")
print("  cada slider mueve el parametro que muestra")
print("  los campos de la fila de fecha declaran la misma caja")
