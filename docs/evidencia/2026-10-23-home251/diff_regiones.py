#!/usr/bin/env python3
"""Radio de impacto del cambio de la barra (issue #251): cuantos pixeles se mueven
y si alguno cae fuera de la barra superior IZQUIERDA.

Compara el PNG "antes" y el "despues" de cada fixture (misma escena, misma
revision del arnes: solo cambia src/info.c + src/ui_tokens.h) con el umbral de
pixeles del pipeline A/B (|dR|+|dG|+|dB| > 12, quality/budgets.yaml).

La region que el cambio puede tocar es la barra de `info.c`: x=0..209, y=0..61
(la caja de `info_style`, 206 px legacy / 209 simple, 61 de alto). El S-meter
arranca en x=210 y el reloj/bateria viven en x=591..; cualquier pixel cambiado
ahi es una regresion.

Uso: python3 diff_regiones.py <dir-antes> <dir-despues>

SPDX-License-Identifier: GPL-2.0-or-later
"""

from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image

BARRA = (0, 0, 210, 62)  # x, y, x2, y2 (exclusivo) de la barra izquierda


def principales(antes: Path, despues: Path) -> tuple[int, int, int]:
    a = Image.open(antes).convert("RGB")
    b = Image.open(despues).convert("RGB")
    if a.size != b.size:
        raise SystemExit(f"{antes.name}: tamanos distintos {a.size} != {b.size}")
    pa, pb = a.load(), b.load()
    total = dentro = fuera = 0
    for y in range(a.height):
        for x in range(a.width):
            ca, cb = pa[x, y], pb[x, y]
            if abs(ca[0] - cb[0]) + abs(ca[1] - cb[1]) + abs(ca[2] - cb[2]) <= 12:
                continue
            total += 1
            if BARRA[0] <= x < BARRA[2] and BARRA[1] <= y < BARRA[3]:
                dentro += 1
            else:
                fuera += 1
    return total, dentro, fuera


def main(argv: list[str]) -> int:
    antes, despues = Path(argv[0]), Path(argv[1])
    print(f"{'fixture':24} {'cambiados':>9} {'en la barra':>11} {'fuera':>7}")
    fallos = 0
    for png in sorted(antes.glob("*.png")):
        otro = despues / png.name
        if not otro.exists():
            print(f"{png.stem:24} {'--':>9}  (no hay despues)")
            continue
        total, dentro, fuera = principales(png, otro)
        print(f"{png.stem:24} {total:>9} {dentro:>11} {fuera:>7}")
        if fuera:
            fallos += 1
    print(f"\n{fallos} fixture(s) con pixeles cambiados FUERA de la barra izquierda")
    return 1 if fallos else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
