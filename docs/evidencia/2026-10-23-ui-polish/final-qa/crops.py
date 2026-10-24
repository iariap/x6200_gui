#!/usr/bin/env python3
"""Recortes ampliados (vecino mas cercano) para leer los hallazgos, sin escalar la evidencia.

Los PNG de `png/` son la evidencia a 1:1 (800x480). Estos recortes son una ayuda
de lectura: amplian una region con `Image.NEAREST` (sin interpolar, no inventa
pixeles) y, cuando hay dos versiones, las apilan con una etiqueta.

Uso:  python3 .../crops.py [--base DIR]
SPDX-License-Identifier: GPL-2.0-or-later
"""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageDraw

AQUI = Path(__file__).resolve().parent
SALIDA = AQUI / "crops"
ESCALA = 4

# (nombre, fixture, caja, version)  version: actual | base | ambos
RECORTES = (
    ("sunlit-valores-dropdown", "settings-tema-sunlit", (490, 130, 792, 380), "ambos"),
    ("sunlit-operation-valores", "settings-grupo-operation-tema-sunlit", (490, 140, 792, 400), "ambos"),
    ("reloj-44-temas", "main-tema-aurora", (585, 0, 800, 64), "temas"),
    ("bateria-fila-estado", "finalqa-bateria-100", (585, 0, 800, 64), "estados"),
    ("reloj-dseg7-44", "finalqa-reloj-dseg7-tema-aurora", (585, 0, 800, 64), "digits"),
    ("barra-info", "main-tema-sunlit", (0, 0, 212, 64), "ambos"),
)


def recorte(img: Image.Image, caja) -> Image.Image:
    return img.crop(caja).resize(((caja[2] - caja[0]) * ESCALA, (caja[3] - caja[1]) * ESCALA), Image.NEAREST)


def apilar(partes: list[tuple[str, Image.Image]]) -> Image.Image:
    ancho = max(p.width for _t, p in partes)
    alto = sum(p.height + 18 for _t, p in partes)
    lienzo = Image.new("RGB", (ancho, alto), (24, 18, 39))
    d = ImageDraw.Draw(lienzo)
    y = 0
    for titulo, p in partes:
        d.text((4, y + 3), titulo, fill=(255, 255, 255))
        y += 18
        lienzo.paste(p, (0, y))
        y += p.height
    return lienzo


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument(
        "--base",
        type=Path,
        default=Path("/tmp/opencode/qa284-base/docs/evidencia/2026-10-23-ui-polish/final-qa"),
    )
    args = ap.parse_args(argv)
    SALIDA.mkdir(exist_ok=True)
    temas = ("aurora", "cathode", "redshift", "sprint", "sunlit", "trail")
    estados = (
        "finalqa-bateria-desconocida",
        "finalqa-bateria-0",
        "finalqa-bateria-100",
        "finalqa-bateria-cargando",
        "finalqa-bateria-sin-externa",
    )
    digits = ("dseg7", "dseg7mini", "dseg14")

    for nombre, fixture, caja, modo in RECORTES:
        partes = []
        if modo in ("ambos", "temas", "digits", "estados"):
            if modo == "temas":
                for t in temas:
                    partes.append((t, recorte(Image.open(AQUI / "png" / f"main-tema-{t}.png"), caja)))
            elif modo == "estados":
                for f in estados:
                    partes.append((f, recorte(Image.open(AQUI / "png" / f"{f}.png"), caja)))
            elif modo == "digits":
                for d in digits:
                    partes.append(
                        (
                            f"{d}_44",
                            recorte(Image.open(AQUI / "png" / f"finalqa-reloj-{d}-tema-aurora.png"), caja),
                        )
                    )
            else:
                if (args.base / "png" / f"{fixture}.png").exists():
                    partes.append(
                        ("base c90453fd", recorte(Image.open(args.base / "png" / f"{fixture}.png"), caja))
                    )
                partes.append(("actual 1845cb77", recorte(Image.open(AQUI / "png" / f"{fixture}.png"), caja)))
        if not partes:
            continue
        apilar(partes).save(SALIDA / f"{nombre}-x{ESCALA}.png")
        print(f"  crops/{nombre}-x{ESCALA}.png")

    # diff ampliado del hallazgo P1: la misma celda en los seis temas
    partes = [
        (t, recorte(Image.open(AQUI / "png" / f"settings-tema-{t}.png"), (490, 130, 792, 380))) for t in temas
    ]
    apilar(partes).save(SALIDA / f"settings-valores-seis-temas-x{ESCALA}.png")
    print(f"  crops/settings-valores-seis-temas-x{ESCALA}.png")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
