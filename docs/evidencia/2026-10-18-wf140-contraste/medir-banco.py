#!/usr/bin/env python3
"""La linea central del waterfall medida sobre capturas del banco QEMU (#140).

Mide, por caso y fila del waterfall, cuatro cosas sobre el PNG del firmware ARM
real (qemu-arm-static, framebuffer falso, sin radio):

* **ancho**: la corrida horizontal del color de la linea que contiene x=400 (el
  centro del waterfall), en px;
* **color**: el RGB de esa corrida (se compara con el rol `text` del tema, que es
  el color que el fix le da a la linea);
* **filas**: cuantas filas de la banda declaran la linea (de punta a punta);
* **contraste**: WCAG 1.4.11 de la linea contra el piso del waterfall, en dos
  lecturas: contra el **pixel vecino** (la mediana de 8 px a cada lado de la
  corrida, el peor caso) y contra el **piso de la banda** (tinta percentil 90 del
  trazo contra fondo percentil 10 de una banda a la izquierda de la linea, el
  mismo par que mide el gate `x6200-ab themes` sobre el arnes).

Uso:

    python3 medir-banco.py \
        --caso "Aurora (despues)"=aurora-on.png:aurora-off.png:f8eeff \
        --caso "Aurora (antes)"=../2026-10-18-wf140-banco/08-fix-aurora-on.png:../2026-10-18-wf140-banco/09-fix-aurora-off.png:8a7ba8

El caso es `etiqueta=on.png:off.png:hex-esperado` (opcional `--banda y0 y1`,
por defecto 240 478: la franja del waterfall del framebuffer 800x480).
`--json` imprime el detalle. El `off` es el control: la misma pantalla con
`waterfall_center_line` apagado (no tiene que quedar linea).

Para capturas sin control `off` —la radio, donde el parametro no se puede
apagar a mano— el caso es `etiqueta=on.png:hex-esperado` (sin el `off`): se mide
igual pero no se chequea el control. Es la unica forma de medir
`07-restaurado.png` (Redshift, linea de 1 px, `#5b352f`).
"""

from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path
from statistics import median

import numpy as np
from PIL import Image


def leer_png(path: Path) -> np.ndarray:
    with Image.open(path) as handle:
        return np.asarray(handle.convert("RGB"), dtype=np.uint8)


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


def percentil(pixeles: np.ndarray, p: float) -> tuple[int, int, int]:
    """El pixel en el percentil `p` de la region, ordenado por luminancia."""
    planos = pixeles.reshape(-1, 3)
    lum = np.array([luminancia((int(r), int(g), int(b))) for r, g, b in planos])
    orden = np.argsort(lum, kind="stable")
    pixel = planos[orden[round(p / 100.0 * (orden.size - 1))]]
    return int(pixel[0]), int(pixel[1]), int(pixel[2])


