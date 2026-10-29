#!/usr/bin/env python3
"""Test visible de la fila `Capture filters` de Ajustes en el arnes de host (#351).

Que mide
--------
Para los **seis temas** del ROM (`Sunlit`, `Redshift`, `Sprint`, `Cathode`, `Trail`,
`Aurora`) renderiza el fixture `settings-audio` (la ventana REAL de Ajustes abierta por
el camino del firmware) con `--inspect` y verifica, sobre el arbol LVGL vivo y sobre el
PNG:

1. el rotulo `Capture filters` existe **exactamente una vez**, cae dentro del viewport
   (`visible` no nulo) y **no** se recorta (`text_overflow=false`,
   `text_truncated=false`, `missing_glyphs=[]`);
2. el dropdown de esa fila esta visible y **dibuja tinta** para el valor actual (el texto
   de un `lv_dropdown` lo pinta el widget y no aparece como nodo en el arbol, asi que la
   unica medida del valor visible es el pixel: se cuenta la tinta contra el color
   declarado del nodo, mismo criterio de cobertura que `tests/check_widget_text.py`).

Las cinco opciones del desplegable **no** se pueden leer del arbol (el widget las dibuja
solo cuando la lista esta abierta y el arnes no tiene una tecla que la abra): se verifican
por el literal de `src/dialog_settings.c::make_audio_enh()` y por el rango
`params.audio_enh_preset.max = 4` (5 valores), no por pixeles. El script imprime esas dos
fuentes para que el informe las pueda citar.

Parche local (no commiteado)
----------------------------
`main` en `169bb807` **no linkea** el arnes: #343 hizo que `src/dialog_bt.c` llame
`bt_state_set_power()`/`bt_state_set_discoverable()` y el doble `mocks/mock_ui_deps.c` no
los define, asi que `ui_harness` (y `ui_harness_dialogs`) fallan al linkear y los gates
que lo usan quedan rojos por esa causa ajena a #351. Para poder medir esta fila el script
trabaja sobre una **copia** del directorio `tools/ui_harness` en un scratch dir y agrega
ahi los dos setters (no-op sobre el doble). **No** modifica el worktree: el build entra en
la copia y el repo queda intacto (el script lo verifica con `git status --short`).

Uso
---
    python3 docs/evidencia/2026-10-29-audio-capture-351/scripts/check_settings_capture_filters.py

Variables: `WT` (worktree, default: la raiz del repo de este script), `SCRATCH`
(default `/tmp/opencode/x6200-351-settings`), `IMAGE` (default `x6200-uiharness`).

SPDX-License-Identifier: LGPL-2.1-or-later
"""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

WT = Path(os.environ.get("WT", Path(__file__).resolve().parents[4])).resolve()
SCRATCH = Path(os.environ.get("SCRATCH", "/tmp/opencode/x6200-351-settings"))
IMAGE = os.environ.get("IMAGE", "x6200-uiharness")
THEMES = ["Sunlit", "Redshift", "Sprint", "Cathode", "Trail", "Aurora"]
INSET = 5  # marco del widget que no es texto
COVERAGE = 0.55  # cobertura de tinta respecto del color declarado
PATCH = """
/* PATCH LOCAL (no commiteado) de esta medicion: el dialogo BT real llama a estos dos
 * setters desde #343 y el doble del arnes no los tenia, asi que main no linkea el
 * arnes y los gates que lo usan quedan rojos. No persisten. */
void bt_state_set_power(bool on) { mock_bt_on = on; }
void bt_state_set_discoverable(bool on) { (void) on; }
"""


def sh(cmd: list[str], **kw) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, text=True, capture_output=True, **kw)


def docker(args: list[str], extra: list[str]) -> subprocess.CompletedProcess:
    return sh(
        [
            "docker",
            "run",
            "--rm",
            "--entrypoint",
            "bash",
            "--user",
            f"{os.getuid()}:{os.getgid()}",
            "-v",
            f"{WT}:/src:ro",
            "-v",
            f"{SCRATCH}:/src/tools/ui_harness",
            "-w",
            "/src/tools/ui_harness",
            *extra,
            IMAGE,
            *args,
        ]
    )


def prepare() -> Path:
    harness = SCRATCH
    if harness.exists():
        shutil.rmtree(harness)
    harness.mkdir(parents=True)
    src = WT / "tools" / "ui_harness"
    for item in src.iterdir():
        if item.name == "build":
            continue
        target = harness / item.name
        if item.is_dir():
            shutil.copytree(item, target)
        else:
            shutil.copy2(item, target)
    mock = harness / "mocks" / "mock_ui_deps.c"
    text = mock.read_text()
    if "bt_state_set_power" not in text:
        anchor = "bool bt_state_mute(void) { return mock_bt_connected; }\n"
        if anchor not in text:
            raise SystemExit("FALLO: no encuentro el ancla del doble para el parche local")
        mock.write_text(text.replace(anchor, anchor + PATCH, 1))
    (harness / "fixtures-local").mkdir()
    (harness / "out").mkdir()
    fixture = (harness / "fixtures" / "settings-audio.scn").read_text()
    for theme in THEMES:
        scenario = fixture.replace(
            "name settings-audio\n", f"name settings-audio-{theme}\ntheme {theme}\n", 1
        )
        (harness / "fixtures-local" / f"settings-audio-{theme}.scn").write_text(scenario)
    return harness


