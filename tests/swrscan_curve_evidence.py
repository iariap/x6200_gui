#!/usr/bin/env python3
"""Shape check of the drawn ROE/SWR curve (issue #125, script fixed in #132).

The owner's order of 2026-10-17 ("la parabola esta invertida: base abajo y las
puntas tienen que tender a infinito positivo") is about the SHAPE of the drawn
curve, so it is checked on pixels, on the real chart geometry (the chart band is
y=69..399, see draw_cb() in src/dialog_swrscan.c and the stub geometry in
tests/native/test_swrscan.c). No OCR: the curve is read by its ink colour.

The ink is detected per theme, so the same check works on the harness fixtures
(Aurora, text RGB(248,238,255)) and on radio captures (Redshift, text
RGB(255,179,160)): inside the chart band the grid (0xAAAAAA, neutral) and the
background (the most frequent colour) are discarded, and the most frequent
remaining colour is the curve/axis text ink. `--ink RRGGBB` forces a colour and
`--ink-min N` puts a brightness floor (min(r,g,b) >= N) on the auto detection.

Two modes:

* ONE capture -- measure and REPORT the drawn curve, for the radio re-test of
  the #125/#132 (`python3 tests/swrscan_curve_evidence.py /tmp/swrscan-u.png`).
  Since the owner's order of 2026-10-27 ("la curva siempre de los puntos
  medidos") the shape is decided by the DATA, so this mode does not impose an
  aesthetic: it prints the measured trace (ink, valley column, edges) and the
  classified shape (U / domo / rama) and fails only when there is no curve or it
  does not span the chart (nothing measured). It is the diagnostic the operator
  runs against a real capture.
* TWO captures -- the harness mode (`check_harness_swrscan_curve.sh`):

  - `swrscan-owner-40m-domo`: the owner's DOMO (ROE ~1.0 at both edges and ~3.8
    in the middle, `26-roe-multipasada-final.png`). Since the owner's order of
    2026-10-27 ("la curva siempre de los puntos medidos") the drawn shape is the
    MEASURED data: this fixture has to be drawn as a DOME (edges at the floor,
    interior rising), not smoothed into a valley.
  - `swrscan-owner-40m`: the owner's real antenna valley (the seed of #125). Its
    quadratic fit is a valley and it has to be drawn as a valley (both edges above
    the base).

Usage:
  swrscan_curve_evidence.py <capture-png>
  swrscan_curve_evidence.py <dome-png> <valle-png>

Options:
  --ink RRGGBB    force the curve ink (hex, with or without '#')
  --ink-min N     minimum of min(r,g,b) for the auto-detected ink (0..255)

Exit codes: 0 = every shape check passes; 1 = shape failure; 2 = usage/IO.
The measured numbers are printed either way.

SPDX-License-Identifier: LGPL-2.1-or-later
"""

from __future__ import annotations

import argparse
import sys
from collections import Counter
from pathlib import Path
from typing import cast

from PIL import Image, UnidentifiedImageError

# The chart's drawing band, in screen pixels (draw_cb(): y1 = coords.y1 - 5 = 69,
# height = 330, so SWR 1.0 sits at the bottom, y=399, and SWR 5.0 above the top).
BAND_TOP = 69
BAND_BOTTOM = 399
BAND_H = BAND_BOTTOM - BAND_TOP  # 330

# The lowest point of the valley must fall in the bottom quarter of the band:
# SWR near the floor (the measured minimum is ROE ~1.0).
VALLEY_QUARTER = 0.25
VALLEY_MIN_Y = BAND_BOTTOM - VALLEY_QUARTER * BAND_H  # 316.5

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

