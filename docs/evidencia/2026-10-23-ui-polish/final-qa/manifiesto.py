#!/usr/bin/env python3
"""Genera `manifest.csv`: una fila por captura con su cobertura y su medicion.

Columnas: las de la epica #269 (screen, state, theme, source_coverage, sha,
command, exit_code, png, width, height, issue_id, severity, source_path,
observation) + png_sha256, framebuffer_fnv1a, grupo y las dos metricas que
importan para el QA final (texto recortado / glifos faltantes).

Uso:  python3 .../manifiesto.py [--base DIR]
SPDX-License-Identifier: GPL-2.0-or-later
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
from pathlib import Path

from PIL import Image

AQUI = Path(__file__).resolve().parent
SHA = "1845cb77d36b93c4d8cd774fd130735f4d2f6f30"


def grupo(nombre: str) -> str:
    if nombre.startswith(("apps-menu-", "app-bands", "app-close-home", "locked-feedback")):
        return "apps-menu"
    if nombre.startswith(("settings-", "bt-dialog", "cw-tune", "dfl-", "swrscan-", "autotune", "dialog-")):
        return "dialogos"
    if nombre.startswith(("home-", "main-", "net-", "finalqa-", "bar-legacy", "vision-home")):
        return "home"
    return "otros"


def nivel_cobertura(cobertura: list[dict]) -> str:
    orden = ("REAL", "PARTIAL", "SYNTHETIC", "UNSUPPORTED")
    por_nivel: dict[str, list[str]] = {n: [] for n in orden}
    for c in cobertura:
        por_nivel.setdefault(c["level"], []).append(c["component"])
    partes = []
    for n in orden:
        if por_nivel.get(n):
            partes.append(f"{n}: {', '.join(sorted(por_nivel[n]))}")
    for n, comps in por_nivel.items():
        if n not in orden and comps:
            partes.append(f"{n}: {', '.join(sorted(comps))}")
    return " | ".join(partes)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--base", type=Path, default=AQUI)
    args = ap.parse_args(argv)
    base = args.base

    tsv = {}
    for linea in (base / "logs" / "capturas.tsv").read_text().splitlines():
        f = linea.split("\t")
        if len(f) >= 5:
            tsv[f[0]] = {"bin": f[1], "scn": f[2], "rc": f[3], "sha": f[4]}

    elementos = {}
    ruta_elem = base / "mediciones" / "elementos.csv"
    if ruta_elem.exists():
        for r in csv.DictReader(ruta_elem.open()):
            if r["texto"] in ("", "Text"):
                continue
            c = elementos.setdefault(r["captura"], {"truncado": 0, "missing": 0})
            c["truncado"] += int(r["truncado"])
            c["missing"] += int(bool(r["missing"]))

    filas = []
    for ruta in sorted((base / "json").glob("*.json")):
        d = json.loads(ruta.read_text(errors="surrogateescape"))
        nombre = ruta.stem
        png = base / "png" / f"{nombre}.png"
        ancho = alto = 0
        sha_png = ""
        if png.exists():
            with Image.open(png) as im:
                ancho, alto = im.size
            sha_png = hashlib.sha256(png.read_bytes()).hexdigest()
        t = tsv.get(nombre, {})
        e = elementos.get(nombre, {})
        tema = (d.get("theme") or {}).get("active") or (d.get("theme") or {}).get("requested") or ""
        obs = []
        if e.get("truncado"):
            obs.append(f"texto truncado: {e['truncado']}")
        if e.get("missing"):
            obs.append(f"glifos faltantes: {e['missing']}")
        if not obs:
            obs.append("sin recorte ni glifos faltantes")
        filas.append(
            {
                "screen": d.get("screen", ""),
                "state": nombre,
                "theme": tema,
                "source_coverage": nivel_cobertura(d.get("coverage") or []),
                "sha": d.get("revision", ""),
                "command": f"{t.get('bin', '')} --scenario {t.get('scn', '')} --png {png.name} "
                f"--json {ruta.name} --inspect",
                "exit_code": t.get("rc", ""),
                "png": str(png.relative_to(base)) if png.exists() else "",
                "width": ancho,
                "height": alto,
                "issue_id": "#287",
                "severity": "info",
                "source_path": "",
                "observation": "; ".join(obs),
                "png_sha256": sha_png,
                "framebuffer_fnv1a": (d.get("metrics") or {}).get("framebuffer_fnv1a", ""),
                "grupo": grupo(nombre),
            }
        )

    destino = base / "manifest.csv"
    with destino.open("w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=list(filas[0].keys()))
        w.writeheader()
        w.writerows(filas)
    print(f"  manifest.csv: {len(filas)} capturas")
    por_grupo: dict[str, int] = {}
    for f in filas:
        por_grupo[f["grupo"]] = por_grupo.get(f["grupo"], 0) + 1
    for g, n in sorted(por_grupo.items()):
        print(f"    {g:<12} {n}")
    revisiones = {f["sha"] for f in filas}
    print(f"  revisiones en el manifest: {sorted(revisiones)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
