# A/B UI report — AB-DFL173-1

- Date: 2026-10-20
- Profile: `host-ui`
- A (baseline): `9bb1d306dcb7176f6a89be51ae49797474db9a80` (a)
- B (candidate): `f5f3f14f06f77eb87b5b2f425cb3100c9412d1e5` (b)
- Scenarios: `app-bands`, `app-close-home`, `apps-menu-1`, `apps-menu-2`, `apps-menu-3`, `apps-menu-4`, `apps-menu-5`, `apps-menu-6`, `autotune`, `autotune-found`, `autotune-progress`, `bench-render`, `bt-dialog`, `bt-dialog-tema-aurora`, `bt-dialog-tema-cathode`, `bt-dialog-tema-redshift`, `bt-dialog-tema-sprint`, `bt-dialog-tema-sunlit`, `bt-dialog-tema-trail`, `cw-autotune-off`, `cw-autotune-on`, `cw-tune-default`, `cw-tune-ssb`, `cw-tune-tema-sunlit`, `cw-tune-tuner`, `home-bateria-externa`, `home-cb`, `home-cb-a`, `home-cb-cathode`, `home-cb-d`, `home-cb-offchannel`, `home-frecuencia-extrema`, `home-usb-20m`, `main-bateria-baja`, `main-espectro-agua`, `main-foco`, `main-frecuencia-dseg`, `main-natural`, `main-natural-eco`, `main-tema-aurora`, `main-tema-aurora-agua`, `main-tema-aurora-eco`, `main-tema-aurora-tx`, `main-tema-cathode`, `main-tema-cathode-agua`, `main-tema-cathode-eco`, `main-tema-cathode-tx`, `main-tema-redshift`, `main-tema-redshift-agua`, `main-tema-redshift-eco`, `main-tema-redshift-tx`, `main-tema-sprint`, `main-tema-sprint-agua`, `main-tema-sprint-eco`, `main-tema-sprint-tx`, `main-tema-sunlit`, `main-tema-sunlit-agua`, `main-tema-sunlit-eco`, `main-tema-sunlit-tx`, `main-tema-trail`, `main-tema-trail-agua`, `main-tema-trail-eco`, `main-tema-trail-tx`, `main-tx-simulado`, `settings-audio`, `settings-bands`, `settings-home`, `settings-perfil`, `settings-tema-aurora`, `settings-tema-cathode`, `settings-tema-redshift`, `settings-tema-sprint`, `settings-tema-sunlit`, `settings-tema-trail`, `settings-wfbat`, `swrscan-3pass`, `swrscan-3pass-p1`, `swrscan-3pass-p2`, `swrscan-3pass-p3`, `swrscan-3pass-stop`, `swrscan-3pass-tema-aurora`, `swrscan-3pass-tema-cathode`, `swrscan-5pass-moving`, `swrscan-5pass-outliers`, `swrscan-double-valley`, `swrscan-flat`, `swrscan-narrow-zoom`, `swrscan-owner-40m`, `swrscan-owner-40m-domo`, `swrscan-owner-40m-sweep`, `swrscan-quick`, `swrscan-wide-zoom`

## Verdict: **PASS** (exit 0)

| Gate | Required | Status | Summary |
|---|---|---|---|
| Functional | yes | PASS | 92 scenarios captured on both revisions |
| Visual Regression | yes | PASS | no unexplained visual regressions |
| Quality Contract | yes | PASS | 943 contract checks passed on the candidate |
| Performance | yes | PASS | render-cost proxy within budget |
| Hardware | no (advisory) | BLOCKED | no physical X6200 attached; hardware behaviour is not measured |
| Energy | no (advisory) | BLOCKED | no power measurement; energy evidence must be physical |

## Performance (render-cost proxy)

> x86 render counters under virtual time; not a battery measurement.

Every scenario is within the fixed budgets of `quality/budgets.yaml`.

## Visual diff

