#!/usr/bin/env python3
"""Shape check of the drawn ROE/SWR curve (`tests/check_harness_swrscan_curve.sh`).

The owner's order of 2026-10-17 ("la parabola esta invertida: base abajo y las
puntas tienen que tender a infinito positivo") is about the SHAPE of the drawn
curve, so it is checked on the pixels of the harness capture, on the real chart
geometry (the chart band is y=69..399, see draw_cb() in src/dialog_swrscan.c and
the stub geometry in tests/native/test_swrscan.c). No OCR, no radio.

Two captures of the real window (`src/dialog_swrscan.c` + `src/roe_curve.c`) over
the fake acquisition backend are read:

* `swrscan-owner-40m-domo`: the owner's DOMO -- ROE ~1.0 at both edges and ~3.8
  in the middle (`26-roe-multipasada-final.png`, the inverted curve). It has to
  come out as a VALLEY: the lowest point of the drawn curve inside the bottom
  quarter of the band, at an interior column, with BOTH chart edges rising above
  it. Before the #125-fix the same fixture drew the inverted parabola (the
  lowest point at the chart edges) and this check fails: it is the harness RED.
* `swrscan-owner-40m`: the owner's real antenna valley (the seed of #125). It has
  to stay a valley, so the fix does not disturb the case that already worked.

Usage: swrscan_curve_evidence.py <dome-png> <valle-png>
       (prints the measured numbers, exits 1 on a miss)

SPDX-License-Identifier: LGPL-2.1-or-later
"""

from __future__ import annotations

import sys
from collections import Counter
from pathlib import Path

from PIL import Image

# The chart's drawing band, in screen pixels (draw_cb(): y1 = coords.y1 - 5 = 69,
# height = 330, so SWR 1.0 sits at the bottom, y=399, and SWR 5.0 above the top).
BAND_TOP = 69
BAND_BOTTOM = 399
BAND_H = BAND_BOTTOM - BAND_TOP          # 330

# The lowest point of the valley must fall in the bottom quarter of the band:
# SWR near the floor (the measured minimum is ROE ~1.0).
VALLEY_QUARTER = 0.25
VALLEY_MIN_Y = BAND_BOTTOM - VALLEY_QUARTER * BAND_H          # 316.5

# Both chart edges must rise above the valley (the tips tending to +infinity).
# The threshold is the fraction of the band that counts as a clear U; a shallow
# real resonance (ROE ~1.0-1.5, the owner's antenna) only rises a few px, so the
# per-fixture minimum is passed in.
EDGE_MIN_RISE = 0.15 * BAND_H
SHALLOW_EDGE_MIN_RISE = 3.0

# The valley column must be interior, not pinned to a chart border.
EDGE_MARGIN = 20

# Dilation radius that bridges the ~1-2 px gaps LVGL leaves between segments of a
# steep polyline, so the curve reads as ONE connected stroke.
DILATE = 3


def curve_mask(im: Image.Image) -> set[tuple[int, int]]:
    """Pixels of the drawn curve inside the chart band.

    The curve is painted with the theme's `text` role (see draw_cb()), the same
    colour as the axis labels and the readout. It is told apart by shape, not by
    a hard-coded palette: of every bright coloured ink the widest stroke is the
    curve (the labels are short glyph runs).
    """
    px = im.load()
    width, _ = im.size

    counts: Counter[tuple[int, int, int]] = Counter()

    for y in range(BAND_TOP, BAND_BOTTOM):
        for x in range(width):
            r, g, b = px[x, y]

            # bright and coloured: the theme's light text colour, not the panel
            # background (dark) and not the grey grid.
            if min(r, g, b) >= 180 and (max(r, g, b) - min(r, g, b)) >= 12:
                counts[(r, g, b)] += 1

    ink = {c for c, n in counts.items() if n >= 20}
    mask = {
        (x, y)
        for y in range(BAND_TOP, BAND_BOTTOM)
        for x in range(width)
        if px[x, y] in ink
    }
    return mask


