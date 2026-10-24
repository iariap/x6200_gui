#!/usr/bin/env python3
"""Test negativo del sangrado (bleed-through) de la Home bajo la superficie modal.

Que mide y por que asi
----------------------
El cuerpo de la ventana de las apps es `src/styles.c::dialog_style` (796x348 en
x2/y66). Antes de #286 esa superficie era translucida (el bitmap del tema al
~95 %): la fila de frecuencia, las trazas del espectro y las columnas del
waterfall del Home se veian a traves de las filas de texto (P2-1 de la auditoria
de apps, epica #269). #286 la volvio opaca (bg_color TC(dialog) COVER +
bg_img_recolor TC(dialog) COVER).

El metodo es el de #286 y no depende de juicios visuales: se renderiza la MISMA
fixture dos veces cambiando **solo la semilla** del generador de senal
(`seed 20` vs `seed 21`), que cambia el contenido del espectro/waterfall del Home
y nada del dialogo. Si la superficie es opaca, los dos renders son identicos
dentro del rectangulo del dialogo; si la Home sangra, la diferencia entre las dos
semillas aparece adentro.

  pixeles_con_sangrado   px del interior del dialogo que difieren entre semillas
                         (>= 1 nivel de 8 bits en cualquier canal) -> tiene que ser 0
  fuga_mediana           mediana de |delta dialogo| / |delta pantalla| sobre los px
                         donde la pantalla si cambia (fraccion de la estructura del
                         Home que atraviesa la superficie)
  amplitud_max           maximo delta de luminancia dentro del dialogo (0-255)

Ademas mide la **superficie** (el color modal del interior, que es el fondo que el
usuario ve entre los widgets) y lo compara con el rol `dialog` del tema. La paleta
se lee con `tests/check_wcag_rendered.py` (una sola fuente de la formula y de la
paleta): es la prueba de que la superficie visible es el color del tema y no el
bitmap compartido.

Cobertura: el arnes de host solo renderiza REALES los dialogos de Ajustes y
Bluetooth (binario por defecto) y las tres ventanas por tecla (binario
`ui_harness_dialogs`). GPS, Status, WiFi y las demas apps son descriptores
INERTES del arnes (tests/check_dialog_coverage.py): no se pueden renderizar aca y
se declaran, no se inventan.

Uso:
  python3 docs/evidencia/2026-10-23-ui-polish/final-qa/sangrado.py [--reusar]

SPDX-License-Identifier: GPL-2.0-or-later
"""

from __future__ import annotations

import argparse
import csv
import importlib.util
import os
import subprocess
import sys
from pathlib import Path

import numpy as np
from PIL import Image

AQUI = Path(__file__).resolve().parent
ROOT = AQUI.parents[3]
BUILD = Path(os.environ.get("QA284_BUILD", "/tmp/opencode/ui-polish-qa-284/build"))
IMAGE = os.environ.get("UI_HARNESS_IMAGE", "x6200-uiharness")
CONT = os.environ.get("CONT", "x6200-qa284-sangrado")
SALIDA = AQUI / "mediciones" / "sangrado"
SEED_A, SEED_B = 20, 21
THEMES = ["aurora", "cathode", "redshift", "sprint", "sunlit", "trail"]
# Interior de la superficie modal (src/dialog.c: x2/y66 796x348), 4 px de borde.
DIA = (6, 70, 794, 410)

# (fixture, binario, tema)
CASOS = (
    [(f"settings-tema-{t}", "ui_harness", t) for t in THEMES]
    + [(f"bt-dialog-tema-{t}", "ui_harness", t) for t in THEMES]
    + [
        ("dialog-msg-cw", "ui_harness_dialogs", "aurora"),
        ("dialog-msg-voice", "ui_harness_dialogs", "aurora"),
    ]
)


def cargar_paleta():
    """`theme_palette` del gate de contraste renderizado (una sola fuente)."""
    spec = importlib.util.spec_from_file_location("wcag_rendered", ROOT / "tests" / "check_wcag_rendered.py")
    if spec is None or spec.loader is None:
        raise SystemExit("FALLO: no pude cargar tests/check_wcag_rendered.py")
    modulo = importlib.util.module_from_spec(spec)
    sys.modules["wcag_rendered"] = modulo
    spec.loader.exec_module(modulo)
    bases = modulo._base_palettes()
    return {t: modulo.theme_palette(t.capitalize(), bases) for t in THEMES}


def variante(origen: Path, destino: Path, seed: int) -> None:
    """La fixture con la semilla cambiada y nada mas."""
    lineas = [
        f"seed {seed}" if linea.strip().startswith("seed ") else linea
        for linea in origen.read_text().splitlines()
    ]
    destino.parent.mkdir(parents=True, exist_ok=True)
    destino.write_text("\n".join(lineas) + "\n")


def render(scn: Path, png: Path, binario: str) -> None:
    subprocess.run(
        [
            "docker",
            "run",
            "--rm",
            "--name",
            f"{CONT}-{png.stem}",
            "--entrypoint",
            "bash",
            "--user",
            f"{os.getuid()}:{os.getgid()}",
            "-v",
            f"{ROOT}:/src",
            "-v",
            f"{SALIDA}:/out",
            "-v",
            f"{BUILD}:/build:ro",
            "-w",
            "/build",
            IMAGE,
            "-lc",
            f"/build/{binario} --scenario /src/{scn.relative_to(ROOT)} --png /out/{png.name} >/dev/null 2>&1",
        ],
        check=True,
    )


