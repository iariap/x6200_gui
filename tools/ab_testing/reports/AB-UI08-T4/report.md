# A/B UI report — AB-UI08-T4

- Date: 2026-10-14
- Profile: `host-ui`
- A (baseline): `4c711f60b3789372f9e1c117e514418f0eee00c8` (a)
- B (candidate): `625f254b7b1ba359ebcd4ea0f7c29859ceaa082e` (b)
- Scenarios: `bench-render`, `dialog-about`, `home-bateria-externa`, `home-frecuencia-extrema`, `home-usb-20m`, `main-bateria-baja`, `main-espectro-agua`, `main-foco`, `main-natural`, `main-natural-eco`, `main-otro-tema`, `main-tema-classic`, `main-tema-holo`, `main-tema-material-3`, `main-tema-outdoor-negative`, `main-tema-rs`, `main-tema-vision`, `main-tema-vision-cwr`, `main-tema-vision-usbd`, `main-tx-simulado`

## Verdict: **PASS** (exit 0)

| Gate | Required | Status | Summary |
|---|---|---|---|
| Functional | yes | PASS | 20 scenarios captured on both revisions |
| Visual Regression | yes | PASS | captures are pixel-identical to the baseline |
| Quality Contract | yes | PASS | 200 contract checks passed on the candidate |
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
| main-natural | yes | 0 | 0.000% | 0 | — |
| main-natural-eco | yes | 0 | 0.000% | 0 | — |
| main-otro-tema | yes | 0 | 0.000% | 0 | — |
| main-tema-classic | yes | 0 | 0.000% | 0 | — |
| main-tema-holo | yes | 0 | 0.000% | 0 | — |
| main-tema-material-3 | yes | 0 | 0.000% | 0 | — |
| main-tema-outdoor-negative | yes | 0 | 0.000% | 0 | — |
| main-tema-rs | yes | 0 | 0.000% | 0 | — |
| main-tema-vision | yes | 0 | 0.000% | 0 | — |
| main-tema-vision-cwr | yes | 0 | 0.000% | 0 | — |
| main-tema-vision-usbd | yes | 0 | 0.000% | 0 | — |
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
| main-natural | 14 | 4 | 3 | 4 |
| main-natural-eco | 14 | 4 | 3 | 4 |
| main-otro-tema | 14 | 4 | 3 | 4 |
| main-tema-classic | 14 | 4 | 3 | 4 |
| main-tema-holo | 14 | 4 | 3 | 4 |
| main-tema-material-3 | 14 | 4 | 3 | 4 |
| main-tema-outdoor-negative | 14 | 4 | 3 | 4 |
| main-tema-rs | 14 | 4 | 3 | 4 |
| main-tema-vision | 14 | 4 | 3 | 4 |
| main-tema-vision-cwr | 14 | 4 | 3 | 4 |
| main-tema-vision-usbd | 14 | 4 | 3 | 4 |
| main-tx-simulado | 14 | 4 | 3 | 4 |

Components declared below REAL (never written as REAL):

- band_info.c=PARTIAL, cfg (catalogue)=SYNTHETIC, cw_tune_ui.c=PARTIAL, params=SYNTHETIC, radio=SYNTHETIC, spectrum.c=PARTIAL, waterfall.c=PARTIAL

## Known issues

| Issue | Fixture | Severity | Status | A | B | Classification |
|---|---|---|---|---|---|---|
| UI-001 | home-usb-20m | major | fixed | no | no | NOT_REPRODUCED |
| UI-009 | home-bateria-externa | major | fixed | no | no | NOT_REPRODUCED |
| UI-010 | main-otro-tema | critical | fixed | — | — | UNVERIFIED |
| UI-011 | main-tema-vision | major | fixed | — | — | UNVERIFIED |
| UI-012 | main-tema-vision | minor | fixed | — | — | UNVERIFIED |
| UI-013 | main-tema-outdoor-negative | major | fixed | — | — | UNVERIFIED |
| UI-014 | main-tema-vision-usbd | minor | fixed | — | — | UNVERIFIED |
| UI-002 | home-usb-20m | major | unverified | — | — | UNVERIFIED |
| UI-003 | home-usb-20m | major | unverified | — | — | UNVERIFIED |
| UI-004 | home-usb-20m | major | unverified | — | — | UNVERIFIED |
| UI-005 | home-usb-20m | minor | unverified | — | — | UNVERIFIED |
| UI-006 | home-usb-20m | minor | unverified | — | — | UNVERIFIED |
| UI-007 | main-otro-tema | major | unverified | — | — | UNVERIFIED |
| UI-008 | home-usb-20m | info | unverified | — | — | UNVERIFIED |

12 issue(s) are unverified: recorded from the repository but with no detector yet, so they neither block nor pass. They stay in the annex until the harness reproduces them.

## Quality contracts

200 contract checks, 0 failure(s), 0 unchanged baseline defect(s).

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
