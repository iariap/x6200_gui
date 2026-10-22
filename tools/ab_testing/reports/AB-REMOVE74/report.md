# A/B UI report — AB-REMOVE74

- Date: 2026-10-15
- Profile: `host-ui`
- A (baseline): `ed8d35a20035bbaecae4a15acda5d7c8309e890e` (a)
- B (candidate): `1cb587d849b0967e2775480bc31f678c3283b541` (b)
- Scenarios: `app-bands`, `app-close-home`, `apps-menu-1`, `apps-menu-2`, `apps-menu-3`, `apps-menu-4`, `apps-menu-5`, `apps-menu-6`, `bench-render`, `bt-dialog`, `bt-dialog-tema-aurora`, `bt-dialog-tema-cathode`, `bt-dialog-tema-redshift`, `bt-dialog-tema-sprint`, `bt-dialog-tema-sunlit`, `bt-dialog-tema-trail`, `home-bateria-externa`, `home-frecuencia-extrema`, `home-usb-20m`, `main-bateria-baja`, `main-espectro-agua`, `main-foco`, `main-frecuencia-dseg`, `main-natural`, `main-natural-eco`, `main-tema-aurora`, `main-tema-aurora-agua`, `main-tema-aurora-eco`, `main-tema-aurora-tx`, `main-tema-cathode`, `main-tema-cathode-agua`, `main-tema-cathode-eco`, `main-tema-cathode-tx`, `main-tema-redshift`, `main-tema-redshift-agua`, `main-tema-redshift-eco`, `main-tema-redshift-tx`, `main-tema-sprint`, `main-tema-sprint-agua`, `main-tema-sprint-eco`, `main-tema-sprint-tx`, `main-tema-sunlit`, `main-tema-sunlit-agua`, `main-tema-sunlit-eco`, `main-tema-sunlit-tx`, `main-tema-trail`, `main-tema-trail-agua`, `main-tema-trail-eco`, `main-tema-trail-tx`, `main-tx-simulado`, `settings-audio`, `settings-bands`, `settings-home`, `settings-perfil`, `settings-tema-aurora`, `settings-tema-cathode`, `settings-tema-redshift`, `settings-tema-sprint`, `settings-tema-sunlit`, `settings-tema-trail`, `settings-wfbat`

## Verdict: **PASS** (exit 0)

| Gate | Required | Status | Summary |
|---|---|---|---|
| Functional | yes | PASS | 61 scenarios captured on both revisions |
| Visual Regression | yes | PASS | no unexplained visual regressions |
| Quality Contract | yes | PASS | 633 contract checks passed on the candidate |
| Performance | yes | PASS | render-cost proxy within budget |
| Hardware | no (advisory) | BLOCKED | no physical X6200 attached; hardware behaviour is not measured |
| Energy | no (advisory) | BLOCKED | no power measurement; energy evidence must be physical |

## Visual diff

