# `x6200-ab` — A/B testing framework for the X6200 UI

Host-side Python framework that drives the C UI harness (`tools/ui_harness`),
compares two git revisions pixel by pixel, runs objective quality contracts and
known-issue detectors, and turns all of it into a single verdict with evidence.
It runs on the PC and **never ships in the firmware or the rootfs**.

Spec: [`docs/AB_TESTING_PLAN.md`](../../docs/AB_TESTING_PLAN.md) (milestones 2–3)
· Bitácora: [`docs/AB_TESTING.md`](../../docs/AB_TESTING.md).

## What it does

```text
git revisions ─▶ isolated worktrees ─▶ Docker build ─▶ harness render ─▶ PNG + JSON
      │                                                                     │
      └──────────────────▶ pixel diff ▶ contracts ▶ known issues ◀──────────┘
                                          │
                                          ▼
                        gates: Functional · Visual Regression · Quality Contract
                               Performance · Hardware · Energy
                                          │
                                          ▼
                       PASS / FAIL / BLOCKED / INCONCLUSIVE / REVIEW REQUIRED
```

Phases are separate modules, not one script:

| Module | What it does |
|---|---|
| `revisions.py` | one git worktree per exact SHA, `lvgl` submodule pinned by gitlink, never touches the user's worktrees |
| `builds.py` | compiles the harness in the `x6200-uiharness` image; a revision that does not build is BLOCKED, never a silently reused binary |
| `scenarios.py` | parses the `.scn` fixtures and the suite |
| `capture.py` | runs the harness per scenario → PNG + JSON (coverage, metrics, revision, optional structural inspection) |
| `inspection.py` | parses the harness' structural inspection (object tree) into a walkable model |
| `visual_diff.py` | Pillow/NumPy pixel diff: area, bounding boxes, per-region, `diff.png`, `overlay.png`, dimension mismatch |
| `quality_config.py` | contracts, budgets, layout/typography rules and dated expected changes/exceptions |
| `contracts.py` | objective checks on the candidate alone (C of the plan): pixel regions, coverage, layout, typography |
| `layout_checks.py` | geometry rules over the object tree: off-screen, clipped, degenerate boxes, stacked labels, touch targets. Scroll areas (`scroll_dir`) and opaque surfaces painted above a label are respected: content reachable by scrolling is not "off screen" and a label a surface hides is not a stacked text |
| `typography_checks.py` | text-legibility rules: overflow, truncation, missing glyphs, WCAG contrast |
| `theme_checks.py` | the theme resolution rule: the theme a capture rendered must fit `params.theme_name` (UI-013) |
| `theme_files.py` | static WCAG pairs over the theme JSON files the ROM ships, `base` inheritance included |
| `contrast.py` | WCAG 2.1 luminance and contrast ratio, shared by both contrast families |
| `known_issues.py` | detectors and the FIXED / UNCHANGED DEFECT / REGRESSION / NEW DEFECT classification |
| `quality_gates.py` | the six gates and the profile that decides which are required |
| `performance.py` | render-benchmark statistics (p50/p95/p99, FPS, dispersion) and the relative regression verdict |
| `evaluator.py` | combines the gates into one verdict |
| `reports.py` | `reports/<experiment-id>/` with MD, HTML, JSON, screenshots, diffs, metrics, logs |
| `cli.py` | `capture` / `audit` / `contracts` / `themes` / `compare` / `benchmark` / `report` / `evaluate` |

## Setup

```sh
cd tools/ab_testing
uv sync                       # Pillow, NumPy, PyYAML (+ pytest, ruff in the dev group)
```

The harness image must exist (see `tools/ui_harness/README.md`):

```sh
docker build -t x6200-uiharness tools/ui_harness
```

## Usage

```sh
# render every fixture of one revision
uv run x6200-ab capture --revision <sha> --label base --out /tmp/captures

# compare two revisions and write a report under tools/ab_testing/reports/
uv run x6200-ab compare --a <sha> --b <branch> --experiment-id AB-0001

# known-issue detectors on one revision (pixel-region detectors + the structural
# inspection of the live object tree; a structural finding that is not waived
# fails the audit)
uv run x6200-ab audit --revision <sha>          # aliases: --ref <sha> --suite visual
uv run x6200-ab audit --revision <sha> --scenario home-usb-20m

# render-cost repetitions: real p50/p95/p99, FPS, flushes, area, RSS/PSS
uv run x6200-ab benchmark --revision <sha> --repeats 5
# relative render-cost regression gate (FAIL on p95 over budget), A vs B
uv run x6200-ab benchmark --a <sha> --b <branch> --experiment-id BENCH-0001

# objective quality contracts on the candidate alone (no baseline)
uv run x6200-ab contracts --revision <sha>

# static contrast over every theme the ROM serves (offline: no capture, no
# Docker, no revision) — the built-in palettes of src/theme.h + the shipped
# files, on the Settings pairs the rendered checks cannot reach. Themes the
# check cannot resolve (the runtime-built ones) are listed UNSUPPORTED with a
# reason instead of silently missing from the coverage.
uv run x6200-ab themes

# read back a verdict, and recheck it from results.json
uv run x6200-ab report --experiment-id AB-0001
uv run x6200-ab evaluate --results tools/ab_testing/reports/AB-0001/results.json
```

Exit codes (also in `--json` output): `0` PASS, `1` FAIL, `2` BLOCKED,
`3` INCONCLUSIVE, `4` REVIEW REQUIRED.

## Verdicts

* **PASS** — every *required* gate passed. Pixel-identical A/B is a PASS, and a
  change that only removes a known issue is a PASS marked *IMPROVEMENT*.
