# A/B UI report — AB-FONTSE-KERN

- Date: 2026-10-15
- Profile: `host-ui`
- A (baseline): `562d3f212002a5379f448a1ee8eb9a16ffdd8f06` (a)
- B (candidate): `4ec2467c978c67a3e8f7c911537ea39652de1367` (b)
- Scenarios: `app-bands`, `app-close-home`, `apps-menu-1`, `apps-menu-2`, `apps-menu-3`, `apps-menu-4`, `apps-menu-5`, `apps-menu-6`, `bench-render`, `bt-dialog`, `home-bateria-externa`, `home-frecuencia-extrema`, `home-usb-20m`, `main-bateria-baja`, `main-espectro-agua`, `main-foco`, `main-frecuencia-dseg`, `main-natural`, `main-natural-eco`, `main-otro-tema`, `main-tema-classic`, `main-tema-holo`, `main-tema-material-3`, `main-tema-outdoor-negative`, `main-tema-rs`, `main-tema-vision`, `main-tema-vision-cwr`, `main-tema-vision-usbd`, `main-tx-simulado`, `settings-audio`, `settings-bands`, `settings-home`, `settings-perfil`, `settings-rs-theme`, `settings-wfbat`

## Verdict: **PASS** (exit 0)

| Gate | Required | Status | Summary |
|---|---|---|---|
| Functional | yes | PASS | 35 scenarios captured on both revisions |
| Visual Regression | yes | PASS | no unexplained visual regressions |
| Quality Contract | yes | PASS | 373 contract checks passed on the candidate |
| Performance | yes | PASS | render-cost proxy within budget |
| Hardware | no (advisory) | BLOCKED | no physical X6200 attached; hardware behaviour is not measured |
| Energy | no (advisory) | BLOCKED | no power measurement; energy evidence must be physical |

## Visual diff

