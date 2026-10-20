# A/B UI report — AB-DFL179

- Date: 2026-10-20
- Profile: `host-ui`
- A (baseline): `9d72bde1d59bdfc4319245af43e3b732441be4e6` (a)
- B (candidate): `0507cab0dac822eec462143b596b731ee93190b9` (b)
- Scenarios: `app-bands`, `app-close-home`, `apps-menu-1`, `apps-menu-2`, `apps-menu-3`, `apps-menu-4`, `apps-menu-5`, `apps-menu-6`, `autotune`, `autotune-found`, `autotune-progress`, `bench-render`, `bt-dialog`, `bt-dialog-tema-aurora`, `bt-dialog-tema-cathode`, `bt-dialog-tema-redshift`, `bt-dialog-tema-sprint`, `bt-dialog-tema-sunlit`, `bt-dialog-tema-trail`, `cw-autotune-off`, `cw-autotune-on`, `cw-tune-default`, `cw-tune-ssb`, `cw-tune-tema-sunlit`, `cw-tune-tuner`, `dfl-editor-cycles`, `dfl-editor-interaction`, `dfl-editor-lsb-d`, `dfl-editor-tx`, `dfl-editor-usb-d`, `dfl-editor-usb-d-tema-aurora`, `dfl-editor-usb-d-tema-cathode`, `dfl-editor-usb-d-tema-redshift`, `dfl-editor-usb-d-tema-sprint`, `dfl-editor-usb-d-tema-sunlit`, `dfl-editor-usb-d-tema-trail`, `dfl-lsb-d`, `dfl-usb-d`, `dfl-usb-d-span100k`, `dfl-usb-d-span50k`, `dfl-usb-d-tema-cathode`, `dfl-usb-d-tema-redshift`, `dfl-usb-d-tema-sprint`, `dfl-usb-d-tema-sunlit`, `dfl-usb-d-tema-trail`, `home-bateria-externa`, `home-cb`, `home-cb-a`, `home-cb-cathode`, `home-cb-d`, `home-cb-offchannel`, `home-frecuencia-extrema`, `home-usb-20m`, `main-bateria-baja`, `main-espectro-agua`, `main-foco`, `main-frecuencia-dseg`, `main-natural`, `main-natural-eco`, `main-tema-aurora`, `main-tema-aurora-agua`, `main-tema-aurora-eco`, `main-tema-aurora-tx`, `main-tema-cathode`, `main-tema-cathode-agua`, `main-tema-cathode-eco`, `main-tema-cathode-tx`, `main-tema-redshift`, `main-tema-redshift-agua`, `main-tema-redshift-eco`, `main-tema-redshift-tx`, `main-tema-sprint`, `main-tema-sprint-agua`, `main-tema-sprint-eco`, `main-tema-sprint-tx`, `main-tema-sunlit`, `main-tema-sunlit-agua`, `main-tema-sunlit-eco`, `main-tema-sunlit-tx`, `main-tema-trail`, `main-tema-trail-agua`, `main-tema-trail-eco`, `main-tema-trail-tx`, `main-tx-simulado`, `settings-audio`, `settings-bands`, `settings-grupo-actions`, `settings-grupo-display`, `settings-grupo-info`, `settings-grupo-operation`, `settings-grupo-operation-tema-aurora`, `settings-grupo-operation-tema-cathode`, `settings-grupo-operation-tema-redshift`, `settings-grupo-operation-tema-sprint`, `settings-grupo-operation-tema-sunlit`, `settings-grupo-operation-tema-trail`, `settings-home`, `settings-perfil`, `settings-tema-aurora`, `settings-tema-cathode`, `settings-tema-redshift`, `settings-tema-sprint`, `settings-tema-sunlit`, `settings-tema-trail`, `settings-wfbat`, `swrscan-3pass`, `swrscan-3pass-p1`, `swrscan-3pass-p2`, `swrscan-3pass-p3`, `swrscan-3pass-stop`, `swrscan-3pass-tema-aurora`, `swrscan-3pass-tema-cathode`, `swrscan-5pass-moving`, `swrscan-5pass-outliers`, `swrscan-double-valley`, `swrscan-flat`, `swrscan-narrow-zoom`, `swrscan-owner-40m`, `swrscan-owner-40m-domo`, `swrscan-owner-40m-sweep`, `swrscan-quick`, `swrscan-wide-zoom`

