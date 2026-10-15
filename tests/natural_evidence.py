#!/usr/bin/env python3
"""Checks of the natural bench (`tests/check_harness_natural.sh`).

Reads the JSON the harness wrote for `fixtures/main-natural.scn` in the four
modes of the gate and verifies, on the produced numbers:

* the profile of UI-07 increment 1 (docs/AB_TESTING.md, section 2): the spectrum
  invalidates 132000 px per FFT frame, the waterfall 196000 px per new row and
  the clock 15840 px every 500 ms, and *every* rectangle the run reports is one
  of the regions a firmware timer invalidates (the bench invalidates nothing on
  its own);
* the radio's DSP cadence mirrored by the drive (src/dsp.cpp: 1000/15 and
  1000/25, checked once per virtual-time slice);
* the delimitation control (`--natural-static`): with the data drive off the
  spectrum/waterfall work disappears, so the profiled areas can only come from
  the deterministic per-frame drive;
* the compatibility of the existing modes: `--bench` still fabricates its own
  full-screen frame (1 flush, 384000 px) and the plain capture's `metrics` block
  keeps its value, so the A/B pipeline sees the same format and the same numbers
  as before this mode existed.

Usage: natural_evidence.py <out-dir>     (prints the evidence, exits 1 on a miss)

SPDX-License-Identifier: LGPL-2.1-or-later
"""

from __future__ import annotations

import json
import sys
from collections import Counter
from pathlib import Path

# rectangle (x, y, w, h) -> (px, what invalidates it). Measured on
# main-natural; the first three are the profile of UI-07 increment 1, the last
# two are the firmware's 2 s timers, which only fire because the natural run
# reaches t > 3 s (the plain capture advances 1.2 s and never sees them).
PROFILE: dict[tuple[int, int, int, int], tuple[int, str]] = {
    (0, 0, 800, 165): (132000, "spectrum, one FFT frame (src/spectrum.c)"),
    (0, 235, 800, 245): (196000, "waterfall, one new row (src/waterfall.c)"),
    (580, 0, 220, 72): (15840, "clock, every 500 ms (src/clock.c)"),
    (0, 241, 800, 34): (27200, "band info, fading after the 2 s timer (src/band_info.c)"),
    (152, 23, 30, 36): (1080, "BT state, the 2 s timer (src/info.c)"),
}

# The fixture declares `natural_frames 40`; one frame is one LVGL display
# refresh period. The drive is checked once per slice.
FRAMES = 40
FRAME_MS = 30
SLICE_MS = 10
SPECTRUM_FPS_MS = 1000 // 15
WATERFALL_FPS_MS = 1000 // 25

# The settled capture of main-natural: 6 flushes and 1127680 px (the same
# numbers the 18 fixtures of the A/B suite report); the natural run adds its own
# flushes on top, it never replaces them.
SETTLE_FLUSHES = 6
SETTLE_PX = 1127680
FULL_SCREEN_PX = 800 * 480

# How many times each rectangle appears in the 40 frames (deterministic: seed 13,
# fixed frame count). Pinned so a change of the drive, the cadence or the fixture
# has to be justified instead of silently moving the profile.
RECT_COUNTS = Counter(
    {
        (0, 235, 800, 245): 23,  # waterfall: 24 rows, one lands with the faded band info
        (0, 0, 800, 165): 16,  # spectrum: 17 FFT frames, one shares the cycle with the clock
        (0, 241, 800, 34): 4,  # band info fading (part of it lands inside the waterfall)
        (580, 0, 220, 72): 1,  # clock: the other tick lands inside the spectrum
        (152, 23, 30, 36): 1,  # BT icon (its timer fires once in the window)
    }
)


class Checker:
    """Collects the evidence and fails on the first miss."""

    def __init__(self) -> None:
        self.lineas: list[str] = []

    def check(self, condition: bool, message: str) -> None:
        if not condition:
            print(f"FAIL: {message}", file=sys.stderr)
            raise SystemExit(1)

    def note(self, text: str) -> None:
        self.lineas.append(text)


def load(out_dir: Path, name: str) -> dict:
    path = out_dir / f"{name}.json"
    if not path.is_file():
        print(f"FAIL: missing {path}", file=sys.stderr)
        raise SystemExit(1)
    return json.loads(path.read_text())


def expected_drives(total_ms: int, fps_ms: int) -> int:
    """Reproduces the drive cadence of the radio's DSP loop (src/dsp.cpp).

    The harness mirrors `if (now - last > fps_ms) { feed; last = now; }` and
    checks it once per slice, so the effective period is the first slice
    boundary past the interval. Reimplemented here from the DSP constants: the
    produced counts are what ties the drive to the radio's cadence.
    """
    now = last = 0
    feeds = 0
    while now < total_ms:
        now += SLICE_MS
        if now - last > fps_ms:
            feeds += 1
            last = now
    return feeds


