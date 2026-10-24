#!/usr/bin/env python3
"""Capturas antes/despues de las apps de #288 con el arnes REAL de host.

Fase 2 del issue #288 (epica #284): superficie visual ya aprobada (#285 + #286).

Corre un conjunto curado de fixtures de SWRScan con el binario del arnes de la fase
pedida (`--phase before|after`) y escribe PNG 800x480 solo para el conjunto curado
(`png-before/`, `png-after/`) + JSON con `--inspect` en el scratch privado de la
corrida (fuera del worktree). El log exacto (comando, exit, sha256) va a `logs/`.

Los binarios viven fuera del worktree:

    SCRATCH=/tmp/opencode/app-copy-288
    docker run --rm ... -v <arbol>:/src:ro -v $SCRATCH:/scratch \\
        -w /src/tools/ui_harness x6200-uiharness \\
        -lc "cmake -S ... -B /scratch/build-base && cmake --build /scratch/build-base -j8"

    python3 capturar_apps.py --phase before --build /scratch/build-base
    python3 capturar_apps.py --phase after  --build /scratch/build-post

`--build` es la ruta DENTRO del contenedor (el script monta el worktree y el
scratch y corre ahi). Ver README.md §2 para el comando completo.

SPDX-License-Identifier: GPL-2.0-or-later
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
WT = HERE.parents[3]
IMG = os.environ.get("UI_HARNESS_IMAGE", "x6200-uiharness")

# Fixture -> estado visible que cubre. `swrscan-owner-40m-sweep` corta el flow
# (`swr_feed 25`) y dispara el watchdog: estado Error con el motivo de abort.
FIXTURES = {
    "swrscan-quick": "completed (1 pasada)",
    "swrscan-3pass-p1": "scanning (pasada 1, 25/51)",
    "swrscan-3pass": "completed (3 pasadas)",
    "swrscan-3pass-stop": "stopped (parcial)",
    "swrscan-owner-40m-sweep": "error (timeout de RF)",
}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--phase", choices=("before", "after"), required=True)
    ap.add_argument("--build", required=True, help="ruta del build DENTRO del contenedor")
    ap.add_argument("--scratch", default=os.environ.get("SCRATCH", "/tmp/opencode/app-copy-288"))
    args = ap.parse_args()

    png_dir = HERE / f"png-{args.phase}"
    png_dir.mkdir(exist_ok=True)
    (HERE / "logs").mkdir(exist_ok=True)

    rel_png = png_dir.relative_to(WT)
    rel_log = (HERE / "logs" / f"capturar-{args.phase}.log").relative_to(WT)

    lines = ["set -e", "echo 'phase fixtures'; : > /src/" + str(rel_log)]
    for name in FIXTURES:
        lines.append(
            f"./ui_harness --scenario /src/tools/ui_harness/fixtures/{name}.scn "
            f"--png /src/{rel_png}/{name}.png "
            f"--json /scratch/{args.phase}-{name}.json --inspect "
            f">> /src/{rel_log} 2>&1"
        )
        lines.append(f"echo 'ok {name}'")
    lines.append("echo CAPTURE_OK")

    cmd = [
        "docker", "run", "--rm", "--entrypoint", "bash",
        "--user", f"{os.getuid()}:{os.getgid()}",
        "-v", f"{WT}:/src",
        "-v", f"{args.scratch}:/scratch",
        "-w", args.build,
        IMG, "-lc", "\n".join(lines),
    ]
    (HERE / "logs" / f"capturar-{args.phase}.cmd").write_text(" ".join(cmd) + "\n")

    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0 or "CAPTURE_OK" not in proc.stdout:
        sys.stderr.write(proc.stdout + proc.stderr)
        return 1

    inventory = []
    for name, state in FIXTURES.items():
        png = png_dir / f"{name}.png"
        raw = json.loads((Path(args.scratch) / f"{args.phase}-{name}.json").read_text())
        inventory.append(
            {
                "fixture": name,
                "state_cubierto": state,
                "png": str(png.relative_to(HERE)),
                "sha256": hashlib.sha256(png.read_bytes()).hexdigest(),
                "width": raw["framebuffer"]["width"],
                "height": raw["framebuffer"]["height"],
                "swr_state": raw["swr"]["state"],
                "swr_completed": raw["swr"]["completed"],
                "swr_stopped": raw["swr"]["stopped"],
                "swr_samples_fed": raw["swr"]["samples_fed"],
                "coverage_swrscan": next(
                    (c["level"] for c in raw["coverage"] if c["component"] == "dialog_swrscan.c"),
                    None,
                ),
            }
        )

    (HERE / f"inventario-{args.phase}.json").write_text(json.dumps(inventory, indent=1) + "\n")
    for row in inventory:
        print(f"{row['fixture']:26s} {row['swr_state']:24s} {row['sha256'][:12]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
