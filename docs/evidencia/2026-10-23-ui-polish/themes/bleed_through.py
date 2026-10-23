#!/usr/bin/env python3
"""Test negativo del sangrado (bleed-through) del Home a traves de la superficie del dialogo.

Que mide y por que asi
----------------------
El cuerpo de la ventana de un dialogo es `src/styles.c::dialog_style` (796x348 en x2/y66), que
pinta una imagen bmp del tema y NO es opaca: los pixeles del Home (fila de frecuencia, trazas del
espectro, columnas del waterfall) se veian a traves de las filas de texto (hallazgo P2-1 de la
auditoria de apps del 2026-10-23, docs/evidencia/2026-10-23-ui-audit/apps/README.md).

El test renderiza la MISMA fixture dos veces cambiando **solo la semilla** del generador de senal
(`seed`), que cambia el contenido del espectro/waterfall del Home y nada del dialogo. Si la
superficie del dialogo es opaca, los dos renders tienen que ser **byte-identicos dentro del
rectangulo del dialogo**; si el Home sangra, la diferencia entre las dos semillas se ve a traves.

* `pixeles_con_sangrado`: pixeles del interior del dialogo que difieren entre las dos semillas
  (>= 1 nivel de 8 bits en cualquier canal). Es la cuenta que tiene que dar 0.
* `fuga`: la mediana de |desvio del dialogo| / |desvio de la pantalla| sobre los pixeles donde la
  pantalla si cambia (la fraccion de la estructura del Home que atraviesa la superficie).

Uso (corre el arnes en Docker; deja los renders en tools/ui_harness/build/out/bleed-284/):

  python3 docs/evidencia/2026-10-23-ui-polish/themes/bleed_through.py [--json salida.json]

Esto NO reemplaza el barrido ARM/QEMU de las 25 apps (mas abajo): el arnes de host solo renderiza
los dialogos REALES de Settings y Bluetooth (los otros 22 son mocks inertes,
tests/check_dialog_coverage.py), asi que la cobertura multi-app queda PENDING/BLOCKED hasta que
haya un entorno QEMU privado.

SPDX-License-Identifier: GPL-2.0-or-later
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[4]
HARNESS = ROOT / "tools" / "ui_harness"
BUILD = HARNESS / "build"
OUT = BUILD / "out" / "bleed-284"
IMAGE = os.environ.get("UI_HARNESS_IMAGE", "x6200-uiharness")
SEED_A, SEED_B = 20, 21
THEMES = ["aurora", "cathode", "redshift", "sprint", "sunlit", "trail"]
# Interior del dialogo (src/dialog.c: x2/y66 796x348), recortando 4 px del borde.
DIA = (6, 70, 794, 410)


def _canal(c: np.ndarray) -> np.ndarray:
    c = c / 255.0
    return np.where(c <= 0.03928, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def lum(img: np.ndarray) -> np.ndarray:
    return 0.2126 * _canal(img[..., 0]) + 0.7152 * _canal(img[..., 1]) + 0.0722 * _canal(img[..., 2])


def scenario_variant(base: Path, seed: int) -> str:
    """El texto de la fixture con la semilla cambiada (nada mas)."""
    texto = base.read_text()
    lineas = [f"seed {seed}" if l.strip().startswith("seed ") else l for l in texto.splitlines()]
    return "\n".join(lineas) + "\n"


def render(fixture: str, seed: int) -> Path:
    """Renderiza una variante (<fixture>-seed<seed>) y devuelve el PNG."""
    variantes = OUT / "scn"
    variantes.mkdir(parents=True, exist_ok=True)
    scn = variantes / f"{fixture}-seed{seed}.scn"
    scn.write_text(scenario_variant(HARNESS / "fixtures" / f"{fixture}.scn", seed))
    png = OUT / f"{fixture}-seed{seed}.png"
    subprocess.run(
        [
            "docker", "run", "--rm", "--entrypoint", "bash",
            "--user", f"{os.getuid()}:{os.getgid()}",
            "-v", f"{ROOT}:/src", "-v", f"{OUT}:/out",
            "-w", "/src/tools/ui_harness/build", IMAGE,
            "-lc",
            f"./ui_harness --scenario /out/scn/{scn.name} --png /out/{png.name} >/dev/null 2>&1",
        ],
        check=True,
    )
    return png


def medir(a: np.ndarray, b: np.ndarray, s_a: np.ndarray, s_b: np.ndarray, reg) -> dict:
    x0, y0, x1, y1 = reg
    va, vb = lum(a[y0:y1, x0:x1]), lum(b[y0:y1, x0:x1])
    sa, sb = lum(s_a[y0:y1, x0:x1]), lum(s_b[y0:y1, x0:x1])
    d_dialogo = np.abs(va - vb)
    d_pantalla = np.abs(sa - sb)
    dif = np.abs(a[y0:y1, x0:x1] - b[y0:y1, x0:x1]).max(axis=2) >= 1
    con_home = d_pantalla >= 4 / 255.0
    fuga = (
        float(np.median(d_dialogo[con_home] / d_pantalla[con_home]))
        if con_home.any()
        else float("nan")
    )
    return {
        "pixeles_con_sangrado": int(dif.sum()),
        "pixeles_donde_el_home_cambia": int(con_home.sum()),
        "fuga_mediana": round(fuga, 4),
        "amplitud_max_sangrado": round(float(d_dialogo.max()) * 255, 2),
    }


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--json", type=Path)
    args = ap.parse_args(argv)

    if not (BUILD / "ui_harness").exists():
        raise SystemExit(f"FALLO: falta el arnes construido ({BUILD / 'ui_harness'})")
    if subprocess.run(["docker", "image", "inspect", IMAGE], capture_output=True).returncode != 0:
        raise SystemExit(f"FALLO: falta la imagen {IMAGE}")

    filas, fallos = [], 0
    print("fixture                                   px.sangrado(A|B)  px.home  fuga  amplitud")
    for t in THEMES:
        pantalla = f"main-tema-{t}"
        sa = np.asarray(Image.open(render(pantalla, SEED_A)).convert("RGB")).astype(float)
        sb = np.asarray(Image.open(render(pantalla, SEED_B)).convert("RGB")).astype(float)
        for fixture in (f"bt-dialog-tema-{t}", f"settings-tema-{t}"):
            a = np.asarray(Image.open(render(fixture, SEED_A)).convert("RGB")).astype(float)
            b = np.asarray(Image.open(render(fixture, SEED_B)).convert("RGB")).astype(float)
            m = medir(a, b, sa, sb, DIA)
            m["fixture"] = fixture
            filas.append(m)
            estado = "OK" if m["pixeles_con_sangrado"] == 0 else "SANGRA"
            if m["pixeles_con_sangrado"]:
                fallos += 1
            print(
                f"  {fixture:<38} {m['pixeles_con_sangrado']:>9}  {m['pixeles_donde_el_home_cambia']:>7}"
                f"  {m['fuga_mediana']:>6}  {m['amplitud_max_sangrado']:>7}  {estado}"
            )

    total = sum(f["pixeles_con_sangrado"] for f in filas)
    print(
        f"\n  sangrado total: {total} px de {len(filas)} fixtures reales "
        f"(Settings y Bluetooth x {len(THEMES)} temas) -> {'0 glifos/ghosts: OK' if total == 0 else 'FALLA'}"
    )
    if args.json:
        args.json.write_text(json.dumps({"seed_a": SEED_A, "seed_b": SEED_B, "fixtures": filas},
                                       indent=2) + "\n")
        print(f"  json: {args.json}")
    return 1 if fallos else 0


if __name__ == "__main__":
    sys.exit(main())