def rects_of(frame: dict) -> list[tuple[int, int, int, int]]:
    return [(int(rect[0]), int(rect[1]), int(rect[2]), int(rect[3])) for rect in frame["rects"]]


def check_natural(checker: Checker, doc: dict) -> None:
    natural = doc["natural"]
    total_ms = FRAMES * FRAME_MS

    checker.check(natural["frames"] == FRAMES, f"frames = {natural['frames']}, expected {FRAMES}")
    checker.check(natural["frame_ms"] == FRAME_MS, f"frame_ms = {natural['frame_ms']}")
    checker.check(natural["slice_ms"] == SLICE_MS, f"slice_ms = {natural['slice_ms']}")
    checker.check(natural["data_drive"] is True, "the natural run did not drive the data")
    checker.check(
        natural["spectrum_fps_ms"] == SPECTRUM_FPS_MS
        and natural["waterfall_fps_ms"] == WATERFALL_FPS_MS,
        "the drive cadence does not mirror src/dsp.cpp (1000/15, 1000/25)",
    )
    checker.check(
        natural["spectrum_frames"] == expected_drives(total_ms, SPECTRUM_FPS_MS),
        f"spectrum_frames = {natural['spectrum_frames']}, expected "
        f"{expected_drives(total_ms, SPECTRUM_FPS_MS)} at 1000/15 ms over {total_ms} ms",
    )
    checker.check(
        natural["waterfall_rows"] == expected_drives(total_ms, WATERFALL_FPS_MS),
        f"waterfall_rows = {natural['waterfall_rows']}, expected "
        f"{expected_drives(total_ms, WATERFALL_FPS_MS)} at 1000/25 ms over {total_ms} ms",
    )
    # El instrumento del redibujo offscreen (src/waterfall.c): con el perfil
    # QUALITY del fixture la cascada dibuja siempre, asi que la cuenta es una por
    # fila. Es la vara de la medicion de `--display-off` (abajo): mismo fixture,
    # panel oscuro.
    checker.check(
        natural["waterfall_offscreen_redraws"] == expected_drives(total_ms, WATERFALL_FPS_MS),
        f"waterfall_offscreen_redraws = {natural['waterfall_offscreen_redraws']}, expected "
        f"{expected_drives(total_ms, WATERFALL_FPS_MS)} (one per new row at QUALITY)",
    )

    frames = natural["per_frame"]
    checker.check(len(frames) == FRAMES, f"per_frame has {len(frames)} entries")
    base_ms = frames[0]["virtual_end_ms"] - FRAME_MS
    counts: Counter[tuple[int, int, int, int]] = Counter()
    totals = {
        "invalidated_px": 0,
        "invalidated_rects": 0,
        "joined_px": 0,
        "flushed_px": 0,
        "flushes": 0,
        "refresh_cycles": 0,
        "frames_with_flush": 0,
    }

    for index, frame in enumerate(frames):
        where = f"frame {index}"
        checker.check(frame["frame"] == index, f"{where}: frame index")
        checker.check(
            frame["virtual_end_ms"] == base_ms + (index + 1) * FRAME_MS,
            f"{where}: virtual_end_ms = {frame['virtual_end_ms']} (the clock did not advance "
            "one frame of virtual time)",
        )
        checker.check(
            frame["flushes"] == frame["invalidated_rects"],
            f"{where}: {frame['flushes']} flushes for {frame['invalidated_rects']} "
            "invalidated rectangles (the full-screen draw buffer is 1:1)",
        )
        checker.check(
            frame["invalidated_px"] == frame["flushed_px"],
            f"{where}: the invalidated area ({frame['invalidated_px']}) is not the flushed one "
            f"({frame['flushed_px']})",
        )
        checker.check(
            frame["invalidated_px"] == sum(w * h for _, _, w, h in rects_of(frame)),
            f"{where}: invalidated_px does not add up to its rectangles",
        )
        checker.check(frame["rects_complete"] is True, f"{where}: the rectangle list was truncated")
        for rect in rects_of(frame):
            checker.check(
                rect in PROFILE,
                f"{where}: unprofiled invalidated rectangle {rect} ({rect[2] * rect[3]} px)",
            )
            counts[rect] += 1
        for key in (
            "invalidated_px",
            "invalidated_rects",
            "joined_px",
            "flushed_px",
            "flushes",
            "refresh_cycles",
        ):
            totals[key] += frame[key]
        totals["frames_with_flush"] += 1 if frame["flushes"] else 0

    checker.check(
        counts == RECT_COUNTS, f"rectangle histogram moved: {dict(counts)} != {dict(RECT_COUNTS)}"
    )
    for key, value in totals.items():
        checker.check(
            natural["totals"][key] == value,
            f"totals.{key} = {natural['totals'][key]}, summed {value}",
        )

    metrics = doc["metrics"]
    checker.check(
        metrics["flushes"] == SETTLE_FLUSHES + natural["totals"]["flushes"],
        f"metrics.flushes = {metrics['flushes']}: the settled capture ({SETTLE_FLUSHES}) plus the "
        f"natural frames ({natural['totals']['flushes']})",
    )
    checker.check(
        metrics["flushed_pixels"] == SETTLE_PX + natural["totals"]["flushed_px"],
        f"metrics.flushed_pixels = {metrics['flushed_pixels']} (the settled capture must keep its "
        f"{SETTLE_PX} px and the natural frames add their own)",
    )
    checker.note(
        f"  profile: spectrum {PROFILE[(0, 0, 800, 165)][0]} px x {counts[(0, 0, 800, 165)]} frames, "
        f"waterfall {PROFILE[(0, 235, 800, 245)][0]} px x {counts[(0, 235, 800, 245)]} rows, "
        f"clock {PROFILE[(580, 0, 220, 72)][0]} px x {counts[(580, 0, 220, 72)]} tick(s)"
    )
    checker.note(
        f"  natural totals: invalidated {natural['totals']['invalidated_px']} px in "
        f"{natural['totals']['invalidated_rects']} rectangles / {natural['totals']['flushes']} flushes "
        f"over {natural['totals']['frames_with_flush']}/{FRAMES} frames"
    )