# Auto-detection of the curve ink inside the band. The grid is painted by the
# firmware with this exact neutral grey (draw_cb(): lv_color_hex(0xAAAAAA)); the
# background is the band's most frequent colour; text and curve share the theme's
# `text` role. Everything else with enough pixels is a candidate, and the most
# frequent candidate is the ink. This is independent of how bright the theme is:
# Sunlit's text is dark (RGB(22,36,54)), Redshift's is RGB(255,179,160).
GRID_INK = (170, 170, 170)
MIN_INK_PIXELS = 20
MIN_INK_CHROMA = 0  # keeps neutral inks (Sprint RGB(245,247,249)) eligible
MIN_BG_DISTANCE = 120  # RGB distance from the background: ink, not a panel blend

# A component of the ink mask must span at least this fraction of the chart
# width to be the curve: axis labels are short glyph runs. A dome cut by the
# readout panel can split the curve in two long pieces; both are kept. A long
# label that reaches the width rule still has to be tall enough to be the curve.
STROKE_MIN_FRACTION = 0.20
STROKE_TALL_MIN_FRACTION = 0.15
STROKE_TALL_MIN_HEIGHT = 0.15 * BAND_H
DOME_EDGE_TOL = 2  # the two chart edges are "at the floor" for a dome
DOME_INTERIOR_RISE = 3  # and the interior has to rise above them


def _chroma(colour: tuple[int, int, int]) -> int:
    return max(colour) - min(colour)


def _distance2(a: tuple[int, int, int], b: tuple[int, int, int]) -> int:
    return sum((x - y) ** 2 for x, y in zip(a, b, strict=True))


def band_colors(im: Image.Image) -> Counter[tuple[int, int, int]]:
    px = im.load()
    assert px is not None
    width, _ = im.size
    counts: Counter[tuple[int, int, int]] = Counter()

    for y in range(BAND_TOP, BAND_BOTTOM):
        for x in range(width):
            counts[cast(tuple[int, int, int], px[x, y])] += 1

    return counts


def detect_ink(im: Image.Image, ink_min: int | None = None) -> tuple[int, int, int] | None:
    """The theme's text/curve colour, measured inside the chart band.

    The curve is painted with the theme's `text` role (draw_cb()), the same
    colour as the axis labels. The grid is the firmware's neutral 0xAAAAAA and
    the background/panel is the most frequent colour; both are excluded. Of the
    remaining inks the curve+labels colour is by far the most frequent.
    """
    counts = band_colors(im)

    if not counts:
        return None

    background = counts.most_common(1)[0][0]
    limit = MIN_BG_DISTANCE * MIN_BG_DISTANCE
    candidates: list[tuple[int, tuple[int, int, int]]] = []

    for colour, count in counts.items():
        if count < MIN_INK_PIXELS:
            continue
        if colour in (background, GRID_INK):
            continue
        if _chroma(colour) < MIN_INK_CHROMA:
            continue
        if _distance2(colour, background) < limit:
            continue
        if ink_min is not None and min(colour) < ink_min:
            continue
        candidates.append((count, colour))

    if not candidates:
        return None

    return max(candidates)[1]


def curve_mask(im: Image.Image, ink: tuple[int, int, int]) -> set[tuple[int, int]]:
    """Pixels painted with the detected ink inside the chart band."""
    px = im.load()
    assert px is not None
    width, _ = im.size

    return {
        (x, y)
        for y in range(BAND_TOP, BAND_BOTTOM)
        for x in range(width)
        if cast(tuple[int, int, int], px[x, y]) == ink
    }


def _components(mask: set[tuple[int, int]]) -> list[set[tuple[int, int]]]:
    """Connected components of the dilated mask, in real ink pixels."""
    dilated: set[tuple[int, int]] = set()

    for x, y in mask:
        for dx in range(-DILATE, DILATE + 1):
            for dy in range(-DILATE, DILATE + 1):
                dilated.add((x + dx, y + dy))

    seen: set[tuple[int, int]] = set()
    components: list[set[tuple[int, int]]] = []

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

        real = {pt for pt in comp if pt in mask}

        if real:
            components.append(real)

    return components


def _extent(component: set[tuple[int, int]]) -> int:
    xs = [x for x, _ in component]
    return max(xs) - min(xs)


