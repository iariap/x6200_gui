#!/usr/bin/env python3
"""Aisla el efecto del cambio de CONTRATO del efecto del cambio de RENDER (#286).

Corre las dos configuraciones sobre las MISMAS capturas (las que dejo
`x6200-ab contracts --workspace ...`):

  * `visible_under_modal` declarado (la config nueva);
  * `modal_surface`/`visible_under_modal` ausentes (la semantica anterior).

Asi la unica variable es el cambio de alcance de la medicion: si una fixture no tiene
la ventana modal, los numeros tienen que ser IDENTICOS; si la tiene, solo cambia la
region declarada (waterfall).

Uso:
  cd tools/ab_testing && uv run python \
      ../../docs/evidencia/2026-10-23-ui-polish/themes/isolate_contract_scope.py \
      /tmp/opencode/contrast-284/ctr-head

SPDX-License-Identifier: GPL-2.0-or-later
"""

from __future__ import annotations

import json
import sys
from dataclasses import replace
from pathlib import Path

from x6200_ab.capture import Capture
from x6200_ab.contracts import _region_content
from x6200_ab.inspection import inspection_of, iter_tree
from x6200_ab.quality_config import load_contracts
from x6200_ab.regions import load_regions
from x6200_ab.revisions import Revision
from x6200_ab.scenarios import Scenario

REPO = Path(__file__).resolve().parents[4]
QUALITY = REPO / "tools" / "ab_testing" / "quality"


def _captures(workspace: Path) -> list[Capture]:
    root = next(p for p in (workspace / "captures").iterdir() if p.is_dir())
    out: list[Capture] = []
    for png in sorted(root.glob("*.png")):
        report = json.loads(png.with_suffix(".json").read_text())
        out.append(
            Capture(
                scenario=Scenario(name=png.stem, screen="", filename="", seed=0, raw={}),
                revision=Revision(label="head", sha="0" * 40, worktree=Path("/nonexistent")),
                png=png,
                report_path=png.with_suffix(".json"),
                report=report,
                png_sha256="",
            )
        )
    return out


def _has_modal(capture: Capture, rect: tuple[int, int, int, int]) -> bool:
    inspection = inspection_of(capture)
    if inspection is None:
        return False
    for node, _ancestors, _path in iter_tree(inspection.nodes):
        box = node.visible
        if node.hidden or box is None:
            continue
        if (box.x, box.y, box.w, box.h) == rect and (node.bg_opa >= 250 or node.bg_image):
            return True
    return False


def main(argv: list[str]) -> int:
    workspace = Path(argv[1])
    regions = load_regions(QUALITY / "regions.yaml")
    new = load_contracts(QUALITY / "contracts.yaml", set(regions.regions))
    old = replace(new, modal_surface=None, modal_visible_regions=())
    assert new.modal_surface is not None, "contracts.yaml no declara modal_surface"

    con_dialogo = sin_dialogo = 0
    dif_sin = dif_con = 0
    ejemplos: list[str] = []
    for capture in _captures(workspace):
        has = _has_modal(capture, new.modal_surface)
        a = {f.rule: f for f in _region_content(capture, regions, old)}
        b = {f.rule: f for f in _region_content(capture, regions, new)}
        for rule in a:
            if (a[rule].ok, a[rule].detail) != (b[rule].ok, b[rule].detail):
                if has:
                    dif_con += 1
                    if len(ejemplos) < 6:
                        ejemplos.append(
                            f"  {capture.name:<34} {rule:<32} {a[rule].detail} -> {b[rule].detail}"
                        )
                else:
                    dif_sin += 1
        if has:
            con_dialogo += 1
        else:
            sin_dialogo += 1

    print(f"capturas: {con_dialogo + sin_dialogo}  (con ventana modal: {con_dialogo}, sin: {sin_dialogo})")
    print(f"findings de region_min_content que cambian SOLO por el cambio de contrato:")
    print(f"  fixtures SIN ventana modal : {dif_sin}")
    print(f"  fixtures CON ventana modal : {dif_con}")
    for linea in ejemplos:
        print(linea)
    return 0 if dif_sin == 0 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
