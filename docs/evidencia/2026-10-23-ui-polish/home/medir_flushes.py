#!/usr/bin/env python3
"""El gate de costo de render (el proxy de `flushes`) sobre TODA la suite.

Issue #285: la banda de estado y las fuentes nuevas del reloj cambian lo que la
GUI invalida por tick, asi que el costo NO se declara: se mide. Es el mismo proxy
que usa el gate de Performance de `x6200-ab`
(tools/ab_testing/quality/budgets.yaml::performance, +10% px / +20% flushes) y el
que cazo la regresion real del PR #279: `metrics.flushes` y
`metrics.flushed_pixels` del arnes (`--json`), fixture por fixture, entre el
binario de ANTES y el de DESPUES.

Corre un `docker run` por binario con el bucle adentro (205 fixtures, ~30 s).

Uso: python3 medir_flushes.py --before <build-dir> --after <build-dir> --scratch <dir>
SPDX-License-Identifier: GPL-2.0-or-later
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
WT = HERE.parents[3]
IMG = os.environ.get("UI_HARNESS_IMAGE", "x6200-uiharness")

# Presupuestos del gate de Performance de x6200-ab (budgets.yaml): no se relajan.
MAX_FLUSHES_PCT = 20.0
MAX_PX_PCT = 10.0


def run_side(build: str, scratch: Path, side: str) -> None:
    fixtures = sorted((WT / "tools" / "ui_harness" / "fixtures").glob("*.scn"))
    lines = ["set -e"]
    for fx in fixtures:
        name = fx.stem
        lines.append(
            f"./ui_harness --scenario /src/tools/ui_harness/fixtures/{name}.scn "
            f"--png /scratch/px-{side}/{name}.png "
            f"--json /scratch/json-{side}/{name}.json >/dev/null 2>&1"
            f" || {{ echo \"FALLO {name}\"; exit 1; }}"
        )
    lines.append("echo SIDE_OK")
    cmd = [
        "docker", "run", "--rm", "--entrypoint", "bash",
        "--user", f"{os.getuid()}:{os.getgid()}",
        "-v", f"{WT}:/src", "-v", f"{scratch}:/scratch",
        "-v", f"{Path(build).parent}:/build",
        "-w", f"/build/{Path(build).name}",
        IMG, "-lc", "\n".join(lines),
    ]
    (HERE / "logs" / f"flushes-{side}.cmd").write_text(" ".join(cmd) + "\n")
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0 or "SIDE_OK" not in proc.stdout:
        raise SystemExit(f"FALLO el lado {side}: {proc.stdout}\n{proc.stderr}")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--before", required=True)
    ap.add_argument("--after", required=True)
    ap.add_argument("--scratch", required=True)
    args = ap.parse_args()

    scratch = Path(args.scratch)
    for side, build in (("before", args.before), ("after", args.after)):
        (scratch / f"json-{side}").mkdir(parents=True, exist_ok=True)
        (scratch / f"px-{side}").mkdir(parents=True, exist_ok=True)
        print(f"corriendo la suite con el binario {side}...")
        run_side(build, scratch, side)

    rows = []
    names = sorted(p.stem for p in (scratch / "json-before").glob("*.json"))
    for name in names:
        b = json.loads((scratch / "json-before" / f"{name}.json").read_text())["metrics"]
        a = json.loads((scratch / "json-after" / f"{name}.json").read_text())["metrics"]
        dfl = 100.0 * (a["flushes"] - b["flushes"]) / b["flushes"] if b["flushes"] else 0.0
        dpx = 100.0 * (a["flushed_pixels"] - b["flushed_pixels"]) / b["flushed_pixels"] \
            if b["flushed_pixels"] else 0.0
        rows.append(dict(
            fixture=name,
            flushes_before=b["flushes"], flushes_after=a["flushes"], d_flushes_pct=round(dfl, 2),
            px_before=b["flushed_pixels"], px_after=a["flushed_pixels"], d_px_pct=round(dpx, 2),
            fb_igual=(b["framebuffer_fnv1a"] == a["framebuffer_fnv1a"]),
            ok=("OK" if dfl <= MAX_FLUSHES_PCT and dpx <= MAX_PX_PCT else "FUERA"),
        ))

    out = HERE / "mediciones" / "flushes-suite.csv"
    with out.open("w", newline="") as fh:
        wr = csv.DictWriter(fh, fieldnames=list(rows[0]))
        wr.writeheader()
        wr.writerows(rows)

    fuera = [r for r in rows if r["ok"] != "OK"]
    peor_fl = max(rows, key=lambda r: r["d_flushes_pct"])
    peor_px = max(rows, key=lambda r: r["d_px_pct"])
    iguales = sum(1 for r in rows if r["fb_igual"])
    print(f"fixtures: {len(rows)} | framebuffer identico: {iguales}/{len(rows)}")
    print(f"peor flushes: {peor_fl['fixture']} {peor_fl['flushes_before']}->"
          f"{peor_fl['flushes_after']} ({peor_fl['d_flushes_pct']:+.2f}%, budget +{MAX_FLUSHES_PCT})")
    print(f"peor px: {peor_px['fixture']} {peor_px['px_before']}->{peor_px['px_after']} "
          f"({peor_px['d_px_pct']:+.2f}%, budget +{MAX_PX_PCT})")
    print(f"fuera de presupuesto: {len(fuera)}")
    for r in fuera[:10]:
        print(f"  {r['fixture']}: {r['d_flushes_pct']:+.2f}% flushes / {r['d_px_pct']:+.2f}% px")
    return 1 if fuera else 0


if __name__ == "__main__":
    raise SystemExit(main())
