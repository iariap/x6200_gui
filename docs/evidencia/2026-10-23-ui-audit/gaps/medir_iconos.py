#!/usr/bin/env python3
"""Mide, en las capturas `net-*.png`, que rol de color del tema pinta el icono de
WiFi de la barra (src/info.c::wifi_state_change_cb) y la runa de BT
(src/info.c::bt_state_show), por estado inyectado.

La regla de src/info.c es: apagado -> icon_off, prendido -> text_dim,
conectado -> text. Esta tabla es la medicion por PIXELES de esa regla en los ocho
estados capturados (la captura es SYNTHETIC: mide layout/contraste del estado con
el doble, no la funcion real de BT/WiFi).

Salida: tablas/iconos-bt-wifi.csv
"""

from __future__ import annotations

import csv
from pathlib import Path

from PIL import Image

AQUI = Path(__file__).resolve().parent

# Los tres roles del tema Aurora que la barra usa para los dos iconos.
ROLES = {"icon_off": (0x3A, 0x2D, 0x50), "text_dim": (0xD5, 0xC4, 0xE0), "text": (0xF8, 0xEE, 0xFF)}

# Cajas medidas en el arbol de objetos (--inspect): la celda de WiFi y la runa de BT.
REGIONES = {"wifi_cell": (139, 31, 167, 57), "bt_rune": (167, 34, 183, 56)}


def main() -> int:
    filas = []

    for png in sorted((AQUI / "png").glob("net-*.png")):
        im = Image.open(png).convert("RGB")
        fila = {"png": png.name}

        for nombre, (x0, y0, x1, y1) in REGIONES.items():
            crop = im.crop((x0, y0, x1, y1))
            pixeles = list(crop.get_flattened_data())

            for rol, rgb in ROLES.items():
                fila[f"{nombre}_{rol}_px"] = sum(1 for c in pixeles if c == rgb)

            fila[nombre + "_roles"] = "|".join(rol for rol in ROLES if fila[f"{nombre}_{rol}_px"] > 0)

        filas.append(fila)

    cols = ["png"]
    for nombre in REGIONES:
        cols += [f"{nombre}_{rol}_px" for rol in ROLES] + [nombre + "_roles"]

    with (AQUI / "tablas" / "iconos-bt-wifi.csv").open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=cols)
        w.writeheader()
        w.writerows(filas)

    print(f"iconos-bt-wifi.csv: {len(filas)} filas")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
