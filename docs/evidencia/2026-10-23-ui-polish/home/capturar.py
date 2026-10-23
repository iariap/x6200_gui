#!/usr/bin/env python3
"""Captura la matriz de HOME (antes/despues) con el arnes REAL de host.

Issue #285 (hijo 1 de la epica #284). Este script:

1. genera los `.scn` de la matriz (en `scenarios/`) — 6 temas x 6 porcentajes
   (`--%`, 0, 9, 76, 99, 100) x 2 estados de fuente externa x 2 de carga, mas los
   estados de BT (off/on/connected, claves de fixtures-gaps) y la familia
   segmentada (dseg7);
2. corre cada escenario con el binario del arnes de la FASE pedida
   (`--phase before` / `--phase after`), con `--inspect`, y escribe PNG solo para
   el conjunto curado (`png-before/`, `png-after/`) y JSON en el scratch;
3. deja el log exacto (comando, exit code, sha256 del PNG) en `logs/`.

El binario vive FUERA del worktree (scratch privado de la corrida): el arbol solo
gana este directorio de evidencia. Uso:

    python3 capturar.py --phase before --build /tmp/opencode/ui-polish-home-284/build-base
    python3 capturar.py --phase after  --build /tmp/opencode/ui-polish-home-284/build-post

SPDX-License-Identifier: GPL-2.0-or-later
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
WT = HERE.parents[3]                       # worktree de la corrida
SCEN = HERE / "scenarios"
IMG = os.environ.get("UI_HARNESS_IMAGE", "x6200-uiharness")

THEMES = ["Aurora", "Cathode", "Redshift", "Sprint", "Sunlit", "Trail"]
# Etiqueta -> (battery_v, battery_cap, texto esperado del porcentaje)
PCTS = {
    "unknown": (0.0, 0, "--%"),
    "0": (12.1, 0, "0%"),
    "9": (12.1, 9, "9%"),
    "76": (12.1, 76, "76%"),
    "99": (12.1, 99, "99%"),
    "100": (12.1, 100, "100%"),
}
EXTS = {"none": 0.0, "ext138": 13.8}
CHGS = {"uncharged": "false", "charging": "true"}

# El conjunto con PNG (el resto de la matriz queda en JSON: la geometria se mide
# del arbol, no de la imagen).
PNG_CORE = {
    *(f"home-{t.lower()}" for t in THEMES),
    "home-bt-off",
    "home-bt-on",
    "home-bt-connected",
    *(f"home-bat-{p}" for p in PCTS),
    "home-bat-100-chg",
    "home-ext-none",
    "home-dseg",
}


def scenario_text(name: str, theme: str, pct: str, ext: str, chg: str,
                  bt: str | None = None, wifi: str = "off", digits: str | None = None) -> str:
    bat_v, cap, _ = PCTS[pct]
    lines = [
        f"# {name}: matriz de HOME del polish #285 (generado por capturar.py).",
        "# El componente real es src/clock.c + src/info.c; radio/FFT/parametros son",
        "# los mocks del arnes. La captura mide layout y contraste, no la radio.",
        f"name {name}",
        "screen main",
        "seed 20",
    ]
    if digits:
        lines.append(f"digits {digits}")
    lines += [
        f"theme {theme}",
    ]
    if bt:
        lines.append(f"bt_state {bt}")
        lines.append(f"wifi_status {wifi}")
    lines += [
        "frequency_hz 14074000",
        "tx_frequency_hz 14074000",
        "fft_width_hz 384000",
        "mode usb",
        "vfo A",
        "agc auto",
        "split false",
        "att false",
        "pre false",
        "atu false",
        "ant 1",
        "smeter_db -73",
        f"battery_v {bat_v}",
        f"external_v {EXTS[ext]}",
        f"battery_cap {cap}",
        f"charging {CHGS[chg]}",
        "rxtx rx",
        "",
    ]
    return "\n".join(lines)


def build_matrix() -> list[dict]:
    """La lista completa de escenarios (con su flag `png`)."""
    out: list[dict] = []

    # 1. Base por tema (el estado del fixture main-tema-*: 76%, fuente presente).
    for t in THEMES:
        out.append(dict(name=f"home-{t.lower()}", theme=t, pct="76", ext="ext138",
                        chg="uncharged"))

    # 2. Los tres estados de BT (Aurora) — la runa de dos trazos.
    for bt in ("off", "on", "connected"):
        out.append(dict(name=f"home-bt-{bt}", theme="Aurora", pct="76", ext="ext138",
                        chg="uncharged", bt=bt))

    # 3. La matriz completa de la banda de estado (6 temas x 6 pct x 2 ext x 2 chg).
    for t in THEMES:
        for pct in PCTS:
            for ext in EXTS:
                for chg in CHGS:
                    name = f"mx-{t.lower()}-{pct}-{ext}-{chg}"
                    out.append(dict(name=name, theme=t, pct=pct, ext=ext, chg=chg))

    # 4. Los extras curados (porcentajes, fuente ausente; carga; familia segmentada).
    for pct in PCTS:
        out.append(dict(name=f"home-bat-{pct}", theme="Aurora", pct=pct, ext="ext138",
                        chg="uncharged"))
    out.append(dict(name="home-ext-none", theme="Aurora", pct="76", ext="none",
                    chg="uncharged"))
    out.append(dict(name="home-bat-100-chg", theme="Aurora", pct="100", ext="ext138",
                    chg="charging"))
    out.append(dict(name="home-dseg", theme="Aurora", pct="76", ext="ext138",
                    chg="uncharged", digits="dseg7"))
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--phase", required=True, choices=["before", "after"])
    ap.add_argument("--build", required=True, help="directorio del build (dentro del scratch)")
    ap.add_argument("--scratch", required=True, help="scratch privado de la corrida (se monta en /scratch)")
    args = ap.parse_args()

    SCEN.mkdir(parents=True, exist_ok=True)
    scratch = Path(args.scratch)
    png_dir = HERE / f"png-{args.phase}"      # PNG curados (evidencia)
    json_dir = scratch / f"json-{args.phase}"  # arbol completo (fuera del arbol git)
    (HERE / "logs").mkdir(parents=True, exist_ok=True)
    png_dir.mkdir(parents=True, exist_ok=True)
    json_dir.mkdir(parents=True, exist_ok=True)

    matrix = build_matrix()
    for spec in matrix:
        spec["png"] = spec["name"] in PNG_CORE
        path = SCEN / f"{spec['name']}.scn"
        path.write_text(scenario_text(**{k: v for k, v in spec.items() if k != "png"}))

    # Un solo `docker run` con el bucle adentro: 162 corridas por fase.
    uid = os.getuid()
    gid = os.getgid()
    rel_scen = SCEN.relative_to(WT)
    rel_png = png_dir.relative_to(WT)
    # El arnes EXIGE --png; la matriz sin PNG curado lo escribe en el scratch
    # (se descarta: la geometria sale del arbol, no de la imagen).
    lines = ["set -e", "mkdir -p /scratch/mx-png"]
    for spec in matrix:
        name = spec["name"]
        png = (f" --png /src/{rel_png}/{name}.png" if spec["png"]
               else f" --png /scratch/mx-png/{name}.png")
        lines.append(
            f"echo \"--> {name}\" >> /scratch/capture-{args.phase}.log; "
            f"./ui_harness --scenario /src/{rel_scen}/{name}.scn"
            f"{png} --json /scratch/{json_dir.name}/{name}.json --inspect"
            f" >> /scratch/capture-{args.phase}.log 2>&1"
            f" || {{ echo \"FALLO {name} rc=$?\" >> /scratch/capture-{args.phase}.log; exit 1; }}"
        )
    lines.append("echo CAPTURE_OK")
    script = "\n".join(lines)

    cmd = [
        "docker", "run", "--rm", "--entrypoint", "bash",
        "--user", f"{uid}:{gid}",
        "-v", f"{WT}:/src",
        "-v", f"{scratch}:/scratch",
        "-w", f"/scratch/{Path(args.build).name}",
        IMG, "-lc", script,
    ]
    (HERE / "logs" / f"capturar-{args.phase}.cmd").write_text(" ".join(cmd) + "\n")
    print(f"capturando {len(matrix)} escenarios ({args.phase})...")
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0 or "CAPTURE_OK" not in proc.stdout:
        sys.stderr.write(proc.stdout)
        sys.stderr.write(proc.stderr)
        return 1
    shutil.copy(scratch / f"capture-{args.phase}.log", HERE / "logs" / f"capturar-{args.phase}.log")

    # sha256 de cada PNG + inventario (metrics del JSON: el proxy de flushes).
    inv = []
    for spec in matrix:
        name = spec["name"]
        jp = json_dir / f"{name}.json"
        pp = png_dir / f"{name}.png"
        doc = json.loads(jp.read_text())
        row = {
            "name": name,
            "png": spec["png"],
            "theme": doc.get("theme", {}).get("active"),
            "revision": doc.get("revision"),
            "metrics": doc.get("metrics"),
        }
        if pp.exists():
            row["sha256"] = hashlib.sha256(pp.read_bytes()).hexdigest()
        inv.append(row)
    (HERE / f"inventario-{args.phase}.csv").write_text(
        "name,png,theme,revision,flushes,flushed_pixels,framebuffer_fnv1a,sha256\n"
        + "".join(
            f"{r['name']},{int(bool(r['png']))},{r['theme']},{r['revision']},"
            f"{r['metrics']['flushes']},{r['metrics']['flushed_pixels']},"
            f"{r['metrics']['framebuffer_fnv1a']},{r.get('sha256', '')}\n"
            for r in inv
        )
    )
    print(f"OK: {sum(1 for r in inv if r['png'])} PNG en {png_dir} y {len(inv)} JSON en {json_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
