# Host UI harness (`ui_harness`)

Renders **real** X6200 GUI components on a PC, without the radio: the firmware
sources are compiled for the host against the repository's stock LVGL 8.3.11
(submodule `lvgl/`), drawn headless into an 800×480 RAM framebuffer and written
to a PNG. No X11/Wayland, GPU or host display is involved, and no device or
network is touched.

It is the base of the A/B testing plan (`docs/AB_TESTING_PLAN.md`): a screenshot
that can be produced identically twice is the measurement the rest of the
pipeline compares.

## Layout

```text
tools/ui_harness/
  CMakeLists.txt        host build (real sources + stock LVGL + mocks)
  Dockerfile            host build image (x6200-uiharness)
  lv_conf.h             LVGL config for the host (32-bit, 'A:' -> host fs, lv_msg)
  fixtures/             scenario files (one per screen/state)
  src/
    harness_main.c      CLI, wiring, JSON report
    display.c           RAM framebuffer + LVGL flush callback
    png_export.c        PNG writer (libpng)
    virtual_time.c      deterministic tick: advance N ms, no sleeps
    render_metrics.c    flush count / flushed pixels
    bench_metrics.c     real-clock (CLOCK_MONOTONIC) frame timings + /proc memory
    natural_bench.c     natural-invalidation bench: LVGL's own loop over the virtual clock, per-frame invalidation (`--bench-natural`)
    wakeup_bench.c      wakeup profile of the ROM's GUI loop vs its tick thread (`--bench-wakeups`)
    ui_inspector.c      structural inspection of the live LVGL object tree (--inspect)
    screens.c           scenario -> screen builder (calls the firmware's main_screen())
    scenario.c          scenario file parser (radio state, theme, seed)
    signal_gen.c        deterministic spectrum/waterfall magnitudes, seeded by the scenario
    coverage.c          REAL/PARTIAL/SYNTHETIC/UNSUPPORTED table
  mocks/
    mock_radio.c        explicit radio state + a call log (mock_radio_calls)
    mock_cfg.c          cfg / cfg_cur subjects and the band catalogue the components read
    mock_params.c       params_t with the values the layout needs
    mock_main_screen.c  the app dialogs (Settings is real; the rest are inert), DX cluster column, scheduler queue, recorder/RTTY surface
    mock_ui_deps.c      event bus, message helpers, voice, BT/WiFi/backlight, audio volume + DSP preset + wfview server surfaces, the fixed wall clock (`__wrap_time`) and the virtual clock
    mock_dsp.c          the DSP setter mfk.cpp calls (no FFT runs)
  third_party/          vendored build headers (see third_party/README.md)
```

The real firmware sources are compiled here (see `CMakeLists.txt`): `main_screen.c`
plus `spectrum.c`, `waterfall.c`, `tx_info.c`, `band_plan.c`, `band_plan_data.c`,
`pannel.c`, `vol.c`, `msg.c`, `msg_tiny.c`, `adaptive_tuning.c`, `cw_tune_ui.c`,
`clock.c`, `info.c`, `meter.c`, `band_info.c`, `theme.c`, `styles.c`, `fonts/*`,
`dialog.c` + `dialog_settings.c` (the shared dialog machinery and the real Ajustes
window the `settings-home`/`settings-wfbat` fixtures open), the GUI's C++
(`buttons.cpp`, `mfk.cpp`, `controls.cpp`) and the real `cfg/subjects.cpp`.
Nothing under `src/` of the repository is modified: the harness owns its source
list, its LVGL config and its mocks.

Two harness-owned services the Ajustes window needs are worth naming: the wall
clock (the dialog fills its date/time fields from `time()`/`localtime()`, so the
binary is linked with `--wrap=time` onto a fixed epoch and `harness_main` pins
`TZ=UTC`) and the audio/wfview/preset surfaces (`src/audio.c`,
`src/audio_enhancement.c`, `src/wfview_server.c` are not compiled: the mixer
level, the DSP preset load and the wfview server are the explicit mocks in
`mocks/mock_ui_deps.c`).

## Build

The Docker image `x6200-uiharness` is the reproducible build environment. It is
built **from scratch** from `ubuntu:24.04`: the `ci` stage installs the host
build dependencies (`build-essential`, `cmake`, `pkg-config`, `libpng-dev`,
`libnm-dev`, `libsqlite3-dev`, `libliquid-dev`, `libspeexdsp-dev`) and the `full`
stage (the default target) adds the extra packages the ROM's own test suites use
(`libfftw3-dev`, `libsndfile1-dev`, `libglib2.0-dev`, `sqlite3`,
`libboost-dev`, `liquid-dsp 1.4.0`, `Catch2`). It no longer depends on the local
`x6200-buildroot` image, which is 6.7 GB and cannot be rebuilt in CI. The
harness links only libpng and libnm; the signal generator is plain C and does
not use FFTW, but the firmware headers the harness compiles against pull in
`<sqlite3.h>` (`src/cfg/cfg.h`), `<liquid/liquid.h>` (`src/dsp.h`,
`src/util.h`) and `<speex/speex_preprocess.h>` (`src/audio_enhancement.h`, which
`src/dialog_settings.c` includes), hence those three development packages.

```sh
# the default (full) image, same tag the tooling expects
docker build -t x6200-uiharness tools/ui_harness
# the minimal image CI uses (harness + framework only)
docker build -t x6200-uiharness-ci --target ci tools/ui_harness
```

Configure and build the harness (run from the repository root):

```sh
REV=$(git rev-parse HEAD)
docker run --rm --entrypoint bash --user "$(id -u):$(id -g)" \
  -v "$PWD:/src" -w /src/tools/ui_harness x6200-uiharness \
  -lc "mkdir -p build && cd build && cmake .. -DHARNESS_REVISION=$REV && make -j\$(nproc)"
```

