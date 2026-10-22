#!/usr/bin/env python3
"""Mide la linea central del waterfall en una captura del banco (issue #140).

Pensado para las capturas del banco QEMU: ahi no hay radio ni flow, asi que el
fondo del waterfall es negro y el resto de la pantalla no. Metodo determinista,
sin coordenadas escritas a mano:

1. Banda del waterfall: primera y ultima fila de la mitad inferior de la pantalla
   donde el fondo a los costados de la columna central (x-4 y x+4) es oscuro
   (luminancia media < 60). Es el bloque grande oscuro de la pantalla principal y
   no depende de que la linea exista (con el parametro apagado da la misma banda).
2. Medida que pide el issue: en esa banda, para cada fila con fondo visible (las
   dos vecinas oscuras: se excluyen las filas tapadas por texto/controles), se
   compara el pixel de la columna central contra x-4 y x+4. Con la linea dibujada
   de punta a punta la columna central difiere de las dos en ~100 % de las filas
   medibles; sin linea (parametro apagado, o el defecto de largo cero) el fondo es
   parejo y difiere en ~0 %. Se reporta tambien el % sobre todas las filas de la
   banda, para que las filas tapadas no desaparezcan del numero.
3. Soporte: corrida de color constante sobre la columna central dentro de la banda
   (largo y color) y color de x-1/x+1, para mostrar el trazo y su ancho.

Uso: medir-linea.py <captura.png> [--centro 400]

Salida: texto plano por stdout (se guarda en medicion-*.txt).
"""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image

TOL = 2
OSCURO = 60


def same(a: tuple[int, int, int], b: tuple[int, int, int]) -> bool:
    return all(abs(a[i] - b[i]) <= TOL for i in range(3))


def lum(c: tuple[int, int, int]) -> float:
    return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]


def es_oscuro(px, x: int, y: int) -> bool:
    return lum(px[x, y]) < OSCURO


def banda(px, w: int, h: int, centro: int) -> tuple[int, int]:
    """(y0, y1) entre la primera y la ultima fila con fondo oscuro a los costados."""
    izq, der = max(0, centro - 4), min(w - 1, centro + 4)
    filas = [y for y in range(h // 2, h) if es_oscuro(px, izq, y) and es_oscuro(px, der, y)]
    if not filas:
        return (0, -1)
    return (filas[0], filas[-1])


def corrida(px, x: int, y0: int, y1: int) -> tuple[int, tuple[int, int, int]]:
    """Largo de la corrida constante mas larga sobre la columna x dentro de la banda."""
    mejor = 0
    color_mejor = (0, 0, 0)
    inicio = y0
    color = px[x, y0]
    for y in range(y0 + 1, y1 + 2):
        cur = px[x, y] if y <= y1 else None
        if cur is None or not same(cur, color):
            if y - inicio > mejor:
                mejor = y - inicio
                color_mejor = color
            if cur is not None:
                inicio = y
                color = cur
    return mejor, color_mejor


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("png", type=Path)
    ap.add_argument("--centro", type=int, default=400, help="columna central esperada")
    args = ap.parse_args()

    im = Image.open(args.png).convert("RGB")
    px = im.load()
    w, h = im.size

    y0, y1 = banda(px, w, h, args.centro)
    print(f"captura: {args.png} ({w}x{h})")
    if y1 < y0:
        print("SIN BANDA: no encontre el bloque oscuro del waterfall")
        return 1
    print(f"banda del waterfall: y={y0}..{y1} ({y1 - y0 + 1} filas)")

    medibles = 0
    dif = 0
    for y in range(y0, y1 + 1):
        if not (es_oscuro(px, args.centro - 4, y) and es_oscuro(px, args.centro + 4, y)):
            continue
        medibles += 1
        c = px[args.centro, y]
        if not same(c, px[args.centro - 4, y]) and not same(c, px[args.centro + 4, y]):
            dif += 1

    total = y1 - y0 + 1
    if medibles:
        print(
            f"columna central x={args.centro} vs x-4 y x+4: {dif}/{medibles} filas medibles "
            f"con diferencia = {100.0 * dif / medibles:.1f} %"
        )
    else:
        print("columna central: no hay filas medibles (banda tapada)")
    print(f"sobre toda la banda (incluye filas tapadas por texto/controles): {dif}/{total} = "
          f"{100.0 * dif / total:.1f} %")

    largo, color = corrida(px, args.centro, y0, y1)
    print(
        f"corrida constante sobre x={args.centro}: {largo}/{total} filas, color RGB{color}; "
        f"x-1=RGB{px[args.centro - 1, (y0 + y1) // 2]} x+1=RGB{px[args.centro + 1, (y0 + y1) // 2]}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
