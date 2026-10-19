#!/usr/bin/env python3
"""El piso real del waterfall por tema, y la linea antes/despues sobre el (#140).

El banco QEMU no tiene FFT: el waterfall nunca recibe un frame y su piso es
**negro** (medido: 97 % de negro en la banda). Sobre ese piso la tinta oscura de
Sunlit mide 1.34:1, un numero que no dice nada del producto. El piso real de la
radio, sin senal, es el extremo bajo de la paleta del tema: `theme.c`
`make_palette()` con `waterfall_steps=0` deja `palette[0] = waterfall[0]`.

Este script lee los seis temas del repo (el JSON es la fuente, no hay colores
escritos a mano) y calcula, sobre ese piso:

* el rol `text` (la tinta que el fix le da a la linea, a opacidad plena), y
* la tinta del trunk: `panel_border` al 60 % con `LV_BLEND_MODE_ADDITIVE`, que
  LVGL compone como `piso + (canal * 153) >> 8` por canal, saturado a 255 (el
  `>> 8` es lo que reproduce el `(11,41,66)` medido en el banco sobre negro:
  `20*153 >> 8 = 11`, `69*153 >> 8 = 41`, `111*153 >> 8 = 66`).

Uso:

    python3 derivar-piso.py [--themes DIR]

Salida: texto plano por stdout (se guarda en `medicion-piso-derivado.txt`).
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

THEMES = Path(__file__).resolve().parents[3] / "rootfs/usr/share/x6200/themes"
ORDEN = ("Aurora", "Cathode", "Redshift", "Sprint", "Sunlit", "Trail")
OPA = 153  # LV_OPA_60; LVGL mezcla con >> 8 (no /255)


def a_rgb(hex_rol: str) -> tuple[int, int, int]:
    hex_rol = hex_rol.lstrip("#")
    r, g, b = (int(hex_rol[i : i + 2], 16) for i in (0, 2, 4))
    return (r, g, b)


def hex_de(pixel: tuple[int, int, int]) -> str:
    return "#%02x%02x%02x" % pixel


def _canal(v: float) -> float:
    v = v / 255.0
    return v / 12.92 if v <= 0.03928 else ((v + 0.055) / 1.055) ** 2.4


def luminancia(pixel: tuple[int, int, int]) -> float:
    r, g, b = (_canal(float(x)) for x in pixel)
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def contraste(a: tuple[int, int, int], b: tuple[int, int, int]) -> float:
    la, lb = luminancia(a), luminancia(b)
    return (max(la, lb) + 0.05) / (min(la, lb) + 0.05)


def tinta_trunk(piso: tuple[int, int, int], borde: tuple[int, int, int]) -> tuple[int, int, int]:
    """ADDITIVE del trunk: `piso + (borde * 153) >> 8`, saturado a 255."""
    r, g, b = (min(255, p + ((c * OPA) >> 8)) for p, c in zip(piso, borde))
    return (r, g, b)


def main() -> int:
    ap = argparse.ArgumentParser(description="Piso real del waterfall por tema y contraste de la linea (#140)")
    ap.add_argument("--themes", type=Path, default=THEMES)
    args = ap.parse_args()

    print("piso del waterfall por tema (theme.c make_palette, steps=0 -> palette[0] = waterfall[0])")
    print("y contraste WCAG 1.4.11 de la linea sobre ese piso (el piso real de la radio sin senal)")
    print()
    print("| Tema | `waterfall[0]` (piso) | tinta antes (trunk) | ratio antes | tinta despues (`text`) | ratio despues |")
    print("| --- | --- | --- | --- | --- | --- |")
    for nombre in ORDEN:
        tema = json.loads((args.themes / f"{nombre}.json").read_text())
        piso_rgb = a_rgb(tema["waterfall"][0])
        borde = a_rgb(tema["colors"]["panel_border"])
        texto = a_rgb(tema["colors"]["text"])
        antes = tinta_trunk(piso_rgb, borde)
        print(
            f"| {nombre} | `{hex_de(piso_rgb)}` | `{hex_de(antes)}` = `{hex_de(borde)}` × 153/256 + piso | "
            f"{contraste(antes, piso_rgb):.2f}:1 | `{hex_de(texto)}` | **{contraste(texto, piso_rgb):.2f}:1** |"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
