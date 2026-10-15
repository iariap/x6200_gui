#!/usr/bin/env python3
"""Galería comparativa de los seis temas v2 (THEMES v2, issue #73).

Compone una hoja de contactos por **estado** (pantalla principal, Ajustes,
diálogo, TX simulado, waterfall en movimiento, batería baja/ECO) a partir de los
renders del arnés de UI (`tools/ui_harness`): cada hoja es una grilla de los
**seis temas** en el mismo estado de radio, así el tema es la única variable. Las
imágenes son los PNG reales del framebuffer LVGL 8 (800×480), nunca una
ilustración.

Reproducible: los renders son deterministas (dos corridas del mismo fixture dan
el mismo sha256, `tests/check_harness_determinism.sh`), así que la misma entrada
da la misma galería.

Uso:
  python3 tools/galeria-temas.py --renders <dir-con-png> --out <dir-de-salida>

`--renders` es el directorio donde `x6200-ab capture` (o el arnés) dejó
`<fixture>.png`; el script no corre el arnés, sólo compone. Ver
`docs/THEMES.md` (Galería consolidada) para el comando de captura exacto.

SPDX-License-Identifier: GPL-2.0-or-later
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

# El orden del selector (#73): los seis temas v2.
TEMAS = ("Sunlit", "Redshift", "Sprint", "Cathode", "Trail", "Aurora")

# (id, título de la hoja, patrón de fixture). Cada estado usa la fixture del
# arnés que lo rinde para CADA tema, con el mismo estado de radio que su gemela
# de otro tema.
ESTADOS = (
    ("principal", "Pantalla principal (RX)", "main-tema-{t}"),
    ("ajustes", "Ajustes (Apariencia, foco en la fila del tema)", "settings-tema-{t}"),
    ("dialogo", "Diálogo de app (Bluetooth)", "bt-dialog-tema-{t}"),
    ("tx", "TX simulado", "main-tema-{t}-tx"),
    ("cascada", "Waterfall en movimiento", "main-tema-{t}-agua"),
    ("eco", "Batería baja + ECO", "main-tema-{t}-eco"),
)

COLS = 3
ETIQUETA_ALTO = 30
CABECERA_ALTO = 46
FONDO = (24, 26, 30)
TINTA = (240, 240, 240)
ACENTO = (120, 200, 255)
FUENTES = (
    "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
)


def _fuente(size: int) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    for ruta in FUENTES:
        if Path(ruta).is_file():
            return ImageFont.truetype(ruta, size)
    return ImageFont.load_default()


def _render(renders: Path, fixture: str) -> Image.Image | None:
    ruta = renders / f"{fixture}.png"
    if not ruta.is_file():
        return None
    return Image.open(ruta).convert("RGB")


def componer(renders: Path, out: Path, scale: float = 0.5) -> list[str]:
    """Una hoja por estado; devuelve los paths escritos."""
    faltan: list[str] = []
    escritos: list[str] = []
    for estado, titulo, patron in ESTADOS:
        celdas: list[tuple[str, Image.Image]] = []
        for tema in TEMAS:
            fixture = patron.format(t=tema.lower())
            img = _render(renders, fixture)
            if img is None:
                faltan.append(fixture)
                continue
            if scale != 1.0:
                img = img.resize(
                    (max(1, int(img.width * scale)), max(1, int(img.height * scale))),
                    Image.LANCZOS,
                )
            celdas.append((tema, img))
        if not celdas:
            continue
        ancho, alto = celdas[0][1].size
        filas = (len(celdas) + COLS - 1) // COLS
        hoja = Image.new(
            "RGB",
            (COLS * ancho, CABECERA_ALTO + filas * (ETIQUETA_ALTO + alto)),
            FONDO,
        )
        dibujo = ImageDraw.Draw(hoja)
        dibujo.text((16, 12), titulo, font=_fuente(24), fill=ACENTO)
        for i, (tema, img) in enumerate(celdas):
            cx = (i % COLS) * ancho
            cy = CABECERA_ALTO + (i // COLS) * (ETIQUETA_ALTO + alto)
            dibujo.text((cx + 10, cy + 6), tema, font=_fuente(20), fill=TINTA)
            hoja.paste(img, (cx, cy + ETIQUETA_ALTO))
        salida = out / f"galeria-{estado}.png"
        hoja.save(salida, optimize=True)
        escritos.append(str(salida))
    if faltan:
        sys.stderr.write("faltan renders: " + ", ".join(sorted(set(faltan))) + "\n")
    return escritos


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--renders", required=True, help="directorio con los <fixture>.png")
    parser.add_argument("--out", required=True, help="directorio de salida de la galería")
    parser.add_argument(
        "--scale",
        type=float,
        default=0.5,
        help="factor de escala de cada render en la hoja (default 0.5; 1.0 = tamaño completo)",
    )
    args = parser.parse_args(argv)
    renders = Path(args.renders)
    if not renders.is_dir():
        parser.error(f"no existe el directorio de renders: {renders}")
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    escritos = componer(renders, out, scale=args.scale)
    if not escritos:
        sys.stderr.write("no se compuso ninguna hoja (¿faltan los renders de los temas?)\n")
        return 1
    for ruta in escritos:
        print(ruta)
    return 0


if __name__ == "__main__":  # pragma: no cover
    raise SystemExit(main())
