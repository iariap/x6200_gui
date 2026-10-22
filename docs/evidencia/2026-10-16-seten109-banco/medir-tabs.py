"""Mide la barra de tabs de la pantalla de Ajustes sobre una captura del banco (800x480).

La barra de abajo la dibuja buttons.cpp: fondo negro (#000000) y el texto de cada tab en
claro (#FFFFFF / #CCCCCC / #EEEEEE). Este script NO hace OCR: detecta la tinta de los labels
(columnas con pixeles claros en la banda del texto), agrupa las columnas contiguas en labels
(el hueco entre dos tabs es grande, el de dos glifos chico) y reporta la caja de cada label.
El orden y las posiciones son la evidencia determinista; el texto se lee en la captura.

Uso: python3 medir-tabs.py <png> [y0 y1]
"""

import sys

from PIL import Image

LUM = 128  # umbral de "tinta clara" sobre el fondo negro de la barra


def brillo(p):
    return 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2]


def main():
    ruta = sys.argv[1]
    y0 = int(sys.argv[2]) if len(sys.argv) > 2 else 428
    y1 = int(sys.argv[3]) if len(sys.argv) > 3 else 464
    im = Image.open(ruta).convert("RGB")
    px = im.load()
    print(f"# {ruta}  ({im.width}x{im.height})  banda y={y0}..{y1 - 1}")

    # columnas con tinta clara
    cols = []
    for x in range(im.width):
        n = sum(1 for y in range(y0, y1) if brillo(px[x, y]) >= LUM)
        if n >= 2:
            cols.append(x)
    if not cols:
        print("ERROR: sin tinta en la banda")
        return 1

    # agrupa columnas contiguas separadas por huecos chicos (espaciado entre glifos)
    grupos = []
    for x in cols:
        if grupos and x - grupos[-1][-1] <= 16:
            grupos[-1].append(x)
        else:
            grupos.append([x])

    # descarta lineas verticales finas (separadores del espectro) que no son labels
    grupos = [g for g in grupos if g[-1] - g[0] + 1 >= 20]
    print(f"labels detectados: {len(grupos)}")
    for i, g in enumerate(grupos, 1):
        x0, x1 = g[0], g[-1]
        print(f"  {i}: x={x0}..{x1}  ancho={x1 - x0 + 1}  centro={(x0 + x1) // 2}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
