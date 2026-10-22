#!/usr/bin/env python3
"""Comprueba que la zona de controles de Ajustes siga UN solo esquema de grilla (issue #175).

El defecto del #175 no era un bug de render: cada fila de `src/dialog_settings.c` elegia a mano su
columna inicial y su span, y la "columna derecha" habia quedado con TRES origenes distintos
(x=499 para los controles anchos, x=643 para los switches, x=595/691 para las filas de 3 campos).
La captura de la radio lo midio (docs/evidencia/2026-10-20-settings-alineacion/).

El arreglo define UN esquema y lo aplica a todas las filas. Este chequeo lo fija para que no se
pueda volver a la deriva sin que la corrida lo note. Es determinista y no necesita Docker: lee el
codigo y comprueba

  1. los valores de los #define del esquema (si alguien cambia el origen o un span, falla);
  2. que TODA colocacion de un control en la grilla use uno de los pares (columna, span) del
     esquema: no hay columnas ni spans "a mano" fuera de la tabla;
  3. que las cuatro formas de fila del esquema existan de verdad (una pieza, dos piezas, tres
     piezas y la unica excepcion del slider de brillo), asi que borrar el esquema no deja el
     chequeo pasando por vacio.

Los pares permitidos son los de la tabla del encabezado de dialog_settings.c:

    1 control    -> (COL_CONTROLES, SPAN_1)        285 px   499..784
    2 controles  -> (COL_CONTROLES, SPAN_2A)       primera media zona  499..640
                    (COL_2B, SPAN_2B)              segunda media zona  643..784
    3 controles  -> (col, SPAN_3) x3               un tercio cada uno  499/595/691
    excepcion    -> (COL_BRILLO, SPAN_BRILLO)      slider de brillo    595..784

La etiqueta de cada fila va en la columna 0 (`0, 1` o `col++, 1`): eso no es un control.

SPDX-License-Identifier: LGPL-2.1-or-later
"""

import re
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent / "src"
ARCHIVO = SRC / "dialog_settings.c"

# Valores que declara el esquema. Cambiar uno cambia donde arranca la columna derecha o cuanto
# ocupa cada control: es justo lo que el issue #175 fijo.
MACROS = {
    "COL_CONTROLES": 1,
    "SPAN_1": 6,
    "SPAN_2A": 3,
    "COL_2B": 4,
    "SPAN_2B": 3,
    "SPAN_3": 2,
    "COL_BRILLO": 3,
    "SPAN_BRILLO": 4,
}

# (columna, span) permitidos para un CONTROL (nunca para la etiqueta de la fila).
PERMITIDOS = {
    ("COL_CONTROLES", "SPAN_1"),
    ("COL_CONTROLES", "SPAN_2A"),
    ("COL_2B", "SPAN_2B"),
    ("col", "SPAN_3"),
    ("COL_BRILLO", "SPAN_BRILLO"),
}

# Columna/span de las etiquetas (columna 0 de la grilla).
ETIQUETAS = {("0", "1"), ("col++", "1")}

GRID_CELL = re.compile(
    r"lv_obj_set_grid_cell\(\s*[^,]+,\s*LV_GRID_ALIGN_START,\s*([^,]+?)\s*,\s*([^,]+?)\s*,"
)
DEFINE = re.compile(r"^#define\s+([A-Z_0-9]+)\s+(-?\d+)\b")

lineas = ARCHIVO.read_text(errors="surrogateescape").split("\n")

fallos = []

# --- 1. Los valores del esquema ---------------------------------------------------------------
valores = {}
for linea in lineas:
    m = DEFINE.match(linea)
    if m and m.group(1) in MACROS:
        valores[m.group(1)] = int(m.group(2))

for nombre, esperado in MACROS.items():
    if nombre not in valores:
        fallos.append(f"falta el #define {nombre} (el esquema de la grilla del #175)")
    elif valores[nombre] != esperado:
        fallos.append(
            f"#define {nombre} = {valores[nombre]}, se esperaba {esperado}: cambio el esquema "
            f"(origen o span) de la zona de controles del #175"
        )

# --- 2. Cada colocacion de la grilla usa un par del esquema -----------------------------------
controles = 0
etiquetas = 0
pares_vistos = set()

for i, linea in enumerate(lineas, start=1):
    for col, span in GRID_CELL.findall(linea):
        col, span = col.strip(), span.strip()
        if (col, span) in ETIQUETAS:
            etiquetas += 1
            continue
        controles += 1
        pares_vistos.add((col, span))
        if (col, span) not in PERMITIDOS:
            fallos.append(
                f"linea {i}: el control usa la columna {col} con span {span}, que no esta en el "
                f"esquema unico del #175 (permitidos: "
                f"{', '.join(f'{c}/{s}' for c, s in sorted(PERMITIDOS))})"
            )

# El listado de columnas fuera del esquema puede ser largo cuando la grilla entera se reescribe a
# mano: se corta a 8 y se resume el resto (el chequeo igual falla).
FUERA_DE_ESQUEMA_MAX = 8
_col_fuera = [f for f in fallos if "no esta en el esquema unico" in f]
if len(_col_fuera) > FUERA_DE_ESQUEMA_MAX:
    fallos = [f for f in fallos if f not in _col_fuera]
    fallos += _col_fuera[:FUERA_DE_ESQUEMA_MAX]
    fallos.append(
        f"... y {len(_col_fuera) - FUERA_DE_ESQUEMA_MAX} colocacion(es) mas fuera del esquema"
    )

# --- 3. Las formas de fila del esquema existen -----------------------------------------------
formas = {
    "1 control (span 6)": ("COL_CONTROLES", "SPAN_1"),
    "2 controles (media + media)": ("COL_CONTROLES", "SPAN_2A"),
    "2 controles (la segunda media)": ("COL_2B", "SPAN_2B"),
    "3 controles (un tercio)": ("col", "SPAN_3"),
    "excepcion del slider de brillo": ("COL_BRILLO", "SPAN_BRILLO"),
}
for nombre, par in formas.items():
    if par not in pares_vistos:
        fallos.append(
            f"no hay ninguna fila que use '{nombre}' ({par[0]}/{par[1]}): ¿se borro una forma "
            f"de fila del esquema del #175?"
        )

print(f"  macros del esquema: {len(valores)}/{len(MACROS)}")
print(f"  filas de control colocadas: {controles} | etiquetas: {etiquetas}")
print(f"  formas de fila presentes: {len(pares_vistos)}")

if fallos:
    for f in fallos:
        print(f"  FALLA {f}")
    sys.exit(1)

print("  la zona de controles de Ajustes usa un solo esquema de grilla (origen unico x=499)")
