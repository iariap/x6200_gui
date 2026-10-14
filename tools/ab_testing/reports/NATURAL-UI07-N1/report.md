# Natural-invalidation bench — NATURAL-UI07-N1

- Status: **PASS** (`tests/check_harness_natural.sh`, exit 0)
- Date: 2026-10-14
- Revision: `325ec32` (base of `factory/ui07-bench-natural`; the harness adds the
  mode, the ROM's `src/` is untouched)
- Fixture: `tools/ui_harness/fixtures/main-natural.scn` (seed 13, `natural_frames 40`)
- Host: x86 (the Cortex-A7 cadence and the real flush cost stay PENDING)

> New, extra mode of the harness (`--bench-natural`). The forced bench
> (`--bench`), the plain capture, the A/B `compare`, the `contracts` and the
> `audit` keep their format and their numbers.

## What was measured

`--bench-natural` runs LVGL's natural loop over the harness' virtual clock: no
forced `lv_obj_invalidate(lv_scr_act())`, no `lv_refr_now()`. The refresh is the
one LVGL's own refresh timer decides, from the invalidations the real components
queue, and the spectrum/waterfall magnitudes are driven frame by frame by the
harness' deterministic generator (`src/signal_gen.c`) on the radio's cadence
(`src/dsp.cpp`: spectrum every 1000/15 ms, one waterfall row every 1000/25 ms).
One natural frame is one LVGL display refresh period (30 ms of virtual time).

Per frame the JSON reports the invalidation LVGL collected for the cycle (its own
buffer, read before it clears it) and what the display driver was asked to flush.

## Results (40 frames, 1200 ms of virtual time, revision `325ec32`)

| Region | Rectangle | px | Times (40 frames) | What invalidates it |
|---|---|---|---|---|
| spectrum | `0,0 800x165` | **132000** | 16 | `spectrum_refresh()` (src/spectrum.c) |
| waterfall | `0,235 800x245` | **196000** | 23 | `refresh_waterfall()` (src/waterfall.c) |
| clock | `580,0 220x72` | **15840** | 1 | `show_time()`, every 500 ms (src/clock.c) |
| band info | `0,241 800x34` | 27200 | 4 | the 2 s fade timer (src/band_info.c) |
| BT icon | `152,23 30x36` | 1080 | 1 | the 2 s state timer (src/info.c) |

- Natural totals: **6745720 px** invalidated in 45 rectangles / 45 flushes, over
  37 of the 40 frames (the other 3 have nothing to refresh).
- The three profile rectangles of UI-07 increment 1 match **exactly**
  (`docs/AB_TESTING.md`, section 2: 132000 px per FFT frame, 196000 px per
  waterfall row, 15840 px per 500 ms tick). The last two come from the firmware's
  2 s timers, which only fire because this run reaches t > 3 s; the plain capture
  (1.2 s) never sees them. **No other region is invalidated**: every rectangle of
  every frame is one of the five above (asserted by the gate).
- Drive cadence: 17 spectrum frames and 24 waterfall rows in 1200 ms — the same
  counts an independent reimplementation of `dsp_samples()`'s condition
  (`now - last > fps_ms`, checked once per 10 ms slice) produces at 1000/15 and
  1000/25. `metrics.flushes = 6 + 45` and `metrics.flushed_pixels = 1127680 +
  6745720`: the settled capture keeps its own numbers and the natural frames add
  theirs.

## Self-tests of the gate

| Self-test | Result |
|---|---|
| Idempotency: 2 runs, same seed | PNG sha `8ec63121…` twice (**identical**) and the whole JSON byte-identical (`cmp`) |
| Profile correlation | spectrum 132000 x16, waterfall 196000 x23, clock 15840 x1 — exact |
| Delimitation vs the drive (`--natural-static`) | 304760 px = **4.5%** of the 6745720 px: with the drive off only the clock/BT/band-info rectangles remain, so the profiled areas can only come from the deterministic drive |
| The driven frames reach the screen | the natural run's `framebuffer_fnv1a` (`0x1cf41b099f85ece4`) differs from the control's (`0x6576db34a91c95f8`) |
| Forced mode untouched | `--bench`: 3 frames x 1 flush x 384000 px, no `natural` block, same PNG as the plain capture (`3986bc9a…`) |
| CLI override | `--bench-natural 10` ran 10 frames (the fixture declares 40) |

`main-natural.png` is the PNG of the profiled run. `sha256.txt` carries the
hashes of the four runs. `natural.json`, `static.json`, `forced.json` and
`cli.json` are the raw output (the same files `tests/natural_evidence.py` checks
in the non-e2e suite).

## Honest limits

- x86 host: the areas are exact (they are the same code paths as the radio), but
  the *time* the real driver spends copying those rectangles to the panel, the
  real FFT cadence under load and anything about energy are the hardware pass
  (BLOCKED, no radio attached).
- The five rectangles are the invalidation *of this fixture* (default theme,
  peaks off, seed 13). A fixture with another theme moves the clock/band-info
  rectangles (fonts change the boxes); the profile of increment 1 was measured on
  `home-usb-20m` and the three main rectangles are the same ones here.
- `_lv_inv_area: detected modifying dirty areas in render` still appears **once**
  per run in every mode, the plain capture included (whose PNGs are byte-identical
  to the base revision `325ec32`, hashes `a656e096…`/`3986bc9a…`): it is the
  firmware's `band_info`/`middle_line` draw-end callback touching the object tree
  during a render. Pre-existing, not introduced here.
