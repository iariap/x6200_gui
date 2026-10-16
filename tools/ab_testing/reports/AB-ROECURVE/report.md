# A/B UI report — AB-ROECURVE

- Date: 2026-10-15
- Profile: `host-ui`
- A (baseline): `5e252dfb075c2c3d07c19806a5b4eb1043d8e147` (a)
- B (candidate): `38cae4ded61e866efa1a5befe71f6b2075577ba1` (b)
- Scenarios: `app-bands`, `app-close-home`, `apps-menu-1`, `apps-menu-2`, `apps-menu-3`, `apps-menu-4`, `apps-menu-5`, `apps-menu-6`, `bench-render`, `bt-dialog`, `bt-dialog-tema-aurora`, `bt-dialog-tema-cathode`, `bt-dialog-tema-redshift`, `bt-dialog-tema-sprint`, `bt-dialog-tema-sunlit`, `bt-dialog-tema-trail`, `home-bateria-externa`, `home-frecuencia-extrema`, `home-usb-20m`, `main-bateria-baja`, `main-espectro-agua`, `main-foco`, `main-frecuencia-dseg`, `main-natural`, `main-natural-eco`, `main-otro-tema`, `main-tema-aurora`, `main-tema-aurora-agua`, `main-tema-aurora-eco`, `main-tema-aurora-tx`, `main-tema-cathode`, `main-tema-cathode-agua`, `main-tema-cathode-eco`, `main-tema-cathode-tx`, `main-tema-classic`, `main-tema-holo`, `main-tema-material-3`, `main-tema-outdoor-negative`, `main-tema-redshift`, `main-tema-redshift-agua`, `main-tema-redshift-eco`, `main-tema-redshift-tx`, `main-tema-rs`, `main-tema-sprint`, `main-tema-sprint-agua`, `main-tema-sprint-eco`, `main-tema-sprint-tx`, `main-tema-sunlit`, `main-tema-sunlit-agua`, `main-tema-sunlit-eco`, `main-tema-sunlit-tx`, `main-tema-trail`, `main-tema-trail-agua`, `main-tema-trail-eco`, `main-tema-trail-tx`, `main-tema-vision`, `main-tema-vision-cwr`, `main-tema-vision-usbd`, `main-tx-simulado`, `settings-audio`, `settings-bands`, `settings-home`, `settings-perfil`, `settings-rs-theme`, `settings-tema-aurora`, `settings-tema-cathode`, `settings-tema-redshift`, `settings-tema-sprint`, `settings-tema-sunlit`, `settings-tema-trail`, `settings-wfbat`

## Verdict: **PASS** (exit 0)

| Gate | Required | Status | Summary |
|---|---|---|---|
| Functional | yes | PASS | 71 scenarios captured on both revisions |
| Visual Regression | yes | PASS | captures are pixel-identical to the baseline |
| Quality Contract | yes | PASS | 741 contract checks passed on the candidate |
| Performance | yes | PASS | render-cost proxy within budget |
| Hardware | no (advisory) | BLOCKED | no physical X6200 attached; hardware behaviour is not measured |
| Energy | no (advisory) | BLOCKED | no power measurement; energy evidence must be physical |

## Visual diff

