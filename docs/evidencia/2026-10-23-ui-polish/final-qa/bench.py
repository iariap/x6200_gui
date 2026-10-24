#!/usr/bin/env python3
"""Proxy de costo de render: baseline (c90453fd) vs candidato (1845cb77).

Corre `ui_harness --bench` (reloj real CLOCK_MONOTONIC, warm-up descartado) sobre
la misma lista de fixtures en los DOS builds y compara:

  render_ms   tiempo de pared del frame completo (draw + flush)
  draw_ms     layout + dibujo (derivado, no inventado)
  flush_ms    tiempo dentro del callback de flush
  flushes / redrawn_area_px   contadores deterministas del arnes

El entorno es un host x86: es un **proxy relativo** entre las dos revisiones, no
el costo del Cortex-A7. Un delta porcentual no es una regresion del dispositivo;
para decidir eso esta la pasada de hardware (PENDING).

Uso:
  python3 .../bench.py [--base-build /tmp/opencode/qa284-base-scratch/build]
SPDX-License-Identifier: GPL-2.0-or-later
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import statistics
import subprocess
from pathlib import Path

AQUI = Path(__file__).resolve().parent
ROOT = AQUI.parents[3]
ACT_BUILD = Path(os.environ.get("QA284_BUILD", "/tmp/opencode/ui-polish-qa-284/build"))
IMAGE = os.environ.get("UI_HARNESS_IMAGE", "x6200-uiharness")
SALIDA = AQUI / "mediciones" / "bench"  # se reescribe en main()

FIXTURES = (
    "home-usb-20m",
    "main-tema-aurora",
    "main-tema-cathode",
    "main-tema-redshift",
    "main-tema-sprint",
    "main-tema-sunlit",
    "main-tema-trail",
    "settings-tema-aurora",
    "settings-tema-redshift",
    "bt-dialog-tema-aurora",
    "bt-dialog-tema-redshift",
    "dfl-presets-cycles",
    "cw-tune-default",
    "swrscan-3pass",
)
WARMUP, REPEATS = 3, 7


def correr(build: Path, etiqueta: str) -> dict[str, dict]:
    destino = SALIDA / etiqueta
    destino.mkdir(parents=True, exist_ok=True)
    lista = destino / "lista.txt"
    lista.write_text("\n".join(FIXTURES) + "\n")
    subprocess.run(
        [
            "docker",
            "run",
            "--rm",
            "--name",
            f"x6200-qa284-bench-{etiqueta}",
            "--entrypoint",
            "bash",
            "--user",
            f"{os.getuid()}:{os.getgid()}",
            "-v",
            f"{ROOT}:/src",
            "-v",
            f"{build}:/build:ro",
            "-v",
            f"{destino}:/out",
            "-w",
            "/build",
            IMAGE,
            "-lc",
            "while read -r f; do /build/ui_harness "
            "--scenario /src/tools/ui_harness/fixtures/$f.scn "
            "--png /out/$f.png --json /out/$f.json "
            f"--bench --warmup {WARMUP} --repeats {REPEATS} >/dev/null 2>&1 || exit 1; "
            "done < /out/lista.txt",
        ],
        check=True,
    )
    datos = {}
    for f in FIXTURES:
        d = json.loads((destino / f"{f}.json").read_text())
        datos[f] = d
    return datos


def resumen(d: dict) -> dict:
    frames = d.get("benchmark", {}).get("frames") or []
    if not frames:
        return {}
    return {
        "render_p50": round(statistics.median(f["render_ms"] for f in frames), 3),
        "draw_p50": round(statistics.median(f["draw_ms"] for f in frames), 3),
        "flush_p50": round(statistics.median(f["flush_ms"] for f in frames), 3),
        "flushes": frames[0].get("flushes"),
        "redrawn_px": frames[0].get("redrawn_area_px"),
        "rss_bytes": (d.get("benchmark", {}).get("memory") or {}).get("rss_bytes"),
    }


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--base-build", type=Path, default=Path("/tmp/opencode/qa284-base-scratch/build"))
    ap.add_argument(
        "--base",
        type=Path,
        default=AQUI,
        help="directorio donde escribir mediciones/bench (default: este script)",
    )
    args = ap.parse_args(argv)
    global SALIDA
    SALIDA = args.base / "mediciones" / "bench"
    SALIDA.mkdir(parents=True, exist_ok=True)

    for build in (ACT_BUILD, args.base_build):
        if not (build / "ui_harness").exists():
            raise SystemExit(f"FALLO: falta el arnes en {build}")

    print(f"== --bench warmup={WARMUP} repeats={REPEATS}, {len(FIXTURES)} fixtures x 2 builds ==")
    base = correr(args.base_build, "base")
    act = correr(ACT_BUILD, "actual")

    filas = []
    for f in FIXTURES:
        b, a = resumen(base[f]), resumen(act[f])
        fila = {"fixture": f}
        for k in ("render_p50", "draw_p50", "flush_p50"):
            fila[f"{k}_base"] = b[k]
            fila[f"{k}_actual"] = a[k]
            fila[f"{k}_delta_pct"] = round((a[k] - b[k]) * 100.0 / b[k], 2) if b[k] else ""
        fila["flushes_base"], fila["flushes_actual"] = b["flushes"], a["flushes"]
        fila["redrawn_px_base"], fila["redrawn_px_actual"] = b["redrawn_px"], a["redrawn_px"]
        fila["rss_base"], fila["rss_actual"] = b["rss_bytes"], a["rss_bytes"]
        filas.append(fila)

    with (SALIDA.parent / "bench.csv").open("w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=list(filas[0].keys()))
        w.writeheader()
        w.writerows(filas)

    print(
        f"\n  {'fixture':<24} {'render p50 base':>16} {'actual':>8} {'delta':>8}"
        f" {'flushes':>8} {'redrawn px':>12}"
    )
    for f in filas:
        print(
            f"  {f['fixture']:<24} {f['render_p50_base']:>16} {f['render_p50_actual']:>8}"
            f" {f['render_p50_delta_pct']:>7}% {f['flushes_base']:>4}->{f['flushes_actual']:<3}"
            f" {f['redrawn_px_base']:>12}"
        )
    deltas = [f["render_p50_delta_pct"] for f in filas if f["render_p50_delta_pct"] != ""]
    print(f"\n  render p50 delta: p50={statistics.median(deltas)}% min={min(deltas)}% max={max(deltas)}%")
    print(f"  flushes iguales en todas: {all(f['flushes_base'] == f['flushes_actual'] for f in filas)}")
    print(
        f"  redrawn_area igual en todas: {all(f['redrawn_px_base'] == f['redrawn_px_actual'] for f in filas)}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
