#!/usr/bin/env python3
"""Recortes ampliados (vecino mas cercano) de la evidencia del #285.

Ayuda de lectura, NO prueba: agrandan la media celda de la runa de BT y el panel
del reloj de los PNG de `png-before/`/`png-after/` para comparar antes/despues.
La legibilidad fisica sobre el LCD de 4" queda PENDING por contrato.

Uso: python3 crops.py
SPDX-License-Identifier: GPL-2.0-or-later
"""

from __future__ import annotations

from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
OUT = HERE / "crops"
NEAREST = Image.Resampling.NEAREST

# La media celda derecha de la barra (WiFi/BT) y el panel del reloj completo.
BT_BOX = (139, 22, 200, 62)
CLOCK_BOX = (580, 0, 800, 62)
TEMAS = ["aurora", "cathode", "redshift", "sprint", "sunlit", "trail"]


def recorte(phase: str, name: str, box, scale: int) -> Image.Image:
    img = Image.open(HERE / f"png-{phase}" / f"{name}.png").convert("RGB")
    sub = img.crop(box)
    return sub.resize((sub.width * scale, sub.height * scale), NEAREST)


def stack(images: list[Image.Image], gap: int = 6) -> Image.Image:
    w = max(i.width for i in images)
    h = sum(i.height for i in images) + gap * (len(images) - 1)
    out = Image.new("RGB", (w, h), (24, 24, 24))
    y = 0
    for i in images:
        out.paste(i, (0, y))
        y += i.height + gap
    return out


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)

    # 1. La runa de BT: antes (un zigzag) y despues en los tres estados.
    imgs = [recorte("before", "home-bt-off", BT_BOX, 8), recorte("after", "home-bt-off", BT_BOX, 8),
            recorte("after", "home-bt-on", BT_BOX, 8),
            recorte("after", "home-bt-connected", BT_BOX, 8)]
    stack(imgs).save(OUT / "runa-bt-antes-despues-x8.png")

    # 2. El panel del reloj: los seis temas, antes | despues.
    for tema in TEMAS:
        par = [recorte("before", f"home-{tema}", CLOCK_BOX, 3),
               recorte("after", f"home-{tema}", CLOCK_BOX, 3)]
        stack(par).save(OUT / f"reloj-{tema}-antes-despues-x3.png")

    # 3. El reloj en los seis temas, despues, apilado (contraste de la banda).
    stack([recorte("after", f"home-{t}", CLOCK_BOX, 3) for t in TEMAS]).save(
        OUT / "reloj-temas-despues-x3.png")

    # 4. La banda de estado con los porcentajes de la matriz (Aurora, despues).
    stack([recorte("after", f"home-bat-{p}", (590, 38, 800, 62), 5)
           for p in ("unknown", "0", "9", "76", "99", "100")]).save(
        OUT / "banda-porcentajes-despues-x5.png")

    print(f"OK: {len(list(OUT.glob('*.png')))} recortes en {OUT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
