"""Mide la fila Day/Month/Year de la pantalla de Ajustes sobre una captura PNG (800x480).

Detecta los bordes verticales (color control_border = #E0E0E0) de la banda de la fila de
fecha y agrupa columnas contiguas en "grupos de borde". Las cajas de la fila son los
intervalos entre grupos. Tambien mide el texto "2026" (pixeles blancos) dentro de la caja
del Anio.

Uso: python3 medir.py <png> [y0 y1]
"""

import sys
from PIL import Image

BORDER = (224, 224, 224)
TEXT = (255, 255, 255)


def grupos_borde(im, y0, y1, x0=460, x1=800, min_px=30):
    px = im.load()
    cols = []
    for x in range(x0, x1):
        n = sum(1 for y in range(y0, y1) if px[x, y] == BORDER)
        if n >= min_px:
            cols.append(x)
    grupos = []
    for x in cols:
        if grupos and x - grupos[-1][-1] <= 1:
            grupos[-1].append(x)
        else:
            grupos.append([x])
    return [(g[0], g[-1]) for g in grupos]


def texto_blanco(im, y0, y1, x0, x1):
    px = im.load()
    xs = [x for x in range(x0, x1) for y in range(y0, y1) if px[x, y] == TEXT]
    ys = [y for x in range(x0, x1) for y in range(y0, y1) if px[x, y] == TEXT]
    if not xs:
        return None
    return (min(xs), min(ys), max(xs) - min(xs) + 1, max(ys) - min(ys) + 1)


def main():
    ruta = sys.argv[1]
    y0 = int(sys.argv[2]) if len(sys.argv) > 2 else 100
    y1 = int(sys.argv[3]) if len(sys.argv) > 3 else 156
    im = Image.open(ruta).convert("RGB")
    print(f"# {ruta}  ({im.width}x{im.height})  banda y={y0}..{y1 - 1}")
    g = grupos_borde(im, y0, y1)
    print(f"bordes verticales (grupos): {g}")
    # las cajas son los intervalos ENTRE grupos consecutivos (borde izq -> borde der)
    for i in range(len(g) - 1):
        izq = g[i][1]
        der = g[i + 1][0]
        print(f"  caja entre grupo {i} y {i + 1}: x={izq}..{der}  ancho={der - izq + 1}")
    # texto "2026": dentro de la ultima caja (Anio)
    if len(g) >= 2:
        izq = g[-2][1]
        der = g[-1][0]
        t = texto_blanco(im, y0, y1, izq, der + 1)
        if t:
            margen_izq = t[0] - izq
            margen_der = der - (t[0] + t[2] - 1)
            print(f"  texto '2026' en la caja del Anio: {t[0]},{t[1]} {t[2]}x{t[3]}"
                  f"  (margen izq={margen_izq} px, der={margen_der} px)")


if __name__ == "__main__":
    main()
