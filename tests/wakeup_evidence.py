#!/usr/bin/env python3
"""Checks of the wakeup profile (`tests/check_harness_wakeups.sh`).

Reads the JSON the harness wrote for `fixtures/main-natural.scn` in the modes of
the gate and verifies, on the produced numbers:

* the ROM's own refresh period (`LV_DISP_DEF_REFR_PERIOD`, lv_conf.h) and the
  tick thread's cadence (`usleep(5 * 1000)` + `lv_tick_inc(5)`, src/main.c) are
  the ones the profile ran with: the numbers are read from the firmware sources,
  so neither config can drift out from under the measurement;
* the data drive cadence is the radio's DSP loop (1000/15 and 1000/25,
  src/dsp.cpp), the same mirror the natural bench uses;
* the loop's sleep accounting adds up to the window and the loop never
  busy-spins (no `immediate`, no `no_timer`): what is counted is sleep-and-wake
  cycles, not iterations of a spinning loop;
* the headline of issue #13: the tick thread's wakeups dominate the ROM's own
  loop's by at least 5x on the main screen, with and without the data path
  (the delimitation control);
* the window is a CLI knob and the counts scale with it;
* a plain capture (no mode) carries no `wakeups` block, and the block declares
  that the wakeup *cost* is not measured on this host.

Usage: wakeup_evidence.py <out-dir> <repo-root>   (prints the evidence, exits 1 on a miss)

SPDX-License-Identifier: LGPL-2.1-or-later
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

# The window the gate runs without an explicit one (`--bench-wakeups`).
DEFAULT_WINDOW_MS = 1000
# The slice the profile advances the virtual clock by (wakeup_bench.c:
# NATURAL_BENCH_SLICE_MS).
SLICE_MS = 10
# Los timers de la pantalla principal dejan al lazo durmiendo, no girando: si
# alguna vez apareciera una vuelta con `lv_timer_handler()` en 0 o sin ningun
# timer armado, el conteo de wakeups dejaria de ser "una vuelta por sueno" y el
# gate tiene que cortar en vez de reportar un numero que no significa lo mismo.
MAX_IMMEDIATE = 0
MAX_NO_TIMER = 0
# El titular: el tick despierta al menos 5x mas que el lazo de la ROM.
MIN_TICK_RATIO = 5


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


def source_number(path: Path, pattern: str, what: str) -> int:
    match = re.search(pattern, path.read_text(), re.M)
    if match is None:
        print(f"FAIL: {what}: {pattern!r} not found in {path}", file=sys.stderr)
        raise SystemExit(1)
    return int(match.group(1))


def rom_refresh_period_ms(root: Path) -> int:
    """`LV_DISP_DEF_REFR_PERIOD` de lv_conf.h (la config de la ROM)."""
    return source_number(
        root / "lv_conf.h",
        r"^#define\s+LV_DISP_DEF_REFR_PERIOD\s+(\d+)",
        "the ROM's refresh period",
    )


def rom_tick_thread_ms(root: Path) -> int:
    """La cadencia del hilo del tick (src/main.c, tick_thread()).

    El `usleep(N * 1000)` y el `lv_tick_inc(N)` tienen que ser el MISMO numero:
    el hilo declara a LVGL el tiempo que durmio, y si los dos numeros se separan
    el tick deja de ser la medida del tiempo que el lazo reporta.
    """
    text = (root / "src/main.c").read_text()
    head = re.search(r"tick_thread\s*\(void\s*\*args\)\s*\n\{", text)
    if head is None:
        print("FAIL: tick_thread() definition not found in src/main.c", file=sys.stderr)
        raise SystemExit(1)

    rest = text[head.end() :]
    closing = rest.find("\n}")
    body = rest if closing < 0 else rest[:closing]

    match = re.search(r"usleep\s*\(\s*(\d+)\s*\*\s*1000\s*\)", body)
    if match is None:
        print("FAIL: tick_thread() has no usleep(N * 1000)", file=sys.stderr)
        raise SystemExit(1)
    sleep_ms = int(match.group(1))

    match = re.search(r"lv_tick_inc\s*\(\s*(\d+)\s*\)", body)
    if match is None:
        print("FAIL: tick_thread() has no lv_tick_inc(N)", file=sys.stderr)
        raise SystemExit(1)
    tick_ms = int(match.group(1))

    if sleep_ms != tick_ms:
        print(
            f"FAIL: tick_thread() sleeps {sleep_ms} ms but reports {tick_ms} ms to LVGL",
            file=sys.stderr,
        )
        raise SystemExit(1)
    return sleep_ms


def dsp_fps_ms(root: Path, name: str) -> int:
    """`spectrum_fps_ms`/`waterfall_fps_ms` de src/dsp.cpp, en ms (1000 / N)."""
    divisor = source_number(
        root / "src/dsp.cpp",
        rf"{name}\s*=\s*\(1000\s*/\s*(\d+)\)",
        f"{name} in src/dsp.cpp",
    )
    return 1000 // divisor


def check_block(checker: Checker, name: str, block: dict, window_ms: int) -> None:
    checker.check(block["wakeups"] > 0, f"{name}: no wakeup was measured over {window_ms} ms")
    checker.check(
        block["sleep_ms_total"] == window_ms,
        f"{name}: the sleeps add up to {block['sleep_ms_total']} ms, not the window's {window_ms} ms",
    )
    checker.check(
        block["immediate"] <= MAX_IMMEDIATE,
        f"{name}: {block['immediate']} wakeup(s) with lv_timer_handler() at 0 ms: the loop "
        "did not sleep (the count is no longer one wakeup per sleep)",
    )
    checker.check(
        block["no_timer"] <= MAX_NO_TIMER,
        f"{name}: {block['no_timer']} wakeup(s) with no timer armed: the loop would spin",
    )
    checker.check(
        block["sleep_ms_min"] > 0 and block["sleep_ms_min"] <= block["sleep_ms_max"],
        f"{name}: sleep_ms_min/max = {block['sleep_ms_min']}/{block['sleep_ms_max']}",
    )
    checker.check(
        block["with_flush"] <= block["wakeups"],
        f"{name}: {block['with_flush']} flushing wakeups of {block['wakeups']}",
    )


def check_wakeups(checker: Checker, doc: dict, root: Path) -> None:
    wakeups = doc["wakeups"]
    window_ms = wakeups["window_ms"]
    period_ms = rom_refresh_period_ms(root)
    tick_ms = rom_tick_thread_ms(root)
    spectrum_ms = dsp_fps_ms(root, "spectrum_fps_ms")
    waterfall_ms = dsp_fps_ms(root, "waterfall_fps_ms")

    checker.check(
        window_ms == DEFAULT_WINDOW_MS,
        f"the default window is {window_ms} ms, expected {DEFAULT_WINDOW_MS}",
    )
    checker.check(
        wakeups["refresh_period_ms"] == period_ms,
        f"the profile ran with a {wakeups['refresh_period_ms']} ms refresh period, but the ROM "
        f"configures {period_ms} ms (lv_conf.h)",
    )
    checker.check(
        wakeups["tick_thread_ms"] == tick_ms,
        f"the profile declares a {wakeups['tick_thread_ms']} ms tick thread, but src/main.c "
        f"sleeps {tick_ms} ms",
    )
    checker.check(
        wakeups["slice_ms"] == SLICE_MS,
        f"slice_ms = {wakeups['slice_ms']}, expected {SLICE_MS}",
    )
    checker.check(
        wakeups["spectrum_fps_ms"] == spectrum_ms and wakeups["waterfall_fps_ms"] == waterfall_ms,
        "the drive cadence does not mirror src/dsp.cpp (1000/15, 1000/25)",
    )
    checker.check(
        wakeups["tick_thread_wakeups"] == window_ms // tick_ms,
        f"tick_thread_wakeups = {wakeups['tick_thread_wakeups']}, expected "
        f"{window_ms // tick_ms} ({window_ms} ms / {tick_ms} ms)",
    )
    checker.check(
        "is not measured" in wakeups["environment"],
        "the block does not declare that the wakeup cost is not measured on this host: "
        f"{wakeups['environment']!r}",
    )

    drive = wakeups["with_drive"]
    static = wakeups["static"]
    check_block(checker, "with_drive", drive, window_ms)
    check_block(checker, "static", static, window_ms)

    # La delimitacion: el camino de datos (espectro + cascada) agrega vueltas y
    # trabajo de render; sin el, quedan solo los timers del firmware.
    checker.check(
        static["wakeups"] < drive["wakeups"],
        f"the data drive added no wakeups ({static['wakeups']} without it, {drive['wakeups']} with it)",
    )
    checker.check(
        static["flushed_px"] < drive["flushed_px"],
        f"the data drive added no render work ({static['flushed_px']} px without it, "
        f"{drive['flushed_px']} with it)",
    )

    # El titular de #13: el tick del hilo, que despierta pase lo que pase, pesa
    # mucho mas que el lazo de la ROM.
    tick = wakeups["tick_thread_wakeups"]
    checker.check(
        tick >= MIN_TICK_RATIO * drive["wakeups"],
        f"the tick thread's {tick} wakeups are less than {MIN_TICK_RATIO}x the loop's "
        f"{drive['wakeups']}: the profile no longer shows the tick dominating",
    )
    checker.note(
        f"  loop: {drive['wakeups']} wakeups with the data drive ({static['wakeups']} without "
        f"it) in {window_ms} ms; sleeps {drive['sleep_ms_min']}-{drive['sleep_ms_max']} ms"
    )
    checker.note(
        f"  tick thread: {tick} wakeups in {window_ms} ms ({tick_ms} ms, src/main.c) = "
        f"{100 * tick / (tick + drive['wakeups']):.1f}% of the two sources"
    )


def check_window(checker: Checker, doc: dict) -> None:
    """`--bench-wakeups 3000` corre 3 s: los conteos escalan con la ventana."""
    wakeups = doc["wakeups"]
    window_ms = wakeups["window_ms"]
    tick_ms = wakeups["tick_thread_ms"]

    checker.check(window_ms == 3000, f"--bench-wakeups 3000 ran {window_ms} ms")
    checker.check(
        wakeups["tick_thread_wakeups"] == window_ms // tick_ms,
        f"the 3000 ms window reports {wakeups['tick_thread_wakeups']} tick wakeups, expected "
        f"{window_ms // tick_ms}",
    )
    checker.check(
        wakeups["with_drive"]["sleep_ms_total"] == window_ms,
        "the 3000 ms window's sleeps do not add up to the window",
    )
    checker.check(
        wakeups["tick_thread_wakeups"] >= MIN_TICK_RATIO * wakeups["with_drive"]["wakeups"],
        "the 3000 ms window no longer shows the tick dominating",
    )
    checker.note(
        f"  window override: --bench-wakeups 3000 ran {window_ms} ms with "
        f"{wakeups['with_drive']['wakeups']} loop wakeups (the default window: "
        f"{DEFAULT_WINDOW_MS} ms)"
    )


def check_plain(checker: Checker, plain: dict, wakeup_doc: dict) -> None:
    """El bloque es extra: la captura comun no lo lleva.

    Y el trabajo de render del modo cuadra con esa captura: el settle (los mismos
    px que la captura comun) mas lo que flusheo cada pase. Si un pase dejara de
    contarse o se contara dos veces, la suma no cierra.
    """
    checker.check("wakeups" not in plain, "the plain capture carries a `wakeups` block")

    wakeups = wakeup_doc["wakeups"]
    settle_px = plain["metrics"]["flushed_pixels"]
    passes_px = wakeups["with_drive"]["flushed_px"] + wakeups["static"]["flushed_px"]
    checker.check(
        wakeup_doc["metrics"]["flushed_pixels"] == settle_px + passes_px,
        f"the wakeup run flushed {wakeup_doc['metrics']['flushed_pixels']} px: the settled capture "
        f"({settle_px}) plus the two passes ({passes_px})",
    )
    checker.note(
        f"  plain capture: no `wakeups` block (the mode is additive) and its {settle_px} px "
        "settle plus the two passes add up to the run's flushed pixels"
    )


def main(argv: list[str]) -> int:
    if len(argv) != 3:
        print("usage: wakeup_evidence.py <out-dir> <repo-root>", file=sys.stderr)
        return 2
    out_dir = Path(argv[1])
    root = Path(argv[2])
    checker = Checker()

    # Las dos corridas del gate (wakeups.json / run1.json) tienen que ser byte a
    # byte iguales; el shell compara los bytes, aca se leen los numeros.
    wakeup_doc = load(out_dir, "wakeups")
    check_wakeups(checker, wakeup_doc, root)
    check_window(checker, load(out_dir, "window"))
    check_plain(checker, load(out_dir, "plain"), wakeup_doc)

    print("\n".join(checker.lineas))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