def widest_stroke(mask: set[tuple[int, int]]) -> set[tuple[int, int]]:
    """The connected (dilated) stroke with the widest horizontal extent."""
    dilated: set[tuple[int, int]] = set()

    for x, y in mask:
        for dx in range(-DILATE, DILATE + 1):
            for dy in range(-DILATE, DILATE + 1):
                dilated.add((x + dx, y + dy))

    seen: set[tuple[int, int]] = set()
    best: list[tuple[int, int]] = []

    for start in dilated:
        if start in seen:
            continue

        stack = [start]
        seen.add(start)
        comp = []

        while stack:
            cx, cy = stack.pop()
            comp.append((cx, cy))

            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (1, -1), (-1, 1), (-1, -1)):
                n = (cx + dx, cy + dy)

                if n in dilated and n not in seen:
                    seen.add(n)
                    stack.append(n)

        xs = [x for x, _ in comp]

        if not best or (max(xs) - min(xs)) > (max(x for x, _ in best) - min(x for x, _ in best)):
            best = comp

    # Keep only the real curve-colour pixels of that stroke.
    return {pt for pt in best if pt in mask}


def stroke_bottom(mask: set[tuple[int, int]]) -> dict[int, int]:
    """For every column, the lowest y of the stroke: the curve trace."""
    bottom: dict[int, int] = {}

    for x, y in mask:
        if x not in bottom or y > bottom[x]:
            bottom[x] = y

    return bottom


class Checker:
    def __init__(self) -> None:
        self.failures = 0

    def check(self, ok: bool, message: str) -> None:
        print(f"  {'ok  ' if ok else 'FAIL'} {message}")

        if not ok:
            self.failures += 1

    def note(self, text: str) -> None:
        print(f"       {text}")


def check_valley(checker: Checker, name: str, png: Path, min_rise: float) -> None:
    checker.note(f"-- {name} ({png.name}) --")
    im = Image.open(png).convert("RGB")
    mask = curve_mask(im)
    curve = widest_stroke(mask)

    if not curve:
        checker.check(False, f"{name}: no encontre la curva dibujada")
        return

    bottom = stroke_bottom(curve)
    cols = sorted(bottom)
    xl, xr = cols[0], cols[-1]
    span = xr - xl

    valley_y = max(bottom.values())
    x_valley = min(x for x in cols if bottom[x] == valley_y)

    width, _ = im.size

    checker.note(
        f"curva x={xl}..{xr} ({span} px de {width}); banda y={BAND_TOP}..{BAND_BOTTOM}; "
        f"cuarto inferior y>={VALLEY_MIN_Y:.0f}"
    )
    checker.note(
        f"valle: y={valley_y} en x={x_valley}; bordes: izq y={bottom[xl]} (x={xl}), "
        f"der y={bottom[xr]} (x={xr})"
    )

    # The curve spans the run: a whole sweep is much wider than any glyph run.
    checker.check(span >= 0.7 * width, f"{name}: la curva cruza el chart ({span} px)")

    # Base at the bottom (the owner's "base abajo"): the lowest point of the
    # drawn curve lives in the bottom quarter of the chart band.
    checker.check(
        valley_y >= VALLEY_MIN_Y,
        f"{name}: base en el cuarto inferior (y={valley_y} >= {VALLEY_MIN_Y:.0f})",
    )

    # The valley is interior: a dome has its lowest point at the chart edges.
    checker.check(
        xl + EDGE_MARGIN <= x_valley <= xr - EDGE_MARGIN,
        f"{name}: valle interior (x={x_valley}, bordes {xl}..{xr})",
    )

    # Both tips rise above the base (the owner's "puntas a infinito positivo").
    left_rise = valley_y - bottom[xl]
    right_rise = valley_y - bottom[xr]

    checker.check(
        left_rise >= min_rise,
        f"{name}: borde izquierdo sube {left_rise} px sobre la base (>={min_rise:.0f})",
    )
    checker.check(
        right_rise >= min_rise,
        f"{name}: borde derecho sube {right_rise} px sobre la base (>={min_rise:.0f})",
    )


def main(argv: list[str]) -> int:
    if len(argv) != 3:
        print(__doc__)
        return 2

    checker = Checker()

    # The owner's dome is the RED/GREEN case: the fix turns the inverted parabola
    # into a deep U. The real valley is shallow (ROE ~1.0-1.5) and only has to
    # stay a valley (both edges still strictly above the base).
    check_valley(checker, "domo del owner -> valle", Path(argv[1]), EDGE_MIN_RISE)
    check_valley(checker, "valle real del owner -> valle", Path(argv[2]), SHALLOW_EDGE_MIN_RISE)

    if checker.failures:
        print(f"FAIL: {checker.failures} comprobacion(es) de forma")
        return 1

    print("PASS: la curva dibujada es un valle (base abajo, puntas hacia arriba)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