def check_static(checker: Checker, static: dict, natural_doc: dict) -> None:
    """The delimitation control: without the drive there is no spectrum/waterfall work."""
    natural = natural_doc["natural"]
    control = static["natural"]
    checker.check(control["data_drive"] is False, "the control run must not drive the data")
    checker.check(control["spectrum_frames"] == 0, "the control run fed the spectrum")
    checker.check(control["waterfall_rows"] == 0, "the control run fed the waterfall")
    rects = {rect for frame in control["per_frame"] for rect in rects_of(frame)}
    profiled_data = {(0, 0, 800, 165), (0, 235, 800, 245)}
    checker.check(
        not (rects & profiled_data),
        f"the control run invalidated a data region anyway: {rects & profiled_data}",
    )
    checker.check(
        control["totals"]["invalidated_px"] * 10 < natural["totals"]["invalidated_px"],
        f"the drive only explains {100 * control['totals']['invalidated_px'] / natural['totals']['invalidated_px']:.0f}% "
        "of the natural area: the control does not delimit the measurement",
    )
    checker.check(
        static["metrics"]["framebuffer_fnv1a"] != natural_doc["metrics"]["framebuffer_fnv1a"],
        "the natural run and the control rendered the same framebuffer: the driven frames never "
        "reached the screen",
    )
    checker.note(
        f"  delimitation: drive off -> {control['totals']['invalidated_px']} px "
        f"({100 * control['totals']['invalidated_px'] / natural['totals']['invalidated_px']:.1f}% of the "
        f"{natural['totals']['invalidated_px']} px with the drive), only clock/BT/band-info rectangles"
    )


def check_forced(checker: Checker, forced: dict, natural_doc: dict) -> None:
    """`--bench` is untouched: it still fabricates one full-screen frame per pass."""
    natural = natural_doc["natural"]
    benchmark = forced["benchmark"]
    checker.check(benchmark["enabled"] is True, "the forced benchmark block is missing")
    checker.check(len(benchmark["frames"]) == 3, f"{len(benchmark['frames'])} forced frames")
    for index, frame in enumerate(benchmark["frames"]):
        checker.check(
            frame["flushes"] == 1
            and frame["redrawn_area_px"] == FULL_SCREEN_PX
            and frame["unique_redrawn_px"] == FULL_SCREEN_PX,
            f"forced frame {index}: {frame['flushes']} flush(es) and {frame['redrawn_area_px']} px "
            f"(the forced mode must still repaint the whole {FULL_SCREEN_PX} px screen)",
        )
    checker.check("natural" not in forced, "the forced mode must not emit a natural block")
    checker.check(
        forced["metrics"]["framebuffer_fnv1a"] != natural_doc["metrics"]["framebuffer_fnv1a"],
        "the forced and the natural modes rendered the same bytes: the natural mode changed the PNG",
    )
    checker.note(
        f"  forced mode: 3 frames x 1 flush x {FULL_SCREEN_PX} px "
        f"(natural: {natural['totals']['invalidated_px']} px in {natural['frames']} frames)"
    )


