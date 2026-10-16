# A/B UI report — AB-SETEN109

- Date: 2026-10-16
- Profile: `host-ui`
- A (baseline): `3d6c022e572e113cf02ae037e74e37f90491e941` (a)
- B (candidate): `e4cfb8c9933869128cf6490e2b63a461ffab849b` (b)
- Scenarios: `app-bands`, `app-close-home`, `apps-menu-1`, `apps-menu-2`, `apps-menu-3`, `apps-menu-4`, `apps-menu-5`, `apps-menu-6`, `autotune`, `autotune-found`, `autotune-progress`, `bench-render`, `bt-dialog`, `bt-dialog-tema-aurora`, `bt-dialog-tema-cathode`, `bt-dialog-tema-redshift`, `bt-dialog-tema-sprint`, `bt-dialog-tema-sunlit`, `bt-dialog-tema-trail`, `home-bateria-externa`, `home-frecuencia-extrema`, `home-usb-20m`, `main-bateria-baja`, `main-espectro-agua`, `main-foco`, `main-frecuencia-dseg`, `main-natural`, `main-natural-eco`, `main-tema-aurora`, `main-tema-aurora-agua`, `main-tema-aurora-eco`, `main-tema-aurora-tx`, `main-tema-cathode`, `main-tema-cathode-agua`, `main-tema-cathode-eco`, `main-tema-cathode-tx`, `main-tema-redshift`, `main-tema-redshift-agua`, `main-tema-redshift-eco`, `main-tema-redshift-tx`, `main-tema-sprint`, `main-tema-sprint-agua`, `main-tema-sprint-eco`, `main-tema-sprint-tx`, `main-tema-sunlit`, `main-tema-sunlit-agua`, `main-tema-sunlit-eco`, `main-tema-sunlit-tx`, `main-tema-trail`, `main-tema-trail-agua`, `main-tema-trail-eco`, `main-tema-trail-tx`, `main-tx-simulado`, `settings-audio`, `settings-bands`, `settings-home`, `settings-perfil`, `settings-tema-aurora`, `settings-tema-cathode`, `settings-tema-redshift`, `settings-tema-sprint`, `settings-tema-sunlit`, `settings-tema-trail`, `settings-wfbat`

## Verdict: **PASS** (exit 0)

| Gate | Required | Status | Summary |
|---|---|---|---|
| Functional | yes | PASS | 64 scenarios captured on both revisions |
| Visual Regression | yes | PASS | no unexplained visual regressions |
| Quality Contract | yes | PASS | 663 contract checks passed on the candidate |
| Performance | yes | PASS | render-cost proxy within budget; 10 scenario(s) accepted by a dated exception |
| Hardware | no (advisory) | BLOCKED | no physical X6200 attached; hardware behaviour is not measured |
| Energy | no (advisory) | BLOCKED | no power measurement; energy evidence must be physical |

## Performance (render-cost proxy)

> x86 render counters under virtual time; not a battery measurement.

| Scenario | flushes A→B | Δ | flushed_pixels A→B | Δ | Verdict | Expires |
|---|---|---|---|---|---|---|
| settings-audio | 7 → 15 | +114.3% | 1135170 → 1267780 | +11.7% | PASS (by exception) | 2027-03-31 |
| settings-home | 7 → 15 | +114.3% | 1135170 → 1267780 | +11.7% | PASS (by exception) | 2027-03-31 |
| settings-perfil | 6 → 14 | +133.3% | 1003170 → 1135780 | +13.2% | PASS (by exception) | 2027-03-31 |
| settings-tema-aurora | 6 → 14 | +133.3% | 1003170 → 1135780 | +13.2% | PASS (by exception) | 2027-03-31 |
| settings-tema-cathode | 6 → 14 | +133.3% | 1002010 → 1134620 | +13.2% | PASS (by exception) | 2027-03-31 |
| settings-tema-redshift | 6 → 14 | +133.3% | 1003170 → 1135780 | +13.2% | PASS (by exception) | 2027-03-31 |
| settings-tema-sprint | 6 → 14 | +133.3% | 1002010 → 1134620 | +13.2% | PASS (by exception) | 2027-03-31 |
| settings-tema-sunlit | 6 → 14 | +133.3% | 1003170 → 1135780 | +13.2% | PASS (by exception) | 2027-03-31 |
| settings-tema-trail | 6 → 14 | +133.3% | 1003170 → 1135780 | +13.2% | PASS (by exception) | 2027-03-31 |
| settings-wfbat | 7 → 15 | +114.3% | 1135170 → 1267780 | +11.7% | PASS (by exception) | 2027-03-31 |

