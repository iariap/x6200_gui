#!/usr/bin/env python3
"""Contratos objetivos del proyecto (`tools/ab_testing`) sobre la matriz del QA final.

Reusa las MISMAS reglas que el job `contracts` del CI (no una copia): geometria
del arbol LVGL, tipografia, contenido de regiones, niveles de cobertura y el tema
efectivamente renderizado. Lee los PNG + JSON que dejo `capturar.sh`, asi que no
vuelve a construir ni a renderizar nada: mide exactamente las capturas que el
resto de la evidencia cita.

  python3 docs/evidencia/2026-10-23-ui-polish/final-qa/contratos.py

Salida: `mediciones/contratos.json` (crudo) y `mediciones/contratos.csv`
(un renglon por finding) + el resumen por estado en stdout.

Un finding `ok`/`waived` no es un fallo; `failure`/`defect` si. El veredicto del
script es 1 si hay algun failure.

Los imports del paquete `x6200_ab` van dentro de `main()` a proposito: el paquete
vive en `tools/ab_testing/src` y agregarlo a `sys.path` antes de los imports de
modulo seria un import fuera de orden (E402). Adentro de la funcion no lo es, y
no hace falta ninguna supresion.

SPDX-License-Identifier: GPL-2.0-or-later
"""

from __future__ import annotations

import argparse
import csv
import json
import sys
from collections import Counter
from pathlib import Path

AQUI = Path(__file__).resolve().parent
ROOT = AQUI.parents[3]
SHA = "1845cb77d36b93c4d8cd774fd130735f4d2f6f30"
QUALITY = ROOT / "tools" / "ab_testing" / "quality"


def main(argv=None) -> int:
    sys.path.insert(0, str(ROOT / "tools" / "ab_testing" / "src"))
    from x6200_ab.capture import Capture
    from x6200_ab.contracts import run_contracts
    from x6200_ab.known_issues import load_known_issues
    from x6200_ab.quality_config import load_contracts
    from x6200_ab.regions import load_regions
    from x6200_ab.revisions import Revision
    from x6200_ab.scenarios import Scenario

    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument(
        "--base",
        type=Path,
        default=AQUI,
        help="directorio de evidencia con png/ y json/ (default: el de este script)",
    )
    ap.add_argument("--json", type=Path)
    ap.add_argument("--csv", type=Path)
    ap.add_argument(
        "--solo-corpus",
        action="store_true",
        help="evaluar solo las fixtures que existen en el repo (205 + 13 gaps)",
    )
    args = ap.parse_args(argv)
    base = args.base
    sufijo = "-corpus" if args.solo_corpus else ""
    args.json = args.json or base / "mediciones" / f"contratos{sufijo}.json"
    args.csv = args.csv or base / "mediciones" / f"contratos{sufijo}.csv"

    region_map = load_regions(QUALITY / "regions.yaml")
    contracts = load_contracts(QUALITY / "contracts.yaml", set(region_map.regions))
    known = load_known_issues(QUALITY / "known_issues.yaml", set(region_map.regions))

    revision = Revision(label="candidate", sha=SHA, worktree=ROOT)
    capturas = []
    del_repo = {p.stem for p in (ROOT / "tools" / "ui_harness" / "fixtures").glob("*.scn")}
    del_repo |= {f"gaps-{p.stem}" for p in (ROOT / "tools" / "ui_harness" / "fixtures-gaps").glob("*.scn")}
    for ruta in sorted((base / "json").glob("*.json")):
        nombre = ruta.stem
        if args.solo_corpus and nombre not in del_repo:
            continue
        png = base / "png" / f"{nombre}.png"
        if not png.exists():
            print(f"  FALTA el PNG de {nombre}", file=sys.stderr)
            continue
        report = json.loads(ruta.read_text(errors="surrogateescape"))
        # El `screen` del JSON es el del arnes; el nombre del archivo es la
        # identidad de la captura (el corpus tiene fixtures con el mismo screen).
        scen = Scenario(
            name=nombre,
            screen=str(report.get("screen") or "main"),
            filename=f"{nombre}.scn",
            seed=report.get("seed"),
            raw={},
        )
        capturas.append(
            Capture(
                scenario=scen,
                revision=revision,
                png=png,
                report_path=ruta,
                report=report,
                png_sha256="",
            )
        )

    print(f"== {len(capturas)} capturas, {len(region_map.regions)} regiones, {len(known)} known issues ==")
    findings = run_contracts(capturas, region_map, contracts, known)

    filas = [
        {
            "captura": f.fixture,
            "regla": f.rule,
            "estado": "ok" if f.ok else ("defect" if f.baseline_issue else "failure"),
            "ok": int(f.ok),
            "detalle": f.detail,
            "path": f.path or "",
            "baseline_issue": f.baseline_issue or "",
        }
        for f in findings
    ]

    args.json.parent.mkdir(parents=True, exist_ok=True)
    args.json.write_text(
        json.dumps({"sha": SHA, "capturas": len(capturas), "findings": filas}, indent=1) + "\n"
    )
    with args.csv.open("w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=list(filas[0].keys()) if filas else ["vacio"])
        w.writeheader()
        w.writerows(filas)

    estados = Counter("ok" if f.ok else ("defect" if f.baseline_issue else "failure") for f in findings)
    print(f"  findings: {len(findings)}")
    for estado, n in sorted(estados.items()):
        print(f"    {estado:<10} {n}")

    fallos = [f for f in findings if not f.ok]
    if fallos:
        print(f"\n  {len(fallos)} FALLOS:")
        for f in fallos[:40]:
            print(f"    {f.fixture}: {f.rule}: {f.detail}")
    return 1 if fallos else 0


if __name__ == "__main__":
    raise SystemExit(main())
