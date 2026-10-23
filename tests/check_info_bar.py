#!/usr/bin/env python3
"""Contrato de la barra izquierda de HOME (issue #251): tokens, pad y tipografia.

La barra son 3x2 celdas de 56x26 pintadas sobre el bitmap del fondo (`info_style`,
206 px legacy / 209 px simple, 61 px de alto). El issue #251 cierra su geometria:
**6 px de aire horizontal**, **pitch vertical 26** (sin el solapamiento de 4 px que
tenian las dos filas), **22 px de fuente en los estados compactos** (AGC, PRE/ATT,
ATU) y **20 px en VFO, modo y WiFi** -- los textos largos --, con cero clipping.

Este chequeo es el lado determinista y barato de ese contrato: lee los tokens de
`src/ui_tokens.h` y las decisiones locales de `src/info.c`, y falla si alguno se
sale de la caja (los limites 206/61 son los del fondo de la barra, `src/styles.c`).
El otro lado -- las cajas REALES -- lo mide el arnes: `ui_harness --inspect`
(ver docs/evidencia/2026-10-23-home251/).

SPDX-License-Identifier: GPL-2.0-or-later
"""

import ast
import re
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent / "src"
TOKENS = SRC / "ui_tokens.h"
INFO = SRC / "info.c"
STYLES = SRC / "styles.c"

# El fondo de la barra (info_style): 206 px en los bitmaps legacy y 209 en los
# simples, siempre 61 de alto (src/styles.c). El caso mas ajustado es 206.
BARRA_W = 206
BARRA_H = 61

# El ancho y el alto de la runa de BT (bt_pts[], src/info.c): 14x20.
RUNA_W, RUNA_H = 14, 20

# Las seis celdas de la barra, en orden de lectura, y la fuente que el contrato
# #251 les asigna: 22 px en los tres estados compactos (AGC, PRE/ATT, ATU) y 20 px
# en VFO, modo y WiFi. Las tres compactas llevan la fuente local del objeto;
# VFO/modo la re-fijan con el fallback de VISION y la celda de WiFi/BT hereda la de
# `info_item_style` (que sigue en sony_20).
CELDAS_BARRA = ("INFO_VFO", "INFO_MODE", "INFO_AGC", "INFO_PRE_ATT", "INFO_ATU", "INFO_WIFI")

fallos = []
tokens = {}

# --- 1. Los tokens de la rejilla (src/ui_tokens.h) -----------------------------
DEFINE = re.compile(r"^#define\s+(UI_INFO_[A-Z_]+)\s+(.+?)\s*(?:/\*.*)?$")


def evaluar(expr):
    """El valor entero de una expresion de tokens: solo numeros y + - * / ( ).

    Se evalua con `ast` (no `eval`) y con una lista blanca de nodos: el archivo es
    de tokens y una expresion que no sea aritmetica es un fallo, no un valor.
    """
    for _ in range(16):
        nombres = re.findall(r"[A-Za-z_][A-Za-z_0-9]*", expr)
        if not nombres:
            break
        for nombre in dict.fromkeys(nombres):
            if nombre not in tokens:
                raise KeyError(nombre)
            expr = re.sub(rf"\b{nombre}\b", str(tokens[nombre]), expr)
    return _aritmetica(ast.parse(expr, mode="eval").body)


def _aritmetica(nodo):
    if isinstance(nodo, ast.Constant) and isinstance(nodo.value, int):
        return nodo.value
    if isinstance(nodo, ast.BinOp) and isinstance(nodo.op, (ast.Add, ast.Sub, ast.Mult, ast.Div)):
        izq, der = _aritmetica(nodo.left), _aritmetica(nodo.right)
        if isinstance(nodo.op, ast.Add):
            return izq + der
        if isinstance(nodo.op, ast.Sub):
            return izq - der
        if isinstance(nodo.op, ast.Mult):
            return izq * der
        if der == 0:
            raise ValueError("division por cero en un token")
        return izq // der
    raise ValueError(f"token no aritmetico: {ast.dump(nodo)}")


for linea in TOKENS.read_text(errors="surrogateescape").split("\n"):
    m = DEFINE.match(linea)
    if not m:
        continue
    nombre, expr = m.group(1), m.group(2).strip()
    try:
        tokens[nombre] = evaluar(expr)
    except (KeyError, ValueError):
        continue