## Verdict: **PASS** (exit 0)

| Gate | Required | Status | Summary |
|---|---|---|---|
| Functional | yes | PASS | 122 scenarios captured on both revisions |
| Visual Regression | yes | PASS | no unexplained visual regressions |
| Quality Contract | yes | PASS | 1274 contract checks passed on the candidate |
| Performance | yes | PASS | render-cost proxy within budget |
| Hardware | no (advisory) | BLOCKED | no physical X6200 attached; hardware behaviour is not measured |
| Energy | no (advisory) | BLOCKED | no power measurement; energy evidence must be physical |

## Performance (render-cost proxy)

> x86 render counters under virtual time; not a battery measurement.

Every scenario is within the fixed budgets of `quality/budgets.yaml`.

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
| autotune | yes | 0 | 0.000% | 0 | — |
| autotune-found | yes | 0 | 0.000% | 0 | — |
| autotune-progress | yes | 0 | 0.000% | 0 | — |
| bench-render | yes | 0 | 0.000% | 0 | — |
| bt-dialog | yes | 0 | 0.000% | 0 | — |
| bt-dialog-tema-aurora | yes | 0 | 0.000% | 0 | — |
| bt-dialog-tema-cathode | yes | 0 | 0.000% | 0 | — |
| bt-dialog-tema-redshift | yes | 0 | 0.000% | 0 | — |
| bt-dialog-tema-sprint | yes | 0 | 0.000% | 0 | — |
| bt-dialog-tema-sunlit | yes | 0 | 0.000% | 0 | — |
| bt-dialog-tema-trail | yes | 0 | 0.000% | 0 | — |
| cw-autotune-off | yes | 0 | 0.000% | 0 | — |
| cw-autotune-on | yes | 0 | 0.000% | 0 | — |
| cw-tune-default | yes | 0 | 0.000% | 0 | — |
| cw-tune-ssb | yes | 0 | 0.000% | 0 | — |
| cw-tune-tema-sunlit | yes | 0 | 0.000% | 0 | — |
| cw-tune-tuner | yes | 0 | 0.000% | 0 | — |
| dfl-editor-cycles | no | 155115 | 40.395% | 1 | waterfall_ticks, waterfall, botones, ajustes_perfil, ajustes_scrollbar, ajustes_dialogo, dfl_inline |
| dfl-editor-interaction | no | 166198 | 43.281% | 1 | waterfall_ticks, waterfall, botones, ajustes_perfil, ajustes_scrollbar, ajustes_dialogo, dfl_inline |
| dfl-editor-lsb-d | no | 149857 | 39.025% | 64 | waterfall_ticks, waterfall, botones, ajustes_perfil, ajustes_scrollbar, ajustes_dialogo, dfl_inline |
| dfl-editor-tx | no | 154361 | 40.198% | 1 | waterfall_ticks, waterfall, botones, ajustes_perfil, ajustes_scrollbar, ajustes_dialogo, dfl_inline |
| dfl-editor-usb-d | no | 149863 | 39.027% | 65 | waterfall_ticks, waterfall, botones, ajustes_perfil, ajustes_scrollbar, ajustes_dialogo, dfl_inline |
| dfl-editor-usb-d-tema-aurora | no | 149863 | 39.027% | 65 | waterfall_ticks, waterfall, botones, ajustes_perfil, ajustes_scrollbar, ajustes_dialogo, dfl_inline |
| dfl-editor-usb-d-tema-cathode | no | 136892 | 35.649% | 47 | waterfall_ticks, waterfall, botones, ajustes_perfil, ajustes_scrollbar, ajustes_dialogo, dfl_inline |
| dfl-editor-usb-d-tema-redshift | no | 134321 | 34.979% | 47 | waterfall_ticks, waterfall, botones, ajustes_perfil, ajustes_scrollbar, ajustes_dialogo, dfl_inline |
| dfl-editor-usb-d-tema-sprint | no | 145445 | 37.876% | 48 | waterfall_ticks, waterfall, botones, ajustes_perfil, ajustes_scrollbar, ajustes_dialogo, dfl_inline |
| dfl-editor-usb-d-tema-sunlit | no | 128869 | 33.560% | 58 | waterfall_ticks, waterfall, botones, ajustes_perfil, ajustes_scrollbar, ajustes_dialogo, dfl_inline |
| dfl-editor-usb-d-tema-trail | no | 143456 | 37.358% | 70 | waterfall_ticks, waterfall, botones, ajustes_perfil, ajustes_scrollbar, ajustes_dialogo, dfl_inline |
| dfl-lsb-d | yes | 0 | 0.000% | 0 | — |
| dfl-usb-d | yes | 0 | 0.000% | 0 | — |
| dfl-usb-d-span100k | yes | 0 | 0.000% | 0 | — |
| dfl-usb-d-span50k | yes | 0 | 0.000% | 0 | — |
| dfl-usb-d-tema-cathode | yes | 0 | 0.000% | 0 | — |
| dfl-usb-d-tema-redshift | yes | 0 | 0.000% | 0 | — |
| dfl-usb-d-tema-sprint | yes | 0 | 0.000% | 0 | — |
| dfl-usb-d-tema-sunlit | yes | 0 | 0.000% | 0 | — |
| dfl-usb-d-tema-trail | yes | 0 | 0.000% | 0 | — |
| home-bateria-externa | yes | 0 | 0.000% | 0 | — |
| home-cb | yes | 0 | 0.000% | 0 | — |
| home-cb-a | yes | 0 | 0.000% | 0 | — |
| home-cb-cathode | yes | 0 | 0.000% | 0 | — |
| home-cb-d | yes | 0 | 0.000% | 0 | — |
| home-cb-offchannel | yes | 0 | 0.000% | 0 | — |
| home-frecuencia-extrema | yes | 0 | 0.000% | 0 | — |
| home-usb-20m | yes | 0 | 0.000% | 0 | — |
| main-bateria-baja | yes | 0 | 0.000% | 0 | — |
| main-espectro-agua | yes | 0 | 0.000% | 0 | — |
| main-foco | yes | 0 | 0.000% | 0 | — |
| main-frecuencia-dseg | yes | 0 | 0.000% | 0 | — |
| main-natural | yes | 0 | 0.000% | 0 | — |
| main-natural-eco | yes | 0 | 0.000% | 0 | — |
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
| main-tx-simulado | yes | 0 | 0.000% | 0 | — |
| settings-audio | yes | 0 | 0.000% | 0 | — |
| settings-bands | yes | 0 | 0.000% | 0 | — |
| settings-grupo-actions | yes | 0 | 0.000% | 0 | — |
| settings-grupo-display | yes | 0 | 0.000% | 0 | — |
| settings-grupo-info | yes | 0 | 0.000% | 0 | — |
| settings-grupo-operation | yes | 0 | 0.000% | 0 | — |
| settings-grupo-operation-tema-aurora | yes | 0 | 0.000% | 0 | — |
| settings-grupo-operation-tema-cathode | yes | 0 | 0.000% | 0 | — |
| settings-grupo-operation-tema-redshift | yes | 0 | 0.000% | 0 | — |
| settings-grupo-operation-tema-sprint | yes | 0 | 0.000% | 0 | — |
| settings-grupo-operation-tema-sunlit | yes | 0 | 0.000% | 0 | — |
| settings-grupo-operation-tema-trail | yes | 0 | 0.000% | 0 | — |
| settings-home | yes | 0 | 0.000% | 0 | — |
| settings-perfil | yes | 0 | 0.000% | 0 | — |
| settings-tema-aurora | yes | 0 | 0.000% | 0 | — |
| settings-tema-cathode | yes | 0 | 0.000% | 0 | — |
| settings-tema-redshift | yes | 0 | 0.000% | 0 | — |
| settings-tema-sprint | yes | 0 | 0.000% | 0 | — |
| settings-tema-sunlit | yes | 0 | 0.000% | 0 | — |
| settings-tema-trail | yes | 0 | 0.000% | 0 | — |
| settings-wfbat | yes | 0 | 0.000% | 0 | — |
| swrscan-3pass | yes | 0 | 0.000% | 0 | — |
| swrscan-3pass-p1 | yes | 0 | 0.000% | 0 | — |
| swrscan-3pass-p2 | yes | 0 | 0.000% | 0 | — |
| swrscan-3pass-p3 | yes | 0 | 0.000% | 0 | — |
| swrscan-3pass-stop | yes | 0 | 0.000% | 0 | — |
| swrscan-3pass-tema-aurora | yes | 0 | 0.000% | 0 | — |
| swrscan-3pass-tema-cathode | yes | 0 | 0.000% | 0 | — |
| swrscan-5pass-moving | yes | 0 | 0.000% | 0 | — |
| swrscan-5pass-outliers | yes | 0 | 0.000% | 0 | — |
| swrscan-double-valley | yes | 0 | 0.000% | 0 | — |
| swrscan-flat | yes | 0 | 0.000% | 0 | — |
| swrscan-narrow-zoom | yes | 0 | 0.000% | 0 | — |
| swrscan-owner-40m | yes | 0 | 0.000% | 0 | — |
| swrscan-owner-40m-domo | yes | 0 | 0.000% | 0 | — |
| swrscan-owner-40m-sweep | yes | 0 | 0.000% | 0 | — |
| swrscan-quick | yes | 0 | 0.000% | 0 | — |
| swrscan-wide-zoom | yes | 0 | 0.000% | 0 | — |

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
| dfl-editor-cycles | 15 | 4 | 3 | 3 |
| dfl-editor-interaction | 15 | 4 | 3 | 3 |
| dfl-editor-lsb-d | 15 | 4 | 3 | 3 |
| dfl-editor-tx | 15 | 4 | 3 | 3 |
| dfl-editor-usb-d | 15 | 4 | 3 | 3 |
| dfl-editor-usb-d-tema-aurora | 15 | 4 | 3 | 3 |
| dfl-editor-usb-d-tema-cathode | 15 | 4 | 3 | 3 |
| dfl-editor-usb-d-tema-redshift | 15 | 4 | 3 | 3 |
| dfl-editor-usb-d-tema-sprint | 15 | 4 | 3 | 3 |
| dfl-editor-usb-d-tema-sunlit | 15 | 4 | 3 | 3 |
| dfl-editor-usb-d-tema-trail | 15 | 4 | 3 | 3 |
| dfl-lsb-d | 15 | 4 | 3 | 3 |
| dfl-usb-d | 15 | 4 | 3 | 3 |
| dfl-usb-d-span100k | 15 | 4 | 3 | 3 |
| dfl-usb-d-span50k | 15 | 4 | 3 | 3 |
| dfl-usb-d-tema-cathode | 15 | 4 | 3 | 3 |
| dfl-usb-d-tema-redshift | 15 | 4 | 3 | 3 |
| dfl-usb-d-tema-sprint | 15 | 4 | 3 | 3 |
| dfl-usb-d-tema-sunlit | 15 | 4 | 3 | 3 |
| dfl-usb-d-tema-trail | 15 | 4 | 3 | 3 |
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
| settings-grupo-actions | 15 | 4 | 3 | 3 |
| settings-grupo-display | 15 | 4 | 3 | 3 |
| settings-grupo-info | 15 | 4 | 3 | 3 |
| settings-grupo-operation | 15 | 4 | 3 | 3 |
| settings-grupo-operation-tema-aurora | 15 | 4 | 3 | 3 |
| settings-grupo-operation-tema-cathode | 15 | 4 | 3 | 3 |
| settings-grupo-operation-tema-redshift | 15 | 4 | 3 | 3 |
| settings-grupo-operation-tema-sprint | 15 | 4 | 3 | 3 |
| settings-grupo-operation-tema-sunlit | 15 | 4 | 3 | 3 |
| settings-grupo-operation-tema-trail | 15 | 4 | 3 | 3 |
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

1274 contract checks, 0 failure(s), 0 unchanged baseline defect(s).

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
