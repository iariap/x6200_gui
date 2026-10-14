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
    ui_inspector.c      structural inspection of the live LVGL object tree (--inspect)
    screens.c           scenario -> screen builder (calls the firmware's main_screen())
    scenario.c          scenario file parser (radio state, theme, seed)
    signal_gen.c        deterministic spectrum/waterfall magnitudes, seeded by the scenario
    coverage.c          REAL/PARTIAL/SYNTHETIC/UNSUPPORTED table
  mocks/
    mock_radio.c        explicit radio state + a call log (mock_radio_calls)
    mock_cfg.c          cfg / cfg_cur subjects and the band catalogue the components read
    mock_params.c       params_t with the values the layout needs
    mock_main_screen.c  the app dialogs (About is real; the rest are inert), DX cluster column, scheduler queue, recorder/RTTY surface
    mock_ui_deps.c      event bus, message helpers, voice, BT/WiFi/backlight, clock
    mock_dsp.c          the DSP setter mfk.cpp calls (no FFT runs)
  third_party/          vendored build headers (see third_party/README.md)
```

The real firmware sources are compiled here (see `CMakeLists.txt`): `main_screen.c`
plus `spectrum.c`, `waterfall.c`, `tx_info.c`, `band_plan.c`, `band_plan_data.c`,
`pannel.c`, `vol.c`, `msg.c`, `msg_tiny.c`, `adaptive_tuning.c`, `cw_tune_ui.c`,
`clock.c`, `info.c`, `meter.c`, `band_info.c`, `theme.c`, `styles.c`, `fonts/*`,
`dialog.c` + `dialog_about.c` (the shared dialog machinery and the real About
window the `dialog-about` fixture opens), the GUI's C++ (`buttons.cpp`, `mfk.cpp`,
`controls.cpp`) and the real `cfg/subjects.cpp`. Nothing under `src/` of the
repository is modified: the harness owns its source list, its LVGL config and its
mocks.

## Build

The Docker image `x6200-uiharness` is the reproducible build environment. It is
built **from scratch** from `ubuntu:24.04`: the `ci` stage installs the host
build dependencies (`build-essential`, `cmake`, `pkg-config`, `libpng-dev`,
`libnm-dev`, `libsqlite3-dev`, `libliquid-dev`) and the `full` stage (the
default target) adds the extra packages the ROM's own test suites use
(`libfftw3-dev`, `libsndfile1-dev`, `libglib2.0-dev`, `sqlite3`,
`libboost-dev`, `liquid-dsp 1.4.0`, `Catch2`). It no longer depends on the local
`x6200-buildroot` image, which is 6.7 GB and cannot be rebuilt in CI. The
harness links only libpng and libnm; the signal generator is plain C and does
not use FFTW, but the firmware headers the harness compiles against pull in
`<sqlite3.h>` (`src/cfg/cfg.h`) and `<liquid/liquid.h>` (`src/dsp.h`,
`src/util.h`), hence those two development packages.

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

CLI: `ui_harness --scenario <file> --png <out.png> [--json <out.json>] [--bench [--warmup N] [--repeats N]] [--inspect] [--bench-natural [N]] [--natural-frames N] [--natural-static]`.

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
      "hidden": false, "opa": 255, "scrollable": true,
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
* Text sizes come from LVGL's own font metrics (`lv_txt_get_size`), with the
  label's letter/line space and long mode. `text_overflow` and `text_truncated`
  compare the text against the object's box; **no OCR** is involved.
* `missing_glyphs` lists code points that are a placeholder in *every* font of
  the chain (`lv_font_glyph_dsc_t.is_placeholder`); `fallback_glyphs` lists the
  ones resolved in another font (`resolved_font`). The fallback is expected here:
  the alternate fonts are ASCII-only and icons come from the Sony copy.
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
* **One natural frame is one LVGL display refresh period** of virtual time
  (`LV_DISP_DEF_REFR_PERIOD`, 30 ms). `--bench-natural [N]`,
  `--natural-frames N` or the fixture's `natural_frames N` set how many frames
  the run does (40 by default).
* The JSON gains a `natural` block; `metrics` and `benchmark` keep their format
  and their meaning (the settled capture's own numbers, plus what the natural
  frames add).

```jsonc
"natural": {
  "enabled": true,
  "clock": "virtual-time",
  "environment": "x86 host: the channel/ARM energy cost is not measured",
  "frame_ms": 30, "slice_ms": 10, "frames": 40,
  "data_drive": true, "seed": 13,
  "spectrum_fps_ms": 66, "waterfall_fps_ms": 40,
  "spectrum_frames": 17, "waterfall_rows": 24,
  "per_frame": [
    { "frame": 1, "virtual_end_ms": 2260, "refresh_cycles": 1,
      "invalidated_px": 196000, "invalidated_rects": 1, "joined_px": 0,
      "flushed_px": 196000, "flushes": 1, "rects_complete": true,
      "rects": [[0, 235, 800, 245]] },
    { "frame": 2, "virtual_end_ms": 2290, "refresh_cycles": 1,
      "invalidated_px": 132000, "invalidated_rects": 1, "joined_px": 0,
      "flushed_px": 132000, "flushes": 1, "rects_complete": true,
      "rects": [[0, 0, 800, 165]] }
  ],
  "totals": { "invalidated_px": 6745720, "invalidated_rects": 45, "joined_px": 784000,
              "flushed_px": 6745720, "flushes": 45, "refresh_cycles": 37,
              "frames_with_flush": 37 }
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

`tests/check_harness_natural.sh` is the gate: it rebuilds the harness, runs the
fixture in the four modes, checks the PNG and the whole JSON are identical across
two natural runs, and checks the produced areas against the profile of UI-07
increment 1 (spectrum 132000 px per FFT frame, waterfall 196000 px per row, clock
15840 px per 500 ms tick), the drive cadence against `src/dsp.cpp`'s constants,
and that `--bench` still repaints the whole 384000 px screen. The raw output of a
run is in `tools/ab_testing/reports/NATURAL-UI07-N1/`.

## Scenarios

A scenario is a `key value` file (unknown keys are an error). Besides the radio
state (`frequency_hz`, `mode`, `rxtx`, `smeter_db`, `battery_v`, …) it can pick
the `theme` and a `seed`, and toggle `band_plan`, `spectrum_filled`,
`spectrum_peak` and `waterfall_center_line`. `bench_warmup`, `bench_repeats`,
`bench_redraw_passes` (the forced bench) and `natural_frames` (the natural bench)
are honoured only in their own mode and never change a rendered pixel.

`mode` takes the names the radio itself lists and accepts (`mode_names[]`,
`src/remote_server.cpp`): `LSB`, `LSB-D`, `USB`, `USB-D`, `CW`, `CW-R`, `AM`,
`NFM`, plus `SAM` and `WFM` (the enum has them, the remote list does not). The
older enum-ish spellings (`cwr`, `usb_dig`) still work; the mode label the top bar
draws is the one `info_params_mode_label_get()` (src/info.c) returns, so a fixture
can measure how a given mode's text fits its fixed cell.

The `screen` selects what is built: `main` (the real main screen), `dialog-about`
(the same screen with the real About window opened through the firmware's own
`main_screen_action(ACTION_APP_ABOUT)`) and `main-foco` (the same screen with the
real keyboard group, so `lv_group_focus_next()` runs over it).

The `theme` is the full name the GUI lists: a builtin (`IU1WXY`, `Legacy`,
`Outdoor`, `Classic`, `Outdoor-Negative`, `VISION`) or a JSON theme the ROM ships
(`RS`, `Holo`, `Material 3`). The harness reads the shipped themes from this
repository's `rootfs/usr/share/x6200/themes` (the same files the image installs)
by setting `THEMES_DIR_SHIPPED` before `theme_init()`; an explicit
`THEMES_DIR_SHIPPED` still wins. The theme's own font (Classic/RS name Arial) and
VISION's `fonts_scale()` are applied exactly as the firmware's `main()` does.

The name is kept whole by the scenario and handed to `theme_init()` as the
firmware would (a name the firmware cannot store is the firmware's problem, not
the fixture's: the JSON `theme` block shows both, see UI-013).

| Fixture | What it shows |
|---|---|
| `home-usb-20m` | 20 m USB, S9, on battery: the plain main screen |
| `home-bateria-externa` | external supply (13.8 V) with the battery at 12.1 V, charging; exercises the power clock above the firmware's 0..10 V mapping (UI-009) |
| `main-tx-simulado` | TX state with split (TX visor, TX info). No audio/RF is generated |
| `main-bateria-baja` | nearly flat battery; the clock drops to the battery-percentage view |
| `home-frecuencia-extrema` | the dial at the top band edge; frequency/band-plan strips run past it |
| `main-otro-tema` | the same screen under the builtin "Outdoor" theme |
| `main-espectro-agua` | the waterfall with a fixed seed, peaks on, centre line, Legacy theme |
| `bench-render` | the benchmark fixture (waterfall peaks, its own seed); `bench_*` knobs for the render benchmark |
| `main-natural` | the natural-bench fixture (its own seed, `natural_frames`); the spectrum/waterfall are driven frame by frame by `--bench-natural` |
| `main-natural-eco` | the natural-bench fixture with the pack on battery at 11.2 V (`external_v 0.0`): the same screen/data as `main-natural`, but the visual policy puts the waterfall in ECO, so `--bench-natural` measures the reduced cadence (UI-08 incr. 2) |
| `dialog-about` | the real main screen with the real About window open over it (`dialog.c` + `dialog_about.c`) |
| `main-foco` | the real main screen with the real keyboard group and the focus moved with `lv_group_focus_next()` |
| `main-tema-rs` | the same screen under the shipped "RS" JSON theme (with its images), for UI-007 |
| `main-tema-vision` | the builtin "VISION" theme (every font three sizes up through `fonts_scale()`), for the icon-glyph and top-bar layout findings of UI-011/UI-012 |
| `main-tema-vision-cwr` | "VISION" with the mode in `CW-R`: the 4-character mode whose label measures exactly the mode cell's 56 px (0 px of margin, the `CW-RAUTO` pegging of UI-014), for the pixel/visual evidence |
| `main-tema-vision-usbd` | "VISION" with the mode in `USB-D`: the widest mode the radio lists, whose label measures 66 px in the same 56 px cell and grows past the box, so the contract `typography_text_overflow` fails on it (UI-014's structural detector) |
| `main-tema-outdoor-negative` | the dark pair of `main-otro-tema` (every colour run through `negative()`, `src/theme.c`) |
| `main-tema-classic` | the builtin "Classic" theme (names the Arial family) |
| `main-tema-holo` | the shipped "Holo" JSON theme (flat, no images) |
| `main-tema-material-3` | the shipped "Material 3" JSON theme (the one the radio runs) |

## Determinism

Nothing that shapes the pixels reads the wall clock or sleeps:

- the colour/geometry inputs come from `theme.c`, `styles.c`, the fixtures and
  the mocks, all fixed;
- LVGL's timers and animations advance only through `virtual_time_advance()`
  (`lv_tick_inc` + `lv_timer_handler` in fixed slices); `get_time()` (the
  firmware's monotonic clock) is wired to the same counter;
- the radio state, the band catalogue and the cfg subjects are the mocks'
  fixed values, never a device or a database.

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
| `cfg` (catalogue), `params`, `radio` | SYNTHETIC | harness-owned values; no DB, no device |
| `spot_marks.c`, `cluster_column.c`, `freq_strip.c`, dialogs | UNSUPPORTED | not rendered in these scenarios |

The `dialog-about` screen swaps the dialog row for the truth of the About path:
`src/dialog.c` and `src/dialog_about.c` become REAL, and the other app dialogs
stay UNSUPPORTED (they keep their inert mock descriptors). The coverage is chosen
per `screen` (`coverage_for_screen()` in `src/coverage.c`).

The exact, per-scenario table is in each JSON's `coverage` array. It is never
inflated: a PARTIAL is never written as REAL.