Git cannot see the linked worktree from inside the container (the mount hides
`.git`), so the revision is passed in with `-DHARNESS_REVISION`. On a host that
can run `git`, omitting it lets CMake read it (`git -C <repo> rev-parse HEAD`);
otherwise the JSON reports `"unknown"`.

## Run

```sh
docker run --rm --entrypoint bash --user "$(id -u):$(id -g)" \
  -v "$PWD:/src" -w /src/tools/ui_harness/build x6200-uiharness \
  -lc "mkdir -p out && ./ui_harness \
      --scenario ../fixtures/home-usb-20m.scn \
      --png      out/home-usb-20m.png \
      --json     out/home-usb-20m.json"
```

CLI: `ui_harness --scenario <file> --png <out.png> [--json <out.json>] [--bench [--warmup N] [--repeats N]] [--inspect] [--font <family>] [--digits <family>] [--bench-natural [N]] [--natural-frames N] [--natural-static] [--bench-wakeups [MS]] [--display-off]`.

`--font <id>` renders with a whole font family and `--digits <id>` with a segmented
one for the digit panels only (the frequency row and the clock); both take the ids
of `src/fonts/font_families.h` (generated, `tools/fonts/README.md`) and are what the
font matrix and the gallery of issue #9 use: one build of the harness carries every
candidate family, so the whole matrix is one revision instead of one revision per
family. Without them the family the theme or `params.font` names is used, exactly
like the firmware's `main()`. A fixture can declare the digit family itself with the
`digits` key (`fixtures/main-frecuencia-dseg.scn`).

The JSON carries the scenario name/screen/seed, the git revision, the
per-component coverage, the radio-call log from the mock, and the render metrics
(flushes, flushed pixels, a framebuffer hash).

It also carries a `theme` block — `requested` (the theme this capture rendered),
`params_name` (what the firmware's own `params.theme_name` holds, read from the
`params_str_t` the mock shares with the firmware), `params_max_chars` and
`active`. The two names differ when the theme name does not survive the
firmware's parameter — 16 bytes before UI-013, 32 (31 useful) since the fix, so
the menu's names all fit. That is what makes the theme resolution measurable
instead of assumed (see the contract `theme_name_fits_params` in
`tools/ab_testing`, and UI-013 in `docs/UI_KNOWN_ISSUES.md`).

## Structural inspection (`--inspect`)

With `--inspect` the JSON gains an `inspection` block: the **live LVGL object
tree** of the rendered screen (`src/ui_inspector.c`). It is what the pixels alone
cannot state, and it is measured, never guessed:

```jsonc
"inspection": {
  "instrumented": true,
  "viewport": { "width": 800, "height": 480 },
  "nodes": [
    {
      "class": "lv_label", "x": 15, "y": 5, "w": 56, "h": 26,
      "clip": { "x": 0, "y": 0, "w": 800, "h": 480 },   // effective clipping
      "visible": { "x": 15, "y": 5, "w": 56, "h": 26 }, // box ∩ clip, null if off screen
      "hidden": false, "opa": 255, "scrollable": true, "scroll_dir": "all",
      "text_color": "#FFFFFF", "text_opa": 255,
      "bg_color": "#3A3F47", "bg_opa": 255, "bg_image": false,
      "text": "VFO-A", "text_font": "sony_20", "long_mode": "WRAP",
      "text_box": { "w": 56, "h": 16 },
      "text_natural": { "w": 51, "h": 17 }, "text_wrapped_h": 17,
      "text_overflow": false, "text_truncated": false,
      "missing_glyphs": [], "fallback_glyphs": [],
      "children": [ /* recursive */ ]
    }
  ]
}
```

* `visible` is the object's own box intersected with the clipping of its
  ancestors (a parent without `LV_OBJ_FLAG_OVERFLOW_VISIBLE` clips its children,
  `lv_refr.c`); `null` when nothing of it lands on the screen.
* `scroll_dir` is the axis the object may scroll on (`none`/`hor`/`ver`/`all`,
  `lv_obj_get_scroll_dir`). LVGL's default is `all` on any object that never
  configured scrolling — the screen and every sub-widget report it — so it is what
  tells a **scroll area** (the Ajustes grid declares `ver`) from a default box: the
  structural layout rules use it to tell content the user can scroll into view from
  content pushed off the screen by a defect (`quality/layout_checks.py`).
* Text sizes come from LVGL's own font metrics (`lv_txt_get_size`), with the
  label's letter/line space and long mode. `text_overflow` and `text_truncated`
  compare the text against the object's box; **no OCR** is involved.
* `in_keyboard_group` says whether the object is registered in the firmware's
  focus group (`lv_obj_get_group(obj) == keyboard_group`); `dialog_item()` is the
  only thing that adds an Ajustes widget to it (`src/dialog.c`), so a control that
  is drawn but not in the group cannot be reached with the keys (UI-004). The block
  also carries `keyboard_group: true` when the fixture created the group at all —
  the `main` fixtures do not (`keyboard_init()` is a no-op on the host).
* `missing_glyphs` lists code points that are a placeholder in *every* font of
  the chain (`lv_font_glyph_dsc_t.is_placeholder`); `fallback_glyphs` lists the
  ones resolved in another font (`resolved_font`). The fallback is expected here:
  the alternate fonts are ASCII-only and the stock family carries the icons
  (`docs/THIRD_PARTY.md`).
* `--inspect` is only a flag on the JSON path: **it never changes a pixel**, so a
  scenario's PNG is byte-identical with and without it, and the benchmark path is
  untouched. `tests/check_harness_determinism.sh` still passes.

## Benchmark

With `--bench` the harness also measures the **real** render cost of each frame
with `CLOCK_MONOTONIC` (`src/bench_metrics.c`), separately from the virtual
clock that decides the pixels. The JSON gains a `benchmark` block:

