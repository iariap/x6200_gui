#!/usr/bin/env python3
"""Checks of the natural bench (`tests/check_harness_natural.sh`).

Reads the JSON the harness wrote for `fixtures/main-natural.scn` in the four
modes of the gate and verifies, on the produced numbers:

* the profile of UI-07 increment 1 (docs/AB_TESTING.md, section 2): the spectrum
  invalidates 132000 px per FFT frame, the waterfall 231200 px per new row and
  the clock 10486 px every 500 ms (issue #250/#285: the permanent clock's time
  line, 214x49 measured -- see PROFILE), and *every* rectangle the run reports is
  one of the regions a firmware timer invalidates (the bench invalidates nothing
  on its own). The rectangles are not typed in: the boxes of the components are
  declared in COMPONENTS (measured on the live LVGL tree with `--inspect`) and the
  rectangle LVGL invalidates is *derived* from them (the object's box expanded by
  its ext_draw_size and clipped to the panel), so a geometry that moves without
  being declared stops matching PROFILE instead of being covered by a declared
  rectangle. #306: the waterfall's box moved to 0,196 800x284 with #298 (the spot
  strip stopped reserving 44 px of height), so it invalidates 231200 px instead
  of 196000 px -- +12.2% of the steady-state area, accepted and documented in
  docs/AB_TESTING.md. OPT-4 #150: the clock's equality guard (src/clock.c)
  suppresses the ticks whose text did not change, so the clock rectangle no
  longer shows up in the harness (time() is pinned) and the gate checks it is 0;
  the guard leaves the framebuffer byte-identical;
* that one natural frame is one refresh period of the ROM (`frame_ms` must be
  the `LV_DISP_DEF_REFR_PERIOD` of the ROM's lv_conf.h, read here from the same
  file the harness' CMake reads at build time: #56's cross-check, applied to
  this mode too);
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
import re
import sys
from collections import Counter
from pathlib import Path

# --- Las cajas de los componentes -------------------------------------------
#
# El banco mide AREAS INVALIDAS, no cajas, y LVGL invalida la caja del objeto
# expandida por su `ext_draw_size` (`lv_obj_invalidate()`, lvgl/src/core/lv_obj_pos.c)
# y recortada al panel (`_lv_inv_area()`, lvgl/src/core/lv_refr.c). Escribir el
# rectangulo invalidado a mano escondia el numero magico y dejaba el perfil
# pegado a la geometria: aca se declara la CAJA de cada componente, medida sobre
# el arbol LVGL vivo (`--inspect`):
#
#   ./ui_harness --scenario ../fixtures/main-natural.scn --png out/x.png \
#       --json out/x.json --inspect      # bloque `inspection`, coords absolutas
#
# y el rectangulo que el banco tiene que ver se DERIVA de ella con
# `invalidated()`. Una caja que se mueve sin declararla cambia el rect derivado,
# deja de estar en PROFILE y el gate falla: se aceptan exactamente los rects
# derivados de los componentes declarados, ni uno mas (no hay alfombra).
COMPONENTS: dict[str, tuple[int, int, int, int]] = {
    # (x, y, w, h) absolutos, medidos con `--inspect` sobre `main-natural`.
    # El espectro arranca en 0 y mide spectrum_height = 480/3 (src/main_screen.c).
    "spectrum": (0, 0, 800, 160),
    # #306/#298: el waterfall arranca debajo de la fila de frecuencia
    # (160 espectro + 36 fila) y llega al borde inferior del panel. #298 dejo de
    # reservar SPOT_MARKS_H (44 px) arriba del waterfall -- la franja de spots es
    # un overlay que se dibuja encima --, asi que la caja paso de 0,240 800x240 a
    # 0,196 800x284: 44 px mas de espectro visible y 44 px mas de area invalidada
    # por fila (196000 -> 231200 px, +18.0% por fila; el regimen +12.2% porque el
    # espectro y el reloj no cambian).
    "waterfall": (0, 196, 800, 284),
    # Hijo del waterfall (src/band_info.c: `y + 6`), 24 px de alto. Queda adentro
    # de la caja del waterfall, asi que en la corrida natural LVGL une las dos
    # invalidaciones en el rect del waterfall y el band-info solo se ve solo en el
    # control `--natural-static` (7 veces), que es donde se verifica.
    "band_info": (0, 202, 800, 24),
}

# LVGL expande la caja por el `ext_draw_size` del objeto: medido, 5 px por lado
# en los tres `lv_obj` de arriba (es el mismo numero para los tres, y es lo que
# explica los rects del perfil: 800x160 -> 800x165, 800x284 -> 800x289,
# 800x24 -> 800x34). El panel es 800x480 (`--inspect`, `viewport`).
EXT_DRAW = 5
PANEL_W = 800
PANEL_H = 480


def invalidated(box: tuple[int, int, int, int]) -> tuple[int, int, int, int]:
    """El rectangulo que LVGL invalida para la caja `box` (x, y, w, h)."""
    x, y, w, h = box
    x1 = max(0, x - EXT_DRAW)
    y1 = max(0, y - EXT_DRAW)
    x2 = min(PANEL_W - 1, x + w - 1 + EXT_DRAW)
    y2 = min(PANEL_H - 1, y + h - 1 + EXT_DRAW)
    return (x1, y1, x2 - x1 + 1, y2 - y1 + 1)


# rectangle (x, y, w, h) -> (px, what invalidates it). Derived from COMPONENTS;
# the first three are the profile of UI-07 increment 1, the last one is a 2 s
# timer, which only fires because the natural run reaches t > 3 s (the plain
# capture advances 1.2 s and never sees it). With the ROM's own refresh period
# (50 ms, the alignment of decision 8) the driven run lands a data rectangle in
# almost every frame, so the 2 s-timer rectangle usually arrives inside the
# spectrum/waterfall one and LVGL joins them: it shows up in the
# `--natural-static` control instead (7 times), which is where the delimitation
# check reads it.
PROFILE: dict[tuple[int, int, int, int], tuple[int, str]] = {
    invalidated(COMPONENTS["spectrum"]): (132000, "spectrum, one FFT frame (src/spectrum.c)"),
    invalidated(COMPONENTS["waterfall"]): (
        231200,
        "waterfall, one new row (src/waterfall.c; #298/#306: box 0,196 800x284)",
    ),
    # El reloj (issue #250; re-medido en #285): la linea del reloj es la etiqueta
    # 209x33 de la fila y=3..36 (591,3 medido con `--inspect`; con sony_44, issue
    # #285), y la invalidacion del label es su caja expandida por el
    # `ext_draw_size` de LVGL, recortada al display: 214x49 = 10486 px en
    # `main-natural` (Aurora, familia de texto) y 214x36 = 7704 px en
    # `main-frecuencia-dseg` (digits dseg7). Con `time()` fijo (el harness) la
    # hora nunca cambia y la guarda de igualdad del #150 apaga la invalidacion,
    # asi que el rectangulo NO aparece en el banco: los dos numeros se midieron
    # con el reloj de pared avanzando con el reloj virtual (un parche local del
    # `__wrap_time` del harness, no versionado). Antes de #285 eran 214x48 =
    # 10272 y 214x35 = 7490 (el label medía 2 px menos de alto): el reloj del
    # #285 invalida 214 px mas por tick (+2.1% / +2.9%). (El reloj de potencia de
    # la main previa al #250 media 15549 px por tick -- 581,0,219,71 en
    # `main-frecuencia-dseg` con `clock_view time`; ese rectangulo ya no existe.)
    # Unico rect que no sale de COMPONENTS: el reloj se invalida como label (su
    # caja depende de la linea de la fuente, no de una caja del layout) y con
    # `time()` fijo nunca dispara en el banco, asi que no hay de donde derivarlo
    # sin el parche del `__wrap_time`. Se deja medido y citado arriba.
    (586, 0, 214, 49): (10486, "clock, every 500 ms (src/clock.c, #250/#285)"),
    invalidated(COMPONENTS["band_info"]): (
        27200,
        "band info, fading after the 2 s timer (src/band_info.c; box 0,202 800x24)",
    ),
}

# The fixture declares `natural_frames 40`; one frame is one LVGL display
# refresh period. The period is NOT a constant of this file: the harness reads
# the ROM's LV_DISP_DEF_REFR_PERIOD (lv_conf.h) at build time
# (HARNESS_ROM_REFR_PERIOD, tools/ui_harness/CMakeLists.txt) and reports it as
# `frame_ms`; this checker re-reads the same lv_conf.h and requires the JSON to
# carry it, so the measurement cannot drift from the ROM's config (the same
# cross-check tests/wakeup_evidence.py does for the wakeup profile, #56).
FRAMES = 40
SLICE_MS = 10
SPECTRUM_FPS_MS = 1000 // 15
WATERFALL_FPS_MS = 1000 // 25

ROOT = Path(__file__).resolve().parent.parent


def rom_refresh_period_ms(root: Path) -> int:
    """`LV_DISP_DEF_REFR_PERIOD` de lv_conf.h (la config de la ROM)."""
    match = re.search(
        r"^#define\s+LV_DISP_DEF_REFR_PERIOD\s+(\d+)", (root / "lv_conf.h").read_text(), re.M
    )
    if match is None:
        print("FAIL: the ROM's refresh period is not in lv_conf.h", file=sys.stderr)
        raise SystemExit(1)
    return int(match.group(1))


# The settled capture of main-natural: 6 flushes and 1100896 px; the natural run
# adds its own flushes on top, it never replaces them. OPT-4 #150: the clock's
# equality guard (src/clock.c) removes the settled capture's needless clock
# ticks; the rendered PNG is byte-identical either way (the guard only skips an
# invalidation that changed no pixel).
#
# El par se re-midio con el reloj permanente del #250 (era 5 / 1111840): el
# panel pasa de una etiqueta de dos lineas (potencia) a un contenedor con la
# linea del reloj y la banda de estado, asi que las invalidaciones del arranque
# cambian de forma y de union con las del espectro (0,0,800,165, que contiene al
# panel): un rectangulo mas y 11488 px menos. El bloque `metrics` no expone los
# rectangulos del settle, asi que la decomposicion exacta no se puede citar; el
# numero esta medido sobre el binario de esta revision y el control
# `--display-off` lo verifica por diferencia (SETTLE_PX - FULL_SCREEN_PX).
SETTLE_FLUSHES = 6
# Re-medido en #285: 1100352 -> 1100896 (+544 px, +0.05%). El settle es la union
# de invalidaciones del arranque, y el reloj es parte de esa union: la etiqueta
# de la hora crecio 2 px de alto (sony_44) y la banda de estado (#285) es una
# superficie nueva del panel. El numero se re-midio sobre el binario de esta
# revision y el control `--display-off` lo verifica por diferencia
# (SETTLE_PX - FULL_SCREEN_PX = 716896, medido).
#
# Re-medido en #306: 1100896 -> 1136096 (+35200 px = 800 x 44, con los mismos 6
# flushes). #298 dejo de reservar los 44 px de la franja de spots arriba del
# waterfall, asi que el waterfall es 44 px mas alto y la invalidacion de arranque
# que lo une con el espectro mide 44 filas mas: 800 x 44 = 35200 px por captura.
# Es el mismo delta que el area por fila (196000 -> 231200) y NO es trabajo por
# frame: es el settle (una vez por captura). El control `--display-off` lo
# verifica por diferencia (1136096 - 384000 = 752096, medido).
SETTLE_PX = 1136096
FULL_SCREEN_PX = 800 * 480

# How many times each rectangle appears in the 40 frames (deterministic: seed 13,
# fixed frame count). Pinned so a change of the drive, the cadence, the frame
# period or the fixture has to be justified instead of silently moving the
# profile. At the ROM's 50 ms period the waterfall fires exactly once per frame
# and the spectrum every other frame, so almost every frame carries a data
# rectangle and the 2 s-timer rectangles are joined into them (they are counted
# in RECT_COUNTS of the static control, below in check_static).
#
# OPT-4 #150: the clock's equality guard drops its rectangle from this histogram
# (see check_natural). Before the guard the clock tick landed once here (the
# other ticks were joined into the spectrum); the guard suppresses every tick
# whose text did not change, and the harness pins time() (mock_ui_deps.c), so in
# the harness none of the 4 ticks of the run changes the text and the clock
# invalidates 0 times. On the radio the clock text changes once per second, so
# the guard halves its invalidations (2 ticks/s -> 1).
RECT_COUNTS = Counter(
    {
        invalidated(COMPONENTS["waterfall"]): 39,  # one row per frame
        invalidated(COMPONENTS["spectrum"]): 27,  # 28 FFT frames, one shares the clock cycle
    }
)

# El control `--natural-static` (drive apagado): solo el band-info de los 2 s.
# Los rects del control se verifican contra el componente declarado, no contra un
# numero suelto: si el band-info se mueve o cambia de tamano sin declararlo, el
# control lo delata (es la unica superficie que el control puede invalidar).
STATIC_RECT_COUNTS = Counter({invalidated(COMPONENTS["band_info"]): 7})


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


def _intersecta(rect: tuple[int, int, int, int], region: tuple[int, int, int, int]) -> bool:
    """¿El rectangulo (x,y,w,h) toca la region (x0,y0,x1,y1)?"""
    x, y, w, h = rect
    x0, y0, x1, y1 = region
    return not (x + w <= x0 or x >= x1 or y + h <= y0 or y >= y1)


def _contiene(outer: tuple[int, int, int, int], inner: tuple[int, int, int, int]) -> bool:
    """¿El rectangulo `inner` esta adentro de `outer` (los dos x,y,w,h)?"""
    x, y, w, h = outer
    ix, iy, iw, ih = inner
    return ix >= x and iy >= y and ix + iw <= x + w and iy + ih <= y + h


def check_natural(checker: Checker, doc: dict, rom_frame_ms: int) -> None:
    natural = doc["natural"]
    frame_ms = natural["frame_ms"]
    checker.check(
        frame_ms == rom_frame_ms,
        f"frame_ms = {frame_ms}, but the ROM configures {rom_frame_ms} ms (lv_conf.h, "
        "LV_DISP_DEF_REFR_PERIOD): a natural frame is not a ROM frame",
    )
    total_ms = FRAMES * frame_ms

    checker.check(natural["frames"] == FRAMES, f"frames = {natural['frames']}, expected {FRAMES}")
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
    # OPT-2 (#148): el anillo rasteriza UNA fila por refresco (800 px), no las
    # `height` filas del baseline (231200 px desde #306). El fixture natural no cambia el
    # centro ni el zoom, asi que no hay ninguna reconstruccion completa: es la
    # medicion de que el ahorro de software es real y no de invalidacion (el
    # area flusheada no cambia, ver los totals de arriba).
    rows = expected_drives(total_ms, WATERFALL_FPS_MS)
    checker.check(
        natural["waterfall_rasterized_px"] == rows * 800,
        f"waterfall_rasterized_px = {natural['waterfall_rasterized_px']}, expected "
        f"{rows * 800} (800 px per new row: the ring rasterizes one row, not height)",
    )
    checker.check(
        natural["waterfall_full_rebuilds"] == 0,
        f"waterfall_full_rebuilds = {natural['waterfall_full_rebuilds']}, expected 0 "
        "(no center/zoom/palette change in the natural fixture)",
    )

    frames = natural["per_frame"]
    checker.check(len(frames) == FRAMES, f"per_frame has {len(frames)} entries")
    base_ms = frames[0]["virtual_end_ms"] - frame_ms
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
            frame["virtual_end_ms"] == base_ms + (index + 1) * frame_ms,
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
    # OPT-4 #150: la guarda de igualdad del reloj (src/clock.c) apaga su
    # invalidacion cuando el texto no cambia. El arnes fija time()
    # (mocks/mock_ui_deps.c), asi que las 4 muestras de 500 ms de la corrida
    # dicen el mismo texto y el reloj tiene que invalidar 0 veces. Sin la guarda
    # el rectangulo del reloj (#250: 586,0,214,48 = 10272 px) aparece en el
    # histograma (medido, ver PROFILE).
    clock_rect = (586, 0, 214, 49)
    checker.check(
        counts[clock_rect] == 0,
        f"the clock invalidated {counts[clock_rect]} time(s) with an unchanged text: "
        "the OPT-4 #150 equality guard (src/clock.c) is not suppressing it",
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
        f"  profile: spectrum {PROFILE[invalidated(COMPONENTS['spectrum'])][0]} px x "
        f"{counts[invalidated(COMPONENTS['spectrum'])]} frames, "
        f"waterfall {PROFILE[invalidated(COMPONENTS['waterfall'])][0]} px x "
        f"{counts[invalidated(COMPONENTS['waterfall'])]} rows, "
        f"clock {PROFILE[clock_rect][0]} px x {counts[clock_rect]} tick(s)"
    )
    totals_note = natural["totals"]
    checker.note(
        f"  natural totals: invalidated {totals_note['invalidated_px']} px in "
        f"{totals_note['invalidated_rects']} rectangles / {totals_note['flushes']} flushes "
        f"over {totals_note['frames_with_flush']}/{FRAMES} frames"
    )


def check_static(checker: Checker, static: dict, natural_doc: dict) -> None:
    """The delimitation control: without the drive there is no spectrum/waterfall work."""
    natural = natural_doc["natural"]
    control = static["natural"]
    checker.check(control["data_drive"] is False, "the control run must not drive the data")
    checker.check(control["spectrum_frames"] == 0, "the control run fed the spectrum")
    checker.check(control["waterfall_rows"] == 0, "the control run fed the waterfall")
    rects = {rect for frame in control["per_frame"] for rect in rects_of(frame)}
    profiled_data = {
        invalidated(COMPONENTS["spectrum"]),
        invalidated(COMPONENTS["waterfall"]),
    }
    checker.check(
        not (rects & profiled_data),
        f"the control run invalidated a data region anyway: {rects & profiled_data}",
    )
    control_share = 100 * control["totals"]["invalidated_px"] / natural["totals"]["invalidated_px"]
    checker.check(
        control["totals"]["invalidated_px"] * 10 < natural["totals"]["invalidated_px"],
        f"the drive only explains {control_share:.0f}% of the natural area: "
        "the control does not delimit the measurement",
    )
    # Con el drive apagado el control no puede invalidar nada que no sea el
    # componente declarado del band-info: no hay espectro ni cascada, y las
    # guardas de OPT-4 #150 apagan el reloj y la runa BT. Se verifica contra la
    # caja declarada (y su histograma), no contra un "area chica": un rect que no
    # sea el del band-info es una invalidacion que el banco no puede atribuir y
    # el gate falla. Es el mismo criterio que el PROFILE de la corrida natural,
    # aplicado al unico componente que el control puede ver solo.
    band_info_rect = invalidated(COMPONENTS["band_info"])
    static_counts = Counter(rect for frame in control["per_frame"] for rect in rects_of(frame))
    for rect in static_counts:
        checker.check(
            rect == band_info_rect,
            f"the control run invalidated {rect} ({rect[2] * rect[3]} px), not the declared "
            f"band-info rectangle {band_info_rect} (box {COMPONENTS['band_info']})",
        )
    checker.check(
        static_counts == STATIC_RECT_COUNTS,
        "the control rectangle histogram moved: "
        f"{dict(static_counts)} != {dict(STATIC_RECT_COUNTS)}",
    )
    # #306: la union de los componentes declarados cubre el rect que la corrida
    # natural reporta. El band-info es hijo del waterfall y su caja cae adentro de
    # la del waterfall, asi que LVGL une las dos invalidaciones en el rect del
    # waterfall -- exactamente el que declara COMPONENTS["waterfall"] -- y no en
    # uno mas grande. Es la invariante de la DECLARACION: si un re-baseline mueve
    # el band-info fuera de la caja del waterfall, el rect reportado por la
    # corrida natural seria la union de los dos y el histograma no cerraria.
    waterfall_rect = invalidated(COMPONENTS["waterfall"])
    checker.check(
        band_info_rect in rects and _contiene(waterfall_rect, band_info_rect),
        f"the control's band-info rectangle {band_info_rect} is not inside the waterfall's "
        f"{waterfall_rect}: the union of the declared components would be bigger than the "
        "profiled rectangle",
    )
    # El settle es el mismo en los cinco modos (es la captura de arranque, previa
    # al modo): el control tambien lo tiene que pagar, con sus 7 flushes.
    checker.check(
        static["metrics"]["flushes"] == SETTLE_FLUSHES + control["totals"]["flushes"],
        f"static metrics.flushes = {static['metrics']['flushes']}: the settled capture "
        f"({SETTLE_FLUSHES}) plus the control frames ({control['totals']['flushes']})",
    )
    checker.check(
        static["metrics"]["flushed_pixels"] == SETTLE_PX + control["totals"]["flushed_px"],
        f"static metrics.flushed_pixels = {static['metrics']['flushed_pixels']} (the settled "
        f"capture must keep its {SETTLE_PX} px and the control frames add their own",
    )
    # OPT-4 #150: con el drive apagado quedan solo los timers de 2 s del
    # firmware. Las guardas de igualdad del reloj (src/clock.c) y de la runa BT
    # (src/info.c) apagan los dos: sin ellas el control invalidaba los ticks de
    # reloj y el de BT aunque el valor no cambiara. Despues de las guardas el
    # control es solo el band-info (7 x 27200 px).
    #
    # La runa de BT se chequea por REGION (la media celda derecha de la barra,
    # x167..195 / y31..61) y no por un rectangulo exacto: #285 movio la runa a
    # x173..190 / y34..57, asi que una clave medida antes de ese cambio dejaria
    # la guarda sin verificar de verdad (el rectangulo nuevo no matchearia).
    bt_region = (167, 31, 195, 61)
    bt_rects = [r for r in rects if _intersecta(r, bt_region)]
    checker.check(
        (586, 0, 214, 49) not in rects and not bt_rects,
        f"the equality guards (OPT-4 #150) did not suppress the identical-value timers: "
        f"{rects} (BT: {bt_rects})",
    )
    checker.check(
        static["metrics"]["framebuffer_fnv1a"] != natural_doc["metrics"]["framebuffer_fnv1a"],
        "the natural run and the control rendered the same framebuffer: the driven frames never "
        "reached the screen",
    )
    checker.note(
        f"  delimitation: drive off -> {control['totals']['invalidated_px']} px "
        f"({control_share:.1f}% of the {natural['totals']['invalidated_px']} px with the drive), "
        "only band-info rectangles (the OPT-4 #150 equality guards removed the clock/BT ticks)"
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
        "the forced and the natural modes rendered the same bytes: the natural mode changed "
        "the PNG",
    )
    # El settle es el mismo en todos los modos (la captura de arranque es previa
    # al modo); el forzado agrega sus 3 repintados de pantalla completa.
    checker.check(
        forced["metrics"]["flushes"] == SETTLE_FLUSHES + len(benchmark["frames"]),
        f"forced metrics.flushes = {forced['metrics']['flushes']}: the settled capture "
        f"({SETTLE_FLUSHES}) plus the {len(benchmark['frames'])} forced frames",
    )
    checker.check(
        forced["metrics"]["flushed_pixels"]
        == SETTLE_PX + len(benchmark["frames"]) * FULL_SCREEN_PX,
        f"forced metrics.flushed_pixels = {forced['metrics']['flushed_pixels']} (the settled "
        f"capture must keep its {SETTLE_PX} px and the forced frames their full screen)",
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
    checker.check(
        cli["metrics"]["flushed_pixels"] == SETTLE_PX + cli["natural"]["totals"]["flushed_px"],
        f"cli metrics.flushed_pixels = {cli['metrics']['flushed_pixels']} (the settled capture "
        f"must keep its {SETTLE_PX} px and the CLI frames add their own)",
    )
    checker.note("  CLI override: --bench-natural 10 ran 10 frames (the fixture says 40)")


def check_display_off(checker: Checker, off: dict, natural_doc: dict, lit_doc: dict) -> None:
    """La ruta de pantalla apagada de la ROM no hace trabajo de render.

    `--display-off` manda la tecla POWER por el camino del firmware
    (src/main_screen.c -> src/backlight.c -> lv_disp_enable_invalidation(false))
    antes de la corrida natural, asi que los 40 frames con el drive andando
    tienen que invalidar y flushear CERO: el espectro, la cascada y el reloj
    siguen disparando, y ninguna de sus invalidaciones llega al driver. La vara
    es el mismo fixture encendido (12580800 px, `natural`): la diferencia es lo
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
    # saltea el computo de la paleta (~231200 px por fila) en vez de hacerlo para
    # nada. El instrumento de src/waterfall.c tiene que dar 0 con las filas
    # llegando igual, contra una por fila con la pantalla encendida (arriba).
    checker.check(
        natural["waterfall_offscreen_redraws"] == 0,
        f"display off: the waterfall ran {natural['waterfall_offscreen_redraws']} offscreen "
        "redraw(s) with the panel dark (the palette computation must be skipped)",
    )
    # OPT-2 (#148): el anillo tampoco rasteriza una sola fila con el panel oscuro.
    checker.check(
        natural["waterfall_rasterized_px"] == 0,
        f"display off: the waterfall rasterized {natural['waterfall_rasterized_px']} px "
        "with the panel dark (the ring must skip the whole render)",
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
    # corrida es justo lo que no ocurre. Los 727840 px de la captura son los que
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
    check_natural(checker, natural_doc, rom_refresh_period_ms(ROOT))
    check_static(checker, load(out_dir, "static"), natural_doc)
    check_forced(checker, load(out_dir, "forced"), natural_doc)
    check_cli(checker, load(out_dir, "cli"))
    check_display_off(checker, load(out_dir, "off"), natural_doc, load(out_dir, "forced"))

    print("\n".join(checker.lineas))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
