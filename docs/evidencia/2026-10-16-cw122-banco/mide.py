#!/usr/bin/env python3
"""Mide el widget de CW tune en los renders del arnes (region, colores, contraste).

Region del widget: (30,70) 139x40 = WIDTH x HEIGHT (src/cw_tune_ui.h:17-21).
El contraste es el del objeto `cw_tune_bars` de tools/ab_testing/quality/nontext.yaml:
percentil 90 del rectangulo de la barra i=7 contra percentil 10 del panel, WCAG 2.1.
"""
import sys
from collections import Counter
from pathlib import Path

from PIL import Image

REGION = (30, 70, 139, 40)
OBJ = (97, 72, 5, 38)      # barra i=7 (h = 37*2/(|i-7|+2), apoyada en HEIGHT-1)
BG = (40, 70, 8, 3)        # panel, arriba de las barras cortas


def lum(p):
    def ch(v):
        v /= 255.0
        return v / 12.92 if v <= 0.03928 else ((v + 0.055) / 1.055) ** 2.4
    return 0.2126 * ch(p[0]) + 0.7152 * ch(p[1]) + 0.0722 * ch(p[2])


def contraste(a, b):
    la, lb = lum(a), lum(b)
    if la < lb:
        la, lb = lb, la
    return (la + 0.05) / (lb + 0.05)


def percentil(px, p):
    orden = sorted(px, key=lum)
    return orden[round(p / 100.0 * (len(orden) - 1))]


def recorte(im, r):
    x, y, w, h = r
    px = im.load()
    return [px[i, j] for i in range(x, x + w) for j in range(y, y + h)]


def bbox(im, color):
    px = im.load()
    pts = [(x, y) for x in range(im.width) for y in range(im.height) if px[x, y] == color]
    if not pts:
        return None
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]
    return (min(xs), min(ys), max(xs), max(ys))


def main(paths):
    print("== colores distintos en la region del widget ==")
    for path in paths:
        im = Image.open(path).convert("RGB")
        col = Counter(recorte(im, REGION))
        print(f"  {Path(path).name:<34} {im.size}  colores={len(col)}")
        for c, n in col.most_common(6):
            print(f"      {c}  x{n}")

    print("== contraste del objeto cw_tune_bars (borde vs panel) ==")
    for path in paths:
        im = Image.open(path).convert("RGB")
        op, bp = recorte(im, OBJ), recorte(im, BG)
        ink, far_ink = percentil(op, 90), percentil(op, 10)
        surface, far_surface = percentil(bp, 10), percentil(bp, 90)
        c1, c2 = contraste(ink, far_surface), contraste(far_ink, surface)
        fg, fondo = (ink, far_surface) if c1 >= c2 else (far_ink, surface)
        print(f"  {Path(path).name:<34} objeto={OBJ} fondo={BG}")
        print(f"      {fg} vs {fondo} = {max(c1, c2):.2f}:1")

    print("== conteo por color (panel, barras, borde y bloque activo) ==")
    for path in paths:
        im = Image.open(path).convert("RGB")
        px = im.load()
        col = Counter(recorte(im, REGION))
        panel = col.most_common(1)[0][0]
        print(f"  {Path(path).name:<34} panel {panel} n={col[panel]:>5} bbox={bbox(im, panel)}")
        for c, n in col.most_common(6)[1:]:
            print(f"      {c} n={n:>5} bbox={bbox(im, c)}")


if __name__ == "__main__":
    main(sys.argv[1:])