| Scenario | Identical | Changed px | Area | Boxes | Regions |
|---|---|---|---|---|---|
| app-bands | yes | 0 | 0.000% | 0 | — |
| app-close-home | yes | 0 | 0.000% | 0 | — |
| apps-menu-1 | yes | 0 | 0.000% | 0 | — |
| apps-menu-2 | yes | 0 | 0.000% | 0 | — |
| apps-menu-3 | yes | 0 | 0.000% | 0 | — |
| apps-menu-4 | yes | 0 | 0.000% | 0 | — |
| apps-menu-5 | yes | 0 | 0.000% | 0 | — |
| apps-menu-6 | yes | 0 | 0.000% | 0 | — |
| bench-render | yes | 0 | 0.000% | 0 | — |
| bt-dialog | yes | 0 | 0.000% | 0 | — |
| bt-dialog-tema-aurora | yes | 0 | 0.000% | 0 | — |
| bt-dialog-tema-cathode | yes | 0 | 0.000% | 0 | — |
| bt-dialog-tema-redshift | yes | 0 | 0.000% | 0 | — |
| bt-dialog-tema-sprint | yes | 0 | 0.000% | 0 | — |
| bt-dialog-tema-sunlit | yes | 0 | 0.000% | 0 | — |
| bt-dialog-tema-trail | yes | 0 | 0.000% | 0 | — |
| home-bateria-externa | yes | 0 | 0.000% | 0 | — |
| home-frecuencia-extrema | yes | 0 | 0.000% | 0 | — |
| home-usb-20m | yes | 0 | 0.000% | 0 | — |
| main-bateria-baja | yes | 0 | 0.000% | 0 | — |
| main-espectro-agua | yes | 0 | 0.000% | 0 | — |
| main-foco | yes | 0 | 0.000% | 0 | — |
| main-frecuencia-dseg | yes | 0 | 0.000% | 0 | — |
| main-natural | yes | 0 | 0.000% | 0 | — |
| main-natural-eco | yes | 0 | 0.000% | 0 | — |
| main-otro-tema | yes | 0 | 0.000% | 0 | — |
| main-tema-aurora | yes | 0 | 0.000% | 0 | — |
| main-tema-aurora-agua | yes | 0 | 0.000% | 0 | — |
| main-tema-aurora-eco | yes | 0 | 0.000% | 0 | — |
| main-tema-aurora-tx | yes | 0 | 0.000% | 0 | — |
| main-tema-cathode | yes | 0 | 0.000% | 0 | — |
| main-tema-cathode-agua | yes | 0 | 0.000% | 0 | — |
| main-tema-cathode-eco | yes | 0 | 0.000% | 0 | — |
| main-tema-cathode-tx | yes | 0 | 0.000% | 0 | — |
| main-tema-classic | yes | 0 | 0.000% | 0 | — |
| main-tema-holo | yes | 0 | 0.000% | 0 | — |
| main-tema-material-3 | yes | 0 | 0.000% | 0 | — |
| main-tema-outdoor-negative | yes | 0 | 0.000% | 0 | — |
| main-tema-redshift | yes | 0 | 0.000% | 0 | — |
| main-tema-redshift-agua | yes | 0 | 0.000% | 0 | — |
| main-tema-redshift-eco | yes | 0 | 0.000% | 0 | — |
| main-tema-redshift-tx | yes | 0 | 0.000% | 0 | — |
| main-tema-rs | yes | 0 | 0.000% | 0 | — |
| main-tema-sprint | yes | 0 | 0.000% | 0 | — |
| main-tema-sprint-agua | yes | 0 | 0.000% | 0 | — |
| main-tema-sprint-eco | yes | 0 | 0.000% | 0 | — |
| main-tema-sprint-tx | yes | 0 | 0.000% | 0 | — |
| main-tema-sunlit | yes | 0 | 0.000% | 0 | — |
| main-tema-sunlit-agua | yes | 0 | 0.000% | 0 | — |
| main-tema-sunlit-eco | yes | 0 | 0.000% | 0 | — |
| main-tema-sunlit-tx | yes | 0 | 0.000% | 0 | — |
| main-tema-trail | yes | 0 | 0.000% | 0 | — |
| main-tema-trail-agua | yes | 0 | 0.000% | 0 | — |
| main-tema-trail-eco | yes | 0 | 0.000% | 0 | — |
| main-tema-trail-tx | yes | 0 | 0.000% | 0 | — |
| main-tema-vision | yes | 0 | 0.000% | 0 | — |
| main-tema-vision-cwr | yes | 0 | 0.000% | 0 | — |
| main-tema-vision-usbd | yes | 0 | 0.000% | 0 | — |
| main-tx-simulado | yes | 0 | 0.000% | 0 | — |
| settings-audio | yes | 0 | 0.000% | 0 | — |
| settings-bands | yes | 0 | 0.000% | 0 | — |
| settings-home | yes | 0 | 0.000% | 0 | — |
| settings-perfil | yes | 0 | 0.000% | 0 | — |
| settings-rs-theme | yes | 0 | 0.000% | 0 | — |
| settings-tema-aurora | yes | 0 | 0.000% | 0 | — |
| settings-tema-cathode | yes | 0 | 0.000% | 0 | — |
| settings-tema-redshift | yes | 0 | 0.000% | 0 | — |
| settings-tema-sprint | yes | 0 | 0.000% | 0 | — |
| settings-tema-sunlit | yes | 0 | 0.000% | 0 | — |
| settings-tema-trail | yes | 0 | 0.000% | 0 | — |
| settings-wfbat | yes | 0 | 0.000% | 0 | — |

