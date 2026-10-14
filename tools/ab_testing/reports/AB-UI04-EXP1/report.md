# A/B UI report — AB-UI04-EXP1

- Date: 2026-10-14
- Profile: `host-ui`
- A (baseline): `b02990799fd9a974566f2520b361d2c4214632e2` (a)
- B (candidate): `c471ecb6760e5ddb4297f287562a4633fed6fd3f` (b)
- Scenarios: `bench-render`, `dialog-about`, `home-bateria-externa`, `home-frecuencia-extrema`, `home-usb-20m`, `main-bateria-baja`, `main-espectro-agua`, `main-foco`, `main-otro-tema`, `main-tema-classic`, `main-tema-holo`, `main-tema-material-3`, `main-tema-outdoor-negative`, `main-tema-rs`, `main-tema-vision`, `main-tx-simulado`

## Verdict: **PASS** (exit 0)

| Gate | Required | Status | Summary |
|---|---|---|---|
| Functional | yes | PASS | 16 scenarios captured on both revisions |
| Visual Regression | yes | PASS | no unexplained visual regressions |
| Quality Contract | yes | PASS | 160 contract checks passed; 1 unchanged baseline defect(s) recorded, not blocking |
| Performance | yes | PASS | render-cost proxy within budget |
| Hardware | no (advisory) | BLOCKED | no physical X6200 attached; hardware behaviour is not measured |
| Energy | no (advisory) | BLOCKED | no power measurement; energy evidence must be physical |

## Visual diff

| Scenario | Identical | Changed px | Area | Boxes | Regions |
|---|---|---|---|---|---|
| bench-render | yes | 0 | 0.000% | 0 | — |
| dialog-about | yes | 0 | 0.000% | 0 | — |
| home-bateria-externa | yes | 0 | 0.000% | 0 | — |
| home-frecuencia-extrema | yes | 0 | 0.000% | 0 | — |
| home-usb-20m | yes | 0 | 0.000% | 0 | — |
| main-bateria-baja | yes | 0 | 0.000% | 0 | — |
| main-espectro-agua | yes | 0 | 0.000% | 0 | — |
| main-foco | yes | 0 | 0.000% | 0 | — |
| main-otro-tema | yes | 0 | 0.000% | 0 | — |
| main-tema-classic | yes | 0 | 0.000% | 0 | — |
| main-tema-holo | yes | 0 | 0.000% | 0 | — |
| main-tema-material-3 | yes | 0 | 0.000% | 0 | — |
| main-tema-outdoor-negative | yes | 0 | 0.000% | 0 | — |
| main-tema-rs | yes | 0 | 0.000% | 0 | — |
| main-tema-vision | no | 1943 | 0.506% | 28 | barra_superior, espectro, reloj_potencia |
| main-tx-simulado | yes | 0 | 0.000% | 0 | — |

## Coverage (declared by the harness)

| Scenario | REAL | PARTIAL | SYNTHETIC | UNSUPPORTED |
|---|---|---|---|---|
| bench-render | 14 | 4 | 3 | 4 |
| dialog-about | 16 | 4 | 3 | 4 |
| home-bateria-externa | 14 | 4 | 3 | 4 |
| home-frecuencia-extrema | 14 | 4 | 3 | 4 |
| home-usb-20m | 14 | 4 | 3 | 4 |
| main-bateria-baja | 14 | 4 | 3 | 4 |
| main-espectro-agua | 14 | 4 | 3 | 4 |
| main-foco | 14 | 4 | 3 | 4 |
| main-otro-tema | 14 | 4 | 3 | 4 |
| main-tema-classic | 14 | 4 | 3 | 4 |
| main-tema-holo | 14 | 4 | 3 | 4 |
| main-tema-material-3 | 14 | 4 | 3 | 4 |
| main-tema-outdoor-negative | 14 | 4 | 3 | 4 |
| main-tema-rs | 14 | 4 | 3 | 4 |
| main-tema-vision | 14 | 4 | 3 | 4 |
| main-tx-simulado | 14 | 4 | 3 | 4 |

Components declared below REAL (never written as REAL):

- band_info.c=PARTIAL, cfg (catalogue)=SYNTHETIC, cw_tune_ui.c=PARTIAL, params=SYNTHETIC, radio=SYNTHETIC, spectrum.c=PARTIAL, waterfall.c=PARTIAL

## Known issues

| Issue | Fixture | Severity | Status | A | B | Classification |
|---|---|---|---|---|---|---|
| UI-001 | home-usb-20m | major | fixed | no | no | NOT_REPRODUCED |
| UI-009 | home-bateria-externa | major | fixed | no | no | NOT_REPRODUCED |
| UI-010 | main-otro-tema | critical | verified | — | — | UNVERIFIED |
| UI-011 | main-tema-vision | major | fixed | — | — | UNVERIFIED |
| UI-012 | main-tema-vision | minor | verified | — | — | UNVERIFIED |
| UI-013 | main-tema-outdoor-negative | major | verified | — | — | UNVERIFIED |
| UI-002 | home-usb-20m | major | unverified | — | — | UNVERIFIED |
| UI-003 | home-usb-20m | major | unverified | — | — | UNVERIFIED |
| UI-004 | home-usb-20m | major | unverified | — | — | UNVERIFIED |
| UI-005 | home-usb-20m | minor | unverified | — | — | UNVERIFIED |
| UI-006 | home-usb-20m | minor | unverified | — | — | UNVERIFIED |
| UI-007 | main-otro-tema | major | unverified | — | — | UNVERIFIED |
| UI-008 | home-usb-20m | info | unverified | — | — | UNVERIFIED |

11 issue(s) are unverified: recorded from the repository but with no detector yet, so they neither block nor pass. They stay in the annex until the harness reproduces them.

## Quality contracts

161 contract checks, 0 failure(s), 1 unchanged baseline defect(s).
- **UNCHANGED DEFECT** `region_min_content:frecuencia` [main-otro-tema] (UI-010): content 0.0000 >= 0.0200 — unchanged defect UI-010: Tema Outdoor: franja de frecuencia invisible sobre fondo blanco

## Evidence

- Screenshots: `screenshots/`
- Diffs: `diffs/`
- Logs: `logs/`
- Docker image: `x6200-uiharness`

### Gates without instrumentation (BLOCKED, never PASS)

- **Hardware**: no physical X6200 attached; hardware behaviour is not measured
  - cause: the framework runs on the host and never in the ROM
  - needed: an X6200 unit with the capture/instrumentation flow of milestone 7
- **Energy**: no power measurement; energy evidence must be physical
  - cause: render counters are not joules
  - needed: physical energy measurement (milestone 7)