def medir(
    caso: str, on_png: Path, off_png: Path | None, hex_esperado: str, banda: tuple[int, int]
) -> dict:
    on = leer_png(on_png).astype(int)
    esperado = np.array(a_rgb(hex_esperado))
    igual = (on == esperado).all(axis=2)
    ancho_img = on.shape[1]
    y0, y1 = banda

    anchos: Counter = Counter()
    ratios: list[float] = []
    filas_linea = 0
    for y in range(y0, y1 + 1):
        if not igual[y, 400]:
            continue
        x0 = 400
        while x0 > 0 and igual[y, x0 - 1]:
            x0 -= 1
        x1 = 400
        while x1 + 1 < ancho_img and igual[y, x1 + 1]:
            x1 += 1
        anchos[x1 - x0 + 1] += 1
        filas_linea += 1
        tinta = tuple(int(c) for c in on[y, 400])
        vecinos = [tuple(int(c) for c in on[y, x]) for x in range(max(0, x0 - 9), x0)]
        vecinos += [tuple(int(c) for c in on[y, x]) for x in range(x1 + 1, min(ancho_img, x1 + 10))]
        fondo = tuple(int(median([c[i] for c in vecinos])) for i in range(3))
        ratios.append(contraste(tinta, fondo))

    # Control: con el parametro apagado no queda linea en la columna del centro.
    control_off_px = None
    if off_png is not None:
        off = leer_png(off_png).astype(int)
        control_off_px = int((off == esperado).all(axis=2)[y0 : y1 + 1, 398:401].sum())

    # El par del gate: tinta percentil 90 del trazo vs fondo percentil 10 de la
    # banda a la izquierda de la linea (mismo framebuffer, mismas filas).
    trazo = on[y0 : y1 + 1, 398:401].reshape(-1, 3).astype(np.uint8)
    piso = on[y0 : y1 + 1, 330:390].reshape(-1, 3).astype(np.uint8)
    tinta_gate = percentil(trazo, 90.0)
    fondo_gate = percentil(piso, 10.0)

    filas_banda = y1 - y0 + 1
    return {
        "caso": caso,
        "on": str(on_png),
        "off": str(off_png) if off_png is not None else None,
        "color": hex_de(tuple(int(c) for c in esperado)),
        "color_medido": hex_de(tuple(int(c) for c in on[300, 400])),
        "ancho_px": anchos.most_common(1)[0][0] if anchos else 0,
        "ancho_hist": dict(anchos),
        "filas_con_linea": filas_linea,
        "filas_banda": filas_banda,
        "filas_pct": round(100.0 * filas_linea / filas_banda, 1),
        "control_off_px": control_off_px,
        "ratio_vecino_min": round(min(ratios), 2) if ratios else None,
        "ratio_vecino_mediana": round(float(median(ratios)), 2) if ratios else None,
        "ratio_vecino_max": round(max(ratios), 2) if ratios else None,
        "ratio_gate": round(contraste(tinta_gate, fondo_gate), 2),
        "tinta_gate": hex_de(tinta_gate),
        "fondo_gate": hex_de(fondo_gate),
    }


def main() -> int:
    ap = argparse.ArgumentParser(description="La linea central del waterfall en el banco QEMU (#140)")
    ap.add_argument("--caso", action="append", required=True, metavar="ETIQUETA=ON[:OFF]:HEX")
    ap.add_argument("--banda", nargs=2, type=int, default=[240, 478], metavar=("Y0", "Y1"))
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    filas = []
    for caso in args.caso:
        etiqueta, resto = caso.split("=", 1)
        partes = resto.split(":")
        if len(partes) == 2:
            on, hex_esperado = partes
            off = None
        else:
            on, off, hex_esperado = partes
        filas.append(medir(etiqueta, Path(on), Path(off) if off else None, hex_esperado, tuple(args.banda)))

    if args.json:
        print(json.dumps(filas, indent=2, ensure_ascii=False))
        return 0

    print("| Caso (banco QEMU, ARM real) | color | ancho | filas con la linea | ratio vs vecino | ratio vs piso (gate) |")
    print("| --- | --- | --- | --- | --- | --- |")
    for f in filas:
        print(
            f"| {f['caso']} | `{f['color_medido']}` | {f['ancho_px']} px | "
            f"{f['filas_con_linea']}/{f['filas_banda']} ({f['filas_pct']} %) | "
            f"{f['ratio_vecino_mediana']:.2f}:1 (min {f['ratio_vecino_min']:.2f}) | "
            f"**{f['ratio_gate']:.2f}:1** ({f['tinta_gate']} vs {f['fondo_gate']}) |"
        )
    print()
    for f in filas:
        control = f"{f['off']}" if f["off"] else "sin control"
        px = f["control_off_px"] if f["control_off_px"] is not None else "—"
        print(
            f"{f['caso']}: {f['on']} + control {control} — lineas por ancho {f['ancho_hist']}, "
            f"px del color esperado en la columna del control (x=398..400): {px}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