| Scenario | Identical | Changed px | Area | Boxes | Regions |
|---|---|---|---|---|---|
| app-bands | no | 309314 | 80.551% | 1 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, multiplicador, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_scrollbar, ajustes_audio_ganancia, ajustes_tema |
| app-close-home | no | 313868 | 81.736% | 1 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, multiplicador, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_scrollbar, ajustes_audio_ganancia, ajustes_tema |
| apps-menu-1 | no | 312075 | 81.270% | 1 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, multiplicador, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_scrollbar, ajustes_audio_ganancia, ajustes_tema |
| apps-menu-2 | no | 312828 | 81.466% | 1 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, multiplicador, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_scrollbar, ajustes_audio_ganancia, ajustes_tema |
| apps-menu-3 | no | 312645 | 81.418% | 1 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, multiplicador, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_scrollbar, ajustes_audio_ganancia, ajustes_tema |
| apps-menu-4 | no | 312443 | 81.365% | 1 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, multiplicador, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_scrollbar, ajustes_audio_ganancia, ajustes_tema |
| apps-menu-5 | no | 312106 | 81.278% | 1 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, multiplicador, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_scrollbar, ajustes_audio_ganancia, ajustes_tema |
| apps-menu-6 | no | 311880 | 81.219% | 1 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, multiplicador, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_scrollbar, ajustes_audio_ganancia, ajustes_tema |
| bench-render | no | 331810 | 86.409% | 1 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, multiplicador, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_scrollbar, ajustes_audio_ganancia, ajustes_tema |
| bt-dialog | no | 253352 | 65.977% | 1 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, multiplicador, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_scrollbar, ajustes_audio_ganancia, ajustes_tema |
| bt-dialog-tema-aurora | yes | 0 | 0.000% | 0 | — |
| bt-dialog-tema-cathode | yes | 0 | 0.000% | 0 | — |
| bt-dialog-tema-redshift | yes | 0 | 0.000% | 0 | — |
| bt-dialog-tema-sprint | yes | 0 | 0.000% | 0 | — |
| bt-dialog-tema-sunlit | yes | 0 | 0.000% | 0 | — |
| bt-dialog-tema-trail | yes | 0 | 0.000% | 0 | — |
| home-bateria-externa | no | 313918 | 81.749% | 1 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, multiplicador, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_scrollbar, ajustes_audio_ganancia, ajustes_tema |
| home-frecuencia-extrema | no | 317129 | 82.586% | 1 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, multiplicador, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_scrollbar, ajustes_audio_ganancia, ajustes_tema |
| home-usb-20m | no | 313868 | 81.736% | 1 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, multiplicador, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_scrollbar, ajustes_audio_ganancia, ajustes_tema |
| main-bateria-baja | no | 316651 | 82.461% | 1 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, multiplicador, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_scrollbar, ajustes_audio_ganancia, ajustes_tema |
| main-espectro-agua | no | 332083 | 86.480% | 1 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, multiplicador, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_scrollbar, ajustes_audio_ganancia, ajustes_tema |
| main-foco | no | 313868 | 81.736% | 1 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, multiplicador, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_scrollbar, ajustes_audio_ganancia, ajustes_tema |
| main-frecuencia-dseg | no | 313033 | 81.519% | 1 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, multiplicador, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_scrollbar, ajustes_audio_ganancia, ajustes_tema |
| main-natural | no | 313785 | 81.715% | 1 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, multiplicador, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_scrollbar, ajustes_audio_ganancia, ajustes_tema |
| main-natural-eco | no | 313829 | 81.726% | 1 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, multiplicador, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_scrollbar, ajustes_audio_ganancia, ajustes_tema |
| main-tema-aurora | yes | 0 | 0.000% | 0 | — |
| main-tema-aurora-agua | yes | 0 | 0.000% | 0 | — |
| main-tema-aurora-eco | yes | 0 | 0.000% | 0 | — |
| main-tema-aurora-tx | yes | 0 | 0.000% | 0 | — |
| main-tema-cathode | yes | 0 | 0.000% | 0 | — |
| main-tema-cathode-agua | yes | 0 | 0.000% | 0 | — |
| main-tema-cathode-eco | yes | 0 | 0.000% | 0 | — |
| main-tema-cathode-tx | yes | 0 | 0.000% | 0 | — |
| main-tema-redshift | yes | 0 | 0.000% | 0 | — |
| main-tema-redshift-agua | yes | 0 | 0.000% | 0 | — |
| main-tema-redshift-eco | yes | 0 | 0.000% | 0 | — |
| main-tema-redshift-tx | yes | 0 | 0.000% | 0 | — |
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
| main-tx-simulado | no | 314023 | 81.777% | 1 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, multiplicador, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_scrollbar, ajustes_audio_ganancia, ajustes_tema |
| settings-audio | no | 79730 | 20.763% | 365 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, multiplicador, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema |
| settings-bands | no | 309314 | 80.551% | 1 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, multiplicador, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_scrollbar, ajustes_audio_ganancia, ajustes_tema |
| settings-home | no | 77863 | 20.277% | 362 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, multiplicador, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema |
| settings-perfil | no | 72382 | 18.849% | 335 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, multiplicador, reloj_potencia, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema |
| settings-tema-aurora | yes | 0 | 0.000% | 0 | — |
| settings-tema-cathode | yes | 0 | 0.000% | 0 | — |
| settings-tema-redshift | yes | 0 | 0.000% | 0 | — |
| settings-tema-sprint | yes | 0 | 0.000% | 0 | — |
| settings-tema-sunlit | yes | 0 | 0.000% | 0 | — |
| settings-tema-trail | yes | 0 | 0.000% | 0 | — |
| settings-wfbat | no | 101333 | 26.389% | 331 | barra_superior, s_metro, espectro, frecuencia, waterfall, botones, multiplicador, reloj_potencia, ajustes_anio, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema |

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
| main-tema-aurora | 15 | 4 | 3 | 3 |
| main-tema-aurora-agua | 15 | 4 | 3 | 3 |
| main-tema-aurora-eco | 15 | 4 | 3 | 3 |
| main-tema-aurora-tx | 15 | 4 | 3 | 3 |
| main-tema-cathode | 15 | 4 | 3 | 3 |
| main-tema-cathode-agua | 15 | 4 | 3 | 3 |
| main-tema-cathode-eco | 15 | 4 | 3 | 3 |
| main-tema-cathode-tx | 15 | 4 | 3 | 3 |
| main-tema-redshift | 15 | 4 | 3 | 3 |
| main-tema-redshift-agua | 15 | 4 | 3 | 3 |
| main-tema-redshift-eco | 15 | 4 | 3 | 3 |
| main-tema-redshift-tx | 15 | 4 | 3 | 3 |
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
| main-tx-simulado | 15 | 4 | 3 | 3 |
| settings-audio | 17 | 4 | 10 | 4 |
| settings-bands | 17 | 5 | 3 | 5 |
| settings-home | 17 | 4 | 10 | 4 |
| settings-perfil | 17 | 4 | 10 | 4 |
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
| UI-015 | settings-wfbat | minor | verified | — | — | UNVERIFIED |
| UI-016 | settings-home | minor | fixed | — | — | UNVERIFIED |
| UI-017 | main-frecuencia-dseg | minor | verified | — | — | UNVERIFIED |
| UI-002 | apps-menu-1 | major | fixed | — | — | UNVERIFIED |
| UI-003 | home-usb-20m | major | unverified | — | — | UNVERIFIED |
| UI-004 | settings-home | major | fixed | — | — | UNVERIFIED |
| UI-005 | bt-dialog | minor | fixed | — | — | UNVERIFIED |
| UI-006 | home-usb-20m | minor | unverified | — | — | UNVERIFIED |
| UI-008 | home-usb-20m | info | out-of-scope | — | — | UNVERIFIED |

9 issue(s) are unverified: recorded from the repository but with no detector yet, so they neither block nor pass. They stay in the annex until the harness reproduces them.

## Quality contracts

633 contract checks, 0 failure(s), 0 unchanged baseline defect(s).

## Evidence

- Screenshots: `screenshots/`
- Diffs: `diffs/`
- Logs: `logs/`
- Docker image: `x6200-uiharness`
- Fixtures of A that B does not ship (REMOVED in B, left out of the comparison): `main-otro-tema`, `main-tema-classic`, `main-tema-holo`, `main-tema-material-3`, `main-tema-outdoor-negative`, `main-tema-rs`, `main-tema-vision`, `main-tema-vision-cwr`, `main-tema-vision-usbd`, `settings-rs-theme`

### Gates without instrumentation (BLOCKED, never PASS)

- **Hardware**: no physical X6200 attached; hardware behaviour is not measured
  - cause: the framework runs on the host and never in the ROM
  - needed: an X6200 unit with the capture/instrumentation flow of milestone 7
- **Energy**: no power measurement; energy evidence must be physical
  - cause: render counters are not joules
  - needed: physical energy measurement (milestone 7)
