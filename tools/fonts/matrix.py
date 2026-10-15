#!/usr/bin/env python3
"""The font matrix: renders every candidate family and scores it against the one
it replaces (issue #9, decision 1).

    tools/fonts/matrix.py capture --out /tmp/fontmatrix
    tools/fonts/matrix.py report  --baseline /tmp/fontbase --captures /tmp/fontmatrix
    tools/fonts/matrix.py gallery --captures /tmp/fontmatrix --out gallery.png

`capture` builds the host harness once (Docker image `x6200-uiharness`) and runs
one fixture per family with `--font <id>` (src/fonts/font_families.h): one build
carries every candidate, so the whole matrix is one revision. Each run writes the
PNG (the gallery) and the JSON with `--inspect`, which is where the measured
numbers come from: the text box `lv_txt_get_size` resolves per label, its
overflow/truncation flags and the glyph coverage of the font chain.

`report` compares each family against the baseline render (the tree before the
migration, i.e. the proprietary family) and prints the table the decision reads:
how far the text boxes moved, and whether any label overflows, truncates or
misses a glyph that the baseline did not.

`gallery` is the human half of the decision: a contact sheet of the candidates
over the same render, for the owner's eye.

SPDX-License-Identifier: LGPL-2.1-or-later
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
MANIFEST_PATH = HERE / "candidates.json"
SRC_FONTS = ROOT / "src" / "fonts"

HARNESS_IMAGE = "x6200-uiharness"
FIXTURES = ROOT / "tools" / "ui_harness" / "fixtures"

# The fixtures the matrix renders: every screen the type appears on, plus the
# themes that scale it (VISION) or name a family of their own (Classic/RS ->
# roboto). They are the 800x480 render of the real firmware, not a mock-up.
DEFAULT_SCENARIOS = [
    "home-usb-20m",
    "main-natural",
    "main-tema-vision",
    "main-tema-classic",
    "main-otro-tema",
    "home-frecuencia-extrema",
    "settings-home",
    "settings-perfil",
]

# The slots the "ink size" column reads: the two the top bar and the clock use.
INK_SLOTS = (20, 30)


def load_manifest() -> dict:
    return json.loads(MANIFEST_PATH.read_text())


def families(manifest: dict) -> list[tuple[str, str]]:
    """`[(font id, label)]`: the stock family first, then every alternate.

    The segmented (digit) families are not whole-family choices: they are captured
    on the digit screens with `--digits` and scored separately (`digit_families`).
    """
    stock = next(
        (entry for entry in manifest["candidates"] if entry["name"] == manifest["stock"]), None
    )
    label = f"stock ({stock['display']})" if stock else "stock"
    return [("stock", label)] + [
        (entry["c_name"], entry["display"])
        for entry in manifest["candidates"]
        if entry is not stock and not entry.get("segmented")
    ]


def digit_families(manifest: dict) -> list[tuple[str, str]]:
    return [
        (entry["c_name"], f"{entry['display']} (digits)")
        for entry in manifest["candidates"]
        if entry.get("segmented")
    ]


def docker(argv: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(argv, capture_output=True, text=True, check=False)


# --- capture ----------------------------------------------------------------


def build_harness() -> None:
    rev = subprocess.run(
        ["git", "-C", str(ROOT), "rev-parse", "HEAD"], capture_output=True, text=True, check=False
    ).stdout.strip()
    script = (
        f"set -euo pipefail; cd build; "
        f"cmake .. -DHARNESS_REVISION={rev} >/dev/null && make -j$(nproc) >/dev/null"
    )
    result = docker(
        [
            "docker", "run", "--rm", "--entrypoint", "bash",
            "-v", f"{ROOT}:/src", "-w", "/src/tools/ui_harness", HARNESS_IMAGE, "-lc", script,
        ]
    )
    if result.returncode != 0:
        raise SystemExit(f"harness build failed:\n{result.stdout[-2000:]}{result.stderr[-2000:]}")


def cmd_capture(args: argparse.Namespace) -> int:
    manifest = load_manifest()
    scenarios = args.scenario or DEFAULT_SCENARIOS
    fonts = args.font or (
        [font for font, _label in digit_families(manifest)]
        if args.digits
        else [font for font, _label in families(manifest)]
    )
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)

    if not args.no_build:
        build_harness()

    for font in fonts:
        target = out / font
        target.mkdir(parents=True, exist_ok=True)
        flag = ""
        if font != "stock":
            flag = f"--digits {font} " if args.digits else f"--font {font} "
        script = (
            "set -euo pipefail; cd build; "
            + " && ".join(
                f"./ui_harness --scenario ../fixtures/{scenario}.scn "
                f"--png /out/{font}/{scenario}.png --json /out/{font}/{scenario}.json "
                f"--inspect {flag}>/dev/null"
                for scenario in scenarios
            )
        )
        result = docker(
            [
                "docker", "run", "--rm", "--entrypoint", "bash",
                "-v", f"{ROOT}:/src", "-w", "/src/tools/ui_harness",
                "-v", f"{out}:/out", HARNESS_IMAGE, "-lc", script,
            ]
        )
        if result.returncode != 0:
            print(f"FAIL {font}: {result.stderr[-800:]}", file=sys.stderr)
            return 1
        print(f"captured {font}: {len(scenarios)} fixtures -> {target}")
    return 0


# --- report -----------------------------------------------------------------

def nodes(document: dict) -> list[dict]:
    """Every inspected node with text, in document order."""
    found: list[dict] = []

    def walk(node: dict) -> None:
        if "text" in node and node.get("text"):
            found.append(node)
        for child in node.get("children", []):
            walk(child)

    for node in document["inspection"]["nodes"]:
        walk(node)
    return found


def load_capture(directory: Path, scenario: str) -> dict:
    return json.loads((directory / f"{scenario}.json").read_text())


def measure(directory: Path, scenarios: list[str]) -> dict:
    """The measured numbers of one family over the fixture set.

    `labels` keeps one entry per text node (per fixture, in document order) so the
    report can compare label by label: an aggregate width sum rewards a family
    whose labels are wrong in both directions (a monospace one matches the total
    with every label off), and that is exactly the "closest to Sony" question.
    """
    total = {
        "nodes": 0,
        "overflow": 0,
        "truncated": 0,
        "missing": 0,
        "fallback": 0,
        "labels": {},
        "line_height": {},
    }
    for scenario in scenarios:
        found = nodes(load_capture(directory, scenario))
        total["labels"][scenario] = [
            (int(node["text_natural"]["w"]), int(node["text_natural"]["h"])) for node in found
        ]
        total["nodes"] += len(found)
        for node in found:
            total["overflow"] += 1 if node["text_overflow"] else 0
            total["truncated"] += 1 if node["text_truncated"] else 0
            total["missing"] += len(node["missing_glyphs"])
            total["fallback"] += len(node["fallback_glyphs"])
            font = node.get("text_font", "")
            for slot in INK_SLOTS:
                if font.endswith(f"_{slot}"):
                    total["line_height"].setdefault(slot, int(node["text_line_height"]))
    return total


def label_drift(got: dict, baseline: dict) -> dict:
    """Per-label drift against the baseline: means, the 95th percentile, the sign.

    Nodes are paired by position: both trees are the same object tree, only the
    font changed, so the n-th text node of a fixture is the same widget. An empty
    comparison (a structure that moved) is reported as such, never as 0 drift.
    """
    errors: list[float] = []
    signed_w = signed_h = 0.0
    paired = 0
    for scenario, labels in baseline["labels"].items():
        for (bw, bh), (gw, gh) in zip(labels, got["labels"].get(scenario, [])):
            paired += 1
            if bw:
                errors.append(abs(gw - bw) / bw)
                signed_w += (gw - bw) / bw
            if bh:
                signed_h += (gh - bh) / bh
    errors.sort()
    p95 = errors[min(len(errors) - 1, int(0.95 * len(errors)))] if errors else 0.0
    return {
        "paired": paired,
        "width_pct": 100 * sum(errors) / paired if paired else 0.0,
        "height_pct": 100 * abs(signed_h) / paired if paired else 0.0,
        "width_signed_pct": 100 * signed_w / paired if paired else 0.0,
        "p95_pct": 100 * p95,
    }


def cap_height(prefix: str, slot: int) -> int | None:
    """The ink height of `A` in a generated family at one slot, in px.

    Read from the generated C instead of from the TTF: it is what the ROM draws,
    and it is the legibility number next to the width drift (a family can match
    the text width with letters that are visibly smaller, which is the regression
    the owner would see).
    """
    path = SRC_FONTS / f"{prefix}_{slot}.c"
    if not path.is_file():
        return None
    descriptor = path.read_text().split("glyph_dsc[] = {", 1)[-1]
    boxes = re.findall(r"\.box_h = (\d+)", descriptor)
    # Glyph id 0 is the reserved entry and id 1 is the space, so `A` (0x41) is at
    # 1 + (0x41 - 0x20) = 34, in both the stock family and the alternates.
    index = 1 + (ord("A") - 0x20)
    return int(boxes[index]) if len(boxes) > index else None


def cmd_report(args: argparse.Namespace) -> int:
    manifest = load_manifest()
    targets = json.loads((HERE / "sony-targets.json").read_text())
    scenarios = args.scenario or DEFAULT_SCENARIOS
    baseline = measure(Path(args.baseline).resolve(), scenarios)
    rows = []
    wanted = families(manifest) + (digit_families(manifest) if args.digits else [])
    for font, label in wanted:
        directory = Path(args.captures).resolve() / font
        if not (directory / f"{scenarios[0]}.json").is_file():
            print(f"skip {font}: no captures in {directory}", file=sys.stderr)
            continue
        got = measure(directory, scenarios)
        drift = label_drift(got, baseline)
        prefix = "sony" if font == "stock" else font
        ink = {
            slot: (cap_height(prefix, slot), targets[str(slot)]["cap_height"])
            for slot in INK_SLOTS
        }
        rows.append(
            {
                "font": font,
                "label": label,
                "nodes": got["nodes"],
                "paired": drift["paired"],
                "width_drift_pct": round(drift["width_pct"], 2),
                "width_signed_pct": round(drift["width_signed_pct"], 2),
                "height_drift_pct": round(drift["height_pct"], 2),
                "p95_pct": round(drift["p95_pct"], 2),
                "overflow": got["overflow"],
                "truncated": got["truncated"],
                "missing": got["missing"],
                "fallback": got["fallback"],
                "ink": ink,
            }
        )

    # The score is the distance from the family being replaced, with the defects
    # that matter first: a family that overflows, truncates or misses a glyph the
    # baseline did not is not a candidate at any distance (marked `!`), and among
    # the rest the one whose text boxes drifted the least over the whole fixture
    # set is the closest to Sony. `distance` = 0.65·|width Δ| + 0.35·|height Δ|.
    base_defects = baseline["overflow"] + baseline["truncated"] + baseline["missing"]
    for row in rows:
        # Two measured distances, equal weight, both in %:
        #  - `box`: the text box of every label (mean |Δw|/w and mean |Δh|/h), the
        #    metric the layout feels;
        #  - `ink`: the drawn size of `A` at the two slots the top bar and the clock
        #    use. Matching the line height does not match the ink: the libre
        #    families draw bigger letters at the same line height than Sony did, and
        #    a family that matches the boxes with visibly bigger letters is not
        #    "closer" to the family it replaces.
        ink_distance = 100 * sum(
            abs(cap / base - 1) for cap, base in row["ink"].values() if cap and base
        ) / max(1, sum(1 for cap, base in row["ink"].values() if cap and base))
        row["box"] = round(0.65 * row["width_drift_pct"] + 0.35 * row["height_drift_pct"], 2)
        row["ink_distance"] = round(ink_distance, 2)
        row["distance"] = round(row["box"] + ink_distance, 2)
        row["defects"] = row["overflow"] + row["truncated"] + row["missing"]
        row["ranked_out"] = row["defects"] > base_defects
    ranked = sorted(rows, key=lambda row: (row["ranked_out"], row["distance"], row["label"]))

    lines = [
        f"Baseline (the proprietary family): {baseline['nodes']} text nodes, "
        f"{baseline['overflow']} overflow, {baseline['truncated']} truncated, "
        f"{baseline['missing']} missing glyph(s), {baseline['fallback']} fallback",
        "",
        "| family | score | boxes | ink | width |Δw| | p95 |Δw| | sign Δw | overflow | truncated | missing | cap(20) | cap(30) |",
        "|---|---|---|---|---|---|---|---|---|---|---|---|",
    ]
    for index, row in enumerate(ranked):
        cap20, base20 = row["ink"][INK_SLOTS[0]]
        cap30, base30 = row["ink"][INK_SLOTS[1]]
        flag = " !" if row["ranked_out"] else ""
        best = " **<- proposed default**" if index == 0 and not row["ranked_out"] else ""
        cap20_text = f"{cap20}/{base20}" if cap20 else "-"
        cap30_text = f"{cap30}/{base30}" if cap30 else "-"
        lines.append(
            f"| {row['label']}{flag}{best} | {row['distance']:.2f} | {row['box']:.2f} "
            f"| {row['ink_distance']:.2f} | {row['width_drift_pct']:.2f} | {row['p95_pct']:.2f} "
            f"| {row['width_signed_pct']:+.1f}% | {row['overflow']} | {row['truncated']} "
            f"| {row['missing']} | {cap20_text} | {cap30_text} |"
        )
    lines += [
        "",
        "`score` = `boxes` + `ink`, both measured and in %. `boxes` = 0.65·mean|Δw|/w +"
        " 0.35·mean|Δh|/h over every text box of the fixture set, paired label by label against"
        " the baseline (0 = the same boxes the proprietary family produced); `ink` = the mean"
        " relative distance of the drawn height of `A` at the two slots of the top bar and the"
        " clock (column `cap(20)`/`cap(30)` against the baseline's own cap height). `p95|Δw|`"
        " is the worst 5% of labels; `sign Δw` says whether the family runs wide (+) or narrow"
        " (-). `!` marks a family that adds a legibility defect (overflow, truncation or a"
        " missing glyph) the baseline did not have: no score qualifies it. Lower is closer to"
        " the family being replaced.",
    ]
    text = "\n".join(lines)
    print(text)
    if args.markdown:
        Path(args.markdown).write_text(text + "\n")
    if args.json:
        Path(args.json).write_text(
            json.dumps({"baseline": baseline, "rows": ranked}, indent=2) + "\n"
        )
    return 0


# --- gallery ----------------------------------------------------------------


def cmd_gallery(args: argparse.Namespace) -> int:
    manifest = load_manifest()
    scenarios = args.scenario or [DEFAULT_SCENARIOS[0], "settings-home"]
    captures = Path(args.captures).resolve()
    families_list = families(manifest)
    tiles = []
    for font, label in families_list:
        for scenario in scenarios:
            png = captures / font / f"{scenario}.png"
            if not png.is_file():
                continue
            tiles.append((f"{label} · {scenario}", Image.open(png).convert("RGB")))
    if not tiles:
        raise SystemExit("no captures to build a gallery from")

    scale = args.scale
    columns = len(scenarios)
    width = int(tiles[0][1].width * scale)
    height = int(tiles[0][1].height * scale)
    label_h = 18
    rows = (len(tiles) + columns - 1) // columns
    sheet = Image.new("RGB", (columns * width, rows * (height + label_h)), "white")
    draw = ImageDraw.Draw(sheet)
    for index, (label, image) in enumerate(tiles):
        column, row = index % columns, index // columns
        x, y = column * width, row * (height + label_h)
        sheet.paste(image.resize((width, height)), (x, y + label_h))
        draw.text((x + 4, y + 4), label, fill="black")
    sheet.save(args.out)
    print(f"gallery: {len(tiles)} tiles -> {args.out}")
    return 0


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = parser.add_subparsers(dest="command", required=True)

    capture = sub.add_parser("capture", help="render every family over the fixture set")
    capture.add_argument("--out", required=True)
    capture.add_argument("--font", action="append", help="only this family id (repeatable)")
    capture.add_argument("--scenario", action="append", help="only this fixture (repeatable)")
    capture.add_argument(
        "--digits",
        action="store_true",
        help="capture the segmented digit families (one fixture, --digits) instead of the "
        "whole-family candidates",
    )
    capture.add_argument("--no-build", action="store_true", help="reuse the built harness")
    capture.set_defaults(function=cmd_capture)

    report = sub.add_parser("report", help="score the captures against a baseline render")
    report.add_argument("--baseline", required=True, help="captures of the pre-migration tree")
    report.add_argument("--captures", required=True)
    report.add_argument("--scenario", action="append")
    report.add_argument(
        "--digits",
        action="store_true",
        help="score the segmented digit families too (their capture dirs are named after "
        "them, like the whole-family ones)",
    )
    report.add_argument("--markdown")
    report.add_argument("--json")
    report.set_defaults(function=cmd_report)

    gallery = sub.add_parser("gallery", help="contact sheet of the candidates")
    gallery.add_argument("--captures", required=True)
    gallery.add_argument("--out", required=True)
    gallery.add_argument("--scenario", action="append")
    gallery.add_argument("--scale", type=float, default=0.5)
    gallery.set_defaults(function=cmd_gallery)

    args = parser.parse_args(argv[1:])
    return int(args.function(args))


if __name__ == "__main__":
    sys.exit(main(sys.argv))