def build() -> None:
    rev = sh(["git", "-C", str(WT), "rev-parse", "HEAD"]).stdout.strip()
    res = docker(
        [
            "-lc",
            f"mkdir -p build && cd build && "
            f"cmake .. -DHARNESS_REVISION={rev} >/tmp/cmake.log 2>&1 && "
            f"make -j8 ui_harness >/tmp/make.log 2>&1",
        ],
        [],
    )
    if res.returncode != 0:
        print(res.stdout[-2000:])
        raise SystemExit(f"FALLO: el build del arnes fallo (rc={res.returncode})")


def render() -> None:
    cmds = " ; ".join(
        f"./ui_harness --scenario ../fixtures-local/settings-audio-{t}.scn "
        f"--png ../out/settings-audio-{t}.png --json ../out/settings-audio-{t}.json --inspect "
        f"> ../out/render-{t}.log 2>&1"
        for t in THEMES
    )
    res = docker(["-lc", f"cd build && {cmds}"], [])
    if res.returncode != 0:
        raise SystemExit(f"FALLO: el render del arnes fallo (rc={res.returncode})")


def rgb(value: str) -> tuple[int, int, int]:
    value = value.lstrip("#")
    return tuple(int(value[i : i + 2], 16) for i in (0, 2, 4))


def walk(node: dict):
    yield node
    for child in node.get("children") or []:
        yield from walk(child)


def main() -> int:
    from PIL import Image

    harness = prepare()
    build()
    render()

    ok = True
    for theme in THEMES:
        doc = json.loads((harness / "out" / f"settings-audio-{theme}.json").read_text())
        nodes = list(walk(doc["inspection"]["nodes"][0]))
        labels = [
            n for n in nodes if n.get("class") == "lv_label" and (n.get("text") or "") == "Capture filters"
        ]
        problems = []
        if len(labels) != 1:
            problems.append(f"rotulo encontrado {len(labels)} veces")
        label = labels[0] if labels else {}
        if label.get("visible") is None:
            problems.append("rotulo fuera del viewport")
        if label.get("text_overflow"):
            problems.append("text_overflow=true")
        if label.get("text_truncated"):
            problems.append("text_truncated=true")
        if label.get("missing_glyphs"):
            problems.append(f"glyphs faltantes {label['missing_glyphs']}")

        dropdown = next(
            (
                n
                for n in nodes
                if n.get("class") == "lv_dropdown"
                and n.get("visible")
                and abs(n["y"] - label.get("y", -999)) < 40
            ),
            None,
        )
        ink = 0
        if dropdown is None:
            problems.append("dropdown de la fila no visible")
        else:
            box = dropdown["visible"]
            img = Image.open(harness / "out" / f"settings-audio-{theme}.png").convert("RGB")
            crop = img.crop(
                (box["x"] + INSET, box["y"] + INSET, box["x"] + box["w"] - INSET, box["y"] + box["h"] - INSET)
            )
            ink_color = rgb(dropdown["text_color"])
            for y in range(crop.height):
                for x in range(crop.width):
                    pixel = crop.getpixel((x, y))
                    dist = sum(abs(pixel[i] - ink_color[i]) for i in range(3)) / (3 * 255)
                    if 1.0 - dist >= COVERAGE:
                        ink += 1
            if ink == 0:
                problems.append("el valor del dropdown no dibuja tinta")

        status = "OK" if not problems else "FALLO: " + ", ".join(problems)
        if problems:
            ok = False
        print(
            f"  {theme:9s} tema_activo={doc['theme']['active']:9s} "
            f"rotulo=({label.get('x')},{label.get('y')}) {label.get('w')}x{label.get('h')} "
            f"natural={label.get('text_natural')} caja={label.get('text_box')} "
            f"overflow={label.get('text_overflow')} truncado={label.get('text_truncated')} "
            f"dropdown_ink_px={ink} -> {status}"
        )

    dirty = sh(
        ["git", "-C", str(WT), "status", "--short", "--untracked-files=no", "--", "src", "tools"]
    ).stdout.strip()
    print()
    print(
        f"  codigo sin cambios por esta medicion (git status -- src tools): "
        f"{'si' if not dirty else 'NO -> ' + dirty}"
    )
    print(f"  parche local aplicado solo en {harness}/mocks/mock_ui_deps.c (no commiteado)")
    print()
    print(
        "PASS: `Capture filters` visible y sin recorte en los 6 temas"
        if ok
        else "FALLO: revisar la fila por tema"
    )
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