def check_cli(checker: Checker, cli: dict) -> None:
    """`--bench-natural 10` overrides the fixture's `natural_frames`."""
    checker.check(
        cli["natural"]["frames"] == 10, f"--bench-natural 10 ran {cli['natural']['frames']} frames"
    )
    checker.note("  CLI override: --bench-natural 10 ran 10 frames (the fixture says 40)")


def check_display_off(checker: Checker, off: dict, natural_doc: dict, lit_doc: dict) -> None:
    """La ruta de pantalla apagada de la ROM no hace trabajo de render.

    `--display-off` manda la tecla POWER por el camino del firmware
    (src/main_screen.c -> src/backlight.c -> lv_disp_enable_invalidation(false))
    antes de la corrida natural, asi que los 40 frames con el drive andando
    tienen que invalidar y flushear CERO: el espectro, la cascada y el reloj
    siguen disparando, y ninguna de sus invalidaciones llega al driver. La vara
    es el mismo fixture encendido (6745720 px, `natural`): la diferencia es lo
    que el panel oscuro se ahorra.
    """
    natural = off["natural"]
    totals = natural["totals"]

    checker.check(natural["data_drive"] is True, "the display-off run did not drive the data")
    checker.check(
        natural["spectrum_frames"] > 0 and natural["waterfall_rows"] > 0,
        "the display-off run has no data to measure: the drive did not run",
    )
    # El corte del redibujo offscreen (issue #13): con el panel oscuro la cascada
    # saltea el computo de la paleta (~196000 px por fila) en vez de hacerlo para
    # nada. El instrumento de src/waterfall.c tiene que dar 0 con las filas
    # llegando igual, contra una por fila con la pantalla encendida (arriba).
    checker.check(
        natural["waterfall_offscreen_redraws"] == 0,
        f"display off: the waterfall ran {natural['waterfall_offscreen_redraws']} offscreen "
        "redraw(s) with the panel dark (the palette computation must be skipped)",
    )
    for key, value in totals.items():
        checker.check(
            value == 0,
            f"display off: totals.{key} = {value} (the dark panel must render nothing)",
        )
    for index, frame in enumerate(natural["per_frame"]):
        checker.check(
            frame["invalidated_px"] == 0 and frame["invalidated_rects"] == 0,
            f"display off, frame {index}: {frame['invalidated_px']} px in "
            f"{frame['invalidated_rects']} rectangles",
        )
        checker.check(
            frame["flushes"] == 0 and frame["flushed_px"] == 0,
            f"display off, frame {index}: {frame['flushes']} flush(es), {frame['flushed_px']} px",
        )

    # La captura: el repaint de pantalla completa que el arnes hace antes de la
    # corrida es justo lo que no ocurre. Los 743680 px de la captura son los que
    # se pintaron ANTES de apretar la tecla (el panel conserva el ultimo frame).
    metrics = off["metrics"]
    checker.check(
        metrics["flushes"] == SETTLE_FLUSHES - 1,
        f"display off: metrics.flushes = {metrics['flushes']}, expected {SETTLE_FLUSHES - 1} "
        "(all but the capture's full-screen repaint)",
    )
    checker.check(
        metrics["flushed_pixels"] == SETTLE_PX - FULL_SCREEN_PX,
        f"display off: metrics.flushed_pixels = {metrics['flushed_pixels']}, expected "
        f"{SETTLE_PX - FULL_SCREEN_PX} ({SETTLE_PX} minus the {FULL_SCREEN_PX} px repaint)",
    )
    checker.check(
        metrics["framebuffer_fnv1a"] == lit_doc["metrics"]["framebuffer_fnv1a"],
        "display off: the dark panel rendered different pixels than the lit capture: the "
        "blanked route changed the image instead of skipping work",
    )
    checker.note(
        f"  display off: 0 px in {natural['frames']} frames (against "
        f"{natural_doc['natural']['totals']['invalidated_px']} px lit) and a capture "
        f"{FULL_SCREEN_PX} px lighter"
    )


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print("usage: natural_evidence.py <out-dir>", file=sys.stderr)
        return 2
    out_dir = Path(argv[1])
    checker = Checker()

    # The two runs of the gate (natural.json / run1.json) must be byte-identical;
    # the shell checks the bytes, here the numbers of the profiled run are read.
    natural_doc = load(out_dir, "natural")
    check_natural(checker, natural_doc)
    check_static(checker, load(out_dir, "static"), natural_doc)
    check_forced(checker, load(out_dir, "forced"), natural_doc)
    check_cli(checker, load(out_dir, "cli"))
    check_display_off(
        checker, load(out_dir, "off"), natural_doc, load(out_dir, "forced")
    )

    print("\n".join(checker.lineas))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
