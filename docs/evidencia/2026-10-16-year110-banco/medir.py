"""Mide la fila Day/Month/Year de la pantalla de Ajustes sobre una captura PNG (800x480).

La fila de fecha son TRES spinboxes de la misma grilla. Cada widget se dibuja con un borde
vertical de 1-2 px: gris (`control_border` = #E0E0E0) cuando el campo no tiene el foco y
blanco (#FFFFFF) cuando lo tiene (el campo enfocado ademas lleva el anillo de foco cyan
alrededor, FUERA del widget). Este script:

1. detecta las columnas que son una linea vertical del widget (>= min_px pixeles del color
   de borde en la banda), ignorando el anillo de foco cyan (que no es el borde del widget);
2. agrupa columnas contiguas y arma las cajas con los pares de grupos separados por un
   interior (> 8 px): el hueco de 3 px entre la caja del Mes y la del Anio no es una caja;
3. reporta el span EXTERNO de cada caja (borde a borde, anillo de foco excluido) = el ancho
   declarado del widget (`lv_obj_set_size`), y el bbox de la tinta del "2026" en la caja del
   Anio con sus margenes al borde interno del widget.

Uso: python3 medir.py <png> [y0 y1]
"""

import sys
from PIL import Image

BORDER = (224, 224, 224)  # control_border (campo sin foco)
TEXT = (255, 255, 255)  # borde del campo con foco y tinta del texto
FOCUS = (140, 197, 238)  # anillo de foco (cyan), fuera del widget
CAMPOS = ("Dia", "Mes", "Anio")


def columnas_borde(im, y0, y1, x0, x1, min_px=30):
    """Columnas que son una linea vertical del widget (gris o blanca), sin el anillo de foco."""
    px = im.load()
    cols = []
    for x in range(x0, x1):
        col = [px[x, y] for y in range(y0, y1)]
        if col.count(BORDER) >= min_px or col.count(TEXT) >= min_px:
            cols.append(x)
    return cols


def grupos(cols):
    """Agrupa columnas contiguas en (primera, ultima)."""
    g = []
    for x in cols:
        if g and x - g[-1][-1] <= 1:
            g[-1].append(x)
        else:
            g.append([x])
    return [(a[0], a[-1]) for a in g]


def cajas(g, hueco=8):
    """Cajas = pares de grupos consecutivos con un interior mayor que `hueco` px.

    `hueco` separa los dos casos chicos de los dos grandes: el hueco de la grilla
    entre dos widgets es ~4 px y el hueco de la tinta dentro de un campo es ~11 px
    (el relleno blanco del campo enfocado parte su caja en dos grupos de columnas);
    el interior real de una caja mide 60+ px.
    """
    return [(g[i][0], g[i + 1][1]) for i in range(len(g) - 1) if g[i + 1][0] - g[i][1] > hueco]


def tinta(im, y0, y1, x0, x1):
    """Bbox de los pixeles blancos (la tinta del texto) en el rectangulo dado."""
    px = im.load()
    pts = [(x, y) for x in range(x0, x1) for y in range(y0, y1) if px[x, y] == TEXT]
    if not pts:
        return None
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]
    return (min(xs), min(ys), max(xs) - min(xs) + 1, max(ys) - min(ys) + 1)


def main():
    ruta = sys.argv[1]
    y0 = int(sys.argv[2]) if len(sys.argv) > 2 else 100
    y1 = int(sys.argv[3]) if len(sys.argv) > 3 else 156
    im = Image.open(ruta).convert("RGB")
    print(f"# {ruta}  ({im.width}x{im.height})  banda y={y0}..{y1 - 1}")
    g = grupos(columnas_borde(im, y0, y1, 460, 800))
    print(f"columnas de borde (grupos): {g}")
    boxes = cajas(g)
    if len(boxes) != 3:
        print(f"ERROR: se esperaban 3 cajas (Dia/Mes/Anio) y se midieron {len(boxes)}: {boxes}")
        return 1
    for campo, (izq, der) in zip(CAMPOS, boxes):
        print(f"  {campo:4}: span externo x={izq}..{der}  ancho={der - izq + 1}")
    izq, der = boxes[2]
    # borde interno del widget: el gris ocupa 2 px por lado, asi que la tinta se mide
    # entre izq+2 y der-2 (el campo del Anio nunca lleva el foco en esta pantalla).
    t = tinta(im, y0, y1, izq + 2, der - 1)
    if t:
        print(f"  tinta '2026' en el Anio: {t[0]},{t[1]} {t[2]}x{t[3]}"
              f"  (margen al borde interno: izq={t[0] - (izq + 2)} px,"
              f" der={(der - 2) - (t[0] + t[2] - 1)} px)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