```jsonc
"benchmark": {
  "clock": "CLOCK_MONOTONIC",
  "environment": "x86 host: relative regression proxy, not the Cortex-A7",
  "warmup_frames": 3, "repeats": 5, "redraw_passes": 1,
  "frames": [
    { "render_ms": 3.79, "flush_ms": 0.41, "draw_ms": 3.37,
      "flushes": 1, "redrawn_area_px": 384000, "unique_redrawn_px": 384000 }
  ],
  "memory": { "rss_bytes": 10100736, "pss_bytes": 8751104,
              "peak_rss_bytes": 10100736, "peak_vm_bytes": 79761408 }
}
```

* `render_ms` is the wall time of the whole frame; `flush_ms` is the time inside
  the display flush callback (measured); `draw_ms` is the remainder
  (layout + draw), derived and never invented.
* `redrawn_area_px` is the sum of the flushed rectangles (a repeated repaint
  counts again); `unique_redrawn_px` is the distinct set. `memory` is read from
  `/proc/self/status` and `/proc/self/smaps_rollup` at the end.
* Warm-up frames are discarded; the measured frames are configurable with
  `--warmup`/`--repeats` or a fixture's `bench_warmup`/`bench_repeats`.
  `bench_redraw_passes` lets a fixture repaint the screen several times per
  measured frame (the controlled cost regression the benchmark self-test uses).
  All three `bench_*` keys are ignored outside `--bench` and never change a
  rendered pixel.

Benchmarking never feeds a wall-clock value back into the GUI: a scenario's PNG
is byte-identical with and without `--bench`. `tests/check_harness_determinism.sh`
still runs every fixture twice and compares the sha256.

## Natural-invalidation bench

The forced bench above measures a full-screen repaint the harness fabricates
itself: every measured frame invalidates the whole screen and calls
`lv_refr_now()`. That is a cost proxy, and it is **blind to the firmware's own
invalidation** — halving the invalidated area of the spectrum moves neither
`flushes` nor `redrawn_area` (measured in UI-07 increment 1,
`docs/AB_TESTING.md`). The natural bench is the opposite measurement:

```sh
./ui_harness --scenario ../fixtures/main-natural.scn \
    --png out/natural.png --json out/natural.json --bench-natural
```

* No forced invalidation and no `lv_refr_now()`: the refresh is the one **LVGL's
  own refresh timer** decides, from the invalidations the real components queue
  through the scheduler.
* The spectrum and the waterfall are driven **frame by frame** by the same
  deterministic generator (`src/signal_gen.c`) on the radio's own cadence:
  `signal_gen_spectrum_frame()` every `1000/15` ms and
  `signal_gen_waterfall_row()` every `1000/25` ms (`src/dsp.cpp`'s
  `spectrum_fps_ms`/`waterfall_fps_ms`), checked once per 10 ms slice. The clock
  invalidates every 500 ms on its own (`src/clock.c`).
* **One natural frame is one LVGL display refresh period** of virtual time: the
  ROM's own (`LV_DISP_DEF_REFR_PERIOD`, 50 ms), which the harness reads from
  `lv_conf.h` at build time (`HARNESS_ROM_REFR_PERIOD`) and puts on the display's
  refresh timer before measuring — the same source the wakeup profile uses, so
  the two cannot disagree. `--bench-natural [N]`, `--natural-frames N` or the
  fixture's `natural_frames N` set how many frames the run does (40 by default,
  2000 ms of virtual time).
* The JSON gains a `natural` block; `metrics` and `benchmark` keep their format
  and their meaning (the settled capture's own numbers, plus what the natural
  frames add).

```jsonc
"natural": {
  "enabled": true,
  "clock": "virtual-time",
  "environment": "x86 host: the channel/ARM energy cost is not measured",
  "frame_ms": 50, "slice_ms": 10, "frames": 40,
  "data_drive": true, "seed": 13,
  "spectrum_fps_ms": 66, "waterfall_fps_ms": 40,
  "spectrum_frames": 28, "waterfall_rows": 40,
  "waterfall_offscreen_redraws": 40,
  "per_frame": [
    { "frame": 1, "virtual_end_ms": 2250, "refresh_cycles": 1,
      "invalidated_px": 196000, "invalidated_rects": 1, "joined_px": 0,
      "flushed_px": 196000, "flushes": 1, "rects_complete": true,
      "rects": [[0, 235, 800, 245]] },
    { "frame": 2, "virtual_end_ms": 2300, "refresh_cycles": 1,
      "invalidated_px": 132000, "invalidated_rects": 1, "joined_px": 0,
      "flushed_px": 132000, "flushes": 1, "rects_complete": true,
      "rects": [[0, 0, 800, 165]] }
  ],
  "totals": { "invalidated_px": 11223840, "invalidated_rects": 67, "joined_px": 1372000,
              "flushed_px": 11223840, "flushes": 67, "refresh_cycles": 39,
              "frames_with_flush": 39 }
}
```

* `invalidated_px` is the invalidation **LVGL collected** for the frame's
  refresh cycle (`inv_areas`/`inv_p`, read from inside the flush callback before
  LVGL clears them — the same trace increment 1 used as a temporary
  diagnostic). `joined_px` is the part LVGL merged into another rectangle, so it
  is not counted twice; `flushed_px`/`flushes` are what the display driver was
  asked to flush. With the harness' full-screen draw buffer the two are 1:1.
* `rects` is the evidence: the invalidated rectangles of that frame, `x,y,w,h`.
  On `main-natural` every one of them is a region a firmware timer invalidates
  (spectrum `0,0 800x165`, waterfall `0,235 800x245`, clock `580,0 220x72`, band
  info `0,241 800x34`, BT icon `152,23 30x36`): the bench invalidates nothing on
  its own.
