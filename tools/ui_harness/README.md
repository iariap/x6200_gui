# Host UI harness (`ui_harness`)

Renders **real** X6200 GUI components on a PC, without the radio: the firmware
sources are compiled for the host against the repository's stock LVGL 8.3.11
(submodule `lvgl/`), drawn headless into an 800×480 RAM framebuffer and written
to a PNG. No X11/Wayland, GPU or host display is involved, and no device or
network is touched.

It is the base of the A/B testing plan (`docs/AB_TESTING_PLAN.md`, milestone 1):
a screenshot that can be produced identically twice is the measurement the rest
of the pipeline compares.

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
    screens.c           builders that place real components
    scenario.c          scenario file parser
    coverage.c          REAL/PARTIAL/SYNTHETIC/UNSUPPORTED table
  mocks/
    mock_radio.c        explicit radio state (frequency, mode, S-meter, battery…)
    mock_cfg.c          cfg / cfg_cur and its subjects
    mock_params.c       params_t with the values the layout needs
    mock_subjects.c     Subject/observer implementation (C)
    mock_ui_deps.c      event bus, msg, voice, BT/WiFi/backlight, clock
  third_party/          vendored build headers (see third_party/README.md)
```

Nothing under `src/` of the repository is modified: the harness owns its source
list, its LVGL config and its mocks.

## Build

The Docker image `x6200-uiharness` is the reproducible build environment. It is
`x6200-buildroot` (the ROM Buildroot image, built from the AetherX6200Buildroot
workspace — see `docs/TRD.md`) plus the host build dependencies
(`build-essential`, `cmake`, `pkg-config`, `libpng-dev`, `libsqlite3-dev`,
`libnm-dev`). Rebuild it with:

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

The JSON carries the scenario name, the git revision, the per-component
coverage and the render metrics (flushes, flushed pixels, a framebuffer hash).

## Determinism

Nothing that shapes the pixels reads the wall clock or sleeps:

- the colour/geometry inputs come from `theme.c`, `styles.c`, the fixtures and
  the mocks, all fixed;
- LVGL's timers and animations advance only through `virtual_time_advance()`
  (`lv_tick_inc` + `lv_timer_handler` in fixed slices);
- `get_time()` (the firmware's monotonic clock) is wired to the same counter.

`tests/check_harness_determinism.sh` builds the harness, runs the fixture twice
and compares the two PNGs by sha256 (it must print `PASS`).

## Coverage (home-usb-20m, milestone 1)

The scenario renders the **top bar** of the main screen, built from real
components exactly as `src/main_screen.c` places them:

| Component | Level | Why |
|---|---|---|
| `theme.c`, `styles.c`, `fonts/` | REAL | compiled and drawn |
| `clock.c` | REAL | battery display |
| `info.c` | REAL | VFO, mode, AGC, PRE/ATT, ATU, WiFi, BT |
| `meter.c` | REAL | needle S-meter |
| `band_info.c` | PARTIAL | compiled, not placed in this layout |
| `params`, `cfg`, `radio` | SYNTHETIC | harness-owned state; no DB, no device |
| `spectrum.c`, `waterfall.c`, `main_screen.c`, `buttons.cpp` | UNSUPPORTED | not rendered in milestone 1 |

`main_screen.c` is not linked yet: its visual components (spectrum, waterfall,
frequency strip, band plan, spots, buttons, panels) need the DSP/FFT and
hardware paths, which are the next increment. Declaring them UNSUPPORTED is
deliberate — coverage is never inflated.