| Scenario | Identical | Changed px | Area | Boxes | Regions |
|---|---|---|---|---|---|
| app-bands | no | 1954 | 0.509% | 30 | barra_superior, s_metro, espectro, frecuencia, reloj_potencia, ajustes_audio_ganancia |
| app-close-home | no | 1954 | 0.509% | 30 | barra_superior, s_metro, espectro, frecuencia, reloj_potencia, ajustes_audio_ganancia |
| apps-menu-1 | no | 1954 | 0.509% | 30 | barra_superior, s_metro, espectro, frecuencia, reloj_potencia, ajustes_audio_ganancia |
| apps-menu-2 | no | 2656 | 0.692% | 37 | barra_superior, s_metro, espectro, frecuencia, botones, reloj_potencia, ajustes_audio_ganancia |
| apps-menu-3 | no | 2624 | 0.683% | 39 | barra_superior, s_metro, espectro, frecuencia, botones, reloj_potencia, ajustes_audio_ganancia |
| apps-menu-4 | no | 2181 | 0.568% | 33 | barra_superior, s_metro, espectro, frecuencia, botones, reloj_potencia, ajustes_audio_ganancia |
| apps-menu-5 | no | 2133 | 0.555% | 33 | barra_superior, s_metro, espectro, frecuencia, botones, reloj_potencia, ajustes_audio_ganancia |
| apps-menu-6 | no | 2568 | 0.669% | 37 | barra_superior, s_metro, espectro, frecuencia, botones, reloj_potencia, ajustes_audio_ganancia |
| bench-render | no | 1541 | 0.401% | 21 | barra_superior, s_metro, espectro, frecuencia, ajustes_audio_ganancia |
| bt-dialog | no | 1348 | 0.351% | 21 | barra_superior, s_metro, espectro, waterfall, botones, reloj_potencia |
| home-bateria-externa | no | 1954 | 0.509% | 30 | barra_superior, s_metro, espectro, frecuencia, reloj_potencia, ajustes_audio_ganancia |
| home-frecuencia-extrema | no | 2229 | 0.580% | 43 | barra_superior, s_metro, espectro, frecuencia, reloj_potencia, ajustes_audio_ganancia |
| home-usb-20m | no | 1954 | 0.509% | 30 | barra_superior, s_metro, espectro, frecuencia, reloj_potencia, ajustes_audio_ganancia |
| main-bateria-baja | no | 2244 | 0.584% | 21 | barra_superior, s_metro, espectro, frecuencia, ajustes_audio_ganancia |
| main-espectro-agua | no | 1541 | 0.401% | 21 | barra_superior, s_metro, espectro, frecuencia, ajustes_audio_ganancia |
| main-foco | no | 1954 | 0.509% | 30 | barra_superior, s_metro, espectro, frecuencia, reloj_potencia, ajustes_audio_ganancia |
| main-frecuencia-dseg | no | 418 | 0.109% | 9 | barra_superior, s_metro, espectro, ajustes_audio_ganancia |
| main-natural | no | 1954 | 0.509% | 30 | barra_superior, s_metro, espectro, frecuencia, reloj_potencia, ajustes_audio_ganancia |
| main-natural-eco | no | 1775 | 0.462% | 27 | barra_superior, s_metro, espectro, frecuencia, reloj_potencia, ajustes_audio_ganancia |
| main-otro-tema | no | 2201 | 0.573% | 19 | barra_superior, s_metro, espectro, frecuencia, ajustes_audio_ganancia |
| main-tema-classic | yes | 0 | 0.000% | 0 | — |
| main-tema-holo | yes | 0 | 0.000% | 0 | — |
| main-tema-material-3 | yes | 0 | 0.000% | 0 | — |
| main-tema-outdoor-negative | no | 1953 | 0.509% | 30 | barra_superior, s_metro, espectro, frecuencia, reloj_potencia, ajustes_audio_ganancia |
| main-tema-rs | yes | 0 | 0.000% | 0 | — |
| main-tema-vision | no | 2981 | 0.776% | 58 | barra_superior, s_metro, espectro, frecuencia, botones, reloj_potencia, ajustes_audio_ganancia |
| main-tema-vision-cwr | no | 2981 | 0.776% | 58 | barra_superior, s_metro, espectro, frecuencia, botones, reloj_potencia, ajustes_audio_ganancia |
| main-tema-vision-usbd | no | 2981 | 0.776% | 58 | barra_superior, s_metro, espectro, frecuencia, botones, reloj_potencia, ajustes_audio_ganancia |
| main-tx-simulado | no | 3012 | 0.784% | 35 | barra_superior, s_metro, espectro, frecuencia, ajustes_audio_ganancia |
| settings-audio | no | 3492 | 0.909% | 79 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_audio_ganancia |
| settings-bands | no | 1954 | 0.509% | 30 | barra_superior, s_metro, espectro, frecuencia, reloj_potencia, ajustes_audio_ganancia |
| settings-home | no | 8245 | 2.147% | 113 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_audio_ganancia |
| settings-perfil | no | 5034 | 1.311% | 92 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_perfil, ajustes_audio_ganancia |
| settings-rs-theme | yes | 0 | 0.000% | 0 | — |
| settings-wfbat | no | 11667 | 3.038% | 129 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_perfil, ajustes_audio_ganancia |

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
| main-tema-classic | 15 | 4 | 3 | 3 |
| main-tema-holo | 15 | 4 | 3 | 3 |
| main-tema-material-3 | 15 | 4 | 3 | 3 |
| main-tema-outdoor-negative | 15 | 4 | 3 | 3 |
| main-tema-rs | 15 | 4 | 3 | 3 |
| main-tema-vision | 15 | 4 | 3 | 3 |
| main-tema-vision-cwr | 15 | 4 | 3 | 3 |
| main-tema-vision-usbd | 15 | 4 | 3 | 3 |
| main-tx-simulado | 15 | 4 | 3 | 3 |
| settings-audio | 17 | 4 | 10 | 4 |
| settings-bands | 17 | 5 | 3 | 5 |
| settings-home | 17 | 4 | 10 | 4 |
| settings-perfil | 17 | 4 | 10 | 4 |
| settings-rs-theme | 17 | 4 | 10 | 4 |
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
| UI-002 | apps-menu-1 | major | fixed | — | — | UNVERIFIED |
| UI-003 | home-usb-20m | major | unverified | — | — | UNVERIFIED |
| UI-004 | settings-home | major | fixed | — | — | UNVERIFIED |
| UI-005 | bt-dialog | minor | fixed | — | — | UNVERIFIED |
| UI-006 | home-usb-20m | minor | unverified | — | — | UNVERIFIED |
| UI-007 | settings-rs-theme | major | fixed | — | — | UNVERIFIED |
| UI-008 | home-usb-20m | info | out-of-scope | — | — | UNVERIFIED |

14 issue(s) are unverified: recorded from the repository but with no detector yet, so they neither block nor pass. They stay in the annex until the harness reproduces them.

## Quality contracts

373 contract checks, 0 failure(s), 0 unchanged baseline defect(s).

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