* Nothing in the block is a wall clock value, so two runs with the same seed and
  the same frame count write the same bytes (PNG included).
* `--natural-static` runs the same loop **without** the data drive: the spectrum
  and the waterfall are then never fed, so only the clock/BT/band-info rectangles
  remain. It is the delimitation control of
  `tests/check_harness_natural.sh`, not a product mode.
* `waterfall_offscreen_redraws` is the instrument of `src/waterfall.c` (issue #13):
  how many times the waterfall rebuilt its 800 x height px palette off-screen
  during the run. With the panel lit it is one per new row (24 on `main-natural`);
  with `--display-off` the waterfall skips the palette computation, so it is 0.

`tests/check_harness_natural.sh` is the gate: it rebuilds the harness, runs the
fixture in the four modes, checks the PNG and the whole JSON are identical across
two natural runs, and checks the produced areas against the profile of UI-07
increment 1 (spectrum 132000 px per FFT frame, waterfall 196000 px per row, clock
15840 px per 500 ms tick), the drive cadence against `src/dsp.cpp`'s constants,
and that `--bench` still repaints the whole 384000 px screen. The raw output of a
run is in `tools/ab_testing/reports/NATURAL-ECOFINAL/`.

### Display off (`--display-off`)

`--display-off` runs the ROM's own display-off route before the capture and the
bench: the POWER key (`src/keypad.c` sends `EVENT_KEYPAD` to the active screen),
the main screen's callback and `src/backlight.c` (the real module, compiled
here) -> `lv_disp_enable_invalidation(disp, false)`. With the panel dark the
whole natural run invalidates and flushes **0 px** while the spectrum, the
waterfall and the clock keep firing and the data drive keeps running: the same
image (`metrics.framebuffer_fnv1a` is the lit capture's), no render work. The
capture is also exactly one full-screen repaint lighter (1127680 -> 743680 px),
because the repaint the harness does before every capture is skipped too. The
waterfall also stops rebuilding its off-screen palette (`refresh_waterfall()` skips
it while `backlight_is_on()` is false, `src/waterfall.c`): the run's
`waterfall_offscreen_redraws` drops from one per row to **0**.

`tests/check_harness_natural.sh` runs it as its fifth mode and
`tests/natural_evidence.py` locks it (0 px per frame, the reduced capture, the
same framebuffer as the lit one, 0 off-screen redraws). The property on the ROM
side — one `lv_disp_enable_invalidation` call per switch, `false` off and `true`
on, and the same state the waterfall gate consumes — is in
`tests/native/test_backlight.c`, which compiles the real `src/backlight.c`.

## Wakeup profile

The ROM has two wakeup sources: its GUI loop (`src/main.c`: it sleeps until
`lv_timer_handler()` says a timer is due; the DSP's posts do **not** wake it,
`src/scheduler.cpp` is a plain queue) and — while `LV_TICK_CUSTOM` is off — its
tick thread (`while (1) { usleep(5 * 1000); lv_tick_inc(5); }`). Which one costs
more is a number:

```sh
./ui_harness --scenario ../fixtures/main-natural.scn \
    --png out/wk.png --json out/wk.json --bench-wakeups        # 1000 ms window
./ui_harness … --bench-wakeups 5000                            # CLI window in ms
```

* It reproduces the ROM's loop over the virtual clock — the observer pass, the
  scheduler and `lv_timer_handler()`, then a sleep until the deadline the handler
  returned — on the **firmware's own timer set** (the real components are the ones
  creating and arming timers). During the sleep only the tick and the data drive
  advance (`virtual_time_advance_idle`): running the handler there would count
  wakeups the firmware does not have.
* It runs the loop with the ROM's configured refresh period
  (`LV_DISP_DEF_REFR_PERIOD`, `lv_conf.h`) instead of the harness's own LVGL
  default (30 ms), and reports the ROM's tick mode next to it. Both the period
  and `LV_TICK_CUSTOM` come from the ROM's `lv_conf.h`, read at build time by the
  harness CMake and passed as `HARNESS_ROM_REFR_PERIOD` (one source shared with
  the natural bench, which now measures its frames at the same period) and
  `HARNESS_ROM_TICK_CUSTOM`: with the latter on, `tick_custom` is true, the tick
  thread does not exist and `tick_thread_wakeups` is 0; with it off, the tick
  cadence is read from `src/main.c`. The gate re-reads the same source, so
  neither can drift out from under the measurement.
* Two passes: `with_drive` (the radio's steady state) and `static` (the data
  drive off — the delimitation control).
* The JSON gains a `wakeups` block: `wakeups`, `with_flush`, `immediate`,
  `no_timer`, `sleep_ms_min/max/total` and `flushed_px` per pass, plus
  `window_ms`, `refresh_period_ms`, `slice_ms`, `tick_custom`, `tick_thread_ms`,
  `tick_thread_wakeups` and the two drive cadences. No wall clock is read.
* It is **not** a cost measurement: the time spent in a wakeup and anything about
  the Cortex-A7's energy are the hardware pass (BLOCKED, no radio attached).

Measured on `main-natural` (seed 13, 1000 ms): the loop wakes **14** times with
the data path and **11** without it, sleeping 10-200 ms. In the base run the 5 ms
tick thread woke **200** times (93.5% of the two sources); this revision applies
`LV_TICK_CUSTOM 1` with `CLOCK_MONOTONIC` (lv_conf.h), so the thread is gone and
`tick_thread_wakeups` is **0**. `tests/check_harness_wakeups.sh`
is the gate (idempotency, the sources, the delimitation, the refused
combinations) and the raw output is in `tools/ab_testing/reports/WAKEUPS-ECOFINAL/`.

## Scenarios