def _vertical_extent(component: set[tuple[int, int]]) -> int:
    ys = [y for _, y in component]
    return max(ys) - min(ys)


def curve_stroke(mask: set[tuple[int, int]], width: int) -> set[tuple[int, int]]:
    """The curve: every long (and tall enough) stroke in the ink mask.

    Axis labels are short glyph runs and stay out. Keeping every long stroke
    (instead of only the widest one) lets a dome whose middle is covered by the
    readout panel be measured from both of its halves.
    """
    components = _components(mask)

    if not components:
        return set()

    components.sort(key=lambda c: (-_extent(c), -len(c), min(x for x, _ in c)))
    wide = STROKE_MIN_FRACTION * width
    tall_wide = STROKE_TALL_MIN_FRACTION * width
    longest = [
        c
        for c in components
        if _extent(c) >= wide
        or (_extent(c) >= tall_wide and _vertical_extent(c) >= STROKE_TALL_MIN_HEIGHT)
    ]

    # Fallback for a chart narrower than the rule: keep the widest stroke.
    if not longest:
        longest = components[:1]

    return set().union(*longest)


def stroke_bottom(mask: set[tuple[int, int]]) -> dict[int, int]:
    """For every column, the lowest y of the stroke: the curve trace."""
    bottom: dict[int, int] = {}

    for x, y in mask:
        if x not in bottom or y > bottom[x]:
            bottom[x] = y

    return bottom


def classify(bottom: dict[int, int], xl: int, xr: int, valley_y: int, xv: int) -> str:
    """Measure the shape from the trace: `U`, `domo` or `rama`."""
    left_y = bottom[xl]
    right_y = bottom[xr]
    interior = xl + EDGE_MARGIN <= xv <= xr - EDGE_MARGIN

    if (
        interior
        and valley_y - left_y >= SHALLOW_EDGE_MIN_RISE
        and valley_y - right_y >= SHALLOW_EDGE_MIN_RISE
    ):
        return "U"

    interior_bottom = [bottom[x] for x in bottom if xl + EDGE_MARGIN <= x <= xr - EDGE_MARGIN]
    interior_high = min(interior_bottom) if interior_bottom else valley_y

    if (
        valley_y - left_y <= DOME_EDGE_TOL
        and valley_y - right_y <= DOME_EDGE_TOL
        and valley_y - interior_high >= DOME_INTERIOR_RISE
    ):
        return "domo"

    return "rama"


class Checker:
    def __init__(self) -> None:
        self.failures = 0

    def check(self, ok: bool, message: str) -> None:
        print(f"  {'ok  ' if ok else 'FAIL'} {message}")

        if not ok:
            self.failures += 1

    def note(self, text: str) -> None:
        print(f"       {text}")


def _note_trace(
    checker: Checker,
    width: int,
    ink: tuple[int, int, int],
    bottom: dict[int, int],
) -> None:
    """Imprime la traza medida (tinta, columna del valle, bordes) y la forma.

    Es la evidencia que el operador lee de una captura real: los numeros se
    imprimen SIEMPRE, pase o no la comprobacion de forma.
    """
    cols = sorted(bottom)
    xl, xr = cols[0], cols[-1]
    valley_y = max(bottom.values())
    x_valley = min(x for x in cols if bottom[x] == valley_y)

    checker.note(f"tinta: RGB({ink[0]},{ink[1]},{ink[2]}) (detectada en la banda "
                 f"y={BAND_TOP}..{BAND_BOTTOM})")
    checker.note(
        f"curva x={xl}..{xr} ({xr - xl} px de {width}); banda y={BAND_TOP}..{BAND_BOTTOM}; "
        f"cuarto inferior y>={VALLEY_MIN_Y:.0f}"
    )
    checker.note(
        f"valle: y={valley_y} en x={x_valley}; bordes: izq y={bottom[xl]} (x={xl}, "
        f"sube {valley_y - bottom[xl]} px), der y={bottom[xr]} (x={xr}, "
        f"sube {valley_y - bottom[xr]} px)"
    )
    checker.note(f"forma: {classify(bottom, xl, xr, valley_y, x_valley)}")


