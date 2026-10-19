#!/usr/bin/env python3
"""Recorte x4 de la columna central del waterfall en una captura (issue #140).

La linea central vive en `x=398..400` de las 800 columnas de la pantalla; el
recorte toma una ventana centrada en esa columna y la agranda x4 para que el
trazo se vea a ojo (es la prueba visual: en 1:1 la linea de 1 px no se distingue
de una costura).

Uso:

    python3 recortes.py <captura.png> <salida.png> [--rect x0 y0 x1 y1] [--escala 4]

El rectangulo por defecto es `x=360..440, y=236..480` (80 x 244 px): la columna
central con 40 px de contexto a cada lado, desde arriba del waterfall hasta la
barra de botones. La salida es `(x1-x0)*escala` x `(y1-y0)*escala`.
"""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image

RECT = (360, 236, 440, 480)
ESCALA = 4


def main() -> int:
    ap = argparse.ArgumentParser(description="Recorte x4 de la columna central del waterfall (#140)")
    ap.add_argument("png", type=Path)
    ap.add_argument("salida", type=Path)
    ap.add_argument("--rect", nargs=4, type=int, default=list(RECT), metavar=("X0", "Y0", "X1", "Y1"))
    ap.add_argument("--escala", type=int, default=ESCALA)
    args = ap.parse_args()

    x0, y0, x1, y1 = args.rect
    with Image.open(args.png) as im:
        recorte = im.convert("RGB").crop((x0, y0, x1, y1))
    recorte = recorte.resize(((x1 - x0) * args.escala, (y1 - y0) * args.escala), Image.NEAREST)
    recorte.save(args.salida)
    print(
        f"{args.png} -> {args.salida}: rect x={x0}..{x1} y={y0}..{y1} "
        f"({x1 - x0}x{y1 - y0}) x{args.escala} = {recorte.width}x{recorte.height}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