- **settings-audio** accepted by `ajustes-tabs-109` until `2027-03-31` (ceilings: flushes ≤ 130.0%, flushed_pixels ≤ 15.0%): AB-SETEN109 (#109): reordering the Settings tabs moved the dialog's initial focus from the Clock tab's date spinbox to Operation, whose first focusable widget is the "Freq acceleration" dropdown (make_freq_accel, src/dialog_settings.c). LVGL animates the focus style change with the theme's default transition — 80 ms, LV_THEME_DEFAULT_TRANSITION_TIME in lv_conf.h — so the first open of the window costs a constant +8 flushes and +132610 flushed pixels on every fixture that renders it, whatever the navigation. The steady state is unchanged (the CI benchmark measures flushes_mean 1.0 -> 1.0 and redrawn_area_mean 384000 -> 384000 on the same fixtures), so this is a one-off cost at open, not an operating regression. Measured here: 7 -> 15 flushes (+114.3%) and 1135170 -> 1267780 flushed pixels (+11.7%). Accepted by the owner on 2026-10-16 (issue #109), with budgets.yaml untouched.

- **settings-home** accepted by `ajustes-tabs-109` until `2027-03-31` (ceilings: flushes ≤ 130.0%, flushed_pixels ≤ 15.0%): AB-SETEN109 (#109): reordering the Settings tabs moved the dialog's initial focus from the Clock tab's date spinbox to Operation, whose first focusable widget is the "Freq acceleration" dropdown (make_freq_accel, src/dialog_settings.c). LVGL animates the focus style change with the theme's default transition — 80 ms, LV_THEME_DEFAULT_TRANSITION_TIME in lv_conf.h — so the first open of the window costs a constant +8 flushes and +132610 flushed pixels on every fixture that renders it, whatever the navigation. The steady state is unchanged (the CI benchmark measures flushes_mean 1.0 -> 1.0 and redrawn_area_mean 384000 -> 384000 on the same fixtures), so this is a one-off cost at open, not an operating regression. Measured here: 7 -> 15 flushes (+114.3%) and 1135170 -> 1267780 flushed pixels (+11.7%). Accepted by the owner on 2026-10-16 (issue #109), with budgets.yaml untouched.

- **settings-perfil** accepted by `ajustes-tabs-109` until `2027-03-31` (ceilings: flushes ≤ 150.0%, flushed_pixels ≤ 15.0%): AB-SETEN109 (#109): the same initial-focus change as the sibling entry above (the Settings dialog opens on Operation, whose first widget is the "Freq acceleration" dropdown, animated by LVGL's 80 ms theme transition), measured on the fixtures whose base is 6 flushes: the profile window and the six theme fixtures. The delta is the same constant +8 flushes and +132610 flushed pixels, and the steady state is unchanged (the CI benchmark measures flushes_mean 1.0 -> 1.0 and redrawn_area_mean 384000 -> 384000), so it is a one-off cost at open. Measured here: 6 -> 14 flushes (+133.3%) and 1003170 -> 1135780 flushed pixels (+13.2%). Accepted by the owner on 2026-10-16 (issue #109), with budgets.yaml untouched.

- **settings-tema-aurora** accepted by `ajustes-tabs-109` until `2027-03-31` (ceilings: flushes ≤ 150.0%, flushed_pixels ≤ 15.0%): AB-SETEN109 (#109): the same initial-focus change as the sibling entry above (the Settings dialog opens on Operation, whose first widget is the "Freq acceleration" dropdown, animated by LVGL's 80 ms theme transition), measured on the fixtures whose base is 6 flushes: the profile window and the six theme fixtures. The delta is the same constant +8 flushes and +132610 flushed pixels, and the steady state is unchanged (the CI benchmark measures flushes_mean 1.0 -> 1.0 and redrawn_area_mean 384000 -> 384000), so it is a one-off cost at open. Measured here: 6 -> 14 flushes (+133.3%) and 1003170 -> 1135780 flushed pixels (+13.2%). Accepted by the owner on 2026-10-16 (issue #109), with budgets.yaml untouched.

- **settings-tema-cathode** accepted by `ajustes-tabs-109` until `2027-03-31` (ceilings: flushes ≤ 150.0%, flushed_pixels ≤ 15.0%): AB-SETEN109 (#109): the same initial-focus change as the sibling entry above (the Settings dialog opens on Operation, whose first widget is the "Freq acceleration" dropdown, animated by LVGL's 80 ms theme transition), measured on the fixtures whose base is 6 flushes: the profile window and the six theme fixtures. The delta is the same constant +8 flushes and +132610 flushed pixels, and the steady state is unchanged (the CI benchmark measures flushes_mean 1.0 -> 1.0 and redrawn_area_mean 384000 -> 384000), so it is a one-off cost at open. Measured here: 6 -> 14 flushes (+133.3%) and 1003170 -> 1135780 flushed pixels (+13.2%). Accepted by the owner on 2026-10-16 (issue #109), with budgets.yaml untouched.

- **settings-tema-redshift** accepted by `ajustes-tabs-109` until `2027-03-31` (ceilings: flushes ≤ 150.0%, flushed_pixels ≤ 15.0%): AB-SETEN109 (#109): the same initial-focus change as the sibling entry above (the Settings dialog opens on Operation, whose first widget is the "Freq acceleration" dropdown, animated by LVGL's 80 ms theme transition), measured on the fixtures whose base is 6 flushes: the profile window and the six theme fixtures. The delta is the same constant +8 flushes and +132610 flushed pixels, and the steady state is unchanged (the CI benchmark measures flushes_mean 1.0 -> 1.0 and redrawn_area_mean 384000 -> 384000), so it is a one-off cost at open. Measured here: 6 -> 14 flushes (+133.3%) and 1003170 -> 1135780 flushed pixels (+13.2%). Accepted by the owner on 2026-10-16 (issue #109), with budgets.yaml untouched.

- **settings-tema-sprint** accepted by `ajustes-tabs-109` until `2027-03-31` (ceilings: flushes ≤ 150.0%, flushed_pixels ≤ 15.0%): AB-SETEN109 (#109): the same initial-focus change as the sibling entry above (the Settings dialog opens on Operation, whose first widget is the "Freq acceleration" dropdown, animated by LVGL's 80 ms theme transition), measured on the fixtures whose base is 6 flushes: the profile window and the six theme fixtures. The delta is the same constant +8 flushes and +132610 flushed pixels, and the steady state is unchanged (the CI benchmark measures flushes_mean 1.0 -> 1.0 and redrawn_area_mean 384000 -> 384000), so it is a one-off cost at open. Measured here: 6 -> 14 flushes (+133.3%) and 1003170 -> 1135780 flushed pixels (+13.2%). Accepted by the owner on 2026-10-16 (issue #109), with budgets.yaml untouched.

- **settings-tema-sunlit** accepted by `ajustes-tabs-109` until `2027-03-31` (ceilings: flushes ≤ 150.0%, flushed_pixels ≤ 15.0%): AB-SETEN109 (#109): the same initial-focus change as the sibling entry above (the Settings dialog opens on Operation, whose first widget is the "Freq acceleration" dropdown, animated by LVGL's 80 ms theme transition), measured on the fixtures whose base is 6 flushes: the profile window and the six theme fixtures. The delta is the same constant +8 flushes and +132610 flushed pixels, and the steady state is unchanged (the CI benchmark measures flushes_mean 1.0 -> 1.0 and redrawn_area_mean 384000 -> 384000), so it is a one-off cost at open. Measured here: 6 -> 14 flushes (+133.3%) and 1003170 -> 1135780 flushed pixels (+13.2%). Accepted by the owner on 2026-10-16 (issue #109), with budgets.yaml untouched.

- **settings-tema-trail** accepted by `ajustes-tabs-109` until `2027-03-31` (ceilings: flushes ≤ 150.0%, flushed_pixels ≤ 15.0%): AB-SETEN109 (#109): the same initial-focus change as the sibling entry above (the Settings dialog opens on Operation, whose first widget is the "Freq acceleration" dropdown, animated by LVGL's 80 ms theme transition), measured on the fixtures whose base is 6 flushes: the profile window and the six theme fixtures. The delta is the same constant +8 flushes and +132610 flushed pixels, and the steady state is unchanged (the CI benchmark measures flushes_mean 1.0 -> 1.0 and redrawn_area_mean 384000 -> 384000), so it is a one-off cost at open. Measured here: 6 -> 14 flushes (+133.3%) and 1003170 -> 1135780 flushed pixels (+13.2%). Accepted by the owner on 2026-10-16 (issue #109), with budgets.yaml untouched.

- **settings-wfbat** accepted by `ajustes-tabs-109` until `2027-03-31` (ceilings: flushes ≤ 130.0%, flushed_pixels ≤ 15.0%): AB-SETEN109 (#109): reordering the Settings tabs moved the dialog's initial focus from the Clock tab's date spinbox to Operation, whose first focusable widget is the "Freq acceleration" dropdown (make_freq_accel, src/dialog_settings.c). LVGL animates the focus style change with the theme's default transition — 80 ms, LV_THEME_DEFAULT_TRANSITION_TIME in lv_conf.h — so the first open of the window costs a constant +8 flushes and +132610 flushed pixels on every fixture that renders it, whatever the navigation. The steady state is unchanged (the CI benchmark measures flushes_mean 1.0 -> 1.0 and redrawn_area_mean 384000 -> 384000 on the same fixtures), so this is a one-off cost at open, not an operating regression. Measured here: 7 -> 15 flushes (+114.3%) and 1135170 -> 1267780 flushed pixels (+11.7%). Accepted by the owner on 2026-10-16 (issue #109), with budgets.yaml untouched.

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
| home-bateria-externa | yes | 0 | 0.000% | 0 | — |
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
| settings-audio | no | 21589 | 5.622% | 17 | espectro, frecuencia, botones, ajustes_anio, ajustes_scrollbar, ajustes_dialogo |
| settings-bands | yes | 0 | 0.000% | 0 | — |
| settings-home | no | 64387 | 16.767% | 205 | espectro, frecuencia, waterfall, botones, ajustes_anio, ajustes_perfil, ajustes_scrollbar, ajustes_audio_ganancia, ajustes_tema, ajustes_dialogo |
| settings-perfil | no | 21688 | 5.648% | 17 | waterfall, botones, ajustes_scrollbar, ajustes_dialogo |
| settings-tema-aurora | no | 21688 | 5.648% | 17 | waterfall, botones, ajustes_scrollbar, ajustes_dialogo |
| settings-tema-cathode | no | 21899 | 5.703% | 17 | waterfall, botones, ajustes_scrollbar, ajustes_dialogo |
| settings-tema-redshift | no | 20949 | 5.455% | 15 | waterfall, botones, ajustes_scrollbar, ajustes_dialogo |
| settings-tema-sprint | no | 21438 | 5.583% | 14 | waterfall, botones, ajustes_scrollbar, ajustes_dialogo |
| settings-tema-sunlit | no | 22126 | 5.762% | 16 | waterfall, botones, ajustes_scrollbar, ajustes_dialogo |
| settings-tema-trail | no | 21896 | 5.702% | 17 | waterfall, botones, ajustes_scrollbar, ajustes_dialogo |
| settings-wfbat | no | 21790 | 5.674% | 21 | waterfall, botones, ajustes_scrollbar, ajustes_dialogo |

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

- RTC / system clock (time())=SYNTHETIC, audio.c=SYNTHETIC, audio_enhancement.c=SYNTHETIC, backlight.c=SYNTHETIC, band_info.c=PARTIAL, bt_state.c=SYNTHETIC, btaudio.c=SYNTHETIC, cfg (catalogue)=SYNTHETIC, cfg/transverter.c=SYNTHETIC, cw_tune_ui.c=PARTIAL, main_screen.c (ACTION_APP_BANDS)=PARTIAL, main_screen.c (ACTION_APP_RTTY close)=PARTIAL, main_screen.c (APP key -> buttons_group_app)=PARTIAL, meter.c (S-meter)=PARTIAL, params=SYNTHETIC, radio=SYNTHETIC, rtty.c=SYNTHETIC, spectrum.c=PARTIAL, waterfall.c=PARTIAL, wfview_server.c=SYNTHETIC

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

663 contract checks, 0 failure(s), 0 unchanged baseline defect(s).

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