def check_valley(
    checker: Checker,
    name: str,
    png: Path,
    min_rise: float,
    ink_forced: tuple[int, int, int] | None = None,
    ink_min: int | None = None,
) -> None:
    checker.note(f"-- {name} ({png.name}) --")
    im = Image.open(png).convert("RGB")

    ink = ink_forced if ink_forced is not None else detect_ink(im, ink_min)

    if ink is None:
        checker.check(False, f"{name}: no encontre tinta de curva en la banda")
        return

    mask = curve_mask(im, ink)
    curve = curve_stroke(mask, im.size[0])

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

    _note_trace(checker, width, ink, bottom)

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


def measure_shape(
    png: Path,
    ink_forced: tuple[int, int, int] | None = None,
    ink_min: int | None = None,
) -> dict[str, object] | None:
    """Mide la traza dibujada: devuelve forma y geometria, o None si no hay curva."""
    im = Image.open(png).convert("RGB")
    ink = ink_forced if ink_forced is not None else detect_ink(im, ink_min)

    if ink is None:
        return None

    mask = curve_mask(im, ink)

    if not mask:
        return None

    curve = curve_stroke(mask, im.size[0])

    if not curve:
        return None

    bottom = stroke_bottom(curve)
    cols = sorted(bottom)
    xl, xr = cols[0], cols[-1]
    width, _ = im.size
    low_y = max(bottom.values())
    x_low = min(x for x in cols if bottom[x] == low_y)

    return {
        "ink": ink,
        "bottom": bottom,
        "xl": xl,
        "xr": xr,
        "width": width,
        "span": xr - xl,
        "low_y": low_y,
        "x_low": x_low,
        "shape": classify(bottom, xl, xr, low_y, x_low),
    }


def check_dome(
    checker: Checker,
    name: str,
    png: Path,
    ink_forced: tuple[int, int, int] | None = None,
    ink_min: int | None = None,
) -> None:
    """La curva dibuja el domo MEDIDO, sin suavizarlo a valle."""
    checker.note(f"-- {name} ({png.name}) --")
    data = measure_shape(png, ink_forced, ink_min)

    if data is None:
        checker.check(False, f"{name}: no encontre la curva dibujada")
        return

    bottom = data["bottom"]
    xl, xr = data["xl"], data["xr"]
    low_y = data["low_y"]
    x_low = data["x_low"]
    shape = data["shape"]
    span = data["span"]
    width = data["width"]

    checker.note(f"traza x={xl}..{xr} ({span} px de {width}); forma: {shape}")
    checker.check(span >= 0.7 * width, f"{name}: la curva cruza el chart ({span} px)")
    checker.check(shape == "domo", f"{name}: la forma dibujada es el domo medido ({shape})")

    interior = [bottom[x] for x in sorted(bottom) if xl + EDGE_MARGIN <= x <= xr - EDGE_MARGIN]
    interior_high = min(interior) if interior else low_y

    checker.note(
        f"bordes y={bottom[xl]}/{bottom[xr]}, interior y={interior_high}, "
        f"punto mas bajo en x={x_low} y={low_y}"
    )
    checker.check(
        interior_high < low_y - DOME_INTERIOR_RISE,
        f"{name}: el centro medido sube {low_y - interior_high} px sobre los bordes",
    )


def check_measured_not_dome(
    checker: Checker,
    name: str,
    png: Path,
    ink_forced: tuple[int, int, int] | None = None,
    ink_min: int | None = None,
) -> None:
    """La curva dibuja el dato medido del fixture (no lo invierte/suaviza a domo)."""
    checker.note(f"-- {name} ({png.name}) --")
    data = measure_shape(png, ink_forced, ink_min)

    if data is None:
        checker.check(False, f"{name}: no encontre la curva dibujada")
        return

    span = data["span"]
    width = data["width"]
    shape = data["shape"]

    checker.note(f"traza x={data['xl']}..{data['xr']} ({span} px de {width}); forma: {shape}")
    checker.check(span >= 0.7 * width, f"{name}: la curva cruza el chart ({span} px)")
    checker.check(shape != "domo", f"{name}: se dibuja el dato medido (no un domo invertido) ({shape})")


