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
`x6200-buildroot` (the ROM Buildroot image, built from the AetherX6200Buildroot
workspace — see `docs/TRD.md`) plus the host build dependencies
(`build-essential`, `cmake`, `pkg-config`, `libpng-dev`, `libsqlite3-dev`,
`libnm-dev`, `libfftw3-dev`, `liquid-dsp 1.4.0`). Rebuild it with:

```sh
docker build -t x6200-uiharness tools/ui_harness
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

CLI: `ui_harness --scenario <file> --png <out.png> [--json <out.json>]`.

The JSON carries the scenario name/screen/seed, the git revision, the
per-component coverage, the radio-call log from the mock, and the render metrics
(flushes, flushed pixels, a framebuffer hash).

## Scenarios

A scenario is a `key value` file (unknown keys are an error). Besides the radio
state (`frequency_hz`, `mode`, `rxtx`, `smeter_db`, `battery_v`, …) it can pick
the `theme` and a `seed`, and toggle `band_plan`, `spectrum_filled`,
`spectrum_peak` and `waterfall_center_line`.

| Fixture | What it shows |
|---|---|
| `home-usb-20m` | 20 m USB, S9, on battery: the plain main screen |
| `main-tx-simulado` | TX state with split (TX visor, TX info). No audio/RF is generated |
| `main-bateria-baja` | nearly flat battery; the clock drops to the battery-percentage view |
| `home-frecuencia-extrema` | the dial at the top band edge; frequency/band-plan strips run past it |
| `main-otro-tema` | the same screen under the builtin "Outdoor" theme |
| `main-espectro-agua` | the waterfall with a fixed seed, peaks on, centre line, Legacy theme |

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
