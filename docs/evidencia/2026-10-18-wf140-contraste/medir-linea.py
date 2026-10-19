#!/usr/bin/env python3
"""Contraste de la linea central del waterfall (#140) sobre los renders del arnes.

Mide, para cada uno de los seis temas, el contraste WCAG 1.4.11 de la linea
central del waterfall **antes** y **despues** del fix del issue #140, sobre los
PNG que el harness host produce para los fixtures `<tema>-agua` (el waterfall en
movimiento con la linea dibujada) y sus variantes con el parametro apagado (el
mismo render sin linea: el fondo del waterfall).

Dos medidas por render, porque responden a dos preguntas distintas:

* **gate**: el mismo algoritmo de `x6200-ab themes`
  (`tools/ab_testing/src/x6200_ab/nontext_checks.py:measure_nontext`) sobre el
  objeto declarado en `tools/ab_testing/quality/nontext.yaml`: el objeto es el
  trazo de la linea (`object: [398, 255, 3, 150]`) y el fondo es el piso de ruido
  a su izquierda (`background: [220, 255, 40, 150]`), ambos leidos en el
  percentil de luminancia (tinta 90, fondo 10). Es la medida que el gate exige
  >= 3:1, y su valor para `main-espectro-agua` es el que imprime el gate.
* **peor fila**: la medida local, fila por fila del waterfall (y=255..404, las
  filas del objeto declarado: la barra de botones de abajo queda afuera, ahi la
  linea pasa por debajo del gradiente de los botones y no esta sobre el
  waterfall): la tinta es el pixel de la linea y el fondo es la mediana de 8 px a
  cada lado de la mascara (la mascara sale del diff contra el render con el
  parametro apagado). Es el peor caso cuando el carrier cae justo debajo de la
  linea; se reporta como medicion secundaria (minimo, mediana y % de filas >= 3:1).

Uso (los renders salen del arnes, ver README.md de este directorio):

    cd tools/ab_testing
    uv run python ../../docs/evidencia/2026-10-18-wf140-contraste/medir-linea.py \
        --antes-on /tmp/wf140/antes/on --antes-off /tmp/wf140/antes/off \
        --despues-on /tmp/wf140/despues/on --despues-off /tmp/wf140/despues/off

`--json` imprime lo mismo en JSON. Falla (exit 1) si falta un render.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import statistics
import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[3]

# El trazo (3 px de ancho, x=398..400 en el render de despues) y el piso de ruido
# a su izquierda: exactamente el objeto declarado en quality/nontext.yaml.
OBJETO = (398, 255, 3, 150)
FONDO = (220, 255, 40, 150)
BANDA = (255, 404)  # las filas del objeto declarado: el waterfall, no la barra de botones

# (clave de las variantes `off-<clave>`, fixture, tema del JSON)
TEMAS = (
    ("aurora", "main-tema-aurora-agua", "Aurora"),
    ("cathode", "main-tema-cathode-agua", "Cathode"),
    ("redshift", "main-tema-redshift-agua", "Redshift"),
    ("sprint", "main-tema-sprint-agua", "Sprint"),
    ("sunlit", "main-tema-sunlit-agua", "Sunlit"),
    ("trail", "main-tema-trail-agua", "Trail"),
)


def cargar_gate():
    """El medidor del gate: los mismos objetos y funciones que usa `x6200-ab themes`."""
    sys.path.insert(0, str(ROOT / "tools" / "ab_testing" / "src"))
    from x6200_ab.contrast import contrast_ratio
    from x6200_ab.nontext_checks import NonTextObject, measure_nontext

    return NonTextObject, measure_nontext, contrast_ratio


def leer_png(path: Path) -> np.ndarray:
    with Image.open(path) as handle:
        return np.asarray(handle.convert("RGB"), dtype=np.uint8)


def hex_de(pixel: tuple[int, int, int]) -> str:
    return "#%02x%02x%02x" % pixel


def medir_gate(png: Path, fixture: str, tema: str) -> dict:
    """El ratio del gate (`x6200-ab themes`) para el objeto declarado."""
    nontext_object, measure_nontext, _ = cargar_gate()
    obj = nontext_object(
        name="waterfall_center_line",
        fixture=fixture,
        theme=tema,
        object_rect=OBJETO,
        background_rect=FONDO,
        min_ratio=3.0,
        foreground_percentile=90.0,
        background_percentile=10.0,
        where="waterfall.c:waterfall_init",
    )
    res = measure_nontext(leer_png(png), obj)
    return {
        "ratio": round(res.ratio, 2),
        "ok": res.ok,
        "tinta": res.foreground,
        "fondo": res.background,
    }


def medir_peor_fila(on_png: Path, off_png: Path) -> dict:
    """La medida local fila por fila: la linea contra sus 8 px vecinos."""
    _, _, contrast_ratio = cargar_gate()
    on = leer_png(on_png).astype(int)
    off = leer_png(off_png).astype(int)
    ancho = on.shape[1]
    mascara = np.abs(on - off).sum(axis=2) > 0
    ys, xs = np.nonzero(mascara)
    if not len(xs):
        raise SystemExit(f"{on_png}: no encuentro la linea (diff contra {off_png} vacio)")
    x0, x1 = int(xs.min()), int(xs.max())
    ratios: list[float] = []
    tintas: set[tuple[int, int, int]] = set()
    for y in range(max(BANDA[0], int(ys.min())), min(BANDA[1], int(ys.max())) + 1):
        fila = mascara[y, x0 : x1 + 1]
        if not fila.any():
            continue
        idx = [x0 + i for i, v in enumerate(fila) if v]
        xc = idx[len(idx) // 2]
        tinta = tuple(int(c) for c in on[y, xc])
        vecinos = [tuple(int(c) for c in on[y, x]) for x in range(max(0, x0 - 9), x0)]
        vecinos += [tuple(int(c) for c in on[y, x]) for x in range(x1 + 1, min(ancho, x1 + 10))]
        fondo = tuple(int(statistics.median([c[i] for c in vecinos])) for i in range(3))
        tintas.add(tinta)
        ratios.append(contrast_ratio(hex_de(tinta), hex_de(fondo)))
    return {
        "filas": len(ratios),
        "x": [x0, x1],
        "min": round(min(ratios), 2),
        "mediana": round(float(np.median(ratios)), 2),
        "max": round(max(ratios), 2),
        "ge3_pct": round(100.0 * sum(1 for r in ratios if r >= 3.0) / len(ratios), 1),
        "tintas": [hex_de(c) for c in sorted(tintas)],
    }


def medir_pasada(png: Path, off: Path, fixture: str, tema: str) -> dict:
    medida = {
        "gate": medir_gate(png, fixture, tema),
        "local": medir_peor_fila(png, off),
        "png": str(png),
        "md5": hashlib.md5(png.read_bytes()).hexdigest(),
    }
    return medida


def main() -> int:
    ap = argparse.ArgumentParser(description="Contraste de la linea central del waterfall (#140)")
    ap.add_argument("--antes-on", type=Path, required=True)
    ap.add_argument("--antes-off", type=Path, required=True)
    ap.add_argument("--despues-on", type=Path, required=True)
    ap.add_argument("--despues-off", type=Path, required=True)
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    filas = []
    for clave, fixture, tema in TEMAS:
        entrada: dict = {"tema": tema, "fixture": fixture}
        for nombre, on_dir, off_dir in (
            ("antes", args.antes_on, args.antes_off),
            ("despues", args.despues_on, args.despues_off),
        ):
            png = on_dir / f"{fixture}.png"
            off = off_dir / f"off-{clave}.png"
            if not png.is_file() or not off.is_file():
                raise SystemExit(f"falta {png} o {off}: corre los renders del harness primero")
            entrada[nombre] = medir_pasada(png, off, fixture, tema)
        filas.append(entrada)

    if args.json:
        print(json.dumps(filas, indent=2, ensure_ascii=False))
        return 0

    print("| Tema | fixture | antes gate | antes peor fila | despues gate | despues peor fila |")
    print("| --- | --- | --- | --- | --- | --- |")
    for f in filas:
        a, d = f["antes"], f["despues"]
        print(
            f"| {f['tema']} | `{f['fixture']}` | {a['gate']['ratio']:.2f}:1 | "
            f"{a['local']['mediana']:.2f}:1 (min {a['local']['min']:.2f}, "
            f"{a['local']['ge3_pct']:.0f}% >= 3) | {d['gate']['ratio']:.2f}:1 | "
            f"{d['local']['mediana']:.2f}:1 (min {d['local']['min']:.2f}, "
            f"{d['local']['ge3_pct']:.0f}% >= 3) |"
        )
    print()
    for f in filas:
        a, d = f["antes"], f["despues"]
        ancho_a = a["local"]["x"][1] - a["local"]["x"][0] + 1
        ancho_d = d["local"]["x"][1] - d["local"]["x"][0] + 1
        print(
            f"{f['tema']}: tinta {a['gate']['tinta']} -> {d['gate']['tinta']} sobre "
            f"{d['gate']['fondo']}; ancho {ancho_a} px -> {ancho_d} px"
        )
        print(f"  antes   {a['md5']}  {a['png']}")
        print(f"  despues {d['md5']}  {d['png']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
