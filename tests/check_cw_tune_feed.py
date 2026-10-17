#!/usr/bin/env python3
"""Comprueba que el feed del tono de CW siga a la visibilidad del widget (issue #122).

El widget de sintonia (src/cw_tune_ui.c) dibuja el bloque activo con la frecuencia que le empuja
`cw_tune_set_freq()`, y a esa funcion la llama el feed del tono (`src/cw.cpp`,
`cw_put_audio_samples` -> `update_peak_freq`). El feed no corre en el arnes de UI (ahi no hay DSP:
`src/cw.cpp` no se compila) ni en el banco (sin placa de sonido no llegan muestras), asi que la
unica forma de que esto no se rompa en silencio es mirar la fuente: sin este chequeo, volver a
gatear el feed por un parametro dejaria el widget visible pero con el bloque clavado en el centro,
y ninguna fixture lo veria.

Dos cosas, las dos que causaron el issue #122 y su arreglo:

1. La visibilidad del widget (src/cw_tune_ui.c, `update_visibility`) se decide por el MODO (CW/CWR)
   y no por el parametro `cfg.cw_tune` (el boton "Tuner"), que arranca en 0 (src/cfg/cfg.c:349) y
   dejaba el widget oculto en CW.
2. El gate del feed (src/cw.cpp, `cw_put_audio_samples`) consulta `cw_tune_ui_visible()`, que es lo
   que dice si el widget que hay que alimentar esta a la vista.

Se leen los cuerpos de las dos funciones con los comentarios sacados: la prosa explica el cambio y
menciona los nombres, el codigo es lo que se mira.

SPDX-License-Identifier: GPL-2.0-or-later
"""

import re
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent / "src"
fallos = []


def sin_comentarios(texto: str) -> str:
    """El fuente sin comentarios (de bloque y de linea), respetando los literales."""
    salida = []
    i = 0
    n = len(texto)
    while i < n:
        c = texto[i]
        if c == '"' or c == "'":
            comilla = c
            salida.append(c)
            i += 1
            while i < n:
                salida.append(texto[i])
                if texto[i] == "\\":
                    if i + 1 < n:
                        salida.append(texto[i + 1])
                        i += 1
                elif texto[i] == comilla:
                    break
                i += 1
            i += 1
        elif texto.startswith("/*", i):
            fin = texto.find("*/", i + 2)
            i = n if fin < 0 else fin + 2
            salida.append(" ")
        elif texto.startswith("//", i):
            fin = texto.find("\n", i)
            i = n if fin < 0 else fin
            salida.append(" ")
        else:
            salida.append(c)
            i += 1
    return "".join(salida)


def cuerpo(texto: str, firma: str) -> str:
    """El cuerpo de la funcion cuya firma empieza con `firma`, por llaves balanceadas.

    Un prototipo (`...;`) no es la definicion: se sigue buscando desde ahi, porque el `{` que
    viene despues del prototipo es el de otra funcion.
    """
    desde = 0
    while True:
        i = texto.find(firma, desde)
        if i < 0:
            return ""
        abre = texto.find("{", i)
        cierra = texto.find(";", i)
        if abre < 0:
            return ""
        if 0 <= cierra < abre:
            desde = cierra + 1
            continue
        nivel = 0
        for j in range(abre, len(texto)):
            if texto[j] == "{":
                nivel += 1
            elif texto[j] == "}":
                nivel -= 1
                if nivel == 0:
                    return texto[abre : j + 1]
        return ""


# --- 1. la visibilidad no depende del parametro ---------------------------------------------
ui_c = (SRC / "cw_tune_ui.c").read_text(errors="surrogateescape")
vis = cuerpo(sin_comentarios(ui_c), "static void update_visibility(")

if not vis:
    fallos.append("src/cw_tune_ui.c: no encontre el cuerpo de update_visibility()")
else:
    if "cw_tune" in vis:
        fallos.append(
            "src/cw_tune_ui.c: update_visibility() vuelve a mirar `cw_tune`: con el parametro en su "
            "valor de arranque (0) el widget queda oculto en CW, que es el issue #122"
        )
    if "cfg_cur.mode" not in vis:
        fallos.append("src/cw_tune_ui.c: update_visibility() no mira el modo (cfg_cur.mode)")

# --- 2. el accesor que el feed consulta -------------------------------------------------------
ui_h = (SRC / "cw_tune_ui.h").read_text(errors="surrogateescape")
accesor = cuerpo(sin_comentarios(ui_c), "bool cw_tune_ui_visible(")

if "cw_tune_ui_visible" not in ui_h:
    fallos.append("src/cw_tune_ui.h: falta la declaracion de cw_tune_ui_visible()")
if not accesor:
    fallos.append("src/cw_tune_ui.c: no encontre el cuerpo de cw_tune_ui_visible()")
elif "LV_OBJ_FLAG_HIDDEN" not in accesor:
    fallos.append(
        "src/cw_tune_ui.c: cw_tune_ui_visible() no mira LV_OBJ_FLAG_HIDDEN: no dice si el widget "
        "esta a la vista"
    )

# --- 3. el gate del feed ----------------------------------------------------------------------
cw_cpp = (SRC / "cw.cpp").read_text(errors="surrogateescape")
feed = cuerpo(sin_comentarios(cw_cpp), "void cw_put_audio_samples(")

if not feed:
    fallos.append("src/cw.cpp: no encontre el cuerpo de cw_put_audio_samples()")
else:
    if "cw_tune_ui_visible()" not in feed:
        fallos.append(
            "src/cw.cpp: el gate de cw_put_audio_samples() no consulta cw_tune_ui_visible(): el "
            "widget de sintonia quedaria sin el feed que mueve su bloque activo"
        )

print(f"  cw_tune_ui.c update_visibility: {'ok' if vis and 'cw_tune' not in vis else 'FALLA'}")
print(f"  cw_tune_ui_visible: {'ok' if accesor else 'FALLA'}")
print(f"  cw.cpp feed gate: {'ok' if feed and 'cw_tune_ui_visible()' in feed else 'FALLA'}")

if fallos:
    for f in fallos:
        print(f"  FALLO: {f}")
    sys.exit(1)

print("  el feed del tono sigue a la visibilidad del widget de CW")