def lum(img: np.ndarray) -> np.ndarray:
    def canal(c: np.ndarray) -> np.ndarray:
        c = c / 255.0
        return np.where(c <= 0.03928, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)

    return 0.2126 * canal(img[..., 0]) + 0.7152 * canal(img[..., 1]) + 0.0722 * canal(img[..., 2])


def medir(a: np.ndarray, b: np.ndarray, s_a: np.ndarray, s_b: np.ndarray) -> dict:
    x0, y0, x1, y1 = DIA
    va, vb = lum(a[y0:y1, x0:x1]), lum(b[y0:y1, x0:x1])
    sa, sb = lum(s_a[y0:y1, x0:x1]), lum(s_b[y0:y1, x0:x1])
    d_dialogo = np.abs(va - vb)
    d_pantalla = np.abs(sa - sb)
    dif = np.abs(a[y0:y1, x0:x1] - b[y0:y1, x0:x1]).max(axis=2) >= 1
    con_home = d_pantalla >= 4 / 255.0
    fuga = float(np.median(d_dialogo[con_home] / d_pantalla[con_home])) if con_home.any() else float("nan")
    return {
        "pixeles_con_sangrado": int(dif.sum()),
        "pixeles_donde_el_home_cambia": int(con_home.sum()),
        "fuga_mediana": round(fuga, 4),
        "amplitud_max_sangrado": round(float(d_dialogo.max()) * 255, 2),
    }


def superficie(a: np.ndarray) -> tuple[str, str]:
    """(color modal del interior del dialogo, fraccion de px con ese color)."""
    x0, y0, x1, y1 = DIA
    px = a[y0:y1, x0:x1].reshape(-1, 3)
    colores, cuentas = np.unique(px, axis=0, return_counts=True)
    modal = colores[int(np.argmax(cuentas))]
    return "#{:02X}{:02X}{:02X}".format(*(int(v) for v in modal)), f"{cuentas.max()}/{len(px)}"


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--reusar", action="store_true", help="no re-renderizar lo que ya existe")
    args = ap.parse_args(argv)

    if not (BUILD / "ui_harness").exists():
        raise SystemExit(f"FALLO: falta el arnes construido ({BUILD / 'ui_harness'})")
    SALIDA.mkdir(parents=True, exist_ok=True)
    paletas = cargar_paleta()

    def png_de(fixture: str, binario: str, seed: int) -> Path:
        en_corpus = ROOT / "tools" / "ui_harness" / "fixtures" / f"{fixture}.scn"
        origen = (
            en_corpus
            if en_corpus.exists()
            else (ROOT / "tools" / "ui_harness" / "fixtures-gaps" / f"{fixture}.scn")
        )
        scn = SALIDA / "scn" / f"{fixture}-seed{seed}.scn"
        variante(origen, scn, seed)
        png = SALIDA / f"{fixture}-seed{seed}.png"
        if not (args.reusar and png.exists()):
            render(scn, png, binario)
        return png

    filas, fallos = [], 0
    print("fixture                        sangrado  px.home  fuga  amplitud  superficie  rol")
    for fixture, binario, tema in CASOS:
        s_a = np.asarray(Image.open(png_de(f"main-tema-{tema}", "ui_harness", SEED_A)).convert("RGB")).astype(
            float
        )
        s_b = np.asarray(Image.open(png_de(f"main-tema-{tema}", "ui_harness", SEED_B)).convert("RGB")).astype(
            float
        )
        pa = np.asarray(Image.open(png_de(fixture, binario, SEED_A)).convert("RGB")).astype(float)
        pb = np.asarray(Image.open(png_de(fixture, binario, SEED_B)).convert("RGB")).astype(float)
        m = medir(pa, pb, s_a, s_b)
        color, fraccion = superficie(pa)
        rol = "#" + paletas[tema]["dialog"].lstrip("#").upper()

        fila = {
            "fixture": fixture,
            "tema": tema,
            "binario": binario,
            **m,
            "superficie_medida": color,
            "rol_dialog": rol,
            "superficie_igual_al_rol": int(color == rol),
            "superficie_px": fraccion,
        }
        filas.append(fila)
        sangra = m["pixeles_con_sangrado"] != 0
        fallos += int(sangra)
        print(
            f"  {fixture:<28} {m['pixeles_con_sangrado']:>8}  {m['pixeles_donde_el_home_cambia']:>7}"
            f"  {m['fuga_mediana']:>5}  {m['amplitud_max_sangrado']:>8}"
            f"  {color}   {rol}  {'SANGRA' if sangra else 'OK'}"
        )

    with (AQUI / "mediciones" / "sangrado.csv").open("w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=list(filas[0].keys()))
        w.writeheader()
        w.writerows(filas)

    total = sum(f["pixeles_con_sangrado"] for f in filas)
    iguales = sum(f["superficie_igual_al_rol"] for f in filas)
    print(f"\n  sangrado total: {total} px en {len(filas)} dialogos REALES x semilla")
    print(f"  superficie == rol `dialog` del tema: {iguales}/{len(filas)}")
    return 1 if fallos else 0


if __name__ == "__main__":
    raise SystemExit(main())