* **FAIL** — a required gate failed (an unexplained visual change, a new defect,
  a contract violation, a render-cost regression).
* **BLOCKED** — the run could not produce its evidence (a revision that does not
  build, no Docker image, the harness rejected a fixture).
* **INCONCLUSIVE** — the evidence is incomplete (e.g. no scenario captured on
  both sides).
* **REVIEW REQUIRED** — a human decision is needed.

`Hardware` and `Energy` are always **BLOCKED** on the host (no radio, no power
meter) and never PASS by default. They are advisory: they are reported with
their cause, but they do not turn a host-only PASS into a BLOCKED. The plan
requires exactly this (rule 7: without a physical measurement, energy/hardware
stay BLOCKED).

## Quality files (`quality/`)

* `regions.yaml` — semantic regions of the 800×480 screen (top bar, S-meter,
  spectrum, frequency, waterfall, buttons, multiplier, power clock).
* `contracts.yaml` — rules checked on the candidate alone: minimum content per
  region, honest coverage levels, required declared components, the dated
  `expected_changes` list (the only way a change stops being a regression
  without being a fix; each entry needs a reason, an author and an expiry), the
  `layout` and `typography` rules that read the harness' structural inspection
  (minimum touch target, WCAG contrast thresholds, whether fallbacks are
  allowed) and the dated `exceptions` that can waive one structural finding by
  rule and fixture without hiding it.
* `budgets.yaml` — thresholds fixed **before** a run (pixel tolerance, minimum
  box size, render-cost deltas).
* `known_issues.yaml` — recorded defects. An issue without a detector is
  `unverified`: it never fails and never passes. A detector is either
  `region_content_above` (a pixel region must stay near-uniform) or
  `leading_glyph_range` (the label whose text matches `text_pattern` — the power
  clock is the only one with a voltage line — must start with a code point in
  `glyph_range`; the text comes from the structural inspection, never OCR). A
  `verified` issue whose detector is a contract rule (UI-010, UI-011, UI-012,
  UI-013) carries no `detector:` block and names the rule in its note; UI-010 is
  kept out of the gate by a scoped `exclusions` entry, the structural ones by the
  scoped `exceptions` of `contracts.yaml`.
* `theme_pairs.yaml` — the pairs the **static** theme contract checks over every
  theme the ROM serves (`x6200-ab themes`): the four built-in palettes of
  `src/theme.h` and the shipped files of `rootfs/usr/share/x6200/themes`, through
  each file's `base`. The pairs are the ones that only exist on screens the
  harness cannot open yet (Settings), with the code that draws each one. Themes
  the check cannot resolve (the runtime-built Outdoor-Negative and VISION) are
  declared `unsupported` here with their reason.

## Structural checks

`audit`, `contracts` and the `compare` candidate capture with `ui_harness
--inspect`, so they read the real object tree instead of a picture of it (see
`tools/ui_harness/README.md`). The checks are objective and need no OCR:

* **layout** — an element that paints something completely off the viewport, a
  box cut by the viewport or a clipping parent, a degenerate (zero/negative) box
  that paints, two text labels stacked on each other, an interactive control
  below the minimum touch target;
* **typography** — text that overflows or is truncated in its label, a glyph
  missing from every font in the chain (a placeholder box on screen) and a
  text/background pair below the WCAG 2.1 contrast ratio when the label paints an
  opaque background;
* **theme** — the theme the capture rendered must fit `params.theme_name`, the
  16-byte parameter the firmware stores the choice in: a name that does not
  survive the round trip makes `theme_init()` fall back to IU1WXY.

A failing finding is turned into a recorded waiver (still visible, marked `ok`)
only when a dated, fixture-scoped `exceptions` entry matches it.

The same WCAG formula runs **statically** over every theme the image serves
(`uv run x6200-ab themes`, `quality/theme_pairs.yaml`): with no render and no
Docker, it resolves each file's `base` against the palettes of `src/theme.h`,
measures the built-in palettes too, and reports the pairs that only exist on the
Settings window, which the harness cannot open yet. Themes it cannot resolve
(runtime-built) and surfaces a theme paints with a binary image are declared, not
silently dropped. It is the complementary detector UI-007 lacks; the exhaustive
sweep over the nine palettes stays in `tests/check_wcag.py`.

## Known issues

An issue is only trusted when the harness reproduces it. With a detector, the
A→B comparison classifies it:

| A | B | Classification |
|---|---|---|
| reproduced | absent | **FIXED** (an improvement) |
| reproduced | reproduced | **UNCHANGED DEFECT** |
| absent | reproduced (status `fixed`) | **REGRESSION** |
| absent | reproduced (never seen) | **NEW DEFECT** |
| absent | absent | not reproduced |

Exclusions are scoped by fixture and region with a reason and an expiry date —
never global, never permanent.

## Tests

```sh
uv run ruff check .
uv run pytest                 # unit tests (no Docker)
uv run pytest -m e2e          # end-to-end: builds the real harness with Docker
```

The e2e suite reproduces the milestone gate with ephemeral revisions:
A vs A → PASS (zero diff), A vs a deliberate regression → FAIL with `diff.png`
and a bounding box, and a defective revision vs its fix → IMPROVEMENT (PASS).

`AB-e2e-ui009` is the first real firmware fix evaluated end to end: A is the
`home-bateria-externa` fixture with `bat_sym` uninitialized (`src/clock.c`,
garbage `EHE` glyphs) and B is the fix (a battery icon); the comparison is an
IMPROVEMENT with UI-009 `FIXED` (see `docs/UI_KNOWN_ISSUES.md`).