ESPERADOS = {
    "UI_INFO_CELL_W": 56,
    "UI_INFO_CELL_H": 26,
    "UI_INFO_CELL_GAP": 6,
    "UI_INFO_COL_PITCH": 62,
    "UI_INFO_ROW_PITCH": 26,
    "UI_INFO_COLS": 3,
    "UI_INFO_ROWS": 2,
    "UI_INFO_ORIGIN_X": 15,
    "UI_INFO_ORIGIN_Y": 5,
    "UI_INFO_WIFI_W": 28,
    "UI_INFO_WIFI_ROW": 1,
    "UI_INFO_BT_H": 20,
    "UI_INFO_BT_DY": 8,
    "UI_INFO_CELL_PAD_V": 2,
}
for nombre, esperado in ESPERADOS.items():
    if nombre not in tokens:
        fallos.append(f"src/ui_tokens.h: falta {nombre}")
    elif tokens[nombre] != esperado:
        fallos.append(f"src/ui_tokens.h: {nombre}={tokens[nombre]} (el contrato pide {esperado})")

if not fallos:
    w, h = tokens["UI_INFO_CELL_W"], tokens["UI_INFO_CELL_H"]
    gap, pitch = tokens["UI_INFO_CELL_GAP"], tokens["UI_INFO_ROW_PITCH"]
    cols, rows = tokens["UI_INFO_COLS"], tokens["UI_INFO_ROWS"]
    x0, y0 = tokens["UI_INFO_ORIGIN_X"], tokens["UI_INFO_ORIGIN_Y"]
    wifi_w, wifi_row, bt_dy = tokens["UI_INFO_WIFI_W"], tokens["UI_INFO_WIFI_ROW"], tokens["UI_INFO_BT_DY"]

    # Las columnas y las filas tienen que entrar en el fondo de la barra.
    borde_x = x0 + cols * w + (cols - 1) * gap
    borde_y = y0 + (rows - 1) * pitch + h
    runa_y = y0 + wifi_row * pitch + bt_dy + RUNA_H
    print(f"  rejilla: celdas {cols}x{rows} de {w}x{h} en ({x0},{y0})")
    print(f"  gap horizontal {gap} | pitch vertical {pitch} | col pitch {tokens['UI_INFO_COL_PITCH']}")
    print(f"  borde derecho {borde_x} <= {BARRA_W} | fondo de la ultima fila {borde_y} <= {BARRA_H}")
    print(f"  runa de BT (celda WiFi {wifi_w} px): fondo {runa_y} <= {BARRA_H}")

    if pitch < h:
        fallos.append(f"las filas se solapan {h - pitch} px (pitch {pitch} < celda {h})")
    if tokens["UI_INFO_COL_PITCH"] != w + gap:
        fallos.append("UI_INFO_COL_PITCH no es CELL_W + CELL_GAP")
    if borde_x > BARRA_W:
        fallos.append(f"la ultima celda llega a x={borde_x}, fuera de la barra ({BARRA_W} px)")
    if borde_y > BARRA_H:
        fallos.append(f"la ultima fila llega a y={borde_y}, fuera de la barra ({BARRA_H} px)")
    if runa_y > BARRA_H:
        fallos.append(f"la runa de BT llega a y={runa_y}, fuera de la barra ({BARRA_H} px)")
    if tokens["UI_INFO_WIFI_COL"] != cols - 1 or wifi_row != rows - 1:
        fallos.append("la celda de WiFi/BT dejo de ser la ultima de la rejilla")

# --- 2. Las decisiones locales de src/info.c -----------------------------------
lineas = INFO.read_text(errors="surrogateescape").split("\n")
texto = "\n".join(lineas)

# 2a. El pad vertical de la celda es local y de 2 px (26 - 2*2 = 22, la altura que
# pide la fuente de 22 px). No se toca `info_item_style`.
PAD = re.compile(r"lv_obj_set_style_pad_(top|bottom)\(\s*item\s*,\s*([A-Za-z_0-9]+)\s*,")
pads = [m.group(2) for m in PAD.finditer(texto)]
pad_valores = {tokens.get(p, p) for p in pads}
if len(pads) != 2:
    fallos.append("src/info.c: la celda no fija su pad superior e inferior local (2 llamadas)")
elif pad_valores != {2}:
    fallos.append(f"src/info.c: el pad vertical local de la celda es {sorted(pad_valores)} (el contrato pide 2)")
if re.search(r"lv_style_set_\w+\(\s*&info_item_style", texto):
    fallos.append("src/info.c: modifica info_item_style (el estilo compartido por las seis celdas)")

