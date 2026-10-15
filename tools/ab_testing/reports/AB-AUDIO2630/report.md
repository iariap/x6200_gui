# A/B UI report — AB-AUDIO2630

- Date: 2026-10-14
- Profile: `host-ui`
- A (baseline): `5f07ac689e4db1be40d9796df52376d64057fd14` (a)
- B (candidate): `54341c8e26d964c9bca1dedb2d7522f13ad0b6c7` (b)
- Scenarios: `app-bands`, `bench-render`, `home-bateria-externa`, `home-frecuencia-extrema`, `home-usb-20m`, `main-bateria-baja`, `main-espectro-agua`, `main-foco`, `main-natural`, `main-natural-eco`, `main-otro-tema`, `main-tema-classic`, `main-tema-holo`, `main-tema-material-3`, `main-tema-outdoor-negative`, `main-tema-rs`, `main-tema-vision`, `main-tema-vision-cwr`, `main-tema-vision-usbd`, `main-tx-simulado`, `settings-bands`, `settings-home`, `settings-perfil`, `settings-wfbat`

## Verdict: **PASS** (exit 0)

| Gate | Required | Status | Summary |
|---|---|---|---|
| Functional | yes | PASS | 24 scenarios captured on both revisions |
| Visual Regression | yes | PASS | captures are pixel-identical to the baseline |
| Quality Contract | yes | PASS | 249 contract checks passed on the candidate |
| Performance | yes | PASS | render-cost proxy within budget |
| Hardware | no (advisory) | BLOCKED | no physical X6200 attached; hardware behaviour is not measured |
| Energy | no (advisory) | BLOCKED | no power measurement; energy evidence must be physical |

## Visual diff

| Scenario | Identical | Changed px | Area | Boxes | Regions |
|---|---|---|---|---|---|
| app-bands | yes | 0 | 0.000% | 0 | — |
| bench-render | yes | 0 | 0.000% | 0 | — |
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
| settings-bands | yes | 0 | 0.000% | 0 | — |
| settings-home | yes | 0 | 0.000% | 0 | — |
| settings-perfil | yes | 0 | 0.000% | 0 | — |
| settings-wfbat | yes | 0 | 0.000% | 0 | — |

## Coverage (declared by the harness)

| Scenario | REAL | PARTIAL | SYNTHETIC | UNSUPPORTED |
|---|---|---|---|---|
| app-bands | 16 | 5 | 3 | 6 |
| bench-render | 15 | 4 | 3 | 4 |
| home-bateria-externa | 15 | 4 | 3 | 4 |
| home-frecuencia-extrema | 15 | 4 | 3 | 4 |
| home-usb-20m | 15 | 4 | 3 | 4 |
| main-bateria-baja | 15 | 4 | 3 | 4 |
| main-espectro-agua | 15 | 4 | 3 | 4 |
| main-foco | 15 | 4 | 3 | 4 |
| main-natural | 15 | 4 | 3 | 4 |
| main-natural-eco | 15 | 4 | 3 | 4 |
| main-otro-tema | 15 | 4 | 3 | 4 |
| main-tema-classic | 15 | 4 | 3 | 4 |
| main-tema-holo | 15 | 4 | 3 | 4 |
| main-tema-material-3 | 15 | 4 | 3 | 4 |
| main-tema-outdoor-negative | 15 | 4 | 3 | 4 |
| main-tema-rs | 15 | 4 | 3 | 4 |
| main-tema-vision | 15 | 4 | 3 | 4 |
| main-tema-vision-cwr | 15 | 4 | 3 | 4 |
| main-tema-vision-usbd | 15 | 4 | 3 | 4 |
| main-tx-simulado | 15 | 4 | 3 | 4 |
| settings-bands | 17 | 5 | 3 | 6 |
| settings-home | 17 | 4 | 10 | 5 |
| settings-perfil | 17 | 4 | 10 | 5 |
| settings-wfbat | 17 | 4 | 10 | 5 |

Components declared below REAL (never written as REAL):

- RTC / system clock (time())=SYNTHETIC, audio.c=SYNTHETIC, audio_enhancement.c=SYNTHETIC, backlight.c=SYNTHETIC, band_info.c=PARTIAL, bt_state.c=SYNTHETIC, cfg (catalogue)=SYNTHETIC, cfg/transverter.c=SYNTHETIC, cw_tune_ui.c=PARTIAL, main_screen.c (ACTION_APP_BANDS)=PARTIAL, params=SYNTHETIC, radio=SYNTHETIC, spectrum.c=PARTIAL, waterfall.c=PARTIAL, wfview_server.c=SYNTHETIC

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
| UI-015 | settings-wfbat | minor | verified | — | — | UNVERIFIED |
| UI-016 | settings-home | minor | fixed | — | — | UNVERIFIED |
| UI-002 | home-usb-20m | major | unverified | — | — | UNVERIFIED |
| UI-003 | home-usb-20m | major | unverified | — | — | UNVERIFIED |
| UI-004 | home-usb-20m | major | unverified | — | — | UNVERIFIED |
| UI-005 | home-usb-20m | minor | unverified | — | — | UNVERIFIED |
| UI-006 | home-usb-20m | minor | unverified | — | — | UNVERIFIED |
| UI-007 | main-otro-tema | major | unverified | — | — | UNVERIFIED |
| UI-008 | home-usb-20m | info | unverified | — | — | UNVERIFIED |

14 issue(s) are unverified: recorded from the repository but with no detector yet, so they neither block nor pass. They stay in the annex until the harness reproduces them.

## Quality contracts

249 contract checks, 0 failure(s), 0 unchanged baseline defect(s).

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
