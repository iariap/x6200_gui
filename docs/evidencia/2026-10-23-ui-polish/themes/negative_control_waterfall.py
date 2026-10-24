#!/usr/bin/env python3
"""Control negativo REAL del contrato `region_min_content:waterfall` (#286).

Toma la captura REAL de `bt-dialog-tema-redshift` (la que hoy mide 7.1% en la parte
visible del waterfall) y:

  1. la mide tal cual -> el contrato PASA (la parte visible tiene contenido);
  2. aplana la region del waterfall ENTERA (el componente no se dibuja en ningun lado)
     -> el contrato FALLA, porque la parte visible queda sin contenido.

Es el control que prueba que excluir el area tapada no convierte "el waterfall esta en
blanco" en un pase: se mide sobre la imagen real y el arbol `--inspect` real, sin
fixtures sinteticas.

Uso (desde la raiz del repo, con el entorno del framework ya sincronizado):

  cd tools/ab_testing && uv run python \
      ../../docs/evidencia/2026-10-23-ui-polish/themes/negative_control_waterfall.py

SPDX-License-Identifier: GPL-2.0-or-later
"""

from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image

from x6200_ab.capture import Capture
from x6200_ab.contracts import ContractFinding, run_contracts
from x6200_ab.quality_config import load_contracts
from x6200_ab.regions import load_regions
from x6200_ab.revisions import Revision
from x6200_ab.scenarios import Scenario

REPO = Path(__file__).resolve().parents[4]
EVIDENCE = REPO / "docs" / "evidencia" / "2026-10-23-ui-polish" / "themes"
HARNESS_JSON = REPO / "tools/ui_harness/build/out/wcag-rendered/json"
FIXTURE = "bt-dialog-tema-redshift"
WATERFALL_REGION = (0, 196, 800, 222)  # la region del contrato (regions.yaml)
RULE = "region_min_content:waterfall"


def _capture(out_dir: Path, image: np.ndarray, report: dict) -> Capture:
    """Una `Capture` como la que deja el arnes: PNG + JSON con el arbol `--inspect`."""
    out_dir.mkdir(parents=True, exist_ok=True)
    png = out_dir / f"{FIXTURE}.png"
    Image.fromarray(image).save(png)
    data = dict(report)
    data["framebuffer"] = {"width": int(image.shape[1]), "height": int(image.shape[0])}
    data["metrics"] = {**data.get("metrics", {}), "png_path": str(png)}
    path = out_dir / f"{FIXTURE}.json"
    path.write_text(json.dumps(data))
    return Capture(
        scenario=Scenario(name=FIXTURE, screen="bt-dialog", filename=f"{FIXTURE}.scn", seed=20, raw={}),
        revision=Revision(label="neg-control", sha="0" * 40, worktree=Path("/nonexistent")),
        png=png,
        report_path=path,
        report=data,
        png_sha256=hashlib.sha256(png.read_bytes()).hexdigest(),
    )


def _finding(findings: list[ContractFinding], rule: str) -> ContractFinding:
    for finding in findings:
        if finding.rule == rule:
            return finding
    raise SystemExit(f"no finding for {rule}")


def main() -> int:
    quality = REPO / "tools" / "ab_testing" / "quality"
    regions = load_regions(quality / "regions.yaml")
    contracts = load_contracts(quality / "contracts.yaml", set(regions.regions))
    report = json.loads((HARNESS_JSON / f"{FIXTURE}.json").read_text())
    image = np.asarray(Image.open(EVIDENCE / "despues" / f"{FIXTURE}.png").convert("RGB")).copy()

    real = _capture(EVIDENCE / "neg-control-real", image, report)
    finding = _finding(run_contracts([real], regions, contracts), RULE)
    print(f"1) captura REAL sin tocar   : ok={finding.ok}  {finding.detail}")

    x, y, w, h = WATERFALL_REGION
    flat = image.copy()
    colour = tuple(int(v) for v in np.median(flat[y : y + h, x : x + w].reshape(-1, 3), axis=0))
    flat[y : y + h, x : x + w] = colour
    blank = _capture(EVIDENCE / "neg-control-flat", flat, report)
    finding_blank = _finding(run_contracts([blank], regions, contracts), RULE)
    print(
        f"2) waterfall entero aplanado : ok={finding_blank.ok}  {finding_blank.detail}  "
        f"(color plano {colour})"
    )

    ok = finding.ok is True and finding_blank.ok is False
    print(f"\n  control negativo: {'OK (la parte visible sigue mandando)' if ok else 'FALLA'}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
