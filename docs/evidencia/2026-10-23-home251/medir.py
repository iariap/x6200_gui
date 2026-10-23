#!/usr/bin/env python3
"""Geometria real de la barra izquierda (HOME) medida con `ui_harness --inspect`.

Lee los JSON del arnes (uno por fixture) y extrae, del arbol LVGL vivo, las seis
celdas de `src/info.c`: su caja, el pitch horizontal/vertical, la fuente con la
que dibuja cada una y si el texto entra (ancho natural vs ancho de la celda).

Imprime una tabla por fixture y falla (exit 1) si alguna celda no cumple el
contrato del issue #251: gap horizontal 6 px, pitch vertical 26, fuente 22 en
AGC/PRE-ATT/ATU y 20 en VFO/MODE/WiFi, texto sin envolver, sin truncar y sin
recorte (caja visible == caja).

Uso: python3 medir.py <dir-con-jsons> [...]

SPDX-License-Identifier: GPL-2.0-or-later
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

CELDAS = {
    "VFO": 0,  # indice dentro de la fila: VFO, MODE, AGC / PRE-ATT, ATU, WIFI
    "MODE": 1,
    "AGC": 2,
    "PRE-ATT": 3,
    "ATU": 4,
    "WIFI": 5,
}
FUENTES = {
    "AGC": "sony_22",
    "PRE-ATT": "sony_22",
    "ATU": "sony_22",
    "VFO": "sony_20",
    "MODE": "sony_20",
    "WIFI": "sony_20",
}


def nodos(arbol: list[dict]):
    for nodo in arbol:
        yield nodo
        yield from nodos(nodo.get("children", []))


def panel_info(arbol: list[dict]) -> dict | None:
    """El objeto de la barra izquierda (info.c: 206 px legacy / 209 px simple, 61 alto)."""
    for n in nodos(arbol):
        if n.get("x") == 0 and n.get("y") == 0 and n.get("h") == 61 and n.get("w") in (206, 209):
            return n
    return None


def celdas(doc: dict) -> tuple[list[dict], list[dict], int]:
    """Las seis celdas de la barra (en orden de lectura) y sus dos filas flex."""
    ins = doc.get("inspection") or {}
    panel = panel_info(ins.get("nodes", []))
    if panel is None:
        return [], [], 0
    filas = [n for n in panel.get("children", []) if n.get("h") == 26 and n.get("w", 0) > 56]
    filas.sort(key=lambda n: n["y"])
    celdas = [n for fila in filas for n in fila.get("children", []) if n.get("class") == "lv_label"]
    return celdas, filas, panel["w"]


def medir(ruta: Path) -> list[str]:
    doc = json.loads(ruta.read_text())
    linea = f"-- {ruta.name}"
    c, filas, panel_w = celdas(doc)
    fallos: list[str] = []
    if len(c) != 6:
        return [f"{linea}: {len(c)} celdas en la barra izquierda (se esperaban 6)"]
    print(f"\n== {ruta.name} ==")
    print(f"   barra {panel_w} px | filas {[f['y'] for f in filas]} ancho fila {[f['w'] for f in filas]}")
    print(
        f"{'celda':8} {'x':>4} {'y':>4} {'w':>4} {'h':>4} {'fuente':8} "
        f"{'natural':>7} {'box':>4} {'visible':>16} long/ovf/trunc"
    )
    for nombre, i in CELDAS.items():
        n = c[i]
        vis = n.get("visible") or {}
        visible = f"{vis.get('w')}x{vis.get('h')}"
        natural = (n.get("text_natural") or {}).get("w")
        box = (n.get("text_box") or {}).get("w")
        print(
            f"{nombre:8} {n['x']:>4} {n['y']:>4} {n['w']:>4} {n['h']:>4} {n['text_font']:8} "
            f"{natural!s:>7} {box!s:>4} {visible:>16} "
            f"{n['long_mode']}/{n['text_overflow']}/{n['text_truncated']}  {n['text']!r}"
        )
        if n["text_font"] != FUENTES[nombre]:
            fallos.append(f"{linea}: {nombre} dibuja con {n['text_font']} (se esperaba {FUENTES[nombre]})")
        if n["long_mode"] != "CLIP" and natural is not None and box is not None and natural > box:
            fallos.append(
                f"{linea}: {nombre} ({n['text']!r}) envuelve: natural {natural} > box {box} en la celda de 56"
            )
        if n["text_truncated"] or n["text_overflow"]:
            fallos.append(f"{linea}: {nombre} ({n['text']!r}) se corta (overflow/truncated)")
        if vis.get("w") != n["w"] or vis.get("h") != n["h"]:
            fallos.append(f"{linea}: {nombre} recortada: caja {n['w']}x{n['h']} visible {visible}")
    gaps = [c[i + 1]["x"] - c[i]["x"] - 56 for i in (0, 1, 3, 4)]
    pitches = [filas[1]["y"] - filas[0]["y"]] if len(filas) == 2 else []
    borde_x = c[5]["x"] + 56
    y2 = filas[-1]["y"] + 26 if filas else 0
    runa = [n for n in nodos([panel_info(doc["inspection"]["nodes"]) or {}]) if n.get("class") == "lv_line"]
    print(
        f"   gap horizontal: {gaps}  pitch vertical: {pitches}  "
        f"borde derecho ultima celda x={borde_x} (barra {panel_w})  "
        f"fondo de la ultima fila y={y2} (barra 61)"
    )
    if runa:
        r = runa[0]
        print(f"   runa BT: {r['x']},{r['y']} {r['w']}x{r['h']} -> fondo y={r['y'] + r['h']}")
    if set(gaps) != {6}:
        fallos.append(f"{linea}: gap horizontal {gaps} (se esperaba 6 px)")
    if pitches != [26]:
        fallos.append(f"{linea}: pitch vertical {pitches} (se esperaba 26)")
    if pitches and pitches[0] < 26:
        fallos.append(f"{linea}: las filas se solapan {26 - pitches[0]} px")
    if borde_x > panel_w:
        fallos.append(f"{linea}: la ultima celda llega a x={borde_x}, fuera de la barra ({panel_w} px)")
    if y2 > 61:
        fallos.append(f"{linea}: la ultima fila llega a y={y2}, fuera de la barra (61 px)")
    if runa and runa[0]["y"] + runa[0]["h"] > 61:
        fallos.append(f"{linea}: la runa de BT llega a y={runa[0]['y'] + runa[0]['h']}, fuera de la barra")
    return fallos


def main(argv: list[str]) -> int:
    rutas = []
    csv = None
    args = list(argv)
    if "--csv" in args:
        i = args.index("--csv")
        csv = Path(args[i + 1])
        del args[i : i + 2]
    rutas = sorted(Path(p).glob("*.json") for p in args)
    fallos: list[str] = []
    filas: list[str] = []
    total = 0
    for grupo in rutas:
        for ruta in sorted(grupo):
            doc = json.loads(ruta.read_text())
            c, filas_flex, panel_w = celdas(doc)
            for nombre, i in CELDAS.items():
                if i >= len(c):
                    continue
                n = c[i]
                vis = n.get("visible") or {}
                filas.append(
                    ",".join(
                        str(v)
                        for v in (
                            ruta.stem,
                            nombre,
                            n["x"],
                            n["y"],
                            n["w"],
                            n["h"],
                            n["text_font"],
                            (n.get("text_natural") or {}).get("w"),
                            (n.get("text_natural") or {}).get("h"),
                            (n.get("text_box") or {}).get("w"),
                            (n.get("text_box") or {}).get("h"),
                            n["long_mode"],
                            n["text_overflow"],
                            n["text_truncated"],
                            vis.get("w"),
                            vis.get("h"),
                            panel_w,
                            filas_flex[-1]["y"] + 26 if filas_flex else "",
                            n.get("text", "").replace(",", " "),
                        )
                    )
                )
            fallos.extend(medir(ruta))
            total += 1
    if csv is not None:
        csv.write_text(
            "fixture,celda,x,y,w,h,fuente,natural_w,natural_h,box_w,box_h,long_mode,"
            "overflow,truncado,visible_w,visible_h,barra_w,fila2_y2,texto\n" + "\n".join(filas) + "\n"
        )
    print(f"\n{total} fixture(s) medidos, {len(fallos)} incumplimiento(s)")
    for f in fallos:
        print(f"  FALLA {f}")
    return 1 if fallos else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