# 2b. Los tres estados compactos dibujan con la fuente grande y bajo VISION bajan
# los mismos tres pasos que VFO/modo, para no envolver la celda.
m = re.search(
    r"static const lv_font_t \*status_font_big\(void\)\s*\{(.*?)\n\}",
    texto,
    re.S,
)
if m is None:
    fallos.append("src/info.c: no esta el helper de la fuente grande de los estados")
elif not re.search(
    r"theme_shape\.vision\s*\?\s*font_down\(\s*UI_INFO_FONT_BIG\s*,\s*VISION_FONT_STEPS\s*\)"
    r"\s*:\s*UI_INFO_FONT_BIG",
    m.group(1),
):
    fallos.append("src/info.c: la fuente grande no es sony_22 con el descenso de VISION")
if "#define UI_INFO_FONT_BIG        (&sony_22)" not in TOKENS.read_text(errors="surrogateescape"):
    fallos.append("src/ui_tokens.h: la fuente grande de la barra dejo de ser sony_22")

for celda in ("INFO_AGC", "INFO_PRE_ATT", "INFO_ATU"):
    patron = rf"lv_obj_set_style_text_font\(items\[{celda}\],\s*status_font_big\(\),\s*0\)"
    if not re.search(patron, texto):
        fallos.append(f"src/info.c: {celda} no dibuja con la fuente de estado de 22 px")

# VFO y modo conservan el arreglo ya probado (UI-012/UI-014): sony_20, y bajo
# VISION los mismos tres pasos hacia abajo para que el ancho no cambie.
for celda in ("INFO_VFO", "INFO_MODE"):
    patron = (rf"lv_obj_set_style_text_font\(items\[{celda}\],\s*"
              rf"theme_shape\.vision \? font_down\(UI_INFO_FONT, VISION_FONT_STEPS\)"
              rf" : UI_INFO_FONT,\s*0\)")
    if not re.search(patron, texto):
        fallos.append(f"src/info.c: {celda} no conserva la fuente de 20 px con el fallback de VISION")

# La celda de WiFi/BT no lleva fuente local: hereda las 20 px de info_item_style
# (lo que tambien cubre los dos simbolos). El estilo compartido sigue en sony_20.
if re.search(r"lv_obj_set_style_text_font\(\s*items\[INFO_WIFI\]", texto):
    fallos.append("src/info.c: la celda de WiFi/BT no debe llevar fuente local (hereda las 20 px del estilo)")
if re.search(r"lv_obj_set_style_text_font\(\s*bt_item", texto):
    fallos.append("src/info.c: la runa de BT no lleva fuente (no es texto)")
if "lv_style_set_text_font(&info_item_style, &sony_20)" not in STYLES.read_text(errors="surrogateescape"):
    fallos.append("src/styles.c: la celda de la barra dejo de dibujar con sony_20 (info_item_style)")

# 2c. La runa de BT conserva su geometria (14x20, width 2) y su x/y salen de los
# tokens de la rejilla (sigue al WiFi sin tocar la celda de al lado).
puntos = re.findall(r"\{\s*(\d+),\s*(\d+)\s*\}", texto[texto.find("bt_pts[]"):texto.find("bt_pts[]") + 400])
if not puntos:
    fallos.append("src/info.c: no esta bt_pts[] (la runa de BT)")
else:
    xs = [int(x) for x, _ in puntos]
    ys = [int(y) for _, y in puntos]
    if max(xs) != RUNA_W or max(ys) != RUNA_H:
        fallos.append(f"src/info.c: la runa de BT mide {max(xs)}x{max(ys)} (el contrato pide {RUNA_W}x{RUNA_H})")
if "UI_INFO_BT_LINE_W, 0" not in texto:
    fallos.append("src/info.c: la runa de BT no conserva su ancho de linea (UI_INFO_BT_LINE_W)")
if not re.search(r"UI_INFO_WIFI_COL \* UI_INFO_COL_PITCH \+ UI_INFO_ORIGIN_X \+ UI_INFO_WIFI_W", texto):
    fallos.append("src/info.c: la runa de BT no se ubica con la formula de UI_INFO_COL_PITCH")
if re.search(r"items\[INFO_WIFI\][^\n]*sony_22", texto) or re.search(r"bt_item[^\n]*sony_22", texto):
    fallos.append("src/info.c: los simbolos de WiFi/BT no van con la fuente de 22 px")

print(f"  celdas del contrato: {len(CELDAS_BARRA)} | pad vertical local: {sorted(pad_valores, key=str)}")

if fallos:
    for f in fallos:
        print(f"  FALLA {f}")
    sys.exit(1)

print("  la barra izquierda cumple el contrato de #251 (6 px / pitch 26 / 22-20 / sin solape)")