def check_capture_shape(
    checker: Checker,
    name: str,
    png: Path,
    ink_forced: tuple[int, int, int] | None = None,
    ink_min: int | None = None,
) -> None:
    """Una captura suelta: la curva existe y cruza el chart; se reporta su forma.

    Con la curva = puntos medidos la forma la manda el dato (U, domo o rama), asi
    que este modo REPORTA (tinta, valle, bordes y forma) y no impone una estetica:
    lo que falla es no medir nada (sin curva, o una traza que no cruza el chart).
    Es el comando del re-test en la radio (#125/#132).
    """
    checker.note(f"-- {name} ({png.name}) --")
    data = measure_shape(png, ink_forced, ink_min)

    if data is None:
        checker.check(False, f"{name}: no encontre la curva dibujada")
        return

    span = data["span"]
    width = data["width"]

    _note_trace(
        checker,
        cast(int, width),
        cast(tuple[int, int, int], data["ink"]),
        cast(dict[int, int], data["bottom"]),
    )
    checker.check(span >= 0.7 * width, f"{name}: la curva cruza el chart ({span} px)")


def parse_colour(text: str) -> tuple[int, int, int]:
    value = text.lstrip("#")

    if len(value) != 6:
        raise ValueError(f"color invalido: {text!r} (se espera RRGGBB)")

    return tuple(int(value[i : i + 2], 16) for i in (0, 2, 4))  # type: ignore[return-value]


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("captures", nargs="*", metavar="PNG")
    parser.add_argument("--ink", metavar="RRGGBB", help="forzar el color de la curva")
    parser.add_argument(
        "--ink-min",
        type=int,
        metavar="N",
        help="piso de brillo para la tinta automatica: min(r,g,b) >= N",
    )
    args = parser.parse_args(argv[1:])

    if len(args.captures) not in (1, 2):
        print(__doc__)
        return 2

    if args.ink_min is not None and not 0 <= args.ink_min <= 255:
        print(f"ERROR: --ink-min fuera de rango: {args.ink_min}", file=sys.stderr)
        return 2

    ink_forced = None

    if args.ink is not None:
        try:
            ink_forced = parse_colour(args.ink)
        except ValueError as exc:
            print(f"ERROR: {exc}", file=sys.stderr)
            return 2

    pngs = [Path(p) for p in args.captures]

    for png in pngs:
        if not png.is_file():
            print(f"ERROR: no existe {png}", file=sys.stderr)
            return 2

    checker = Checker()

    try:
        if len(pngs) == 1:
            # Captura suelta: se reporta la forma (la manda el dato medido).
            check_capture_shape(checker, "captura", pngs[0], ink_forced, args.ink_min)
        else:
            # La curva ES el dato: el domo del owner se dibuja domo y el valle real
            # se dibuja valle. No se impone la estetica de #113/#125.
            check_dome(checker, "domo del owner -> domo (cuadratica libre)", pngs[0], ink_forced, args.ink_min)
            check_valley(
                checker,
                "valle real del owner -> valle",
                pngs[1],
                SHALLOW_EDGE_MIN_RISE,
                ink_forced,
                args.ink_min,
            )
    except (OSError, UnidentifiedImageError) as exc:
        print(f"ERROR: no pude leer {exc.filename or 'la captura'}: {exc}", file=sys.stderr)
        return 2

    if checker.failures:
        print(f"FAIL: {checker.failures} comprobacion(es) de forma")
        return 1

    print("PASS: las comprobaciones de forma de la curva dibujada pasan")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
