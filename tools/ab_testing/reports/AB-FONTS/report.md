# A/B UI report — AB-FONTS

- Date: 2026-10-15
- Profile: `host-ui`
- A (baseline): `02c13119039da34c9a888e200d61948602b5a9ac` (a)
- B (candidate): `b9fc0812bc8d4effb6939311bf15668e9d3a32ad` (b)
- Scenarios: `app-bands`, `app-close-home`, `apps-menu-1`, `apps-menu-2`, `apps-menu-3`, `apps-menu-4`, `apps-menu-5`, `apps-menu-6`, `bench-render`, `bt-dialog`, `home-bateria-externa`, `home-frecuencia-extrema`, `home-usb-20m`, `main-bateria-baja`, `main-espectro-agua`, `main-foco`, `main-natural`, `main-natural-eco`, `main-otro-tema`, `main-tema-classic`, `main-tema-holo`, `main-tema-material-3`, `main-tema-outdoor-negative`, `main-tema-rs`, `main-tema-vision`, `main-tema-vision-cwr`, `main-tema-vision-usbd`, `main-tx-simulado`, `settings-audio`, `settings-bands`, `settings-home`, `settings-perfil`, `settings-rs-theme`, `settings-wfbat`

## Verdict: **FAIL** (exit 1)

| Gate | Required | Status | Summary |
|---|---|---|---|
| Functional | yes | PASS | 34 scenarios captured on both revisions |
| Visual Regression | yes | FAIL | visual regressions vs the baseline |
| Quality Contract | yes | PASS | 362 contract checks passed on the candidate |
| Performance | yes | PASS | render-cost proxy within budget |
| Hardware | no (advisory) | BLOCKED | no physical X6200 attached; hardware behaviour is not measured |
| Energy | no (advisory) | BLOCKED | no power measurement; energy evidence must be physical |

## Visual diff

| Scenario | Identical | Changed px | Area | Boxes | Regions |
|---|---|---|---|---|---|
| app-bands | no | 9270 | 2.414% | 67 | barra_superior, s_metro, espectro, frecuencia, waterfall, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_audio_ganancia |
| app-close-home | no | 15398 | 4.010% | 104 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_audio_ganancia |
| apps-menu-1 | no | 12274 | 3.196% | 82 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_audio_ganancia |
| apps-menu-2 | no | 13225 | 3.444% | 81 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_audio_ganancia |
| apps-menu-3 | no | 13566 | 3.533% | 86 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_audio_ganancia |
| apps-menu-4 | no | 13272 | 3.456% | 89 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_audio_ganancia |
| apps-menu-5 | no | 12708 | 3.309% | 83 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_audio_ganancia |
| apps-menu-6 | no | 12274 | 3.196% | 86 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_audio_ganancia |
| bench-render | no | 15452 | 4.024% | 105 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_audio_ganancia |
| bt-dialog | no | 15671 | 4.081% | 109 | barra_superior, s_metro, espectro, waterfall, botones, reloj_potencia, ajustes_anio, ajustes_scrollbar, ajustes_audio_ganancia |
| home-bateria-externa | no | 15512 | 4.040% | 104 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_audio_ganancia |
| home-frecuencia-extrema | no | 14710 | 3.831% | 99 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_perfil, ajustes_audio_ganancia |
| home-usb-20m | no | 15398 | 4.010% | 104 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_audio_ganancia |
| main-bateria-baja | no | 14394 | 3.748% | 92 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_perfil, ajustes_audio_ganancia |
| main-espectro-agua | no | 15452 | 4.024% | 105 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_audio_ganancia |
| main-foco | no | 15398 | 4.010% | 104 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_audio_ganancia |
| main-natural | no | 15398 | 4.010% | 104 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_audio_ganancia |
| main-natural-eco | no | 15107 | 3.934% | 98 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_audio_ganancia |
| main-otro-tema | no | 14143 | 3.683% | 96 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_perfil, ajustes_audio_ganancia |
| main-tema-classic | no | 15718 | 4.093% | 103 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_audio_ganancia |
| main-tema-holo | no | 16589 | 4.320% | 105 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_audio_ganancia |
| main-tema-material-3 | no | 16579 | 4.317% | 107 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_audio_ganancia |
| main-tema-outdoor-negative | no | 15392 | 4.008% | 104 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_audio_ganancia |
| main-tema-rs | no | 15715 | 4.092% | 103 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_audio_ganancia |
| main-tema-vision | no | 21390 | 5.570% | 109 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_audio_ganancia |
| main-tema-vision-cwr | no | 21410 | 5.576% | 109 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_audio_ganancia |
| main-tema-vision-usbd | no | 21506 | 5.601% | 109 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_audio_ganancia |
| main-tx-simulado | no | 17891 | 4.659% | 122 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_audio_ganancia |
| settings-audio | no | 25625 | 6.673% | 208 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_audio_ganancia |
| settings-bands | no | 9270 | 2.414% | 67 | barra_superior, s_metro, espectro, frecuencia, waterfall, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_audio_ganancia |
| settings-home | no | 30217 | 7.869% | 182 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_audio_ganancia |
| settings-perfil | no | 25658 | 6.682% | 183 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_audio_ganancia |
| settings-rs-theme | no | 39477 | 10.280% | 188 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_audio_ganancia |
| settings-wfbat | no | 27517 | 7.166% | 170 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_audio_ganancia |

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

362 contract checks, 0 failure(s), 0 unchanged baseline defect(s).

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