## Coverage (declared by the harness)

| Scenario | REAL | PARTIAL | SYNTHETIC | UNSUPPORTED |
|---|---|---|---|---|
| app-bands | 16 | 5 | 3 | 5 |
| app-close-home | 17 | 5 | 4 | 3 |
| apps-menu-1 | 16 | 5 | 3 | 3 |
| apps-menu-2 | 16 | 5 | 3 | 3 |
| apps-menu-3 | 16 | 5 | 3 | 3 |
| apps-menu-4 | 16 | 5 | 3 | 3 |
| apps-menu-5 | 16 | 5 | 3 | 3 |
| apps-menu-6 | 16 | 5 | 3 | 3 |
| bench-render | 15 | 4 | 3 | 3 |
| bt-dialog | 19 | 4 | 4 | 4 |
| bt-dialog-tema-aurora | 19 | 4 | 4 | 4 |
| bt-dialog-tema-cathode | 19 | 4 | 4 | 4 |
| bt-dialog-tema-redshift | 19 | 4 | 4 | 4 |
| bt-dialog-tema-sprint | 19 | 4 | 4 | 4 |
| bt-dialog-tema-sunlit | 19 | 4 | 4 | 4 |
| bt-dialog-tema-trail | 19 | 4 | 4 | 4 |
| home-bateria-externa | 15 | 4 | 3 | 3 |
| home-frecuencia-extrema | 15 | 4 | 3 | 3 |
| home-usb-20m | 15 | 4 | 3 | 3 |
| main-bateria-baja | 15 | 4 | 3 | 3 |
| main-espectro-agua | 15 | 4 | 3 | 3 |
| main-foco | 15 | 4 | 3 | 3 |
| main-frecuencia-dseg | 15 | 4 | 3 | 3 |
| main-natural | 15 | 4 | 3 | 3 |
| main-natural-eco | 15 | 4 | 3 | 3 |
| main-otro-tema | 15 | 4 | 3 | 3 |
| main-tema-aurora | 15 | 4 | 3 | 3 |
| main-tema-aurora-agua | 15 | 4 | 3 | 3 |
| main-tema-aurora-eco | 15 | 4 | 3 | 3 |
| main-tema-aurora-tx | 15 | 4 | 3 | 3 |
| main-tema-cathode | 15 | 4 | 3 | 3 |
| main-tema-cathode-agua | 15 | 4 | 3 | 3 |
| main-tema-cathode-eco | 15 | 4 | 3 | 3 |
| main-tema-cathode-tx | 15 | 4 | 3 | 3 |
| main-tema-classic | 15 | 4 | 3 | 3 |
| main-tema-holo | 15 | 4 | 3 | 3 |
| main-tema-material-3 | 15 | 4 | 3 | 3 |
| main-tema-outdoor-negative | 15 | 4 | 3 | 3 |
| main-tema-redshift | 15 | 4 | 3 | 3 |
| main-tema-redshift-agua | 15 | 4 | 3 | 3 |
| main-tema-redshift-eco | 15 | 4 | 3 | 3 |
| main-tema-redshift-tx | 15 | 4 | 3 | 3 |
| main-tema-rs | 15 | 4 | 3 | 3 |
| main-tema-sprint | 15 | 4 | 3 | 3 |
| main-tema-sprint-agua | 15 | 4 | 3 | 3 |
| main-tema-sprint-eco | 15 | 4 | 3 | 3 |
| main-tema-sprint-tx | 15 | 4 | 3 | 3 |
| main-tema-sunlit | 15 | 4 | 3 | 3 |
| main-tema-sunlit-agua | 15 | 4 | 3 | 3 |
| main-tema-sunlit-eco | 15 | 4 | 3 | 3 |
| main-tema-sunlit-tx | 15 | 4 | 3 | 3 |
| main-tema-trail | 15 | 4 | 3 | 3 |
| main-tema-trail-agua | 15 | 4 | 3 | 3 |
| main-tema-trail-eco | 15 | 4 | 3 | 3 |
| main-tema-trail-tx | 15 | 4 | 3 | 3 |
| main-tema-vision | 15 | 4 | 3 | 3 |
| main-tema-vision-cwr | 15 | 4 | 3 | 3 |
| main-tema-vision-usbd | 15 | 4 | 3 | 3 |
| main-tx-simulado | 15 | 4 | 3 | 3 |
| settings-audio | 17 | 4 | 10 | 4 |
| settings-bands | 17 | 5 | 3 | 5 |
| settings-home | 17 | 4 | 10 | 4 |
| settings-perfil | 17 | 4 | 10 | 4 |
| settings-rs-theme | 17 | 4 | 10 | 4 |
| settings-tema-aurora | 17 | 4 | 10 | 4 |
| settings-tema-cathode | 17 | 4 | 10 | 4 |
| settings-tema-redshift | 17 | 4 | 10 | 4 |
| settings-tema-sprint | 17 | 4 | 10 | 4 |
| settings-tema-sunlit | 17 | 4 | 10 | 4 |
| settings-tema-trail | 17 | 4 | 10 | 4 |
| settings-wfbat | 17 | 4 | 10 | 4 |

