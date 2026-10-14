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
    ui_inspector.c      structural inspection of the live LVGL object tree (--inspect)
    screens.c           scenario -> screen builder (calls the firmware's main_screen())
    scenario.c          scenario file parser (radio state, theme, seed)
    signal_gen.c        deterministic spectrum/waterfall magnitudes, seeded by the scenario
    coverage.c          REAL/PARTIAL/SYNTHETIC/UNSUPPORTED table
  mocks/
    mock_radio.c        explicit radio state + a call log (mock_radio_calls)
    mock_cfg.c          cfg / cfg_cur subjects and the band catalogue the components read
    mock_params.c       params_t with the values the layout needs
    mock_main_screen.c  the app dialogs, DX cluster column, scheduler queue, recorder/RTTY surface
    mock_ui_deps.c      event bus, message helpers, voice, BT/WiFi/backlight, clock
    mock_dsp.c          the DSP setter mfk.cpp calls (no FFT runs)
  third_party/          vendored build headers (see third_party/README.md)
```

The real firmware sources are compiled here (see `CMakeLists.txt`): `main_screen.c`
plus `spectrum.c`, `waterfall.c`, `tx_info.c`, `band_plan.c`, `band_plan_data.c`,
`pannel.c`, `vol.c`, `msg.c`, `msg_tiny.c`, `adaptive_tuning.c`, `cw_tune_ui.c`,
`clock.c`, `info.c`, `meter.c`, `band_info.c`, `theme.c`, `styles.c`, `fonts/*`,
the GUI's C++ (`buttons.cpp`, `mfk.cpp`, `controls.cpp`) and the real
`cfg/subjects.cpp`. Nothing under `src/` of the repository is modified: the
harness owns its source list, its LVGL config and its mocks.

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

CLI: `ui_harness --scenario <file> --png <out.png> [--json <out.json>] [--bench [--warmup N] [--repeats N]] [--inspect]`.

The JSON carries the scenario name/screen/seed, the git revision, the
per-component coverage, the radio-call log from the mock, and the render metrics
(flushes, flushed pixels, a framebuffer hash).

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

## Scenarios

A scenario is a `key value` file (unknown keys are an error). Besides the radio
state (`frequency_hz`, `mode`, `rxtx`, `smeter_db`, `battery_v`, …) it can pick
the `theme` and a `seed`, and toggle `band_plan`, `spectrum_filled`,
`spectrum_peak` and `waterfall_center_line`.

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

The exact, per-scenario table is in each JSON's `coverage` array. It is never
inflated: a PARTIAL is never written as REAL.
