#!/usr/bin/env python3
"""Legibilidad del texto DENTRO de la superficie modal: tinta real por widget.

Por que existe ademas del gate `tests/check_wcag_rendered.py`
------------------------------------------------------------
El gate mide `lv_label`s: toma la tinta del arbol y el fondo renderizado debajo
del glifo. Hay dos familias de texto que **no** son `lv_label` y por eso el gate
no las mide: el texto de un `lv_dropdown`, el de un `lv_textarea` y el de un
`lv_roller`. Su color sale de un estilo (`dialog_item_style`, src/styles.c:266) y
lo dibuja el widget.

La medicion es directa y no depende del color declarado: para cada widget de
texto visible dentro de la ventana modal se mira el **interior de su caja** (sin
el borde) y se cuentan los pixeles que NO son el color modal de la superficie.
Cero pixeles distintos de la superficie = el control no dibujo ningun glifo.

Para no confundir "widget vacio" con "texto invisible", el veredicto es
**comparativo entre temas**: el mismo widget (misma fixture sin el sufijo del
tema, misma clase, misma caja) tiene tinta en unos temas y no en otros. Un widget
que no dibuja en NINGUN tema no se reporta: esta vacio por diseno.

Uso:  python3 .../legibilidad.py [--base DIR]
SPDX-License-Identifier: GPL-2.0-or-later
"""

from __future__ import annotations

import argparse
import csv
import json
import re
from pathlib import Path

import numpy as np
from PIL import Image

AQUI = Path(__file__).resolve().parent
CLASES_CON_TEXTO = {"lv_label", "lv_dropdown", "lv_textarea", "lv_roller", "lv_checkbox"}
CLASES_SIN_TEXTO_EN_ARBOL = {"lv_dropdown", "lv_textarea", "lv_roller", "lv_checkbox"}
SUFIJO_TEMA = re.compile(r"-tema-(aurora|cathode|redshift|sprint|sunlit|trail)$")


def walk(n: dict, dentro: bool = False):
    yield n, dentro
    es_superficie = n.get("w") == 796 and n.get("h") == 348 and bool(n.get("visible"))
    for c in n.get("children") or []:
        yield from walk(c, dentro or es_superficie)


def familia(nombre: str) -> str:
    """La fixture sin el sufijo del tema (para comparar el mismo widget)."""
    return SUFIJO_TEMA.sub("", nombre)


def tema(nombre: str) -> str:
    m = SUFIJO_TEMA.search(nombre)
    return m.group(1) if m else "default"


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--base", type=Path, default=AQUI)
    args = ap.parse_args(argv)
    base = args.base

    filas = []
    for ruta in sorted((base / "json").glob("*.json")):
        d = json.loads(ruta.read_text(errors="surrogateescape"))
        nodos = (d.get("inspection") or {}).get("nodes") or []
        if not nodos:
            continue
        png = base / "png" / f"{ruta.stem}.png"
        if not png.exists():
            continue
        img = np.asarray(Image.open(png).convert("RGB"))
        interior = img[70:410, 6:794].reshape(-1, 3)
        colores, cuentas = np.unique(interior, axis=0, return_counts=True)
        superficie = tuple(int(v) for v in colores[int(np.argmax(cuentas))])

        for n, dentro in walk(nodos[0]):
            if not dentro or n.get("class") not in CLASES_CON_TEXTO:
                continue
            vis = n.get("visible")
            if not vis or vis["w"] <= 1 or vis["h"] <= 1 or n.get("hidden"):
                continue
            texto = (n.get("text") or "").strip()
            if texto == "Text":
                continue
            if texto.startswith("#") and " " in texto:
                texto = texto.split(" ", 1)[1]  # el mock antepone el color al texto
            if not texto and n.get("class") not in CLASES_SIN_TEXTO_EN_ARBOL:
                continue
            x0, y0 = vis["x"], vis["y"]
            x1, y1 = x0 + vis["w"], y0 + vis["h"]
            if x0 < 0 or y0 < 0 or x1 > img.shape[1] or y1 > img.shape[0]:
                continue
            ix0, iy0, ix1, iy1 = x0 + 5, y0 + 5, x1 - 5, y1 - 5
            if ix1 <= ix0 or iy1 <= iy0:
                ix0, iy0, ix1, iy1 = x0, y0, x1, y1
            caja = img[iy0:iy1, ix0:ix1].reshape(-1, 3).astype(int)
            dif = np.abs(caja - np.array(superficie)).max(axis=1)
            filas.append(
                {
                    "captura": ruta.stem,
                    "familia": familia(ruta.stem),
                    "tema": tema(ruta.stem),
                    "clase": n.get("class"),
                    "texto": texto,
                    "caja": f"{x0},{y0},{vis['w']},{vis['h']}",
                    "color_texto": (n.get("text_color") or "").upper(),
                    "superficie": "#{:02X}{:02X}{:02X}".format(*superficie),
                    "px_tinta": int((dif > 8).sum()),
                    "colores": len(np.unique(caja, axis=0)),
                }
            )

    # Veredicto comparativo entre temas para el mismo widget.
    grupos: dict[tuple[str, str, str], dict[str, int]] = {}
    for f in filas:
        clave = (f["familia"], f["clase"], f["caja"])
        grupos.setdefault(clave, {})[f["tema"]] = f["px_tinta"]
    for f in filas:
        clave = (f["familia"], f["clase"], f["caja"])
        con = [t for t, px in grupos[clave].items() if px > 0]
        sin = [t for t, px in grupos[clave].items() if px == 0]
        invisible = bool(f["px_tinta"] == 0 and con and sin)
        f["temas_con_tinta"] = ",".join(sorted(con))
        f["temas_sin_tinta"] = ",".join(sorted(sin))
        f["hallazgo"] = int(invisible or (f["px_tinta"] == 0 and bool(f["texto"])))

    med = base / "mediciones"
    med.mkdir(exist_ok=True)
    with (med / "legibilidad.csv").open("w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=list(filas[0].keys()))
        w.writeheader()
        w.writerows(filas)

    hallazgos = [f for f in filas if f["hallazgo"]]
    print(f"  widgets de texto dentro de la modal: {len(filas)}")
    print(f"  hallazgos (sin un solo pixel de tinta): {len(hallazgos)}")
    for f in hallazgos:
        print(
            f"    {f['captura']:<36} {f['clase']:<12} {f['texto'][:20]!r:<22}"
            f" caja {f['caja']:<18} tinta {f['color_texto']} sobre {f['superficie']}"
            f"  con-tinta:{f['temas_con_tinta']}"
        )
    # El veredicto del comando es el de la medicion: un widget de texto que no
    # dibuja un solo pixel de tinta es un hallazgo, y el comando sale 1.
    return 1 if hallazgos else 0


if __name__ == "__main__":
    raise SystemExit(main())