Components declared below REAL (never written as REAL):

- RTC / system clock (time())=SYNTHETIC, audio.c=SYNTHETIC, audio_enhancement.c=SYNTHETIC, backlight.c=SYNTHETIC, band_info.c=PARTIAL, bt_state.c=SYNTHETIC, btaudio.c=SYNTHETIC, cfg (catalogue)=SYNTHETIC, cfg/transverter.c=SYNTHETIC, cw_tune_ui.c=PARTIAL, main_screen.c (ACTION_APP_BANDS)=PARTIAL, main_screen.c (ACTION_APP_RTTY close)=PARTIAL, main_screen.c (APP key -> buttons_group_app)=PARTIAL, params=SYNTHETIC, radio=SYNTHETIC, rtty.c=SYNTHETIC, spectrum.c=PARTIAL, waterfall.c=PARTIAL, wfview_server.c=SYNTHETIC

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
| UI-017 | main-frecuencia-dseg | minor | verified | — | — | UNVERIFIED |
| UI-002 | apps-menu-1 | major | fixed | — | — | UNVERIFIED |
| UI-003 | home-usb-20m | major | unverified | — | — | UNVERIFIED |
| UI-004 | settings-home | major | fixed | — | — | UNVERIFIED |
| UI-005 | bt-dialog | minor | fixed | — | — | UNVERIFIED |
| UI-006 | home-usb-20m | minor | unverified | — | — | UNVERIFIED |
| UI-007 | settings-rs-theme | major | fixed | — | — | UNVERIFIED |
| UI-008 | home-usb-20m | info | out-of-scope | — | — | UNVERIFIED |

15 issue(s) are unverified: recorded from the repository but with no detector yet, so they neither block nor pass. They stay in the annex until the harness reproduces them.

## Quality contracts

741 contract checks, 0 failure(s), 0 unchanged baseline defect(s).

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