| Scenario | Identical | Changed px | Area | Boxes | Regions |
|---|---|---|---|---|---|
| app-bands | no | 576 | 0.150% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| app-close-home | no | 576 | 0.150% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| apps-menu-1 | no | 576 | 0.150% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| apps-menu-2 | no | 576 | 0.150% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| apps-menu-3 | no | 576 | 0.150% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| apps-menu-4 | no | 576 | 0.150% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| apps-menu-5 | no | 576 | 0.150% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| apps-menu-6 | no | 576 | 0.150% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| autotune | no | 576 | 0.150% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| autotune-found | no | 576 | 0.150% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| autotune-progress | no | 576 | 0.150% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| bench-render | no | 606 | 0.158% | 2 | barra_superior, s_metro, espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| bt-dialog | no | 49 | 0.013% | 2 | espectro, ajustes_tema, ajustes_dialogo |
| bt-dialog-tema-aurora | no | 49 | 0.013% | 2 | espectro, ajustes_tema, ajustes_dialogo |
| bt-dialog-tema-cathode | no | 55 | 0.014% | 1 | espectro, ajustes_tema, ajustes_dialogo |
| bt-dialog-tema-redshift | no | 47 | 0.012% | 2 | espectro, ajustes_tema, ajustes_dialogo |
| bt-dialog-tema-sprint | no | 48 | 0.013% | 2 | espectro, ajustes_tema, ajustes_dialogo |
| bt-dialog-tema-sunlit | no | 55 | 0.014% | 1 | espectro, ajustes_tema, ajustes_dialogo |
| bt-dialog-tema-trail | no | 48 | 0.013% | 2 | espectro, ajustes_tema, ajustes_dialogo |
| cw-autotune-off | no | 670 | 0.174% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| cw-autotune-on | no | 670 | 0.174% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| cw-tune-default | no | 670 | 0.174% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| cw-tune-ssb | no | 576 | 0.150% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| cw-tune-tema-sunlit | no | 670 | 0.174% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| cw-tune-tuner | no | 670 | 0.174% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| home-bateria-externa | no | 576 | 0.150% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| home-cb | no | 2251 | 0.586% | 5 | barra_superior, s_metro, espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| home-cb-a | no | 2288 | 0.596% | 3 | barra_superior, s_metro, espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| home-cb-cathode | no | 2256 | 0.588% | 5 | barra_superior, s_metro, espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| home-cb-d | no | 2291 | 0.597% | 3 | barra_superior, s_metro, espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| home-cb-offchannel | no | 2279 | 0.593% | 3 | barra_superior, s_metro, espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| home-frecuencia-extrema | no | 576 | 0.150% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| home-usb-20m | no | 576 | 0.150% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| main-bateria-baja | no | 575 | 0.150% | 3 | barra_superior, s_metro, espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| main-espectro-agua | no | 592 | 0.154% | 2 | barra_superior, s_metro, espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| main-foco | no | 576 | 0.150% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| main-frecuencia-dseg | no | 576 | 0.150% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| main-natural | no | 586 | 0.153% | 3 | barra_superior, s_metro, espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| main-natural-eco | no | 586 | 0.153% | 3 | barra_superior, s_metro, espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| main-tema-aurora | no | 576 | 0.150% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| main-tema-aurora-agua | no | 592 | 0.154% | 2 | barra_superior, s_metro, espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| main-tema-aurora-eco | no | 575 | 0.150% | 3 | barra_superior, s_metro, espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| main-tema-aurora-tx | no | 588 | 0.153% | 3 | barra_superior, s_metro, espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| main-tema-cathode | no | 576 | 0.150% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| main-tema-cathode-agua | no | 592 | 0.154% | 2 | barra_superior, s_metro, espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| main-tema-cathode-eco | no | 571 | 0.149% | 3 | barra_superior, s_metro, espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| main-tema-cathode-tx | no | 588 | 0.153% | 3 | barra_superior, s_metro, espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| main-tema-redshift | no | 576 | 0.150% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| main-tema-redshift-agua | no | 592 | 0.154% | 2 | barra_superior, s_metro, espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| main-tema-redshift-eco | no | 586 | 0.153% | 3 | barra_superior, s_metro, espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| main-tema-redshift-tx | no | 588 | 0.153% | 3 | barra_superior, s_metro, espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| main-tema-sprint | no | 576 | 0.150% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| main-tema-sprint-agua | no | 592 | 0.154% | 2 | barra_superior, s_metro, espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| main-tema-sprint-eco | no | 582 | 0.152% | 3 | barra_superior, s_metro, espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| main-tema-sprint-tx | no | 588 | 0.153% | 3 | barra_superior, s_metro, espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| main-tema-sunlit | no | 576 | 0.150% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| main-tema-sunlit-agua | no | 592 | 0.154% | 2 | barra_superior, s_metro, espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| main-tema-sunlit-eco | no | 586 | 0.153% | 3 | barra_superior, s_metro, espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| main-tema-sunlit-tx | no | 588 | 0.153% | 3 | barra_superior, s_metro, espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| main-tema-trail | no | 576 | 0.150% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| main-tema-trail-agua | no | 592 | 0.154% | 2 | barra_superior, s_metro, espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| main-tema-trail-eco | no | 586 | 0.153% | 3 | barra_superior, s_metro, espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| main-tema-trail-tx | no | 588 | 0.153% | 3 | barra_superior, s_metro, espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| main-tx-simulado | no | 588 | 0.153% | 3 | barra_superior, s_metro, espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| settings-audio | no | 78 | 0.020% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_tema, ajustes_dialogo |
| settings-bands | no | 576 | 0.150% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| settings-home | no | 78 | 0.020% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_tema, ajustes_dialogo |
| settings-perfil | no | 78 | 0.020% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_tema, ajustes_dialogo |
| settings-tema-aurora | no | 78 | 0.020% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_tema, ajustes_dialogo |
| settings-tema-cathode | no | 88 | 0.023% | 2 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_tema, ajustes_dialogo |
| settings-tema-redshift | no | 75 | 0.020% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_tema, ajustes_dialogo |
| settings-tema-sprint | no | 82 | 0.021% | 4 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_tema, ajustes_dialogo |
| settings-tema-sunlit | no | 89 | 0.023% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_tema, ajustes_dialogo |
| settings-tema-trail | no | 70 | 0.018% | 2 | espectro, ajustes_tema, ajustes_dialogo |
| settings-wfbat | no | 78 | 0.020% | 3 | espectro, waterfall_ticks, waterfall, ajustes_perfil, ajustes_tema, ajustes_dialogo |
| swrscan-3pass | no | 14 | 0.004% | 1 | espectro, ajustes_dialogo |
| swrscan-3pass-p1 | no | 14 | 0.004% | 1 | espectro, ajustes_dialogo |
| swrscan-3pass-p2 | no | 14 | 0.004% | 1 | espectro, ajustes_dialogo |
| swrscan-3pass-p3 | no | 14 | 0.004% | 1 | espectro, ajustes_dialogo |
| swrscan-3pass-stop | no | 14 | 0.004% | 1 | espectro, ajustes_dialogo |
| swrscan-3pass-tema-aurora | no | 14 | 0.004% | 1 | espectro, ajustes_dialogo |
| swrscan-3pass-tema-cathode | no | 14 | 0.004% | 1 | espectro, ajustes_dialogo |
| swrscan-5pass-moving | no | 14 | 0.004% | 1 | espectro, ajustes_dialogo |
| swrscan-5pass-outliers | no | 14 | 0.004% | 1 | espectro, ajustes_dialogo |
| swrscan-double-valley | no | 14 | 0.004% | 1 | espectro, ajustes_dialogo |
| swrscan-flat | no | 14 | 0.004% | 1 | espectro, ajustes_dialogo |
| swrscan-narrow-zoom | no | 14 | 0.004% | 1 | espectro, ajustes_dialogo |
| swrscan-owner-40m | no | 14 | 0.004% | 1 | espectro, ajustes_dialogo |
| swrscan-owner-40m-domo | no | 14 | 0.004% | 1 | espectro, ajustes_dialogo |
| swrscan-owner-40m-sweep | no | 14 | 0.004% | 1 | espectro, ajustes_dialogo |
| swrscan-quick | no | 14 | 0.004% | 1 | espectro, ajustes_dialogo |
| swrscan-wide-zoom | no | 14 | 0.004% | 1 | espectro, ajustes_dialogo |

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
| autotune | 18 | 5 | 3 | 5 |
| autotune-found | 18 | 5 | 3 | 5 |
| autotune-progress | 18 | 5 | 3 | 5 |
| bench-render | 15 | 4 | 3 | 3 |
| bt-dialog | 19 | 4 | 4 | 4 |
| bt-dialog-tema-aurora | 19 | 4 | 4 | 4 |
| bt-dialog-tema-cathode | 19 | 4 | 4 | 4 |
| bt-dialog-tema-redshift | 19 | 4 | 4 | 4 |
| bt-dialog-tema-sprint | 19 | 4 | 4 | 4 |
| bt-dialog-tema-sunlit | 19 | 4 | 4 | 4 |
| bt-dialog-tema-trail | 19 | 4 | 4 | 4 |
| cw-autotune-off | 16 | 5 | 4 | 4 |
| cw-autotune-on | 16 | 5 | 4 | 4 |
| cw-tune-default | 17 | 5 | 3 | 4 |
| cw-tune-ssb | 17 | 5 | 3 | 4 |
| cw-tune-tema-sunlit | 17 | 5 | 3 | 4 |
| cw-tune-tuner | 17 | 5 | 3 | 4 |
| home-bateria-externa | 15 | 4 | 3 | 3 |
| home-cb | 15 | 4 | 3 | 3 |
| home-cb-a | 15 | 4 | 3 | 3 |
| home-cb-cathode | 15 | 4 | 3 | 3 |
| home-cb-d | 15 | 4 | 3 | 3 |
| home-cb-offchannel | 15 | 4 | 3 | 3 |
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
| swrscan-3pass | 18 | 7 | 6 | 4 |
| swrscan-3pass-p1 | 18 | 7 | 6 | 4 |
| swrscan-3pass-p2 | 18 | 7 | 6 | 4 |
| swrscan-3pass-p3 | 18 | 7 | 6 | 4 |
| swrscan-3pass-stop | 18 | 7 | 6 | 4 |
| swrscan-3pass-tema-aurora | 18 | 7 | 6 | 4 |
| swrscan-3pass-tema-cathode | 18 | 7 | 6 | 4 |
| swrscan-5pass-moving | 18 | 7 | 6 | 4 |
| swrscan-5pass-outliers | 18 | 7 | 6 | 4 |
| swrscan-double-valley | 18 | 7 | 6 | 4 |
| swrscan-flat | 18 | 7 | 6 | 4 |
| swrscan-narrow-zoom | 18 | 7 | 6 | 4 |
| swrscan-owner-40m | 18 | 7 | 6 | 4 |
| swrscan-owner-40m-domo | 18 | 7 | 6 | 4 |
| swrscan-owner-40m-sweep | 18 | 7 | 6 | 4 |
| swrscan-quick | 18 | 7 | 6 | 4 |
| swrscan-wide-zoom | 18 | 7 | 6 | 4 |

