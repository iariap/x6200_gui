#!/usr/bin/env python3
"""Mide las capturas antes/despues de #288 (Fase 2) y arma los recortes.

- diff pixel a pixel de cada fixture entre `png-before/` y `png-after/`
  (mismo binario del arnes, misma fixture, solo cambia el arbol de fuentes);
- exige que el UNICO fixture con pixeles distintos sea `swrscan-3pass-stop` y que
  el cambio quede dentro de la banda de la linea de estado (y 70..100, x < 400);
- recorta esa banda de las dos fases y las apila en `crops/` (x3) para leerlas;
- mide el contraste WCAG 2.1 del texto de estado contra su fondo en las dos fases
  (dos colores dominantes del recorte) y los compara.

Uso: python3 medir_apps.py
SPDX-License-Identifier: GPL-2.0-or-later
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent
STATE_BAND = (0, 70, 400, 100)   # x0, y0, x1, y1: la linea de estado de SWRScan
CROP = (96, 70, 200, 100)        # recorte legible alrededor del texto que cambia


def contrast(rgb1: tuple[int, int, int], rgb2: tuple[int, int, int]) -> float:
    def lin(c: float) -> float:
        c /= 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4

    l1 = 0.2126 * lin(rgb1[0]) + 0.7152 * lin(rgb1[1]) + 0.0722 * lin(rgb1[2])
    l2 = 0.2126 * lin(rgb2[0]) + 0.7152 * lin(rgb2[1]) + 0.0722 * lin(rgb2[2])
    hi, lo = max(l1, l2), min(l1, l2)
    return (hi + 0.05) / (lo + 0.05)


def dominant_colors(png: Path, box: tuple[int, int, int, int]) -> list[tuple[int, int, int]]:
    """Fondo = color modal; texto = el color con >=10 px que mas contrasta con el.

    Es el mismo proxy que usó la auditoría de apps (#272): "fondo vs primer
    plano" sobre el recorte, no el contraste real de cada glifo.
    """
    crop = np.asarray(Image.open(png).convert("RGB").crop(box)).reshape(-1, 3)
    colors, counts = np.unique(crop, axis=0, return_counts=True)
    bg = tuple(int(v) for v in colors[int(np.argmax(counts))])
    candidates = [
        (contrast(tuple(int(v) for v in color), bg), tuple(int(v) for v in color), int(count))
        for color, count in zip(colors, counts, strict=True)
        if count >= 10
    ]
    candidates.sort(reverse=True)
    return [candidates[0][1], bg]


def main() -> int:
    before = json.loads((HERE / "inventario-before.json").read_text())
    after = json.loads((HERE / "inventario-after.json").read_text())
    assert [r["fixture"] for r in before] == [r["fixture"] for r in after]

    changed: list[str] = []
    rows = []
    for b, a in zip(before, after, strict=True):
        pb, pa = HERE / b["png"], HERE / a["png"]
        ib, ia = np.asarray(Image.open(pb).convert("RGB")), np.asarray(Image.open(pa).convert("RGB"))
        assert ib.shape == ia.shape == (480, 800, 3), (pb, ib.shape)

        mask = np.any(ib != ia, axis=2)
        pixels = int(mask.sum())
        if pixels:
            ys, xs = np.nonzero(mask)
            bbox = (int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max()))
            changed.append(b["fixture"])
        else:
            bbox = None

        rows.append(
            {
                "fixture": b["fixture"],
                "estado_antes": b["swr_state"],
                "estado_despues": a["swr_state"],
                "pixeles_distintos": pixels,
                "bbox": bbox,
            }
        )

    print(f"{'fixture':26s} {'pixeles':>8s}  {'bbox':22s} estado")
    for r in rows:
        print(
            f"{r['fixture']:26s} {r['pixeles_distintos']:8d}  "
            f"{str(r['bbox']):22s} {r['estado_antes']} -> {r['estado_despues']}"
        )

    # --- invariantes de la evidencia -------------------------------------
    problemas = []
    if changed != ["swrscan-3pass-stop"]:
        problemas.append(f"fixtures con pixeles distintos: {changed}")

    stopped = next(r for r in rows if r["fixture"] == "swrscan-3pass-stop")
    x0, y0, x1, y1 = stopped["bbox"] or (0, 0, 0, 0)
    if not (y0 >= STATE_BAND[1] and y1 <= STATE_BAND[3] and x0 >= STATE_BAND[0] and x1 <= STATE_BAND[2]):
        problemas.append(f"el cambio se sale de la banda de estado {STATE_BAND}: {stopped['bbox']}")

    # --- recorte y contraste ---------------------------------------------
    crops = HERE / "crops"
    crops.mkdir(exist_ok=True)
    imgs = [Image.open(HERE / r["png"]).convert("RGB").crop(CROP) for r in before if r["fixture"] == "swrscan-3pass-stop"]
    imgs += [Image.open(HERE / r["png"]).convert("RGB").crop(CROP) for r in after if r["fixture"] == "swrscan-3pass-stop"]
    scale = 3
    gap = 6
    w, h = imgs[0].size
    canvas = Image.new("RGB", (w * scale, (h * scale) * 2 + gap), (0, 0, 0))
    for i, im in enumerate(imgs):
        canvas.paste(im.resize((w * scale, h * scale), Image.Resampling.NEAREST), (0, i * (h * scale + gap)))
    out = crops / "swrscan-3pass-stop-antes-despues-x3.png"
    canvas.save(out)
    print(f"\nrecorte (arriba antes, abajo despues): {out.relative_to(HERE)}")

    for tag, inv in (("antes", before), ("despues", after)):
        row = next(r for r in inv if r["fixture"] == "swrscan-3pass-stop")
        fg, bg = dominant_colors(HERE / row["png"], CROP)
        ratio = contrast(fg, bg)
        print(f"contraste {tag:8s} fondo #{bg[0]:02X}{bg[1]:02X}{bg[2]:02X} "
              f"texto #{fg[0]:02X}{fg[1]:02X}{fg[2]:02X}: {ratio:.2f}:1")
        if ratio < 3.0:
            problemas.append(f"contraste {tag} {ratio:.2f}:1 < 3.0")

    (HERE / "mediciones").mkdir(exist_ok=True)
    (HERE / "mediciones" / "diffs.json").write_text(json.dumps(rows, indent=1) + "\n")

    if problemas:
        for p in problemas:
            print(f"FALLA {p}")
        return 1
    print("\nPASS: solo se mueve la linea de estado de swrscan-3pass-stop, dentro de su banda")
    return 0


if __name__ == "__main__":
    sys.exit(main())