A scenario is a `key value` file (unknown keys are an error). Besides the radio
state (`frequency_hz`, `mode`, `rxtx`, `smeter_db`, `battery_v`, …) it can pick
the `theme` and a `seed`, toggle `band_plan`, `spectrum_filled`,
`spectrum_peak`, `waterfall_center_line` and `waterfall_battery` (the user's
waterfall battery mode, the Ajustes switch), set the two audio gains of the
Ajustes Audio group (`play_gain_db`, `rec_gain_db`, the dB the radio loads from
params.db into `params.play_gain_db_f`/`params.rec_gain_db_f`) and pick the
`clock_view` of the top bar's clock (`time`, `power` — the default — or
`time-power`, the firmware's `clock_view_t`), which is what the wall clock of the
`main` screen shows. `bench_warmup`, `bench_repeats`,
`bench_redraw_passes` (the forced bench) and `natural_frames` (the natural bench)
are honoured only in their own mode and never change a rendered pixel.

The `swrscan` screen (issue #125) adds four keys, all of them about the fake ROE
acquisition backend of `src/swr_backend.c` (see its own section below):
`swr_dataset` names one of the datasets in `tests/fixtures/swrscan/`,
`swr_passes` is the number of passes asked of the window (1, 3 or 5 — the
window's own gate still decides whether the session runs), `swr_feed` is how many
flow packets the backend hands the dialog before it stops (0 = the whole
session, a smaller number leaves the session running so a fixture can capture it
mid-sweep) and `swr_stop` presses Stop after the last fed packet (the operator's
cancel). The grid's centre and span are **not** keys: the backend derives them
from the dataset and stops the run if the fixture's `frequency_hz` does not match
the dataset's centre.

`mode` takes the names the radio itself lists and accepts (`mode_names[]`,
`src/remote_server.cpp`): `LSB`, `LSB-D`, `USB`, `USB-D`, `CW`, `CW-R`, `AM`,
`NFM`, plus `SAM` and `WFM` (the enum has them, the remote list does not). The
older enum-ish spellings (`cwr`, `usb_dig`) still work; the mode label the top bar
draws is the one `info_params_mode_label_get()` (src/info.c) returns, so a fixture
can measure how a given mode's text fits its fixed cell.

The `screen` selects what is built: `main` (the real main screen),
`settings-home`, `settings-wfbat`, `settings-perfil` and `settings-audio`
(the same screen with the real Ajustes window, `ACTION_APP_SETTINGS` → `dialog_construct(dialog_settings)`;
`settings-wfbat` then presses the bottom bar's "Mas >" and "Espectro" tabs through
`buttons_press()`, the same path the user takes, so the view is scrolled to the
Spectrum/waterfall group with its battery switch on, `settings-perfil` presses
"Mas >" and "Apariencia" to reach the "Visual profile" row of UI-08 increment 5, and
`settings-audio` presses the "Audio" tab, drags the Rec gain slider and checks which
of the two audio setters ran -the two sliders look alike, see the row's entry in the
table below-),
`settings-bands` (Ajustes and
then the real Bands action, `main_screen_action(ACTION_APP_BANDS)`), `app-bands`
(the app menu the APP key loads, `buttons_load_page_group(buttons_group_app)`, with
the "Bands" button of `APP 3:6` pressed — the route the owner reported), and
`main-foco` (the same screen with the real keyboard
group, so `lv_group_focus_next()` runs over it),
`apps-menu` (the app menu the APP key loads, `buttons_load_page_group(buttons_group_app)`,
on the page a fixture's `apps_page` picks: the six `apps-menu-1..6` cover the 22
app cells after issue #46), and `bt-dialog` (the real Bluetooth window, `src/dialog_bt.c`,
opened by `main_screen_start_app(ACTION_APP_BT)`), and `swrscan` (the real SWR Scan
window, `src/dialog_swrscan.c`, opened by `main_screen_start_app(ACTION_APP_SWRSCAN)`
and driven by the fake ROE acquisition backend of `src/swr_backend.c` — its own
section below).

The `theme` is the full name the GUI lists: one of the six themes of the collection
(`Sunlit`, `Redshift`, `Sprint`, `Cathode`, `Trail`, `Aurora`, docs/THEMES.md). A
fixture that does not set one renders the default, Aurora (issue #74). The harness
reads the shipped themes from this
repository's `rootfs/usr/share/x6200/themes` (the same files the image installs)
by setting `THEMES_DIR_SHIPPED` before `theme_init()`; an explicit
`THEMES_DIR_SHIPPED` still wins. The theme's own font is applied exactly as the
firmware's `main()` does, plus the theme's `font_digits` for the digit panels.

The name is kept whole by the scenario and handed to `theme_init()` as the
firmware would (a name the firmware cannot store is the firmware's problem, not
the fixture's: the JSON `theme` block shows both, see UI-013). A name that is not
one of the six (a retired legacy theme or an unknown one) makes `theme_init()` fall
back to Aurora, and the JSON `active` field says which theme actually rendered.

| Fixture | What it shows |
|---|---|
| `home-usb-20m` | 20 m USB, S9, on battery: the plain main screen |
| `home-bateria-externa` | external supply (13.8 V) with the battery at 12.1 V, charging; exercises the power clock above the firmware's 0..10 V mapping (UI-009) |
| `main-tx-simulado` | TX state with split (TX visor, TX info). No audio/RF is generated |
| `main-bateria-baja` | nearly flat battery; the clock drops to the battery-percentage view |
| `home-frecuencia-extrema` | the dial at the top band edge; frequency/band-plan strips run past it |
| `main-espectro-agua` | the waterfall with a fixed seed, peaks on, centre line, Aurora theme |
| `bench-render` | the benchmark fixture (waterfall peaks, its own seed); `bench_*` knobs for the render benchmark |
| `main-frecuencia-dseg` | the frequency row and the clock drawn with the segmented-digit family (`digits dseg7`) plus the "8 fantasma" layer behind their digits (`clock_view time`, the clock view that has unlit digits to show): the sample screen of the DSEG candidates of issue #9 (a segmented face is digit-only, so it is never a whole-family choice) |
| `main-natural` | the natural-bench fixture (its own seed, `natural_frames`); the spectrum/waterfall are driven frame by frame by `--bench-natural` |
| `main-natural-eco` | the natural-bench fixture with the pack on battery at 11.2 V (`external_v 0.0`): the same screen/data as `main-natural`, but the visual policy puts the waterfall in ECO, so `--bench-natural` measures the reduced cadence (UI-08 incr. 2) |
| `settings-home` | the real main screen with the real Ajustes window open over it (`dialog.c` + `dialog_settings.c`), on its first group (clock: date/time, Clock view, timeouts). The wall clock is the harness' fixed epoch, so the date/time fields are deterministic |
| `settings-wfbat` | Ajustes scrolled to the Spectrum/waterfall group with the waterfall battery mode the user turned on (`waterfall_battery true`); the switch renders checked and the real waterfall runs the saving cadence the visual policy applies |
| `settings-perfil` | Ajustes scrolled to the Apariencia group, where the "Visual profile" row of UI-08 increment 5 lives. The pack is on battery at 11.2 V (`external_v 0.0`), so the auto policy decides ECO: the dropdown renders `Auto` (the persisted setting) while the row label reads the effective profile (`Visual profile (Eco)`). `scenario_settle` codifies that exact text |
| `settings-audio` | Ajustes scrolled to the Audio group (the "Audio" tab, page 1 index 3): the "Line-in, Line-out" row, the "Play,Rec gain" row, "Audio enhance", "Speaker mode" and the voice rows. `play_gain_db`/`rec_gain_db` give the two gain sliders different values (the radio loads them from params.db) so each one can be told apart, and `scenario_settle` drags the Rec one and checks with the audio mock's recorder (`mocks/mock_ui_deps.c`) that the setter it ran is `audio_set_rec_vol()`: with the two callbacks wired to the same function (issue #26) the run stops instead of going green |
| `settings-bands` | Ajustes and then the real Bands action (`main_screen_action(ACTION_APP_BANDS)`, the dispatcher the Ajustes long-press list reaches). `dialog_bands` is an inert mock here, so the fixture codifies the route in `scenario_settle` (the Ajustes window has to be closed and a dialog has to be running) instead of the window's pixels |
| `app-bands` | the app menu the APP key opens (`buttons_group_app`, page `APP 3:6`) with the "Bands" button pressed — the route the owner reported ("the Bands menu does nothing"). Same inert `dialog_bands`: `scenario_settle` stops the run if the button reaches no dialog at all |
| `apps-menu-1` … `apps-menu-6` | the app menu the APP key opens (`buttons_load_page_group(buttons_group_app)`) on each of its six pages (the fixture's `apps_page`); together they render the 22 app cells after issue #46. `scenario_settle` reads the real `buttons_group_app` table and stops the run if the page's `(APP n:6)` label or an app name is not on screen (UI-002) |
| `bt-dialog` | the real Bluetooth window (`src/dialog_bt.c`) over the main screen, opened by `main_screen_start_app(ACTION_APP_BT)`. `btaudio-ctl` is mocked (`__wrap_system` + a seeded `/tmp/bt_ui_list.txt`, `mocks/mock_btaudio.c`) so a device row and the status line render. `scenario_settle` codifies the UI-005 metric: the status label's bottom must end above the button row (y=418) |
| `app-close-home` | menu-hygiene (decisions 5/6): RTTY (the app that is not a dialog) opened with `main_screen_start_app(ACTION_APP_RTTY)` and closed through the remote control's own path (`main_screen_close_app()`), then RTTY again with the real Ajustes window over it and the dialog closed. `scenario_settle` codifies both checkpoints — RTTY is `RTTY_OFF` (its state is a tracking mock, `mocks/mock_main_screen.c`) and the bar is back on the home page (`buttons_get_cur_page()`), not on RTTY's page — and stops the run if either fails |
| `main-foco` | the real main screen with the real keyboard group and the focus moved with `lv_group_focus_next()` |
| `swrscan-quick` | the real SWR Scan window (`src/dialog_swrscan.c`) over the main screen, one pass (Quick — the only one the shipped ROM enables), complete and frozen: the drawn curve, the `Completed` state and the readout of the measured minimum (`min 1.2 +/- 0.1 @ 7.100.000 kHz (n=51/51)`) |
| `swrscan-owner-40m` | the real window over the owner's **real antenna dataset** (`owner_40m_valle`, the seed embedded in `src/swr_backend.c` from the values extracted from his screen, commit `b39d6014`), one pass (Quick), complete and frozen: the drawn curve over the real resonance range (centre 7.026.825 Hz, span 21.562 kHz) and the readout `min 1.0 @ 7.019.925 kHz (n=51/51)`. This is the capture that fixes issue #125's follow-up: the previous bench captures were centred off the antenna's resonance and showed no curve |
| `swrscan-owner-40m-domo` | the owner's **inverted** capture (`docs/evidencia/2026-10-15-validacion-arreglos/26-roe-multipasada-final.png`, commit `b39d6014`): the same 40 m window but with the dome-shaped sweep the radio actually showed (ROE ~1.0 at both edges, ~3.8 in the middle, the last 7 of 51 points missing — `n=44/51`, the radio's `Error: timeout de RF`). The dataset `owner_40m_domo` is embedded in `src/swr_backend.c` as a seed (centre 6.991.334 Hz, span 200 kHz, mock-only). It is the RED of the #125-fix: the free least-squares fit of a dome is concave (`a < 0`) and used to draw the parabola with its base **up**; the fix mirrors it into the physical valley (base down, tips rising) |
| `swrscan-owner-40m-sweep` | the same owner session captured mid-sweep (25 of 51 points): `Scanning pass 1/1  p 26/51`, the button on `Stop`, and the measured curve so far (not frozen: the JSON's `frozen` is `null`) |
| `swrscan-3pass` | the same window, three passes (Stable) over `single_valley_3pass`, complete and frozen. `scenario_settle` also probes the freeze: three extra flow packets after the session ended must move neither the VFO nor the session (the backend stops the run if they do) |
| `swrscan-3pass-p1`, `-p2`, `-p3` | the same three-pass session captured mid-sweep (25, 76 and 127 packets): the per-pass evidence of the issue. The state reads `Scanning pass 1/3  p 26/51`, `2/3`, `3/3`; the graph is the measured data so far and is not frozen (the JSON's `frozen` is `null`) |
| `swrscan-3pass-stop` | the same session cancelled by the operator (the Run button, now "Stop") after 70 packets: `Stopped (parcial)`, the graph frozen at the measured data, the readout marked as partial |
| `swrscan-5pass-outliers` | five passes (Fine) over `outliers_5pass`: the median rejects the false low sample of pass 1 and the false high of pass 3, so the measured minimum is still the real dip. The numeric oracle is the native test (`tests/native/test_swrscan_multipass.c`), not the pixels |
| `swrscan-5pass-moving` | five passes over `moving_5pass`: the resonance moves between passes, so the readout reports the instability (MAD 0.5) instead of one credible minimum |
| `swrscan-narrow-zoom` | three passes over `narrow_zoom_3pass` (50 kHz span, 1 kHz step): the sharp dip of 7.103.000 Hz is resolved |
| `swrscan-wide-zoom` | three passes over `wide_zoom_3pass` (500 kHz span, 10 kHz step): the same sharp dip falls between points and is missed. The window is **mock-only** (it leaves the 40 m allocation, `tests/fixtures/swrscan/README.md`); the firmware has no band-plan TX block for any feature yet, so this capture is render/safety-rejection evidence, not a transmission approval |
| `swrscan-double-valley`, `swrscan-flat` | the two "no single credible resonance" datasets: two real valleys (the lower one is reported) and all medians equal (the readout reports the first tie with MAD 0.0) |
| `swrscan-3pass-tema-aurora`, `swrscan-3pass-tema-cathode` | the same complete three-pass session under two shipped themes, the theme as the only variable (the chart, the dialog frame and the buttons change colour). Aurora is the harness default, so its PNG is byte-identical to `swrscan-3pass` — the same thing `bt-dialog-tema-aurora` does |

## SWR Scan (`swrscan`): the real window over a fake acquisition backend

The SWR Scan window **transmits while it measures** (`radio_start_swrscan()` puts
the radio in `RADIO_SWRSCAN`), so the host cannot run it against hardware. The
`swrscan` fixtures render the **real** window — `src/dialog_swrscan.c` plus the
session engine it drives, `src/swr_session.c`, and the curve resampler,
`src/roe_curve.c`, all compiled here — over a fake radio:

- `src/swr_backend.c` is the fake side of the contract. It reads one of the
  owner's datasets (`tests/fixtures/swrscan/swr_mock_samples.csv`, the same file
  the native test reads) and replays it on the harness' virtual clock: one flow
  packet per point at the real cadence (`SWRSCAN_DWELL_MS`, 35 ms), with the
  radio thread's tick (`dialog_swrscan_poll()`, the stall watchdog) after each
  one, and the scheduler job the window queues when the session ends. The owner's
  **real 40 m dataset** (`owner_40m_valle`, issue #125 follow-up) is **embedded in
  the backend** as a seed (the values extracted from his screen, commit
  `b39d6014`, resampled to the 51-point grid): the fixtures `swrscan-owner-40m*`
  use it to anchor the session to the antenna's real resonance range. The shared
  CSV is left untouched because the native test pins its exact size (1,428
  samples).
- The grid is the dataset's, not the fixture's: the backend derives centre and
  span from the samples, pushes the span into `cfg.swrscan_span` before the
  window opens, and checks the frequency the dialog asks for **point by point**
  against the dataset (which is also how the alternating direction of the odd
  passes is verified). A fixture whose `frequency_hz` is not the dataset's centre
  stops the run.
- `scenario_settle` presses Run (the same button the user presses) and, when the
  fixture asks for it, Stop; then it checks that the sweep really started, and —
  when the session ended — that it is frozen: three extra packets must not move
  the VFO and must not start another session.

**No real TX.** The instrument is `mocks/mock_radio.c`:
`radio_start_swrscan()`/`radio_stop_swrscan()`/`radio_set_freq()` only count and
record, and the binary links no RF library at all (`aether_control` is not part
of the harness), so there is no code path from a fixture to a transmission. The
JSON reports the counters (`radio_calls.start_swrscan`, `stop_swrscan`, and the
`swr` block with `tx_on`/`tx_off`), which is the "TX real = 0" evidence of the
bench.

The `swr.passes_label` field of the JSON is the text of the **real LVGL label** of
the bottom bar's Passes button (`lv_label_get_text()`, not the function that
generates it), so it is what the operator sees. It is there because the passes
button is not a `subject`: unlike span/scale there is no `MSG_PARAM_CHANGED` to
repaint it, so `dialog_swrscan_passes_cb()` asks for the repaint itself
(`buttons_refresh()`). Without that call the window runs 3 or 5 passes while the
button keeps reading `Passes: 1`; the field (and the native test that pins the
request) is what makes that regression visible.

**Two things are compiled differently here, on purpose.** The multipass TX gate
(`SWRSCAN_MULTIPASS_TX_ENABLED`, `src/dialog_swrscan.h`) is 0 in the ROM and is
compiled **on** in this host binary (`tools/ui_harness/CMakeLists.txt`), because
with it off the window refuses 3 and 5 passes and there would be nothing to
capture: the ROM keeps it off (`src/CMakeLists.txt` passes no such define) and
`test_swrscan_multipass_gate` pins that. The settle window
(`SWRSCAN_SETTLE_MS`) is 0 in the window itself and the fixtures do not change
it. Both mean the same thing for what these captures prove: they show the logic
and the rendering, **never** that transmitting three or five passes is safe on
hardware. Enabling multipass TX for real (and any settle window above 0) needs
the radio: `requires-radio`. The coverage table of each `swrscan` JSON says so
per component.

## Determinism

Nothing that shapes the pixels reads the wall clock or sleeps:

- the colour/geometry inputs come from `theme.c`, `styles.c`, the fixtures and
  the mocks, all fixed;
- LVGL's timers and animations advance only through `virtual_time_advance()`
  (`lv_tick_inc` + `lv_timer_handler` in fixed slices); `get_time()` (the
  firmware's monotonic clock) is wired to the same counter;
- the radio state, the band catalogue and the cfg subjects are the mocks'
  fixed values, never a device or a database;
- the wall clock is a harness-owned service too: the Ajustes dialog fills its
  date/time fields from `time()`/`localtime()`, so the binary is linked with
  `--wrap=time` onto a fixed epoch (`__wrap_time`, `mocks/mock_ui_deps.c`) and
  `harness_main` pins `TZ=UTC`. Without it the fixture would render the host's
  current second and every run would differ.

`tests/check_harness_determinism.sh` builds the harness and runs **every**
fixture twice, comparing the PNGs by sha256 (it must print `PASS`). Each run
also writes a JSON, so "PNG + JSON" is checked for every scenario.

The natural bench is deterministic by construction too: it reads no wall clock
(only the harness' virtual counter), its per-frame values are functions of the
scenario's seed and the frame count, and `tests/check_harness_natural.sh` runs it
twice and compares the PNG sha256 and the whole JSON byte for byte.

## Coverage (main screen)

`main_screen()` builds the whole screen with real components; the radio/DSP
inputs are substituted, so coverage is declared honestly per component:

| Component | Level | Why |
|---|---|---|
| `theme.c`, `styles.c`, `fonts/` | REAL | compiled and drawn |
| `main_screen.c` | REAL | `main_screen()` builds the screen |
| `spectrum.c`, `waterfall.c` | PARTIAL | real component; magnitudes from the seed-driven generator, not an FFT/radio |
| `band_info.c` | PARTIAL | real component; band rows from the harness catalogue, not the DB |
| `band_plan.c` + `band_plan_data.c` | REAL | drawn (IARU strip) |
| `meter.c`, `info.c`, `clock.c`, `tx_info.c` | REAL | drawn |
| `buttons.cpp`, `pannel.c`, `msg.c` + `msg_tiny.c` | REAL | drawn |
| `adaptive_tuning.c` | REAL | compiled and initialised (multiplier label hidden) |
| `cw_tune_ui.c` | PARTIAL | compiled; hidden while CW tune is off |
| `cfg/subjects.cpp` | REAL | the real Subject/observer core |
| `cfg/settings_manager.cpp` | REAL | the remote control's mirror of the configuration; its parameters are bound to the real subjects at boot and checked there (`mocks/mock_settings_manager.cpp`) |
| `cfg` (catalogue), `params`, `radio` | SYNTHETIC | harness-owned values; no DB, no device |
| `spot_marks.c`, `cluster_column.c`, dialogs | UNSUPPORTED | not rendered in these scenarios |

The `settings-home`/`settings-wfbat`/`settings-perfil`/`settings-audio` screens swap the dialog row
for the truth of the Ajustes path: `src/dialog.c` and `src/dialog_settings.c` become REAL, and the
other app dialogs stay UNSUPPORTED (they keep their inert mock descriptors). The
services the window calls — mixer volume, DSP preset, wfview server, the RTC, the
transverter subjects — are SYNTHETIC, and `dialog_field.c` is declared UNSUPPORTED
to record that it is **not** a dependency of Ajustes. The
`settings-bands`/`app-bands` screens keep
`dialog_bands.c` (and its subtree: `dialog_field.c`, `field/bands.c`,
`cluster/spots.c`) explicitly UNSUPPORTED: the Bands window is an inert mock, so
those two fixtures measure the route the firmware takes to it — an assertion on
the firmware's own state in `scenario_settle`, never its pixels. The
`app-close-home` screen also brings the Ajustes row REAL (its route opens the
real window over RTTY) and declares `rtty.c` SYNTHETIC: the RTTY codec is not
compiled, only the state the route sets and clears. The coverage is
chosen per `screen` (`coverage_for_screen()` in `src/coverage.c`).

The exact, per-scenario table is in each JSON's `coverage` array. It is never
inflated: a PARTIAL is never written as REAL.

The `swrscan` screen brings `dialog_swrscan.c`, `swr_session.c` and
`roe_curve.c` REAL (the window, the session engine and the curve resampler are
the firmware's own sources) and declares the fake acquisition backend PARTIAL:
the packets the dialog consumes come from the dataset replay, not from an
instrument. `radio` and `scheduler` stay SYNTHETIC (counters and a synchronous
queue), the settle window is SYNTHETIC (a virtual wait is not a measured one),
the multipass TX gate is PARTIAL (compiled on in this binary only) and RF/TX is
UNSUPPORTED — there is no radio, so nothing here proves the sweep is safe on
hardware. The same table, per fixture, is in the JSON.