Components declared below REAL (never written as REAL):

- RTC / system clock (time())=SYNTHETIC, acquisition (fake backend)=PARTIAL, audio.c=SYNTHETIC, audio_enhancement.c=SYNTHETIC, backlight.c=SYNTHETIC, band_info.c=PARTIAL, bt_state.c=SYNTHETIC, btaudio.c=SYNTHETIC, cfg (catalogue)=SYNTHETIC, cfg.cw_decoder_auto=SYNTHETIC, cfg/transverter.c=SYNTHETIC, cw_tune_ui.c=PARTIAL, main_screen.c (ACTION_APP_BANDS)=PARTIAL, main_screen.c (ACTION_APP_RTTY close)=PARTIAL, main_screen.c (APP key -> buttons_group_app)=PARTIAL, main_screen.c (KEY key -> buttons_group_key)=PARTIAL, meter.c (S-meter)=PARTIAL, multipass TX gate=PARTIAL, params=SYNTHETIC, radio=SYNTHETIC, roe_curve.c=PARTIAL, rtty.c=SYNTHETIC, scheduler=SYNTHETIC, settle window / timeouts=SYNTHETIC, spectrum.c=PARTIAL, waterfall.c=PARTIAL, wfview_server.c=SYNTHETIC

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

943 contract checks, 0 failure(s), 0 unchanged baseline defect(s).

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
