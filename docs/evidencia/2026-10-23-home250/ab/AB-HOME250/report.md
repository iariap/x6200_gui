# A/B UI report — AB-HOME250

- Date: 2026-10-23
- Profile: `host-ui`
- A (baseline): `8549891e2cfafeaa970afd582bdacd136601a5d0` (a)
- B (candidate): `65f7842c29f124728556d8c54b5644b8ffdefe0a` (b)
- Scenarios: `app-bands`, `app-close-home`, `apps-menu-1`, `apps-menu-2`, `apps-menu-3`, `apps-menu-4`, `apps-menu-5`, `apps-menu-6`, `autotune`, `autotune-found`, `autotune-notcw`, `autotune-progress`, `bench-render`, `bt-dialog`, `bt-dialog-tema-aurora`, `bt-dialog-tema-cathode`, `bt-dialog-tema-redshift`, `bt-dialog-tema-sprint`, `bt-dialog-tema-sunlit`, `bt-dialog-tema-trail`, `cw-autotune-off`, `cw-autotune-on`, `cw-tune-default`, `cw-tune-ssb`, `cw-tune-tema-sunlit`, `cw-tune-tuner`, `dfl-editor-cycles`, `dfl-editor-interaction`, `dfl-editor-lsb-d`, `dfl-editor-reset`, `dfl-editor-rotary0`, `dfl-editor-sel-bw-tema-aurora`, `dfl-editor-sel-bw-tema-cathode`, `dfl-editor-sel-bw-tema-redshift`, `dfl-editor-sel-bw-tema-sprint`, `dfl-editor-sel-bw-tema-sunlit`, `dfl-editor-sel-bw-tema-trail`, `dfl-editor-sel-high-tema-aurora`, `dfl-editor-sel-high-tema-cathode`, `dfl-editor-sel-high-tema-redshift`, `dfl-editor-sel-high-tema-sprint`, `dfl-editor-sel-high-tema-sunlit`, `dfl-editor-sel-high-tema-trail`, `dfl-editor-tx`, `dfl-editor-usb-d`, `dfl-editor-usb-d-tema-aurora`, `dfl-editor-usb-d-tema-cathode`, `dfl-editor-usb-d-tema-redshift`, `dfl-editor-usb-d-tema-sprint`, `dfl-editor-usb-d-tema-sunlit`, `dfl-editor-usb-d-tema-trail`, `dfl-lsb-d`, `dfl-mirror-lsb`, `dfl-mirror-usb`, `dfl-page-tema-aurora`, `dfl-page-tema-cathode`, `dfl-page-tema-redshift`, `dfl-page-tema-sprint`, `dfl-page-tema-sunlit`, `dfl-page-tema-trail`, `dfl-presets-activo`, `dfl-presets-adj-tema-aurora`, `dfl-presets-adj-tema-cathode`, `dfl-presets-adj-tema-redshift`, `dfl-presets-adj-tema-sprint`, `dfl-presets-adj-tema-sunlit`, `dfl-presets-adj-tema-trail`, `dfl-presets-apply`, `dfl-presets-assign-cancel`, `dfl-presets-assign-ok`, `dfl-presets-assign-tema-aurora`, `dfl-presets-assign-tema-cathode`, `dfl-presets-assign-tema-redshift`, `dfl-presets-assign-tema-sprint`, `dfl-presets-assign-tema-sunlit`, `dfl-presets-assign-tema-trail`, `dfl-presets-confirm-tema-aurora`, `dfl-presets-confirm-tema-cathode`, `dfl-presets-confirm-tema-redshift`, `dfl-presets-confirm-tema-sprint`, `dfl-presets-confirm-tema-sunlit`, `dfl-presets-confirm-tema-trail`, `dfl-presets-custom`, `dfl-presets-cycles`, `dfl-presets-digi`, `dfl-presets-fav-tema-aurora`, `dfl-presets-fav-tema-cathode`, `dfl-presets-fav-tema-redshift`, `dfl-presets-fav-tema-sprint`, `dfl-presets-fav-tema-sunlit`, `dfl-presets-fav-tema-trail`, `dfl-presets-graph-tema-aurora`, `dfl-presets-graph-tema-cathode`, `dfl-presets-graph-tema-redshift`, `dfl-presets-graph-tema-sprint`, `dfl-presets-graph-tema-sunlit`, `dfl-presets-graph-tema-trail`, `dfl-presets-mode-change`, `dfl-presets-more-tema-aurora`, `dfl-presets-more-tema-cathode`, `dfl-presets-more-tema-redshift`, `dfl-presets-more-tema-sprint`, `dfl-presets-more-tema-sunlit`, `dfl-presets-more-tema-trail`, `dfl-presets-nav`, `dfl-presets-nfm`, `dfl-presets-rotary0`, `dfl-presets-save`, `dfl-presets-save-cancel`, `dfl-presets-save-fail`, `dfl-presets-touch`, `dfl-presets-touch-hold`, `dfl-presets-tx`, `dfl-presets-vol`, `dfl-usb-d`, `dfl-usb-d-span100k`, `dfl-usb-d-span50k`, `dfl-usb-d-tema-cathode`, `dfl-usb-d-tema-redshift`, `dfl-usb-d-tema-sprint`, `dfl-usb-d-tema-sunlit`, `dfl-usb-d-tema-trail`, `home-bateria-externa`, `home-cb`, `home-cb-a`, `home-cb-cathode`, `home-cb-d`, `home-cb-offchannel`, `home-frecuencia-extrema`, `home-usb-20m`, `main-bateria-baja`, `main-espectro-agua`, `main-foco`, `main-frecuencia-dseg`, `main-natural`, `main-natural-eco`, `main-tema-aurora`, `main-tema-aurora-agua`, `main-tema-aurora-eco`, `main-tema-aurora-tx`, `main-tema-cathode`, `main-tema-cathode-agua`, `main-tema-cathode-eco`, `main-tema-cathode-tx`, `main-tema-redshift`, `main-tema-redshift-agua`, `main-tema-redshift-eco`, `main-tema-redshift-tx`, `main-tema-sprint`, `main-tema-sprint-agua`, `main-tema-sprint-eco`, `main-tema-sprint-tx`, `main-tema-sunlit`, `main-tema-sunlit-agua`, `main-tema-sunlit-eco`, `main-tema-sunlit-tx`, `main-tema-trail`, `main-tema-trail-agua`, `main-tema-trail-eco`, `main-tema-trail-tx`, `main-tx-simulado`, `settings-audio`, `settings-bands`, `settings-grupo-actions`, `settings-grupo-display`, `settings-grupo-info`, `settings-grupo-operation`, `settings-grupo-operation-tema-aurora`, `settings-grupo-operation-tema-cathode`, `settings-grupo-operation-tema-redshift`, `settings-grupo-operation-tema-sprint`, `settings-grupo-operation-tema-sunlit`, `settings-grupo-operation-tema-trail`, `settings-home`, `settings-perfil`, `settings-tema-aurora`, `settings-tema-cathode`, `settings-tema-redshift`, `settings-tema-sprint`, `settings-tema-sunlit`, `settings-tema-trail`, `settings-wfbat`, `swrscan-3pass`, `swrscan-3pass-p1`, `swrscan-3pass-p2`, `swrscan-3pass-p3`, `swrscan-3pass-stop`, `swrscan-3pass-tema-aurora`, `swrscan-3pass-tema-cathode`, `swrscan-5pass-moving`, `swrscan-5pass-outliers`, `swrscan-double-valley`, `swrscan-flat`, `swrscan-narrow-zoom`, `swrscan-owner-40m`, `swrscan-owner-40m-domo`, `swrscan-owner-40m-sweep`, `swrscan-quick`, `swrscan-wide-zoom`

## Verdict: **PASS** (exit 0)

| Gate | Required | Status | Summary |
|---|---|---|---|
| Functional | yes | PASS | 199 scenarios captured on both revisions |
| Visual Regression | yes | PASS | no unexplained visual regressions |
| Quality Contract | yes | PASS | 2058 contract checks passed on the candidate |
| Performance | yes | PASS | render-cost proxy within budget; 1 scenario(s) accepted by a dated exception |
| Hardware | no (advisory) | BLOCKED | no physical X6200 attached; hardware behaviour is not measured |
| Energy | no (advisory) | BLOCKED | no power measurement; energy evidence must be physical |

## Performance (render-cost proxy)

> x86 render counters under virtual time; not a battery measurement.

| Scenario | flushes A→B | Δ | flushed_pixels A→B | Δ | Verdict | Expires |
|---|---|---|---|---|---|---|
| main-frecuencia-dseg | 4 → 6 | +50.0% | 1096000 → 1100352 | +0.4% | PASS (by exception) | 2027-04-30 |

- **main-frecuencia-dseg** accepted by `home-clock-battery-250` until `2027-04-30` (ceilings: flushes ≤ 55.0%, flushed_pixels ≤ 1.5%): AB-HOME250 (#250): el reloj de la barra superior deja de alternar hora y alimentacion y pasa a ser permanente, con la bateria y la fuente externa en una banda de estado de dos grupos (izquierda/derecha) en la misma caja de 206/209 x 61. La actualizacion de esa banda cuando llega el pack real (el arnes publica `clock_update_power()` despues de `main_screen()`) invalida dos rectangulos (590,32 75x34 y 747,32 53x34, medidos con un `DIAGFLUSH` temporal en el flush callback del arnes, no versionado) donde el reloj de potencia viejo invalidaba uno: el settle de esta fixture pasa de 4 a 6 flushes (+50.0%) y de 1096000 a 1100352 px (+0.397%). Es un costo de ARRANQUE, no de operacion: con el reloj andando el reloj nuevo invalida menos por segundo que el viejo en esta MISMA fixture (7490 contra 15549 px por tick de 500 ms, medido con el reloj de pared avanzando con el reloj virtual), y el resto de la suite queda dentro del presupuesto (5 -> 6 flushes = +20.0%, exactamente el techo, con ~1% MENOS de pixeles). budget.yaml no se toca.

## Visual diff

| Scenario | Identical | Changed px | Area | Boxes | Regions |
|---|---|---|---|---|---|
| app-bands | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| app-close-home | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| apps-menu-1 | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| apps-menu-2 | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| apps-menu-3 | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| apps-menu-4 | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| apps-menu-5 | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| apps-menu-6 | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| autotune | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| autotune-found | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| autotune-notcw | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| autotune-progress | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| bench-render | no | 2270 | 0.591% | 20 | barra_superior, espectro, reloj_potencia |
| bt-dialog | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| bt-dialog-tema-aurora | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| bt-dialog-tema-cathode | no | 2310 | 0.602% | 16 | barra_superior, espectro, reloj_potencia |
| bt-dialog-tema-redshift | no | 1665 | 0.434% | 22 | barra_superior, espectro, reloj_potencia |
| bt-dialog-tema-sprint | no | 2208 | 0.575% | 14 | barra_superior, espectro, reloj_potencia |
| bt-dialog-tema-sunlit | no | 2510 | 0.654% | 22 | barra_superior, espectro, reloj_potencia |
| bt-dialog-tema-trail | no | 2625 | 0.684% | 21 | barra_superior, espectro, reloj_potencia |
| cw-autotune-off | no | 1665 | 0.434% | 22 | barra_superior, espectro, reloj_potencia |
| cw-autotune-on | no | 1665 | 0.434% | 22 | barra_superior, espectro, reloj_potencia |
| cw-tune-default | no | 1665 | 0.434% | 22 | barra_superior, espectro, reloj_potencia |
| cw-tune-ssb | no | 1665 | 0.434% | 22 | barra_superior, espectro, reloj_potencia |
| cw-tune-tema-sunlit | no | 2510 | 0.654% | 22 | barra_superior, espectro, reloj_potencia |
| cw-tune-tuner | no | 1665 | 0.434% | 22 | barra_superior, espectro, reloj_potencia |
| dfl-editor-cycles | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-editor-interaction | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-editor-lsb-d | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-editor-reset | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-editor-rotary0 | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-editor-sel-bw-tema-aurora | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-editor-sel-bw-tema-cathode | no | 2310 | 0.602% | 16 | barra_superior, espectro, reloj_potencia |
| dfl-editor-sel-bw-tema-redshift | no | 1665 | 0.434% | 22 | barra_superior, espectro, reloj_potencia |
| dfl-editor-sel-bw-tema-sprint | no | 2208 | 0.575% | 14 | barra_superior, espectro, reloj_potencia |
| dfl-editor-sel-bw-tema-sunlit | no | 2510 | 0.654% | 22 | barra_superior, espectro, reloj_potencia |
| dfl-editor-sel-bw-tema-trail | no | 2625 | 0.684% | 21 | barra_superior, espectro, reloj_potencia |
| dfl-editor-sel-high-tema-aurora | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-editor-sel-high-tema-cathode | no | 2310 | 0.602% | 16 | barra_superior, espectro, reloj_potencia |
| dfl-editor-sel-high-tema-redshift | no | 1665 | 0.434% | 22 | barra_superior, espectro, reloj_potencia |
| dfl-editor-sel-high-tema-sprint | no | 2208 | 0.575% | 14 | barra_superior, espectro, reloj_potencia |
| dfl-editor-sel-high-tema-sunlit | no | 2510 | 0.654% | 22 | barra_superior, espectro, reloj_potencia |
| dfl-editor-sel-high-tema-trail | no | 2625 | 0.684% | 21 | barra_superior, espectro, reloj_potencia |
| dfl-editor-tx | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-editor-usb-d | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-editor-usb-d-tema-aurora | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-editor-usb-d-tema-cathode | no | 2310 | 0.602% | 16 | barra_superior, espectro, reloj_potencia |
| dfl-editor-usb-d-tema-redshift | no | 1665 | 0.434% | 22 | barra_superior, espectro, reloj_potencia |
| dfl-editor-usb-d-tema-sprint | no | 2208 | 0.575% | 14 | barra_superior, espectro, reloj_potencia |
| dfl-editor-usb-d-tema-sunlit | no | 2510 | 0.654% | 22 | barra_superior, espectro, reloj_potencia |
| dfl-editor-usb-d-tema-trail | no | 2625 | 0.684% | 21 | barra_superior, espectro, reloj_potencia |
| dfl-lsb-d | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-mirror-lsb | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-mirror-usb | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-page-tema-aurora | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-page-tema-cathode | no | 2310 | 0.602% | 16 | barra_superior, espectro, reloj_potencia |
| dfl-page-tema-redshift | no | 1665 | 0.434% | 22 | barra_superior, espectro, reloj_potencia |
| dfl-page-tema-sprint | no | 2208 | 0.575% | 14 | barra_superior, espectro, reloj_potencia |
| dfl-page-tema-sunlit | no | 2510 | 0.654% | 22 | barra_superior, espectro, reloj_potencia |
| dfl-page-tema-trail | no | 2625 | 0.684% | 21 | barra_superior, espectro, reloj_potencia |
| dfl-presets-activo | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-presets-adj-tema-aurora | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-presets-adj-tema-cathode | no | 2310 | 0.602% | 16 | barra_superior, espectro, reloj_potencia |
| dfl-presets-adj-tema-redshift | no | 1665 | 0.434% | 22 | barra_superior, espectro, reloj_potencia |
| dfl-presets-adj-tema-sprint | no | 2208 | 0.575% | 14 | barra_superior, espectro, reloj_potencia |
| dfl-presets-adj-tema-sunlit | no | 2510 | 0.654% | 22 | barra_superior, espectro, reloj_potencia |
| dfl-presets-adj-tema-trail | no | 2625 | 0.684% | 21 | barra_superior, espectro, reloj_potencia |
| dfl-presets-apply | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-presets-assign-cancel | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-presets-assign-ok | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-presets-assign-tema-aurora | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-presets-assign-tema-cathode | no | 2310 | 0.602% | 16 | barra_superior, espectro, reloj_potencia |
| dfl-presets-assign-tema-redshift | no | 1665 | 0.434% | 22 | barra_superior, espectro, reloj_potencia |
| dfl-presets-assign-tema-sprint | no | 2208 | 0.575% | 14 | barra_superior, espectro, reloj_potencia |
| dfl-presets-assign-tema-sunlit | no | 2510 | 0.654% | 22 | barra_superior, espectro, reloj_potencia |
| dfl-presets-assign-tema-trail | no | 2625 | 0.684% | 21 | barra_superior, espectro, reloj_potencia |
| dfl-presets-confirm-tema-aurora | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-presets-confirm-tema-cathode | no | 2310 | 0.602% | 16 | barra_superior, espectro, reloj_potencia |
| dfl-presets-confirm-tema-redshift | no | 1665 | 0.434% | 22 | barra_superior, espectro, reloj_potencia |
| dfl-presets-confirm-tema-sprint | no | 2208 | 0.575% | 14 | barra_superior, espectro, reloj_potencia |
| dfl-presets-confirm-tema-sunlit | no | 2510 | 0.654% | 22 | barra_superior, espectro, reloj_potencia |
| dfl-presets-confirm-tema-trail | no | 2625 | 0.684% | 21 | barra_superior, espectro, reloj_potencia |
| dfl-presets-custom | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-presets-cycles | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-presets-digi | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-presets-fav-tema-aurora | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-presets-fav-tema-cathode | no | 2310 | 0.602% | 16 | barra_superior, espectro, reloj_potencia |
| dfl-presets-fav-tema-redshift | no | 1665 | 0.434% | 22 | barra_superior, espectro, reloj_potencia |
| dfl-presets-fav-tema-sprint | no | 2208 | 0.575% | 14 | barra_superior, espectro, reloj_potencia |
| dfl-presets-fav-tema-sunlit | no | 2510 | 0.654% | 22 | barra_superior, espectro, reloj_potencia |
| dfl-presets-fav-tema-trail | no | 2625 | 0.684% | 21 | barra_superior, espectro, reloj_potencia |
| dfl-presets-graph-tema-aurora | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-presets-graph-tema-cathode | no | 2310 | 0.602% | 16 | barra_superior, espectro, reloj_potencia |
| dfl-presets-graph-tema-redshift | no | 1665 | 0.434% | 22 | barra_superior, espectro, reloj_potencia |
| dfl-presets-graph-tema-sprint | no | 2208 | 0.575% | 14 | barra_superior, espectro, reloj_potencia |
| dfl-presets-graph-tema-sunlit | no | 2510 | 0.654% | 22 | barra_superior, espectro, reloj_potencia |
| dfl-presets-graph-tema-trail | no | 2625 | 0.684% | 21 | barra_superior, espectro, reloj_potencia |
| dfl-presets-mode-change | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-presets-more-tema-aurora | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-presets-more-tema-cathode | no | 2310 | 0.602% | 16 | barra_superior, espectro, reloj_potencia |
| dfl-presets-more-tema-redshift | no | 1665 | 0.434% | 22 | barra_superior, espectro, reloj_potencia |
| dfl-presets-more-tema-sprint | no | 2208 | 0.575% | 14 | barra_superior, espectro, reloj_potencia |
| dfl-presets-more-tema-sunlit | no | 2510 | 0.654% | 22 | barra_superior, espectro, reloj_potencia |
| dfl-presets-more-tema-trail | no | 2625 | 0.684% | 21 | barra_superior, espectro, reloj_potencia |
| dfl-presets-nav | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-presets-nfm | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-presets-rotary0 | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-presets-save | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-presets-save-cancel | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-presets-save-fail | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-presets-touch | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-presets-touch-hold | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-presets-tx | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-presets-vol | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-usb-d | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-usb-d-span100k | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-usb-d-span50k | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| dfl-usb-d-tema-cathode | no | 2310 | 0.602% | 16 | barra_superior, espectro, reloj_potencia |
| dfl-usb-d-tema-redshift | no | 1665 | 0.434% | 22 | barra_superior, espectro, reloj_potencia |
| dfl-usb-d-tema-sprint | no | 2208 | 0.575% | 14 | barra_superior, espectro, reloj_potencia |
| dfl-usb-d-tema-sunlit | no | 2510 | 0.654% | 22 | barra_superior, espectro, reloj_potencia |
| dfl-usb-d-tema-trail | no | 2625 | 0.684% | 21 | barra_superior, espectro, reloj_potencia |
| home-bateria-externa | no | 2413 | 0.628% | 24 | barra_superior, espectro, reloj_potencia |
| home-cb | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| home-cb-a | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| home-cb-cathode | no | 2310 | 0.602% | 16 | barra_superior, espectro, reloj_potencia |
| home-cb-d | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| home-cb-offchannel | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| home-frecuencia-extrema | no | 2426 | 0.632% | 20 | barra_superior, espectro, reloj_potencia |
| home-usb-20m | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| main-bateria-baja | no | 1601 | 0.417% | 12 | barra_superior, espectro, reloj_potencia |
| main-espectro-agua | no | 2270 | 0.591% | 20 | barra_superior, espectro, reloj_potencia |
| main-foco | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| main-frecuencia-dseg | no | 2089 | 0.544% | 22 | barra_superior, espectro, reloj_potencia |
| main-natural | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| main-natural-eco | no | 1757 | 0.458% | 13 | barra_superior, espectro, reloj_potencia |
| main-tema-aurora | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| main-tema-aurora-agua | no | 2270 | 0.591% | 20 | barra_superior, espectro, reloj_potencia |
| main-tema-aurora-eco | no | 1601 | 0.417% | 12 | barra_superior, espectro, reloj_potencia |
| main-tema-aurora-tx | no | 2272 | 0.592% | 22 | barra_superior, espectro, reloj_potencia |
| main-tema-cathode | no | 2310 | 0.602% | 16 | barra_superior, espectro, reloj_potencia |
| main-tema-cathode-agua | no | 2344 | 0.610% | 13 | barra_superior, espectro, reloj_potencia |
| main-tema-cathode-eco | no | 1655 | 0.431% | 5 | barra_superior, espectro, reloj_potencia |
| main-tema-cathode-tx | no | 2357 | 0.614% | 12 | barra_superior, espectro, reloj_potencia |
| main-tema-redshift | no | 1665 | 0.434% | 22 | barra_superior, espectro, reloj_potencia |
| main-tema-redshift-agua | no | 1699 | 0.442% | 22 | barra_superior, espectro, reloj_potencia |
| main-tema-redshift-eco | no | 1258 | 0.328% | 14 | barra_superior, espectro, reloj_potencia |
| main-tema-redshift-tx | no | 1698 | 0.442% | 22 | barra_superior, espectro, reloj_potencia |
| main-tema-sprint | no | 2208 | 0.575% | 14 | barra_superior, espectro, reloj_potencia |
| main-tema-sprint-agua | no | 2237 | 0.583% | 11 | barra_superior, espectro, reloj_potencia |
| main-tema-sprint-eco | no | 1624 | 0.423% | 5 | barra_superior, espectro, reloj_potencia |
| main-tema-sprint-tx | no | 2258 | 0.588% | 10 | barra_superior, espectro, reloj_potencia |
| main-tema-sunlit | no | 2510 | 0.654% | 22 | barra_superior, espectro, reloj_potencia |
| main-tema-sunlit-agua | no | 2563 | 0.667% | 21 | barra_superior, espectro, reloj_potencia |
| main-tema-sunlit-eco | no | 2028 | 0.528% | 12 | barra_superior, espectro, reloj_potencia |
| main-tema-sunlit-tx | no | 2580 | 0.672% | 20 | barra_superior, espectro, reloj_potencia |
| main-tema-trail | no | 2625 | 0.684% | 21 | barra_superior, espectro, reloj_potencia |
| main-tema-trail-agua | no | 2647 | 0.689% | 21 | barra_superior, espectro, reloj_potencia |
| main-tema-trail-eco | no | 2084 | 0.543% | 13 | barra_superior, espectro, reloj_potencia |
| main-tema-trail-tx | no | 2661 | 0.693% | 21 | barra_superior, espectro, reloj_potencia |
| main-tx-simulado | no | 2272 | 0.592% | 22 | barra_superior, espectro, reloj_potencia |
| settings-audio | no | 2333 | 0.608% | 27 | barra_superior, espectro, frecuencia, waterfall, reloj_potencia, ajustes_scrollbar, ajustes_dialogo |
| settings-bands | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| settings-grupo-actions | no | 2305 | 0.600% | 27 | barra_superior, espectro, waterfall, reloj_potencia, ajustes_scrollbar, ajustes_dialogo, dfl_inline |
| settings-grupo-display | no | 2347 | 0.611% | 27 | barra_superior, espectro, waterfall, reloj_potencia, ajustes_scrollbar, ajustes_dialogo, dfl_inline |
| settings-grupo-info | no | 2273 | 0.592% | 26 | barra_superior, espectro, waterfall, reloj_potencia, ajustes_scrollbar, ajustes_dialogo, dfl_inline |
| settings-grupo-operation | no | 2273 | 0.592% | 26 | barra_superior, espectro, reloj_potencia, ajustes_anio, ajustes_scrollbar, ajustes_dialogo |
| settings-grupo-operation-tema-aurora | no | 2273 | 0.592% | 26 | barra_superior, espectro, reloj_potencia, ajustes_anio, ajustes_scrollbar, ajustes_dialogo |
| settings-grupo-operation-tema-cathode | no | 2321 | 0.604% | 17 | barra_superior, espectro, reloj_potencia, ajustes_anio, ajustes_scrollbar, ajustes_dialogo |
| settings-grupo-operation-tema-redshift | no | 1676 | 0.436% | 23 | barra_superior, espectro, reloj_potencia, ajustes_anio, ajustes_scrollbar, ajustes_dialogo |
| settings-grupo-operation-tema-sprint | no | 2219 | 0.578% | 15 | barra_superior, espectro, reloj_potencia, ajustes_anio, ajustes_scrollbar, ajustes_dialogo |
| settings-grupo-operation-tema-sunlit | no | 2521 | 0.657% | 23 | barra_superior, espectro, reloj_potencia, ajustes_anio, ajustes_scrollbar, ajustes_dialogo |
| settings-grupo-operation-tema-trail | no | 2636 | 0.686% | 22 | barra_superior, espectro, reloj_potencia, ajustes_anio, ajustes_scrollbar, ajustes_dialogo |
| settings-home | no | 27011 | 7.034% | 131 | barra_superior, espectro, waterfall_ticks, waterfall, reloj_potencia, ajustes_perfil, ajustes_scrollbar, ajustes_dialogo, dfl_inline |
| settings-perfil | no | 1825 | 0.475% | 15 | barra_superior, espectro, waterfall, reloj_potencia, ajustes_scrollbar, ajustes_dialogo, dfl_inline |
| settings-tema-aurora | no | 1825 | 0.475% | 15 | barra_superior, espectro, waterfall, reloj_potencia, ajustes_scrollbar, ajustes_dialogo, dfl_inline |
| settings-tema-cathode | no | 1858 | 0.484% | 5 | barra_superior, espectro, waterfall, reloj_potencia, ajustes_scrollbar, ajustes_dialogo, dfl_inline |
| settings-tema-redshift | no | 1325 | 0.345% | 16 | barra_superior, espectro, waterfall, reloj_potencia, ajustes_scrollbar, ajustes_dialogo, dfl_inline |
| settings-tema-sprint | no | 1826 | 0.476% | 5 | barra_superior, espectro, waterfall, reloj_potencia, ajustes_scrollbar, ajustes_dialogo, dfl_inline |
| settings-tema-sunlit | no | 2113 | 0.550% | 13 | barra_superior, espectro, waterfall, reloj_potencia, ajustes_scrollbar, ajustes_dialogo, dfl_inline |
| settings-tema-trail | no | 2163 | 0.563% | 13 | barra_superior, espectro, waterfall, reloj_potencia, ajustes_scrollbar, ajustes_dialogo, dfl_inline |
| settings-wfbat | no | 2347 | 0.611% | 27 | barra_superior, espectro, waterfall_ticks, waterfall, reloj_potencia, ajustes_scrollbar, ajustes_dialogo |
| swrscan-3pass | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| swrscan-3pass-p1 | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| swrscan-3pass-p2 | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| swrscan-3pass-p3 | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| swrscan-3pass-stop | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| swrscan-3pass-tema-aurora | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| swrscan-3pass-tema-cathode | no | 2310 | 0.602% | 16 | barra_superior, espectro, reloj_potencia |
| swrscan-5pass-moving | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| swrscan-5pass-outliers | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| swrscan-double-valley | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| swrscan-flat | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| swrscan-narrow-zoom | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| swrscan-owner-40m | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| swrscan-owner-40m-domo | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| swrscan-owner-40m-sweep | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| swrscan-quick | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |
| swrscan-wide-zoom | no | 2262 | 0.589% | 25 | barra_superior, espectro, reloj_potencia |

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
| autotune | 18 | 4 | 3 | 7 |
| autotune-found | 18 | 4 | 3 | 7 |
| autotune-notcw | 18 | 4 | 3 | 7 |
| autotune-progress | 18 | 4 | 3 | 7 |
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
| dfl-editor-reset | 15 | 4 | 3 | 3 |
| dfl-editor-rotary0 | 15 | 4 | 3 | 3 |
| dfl-editor-sel-bw-tema-aurora | 15 | 4 | 3 | 3 |
| dfl-editor-sel-bw-tema-cathode | 15 | 4 | 3 | 3 |
| dfl-editor-sel-bw-tema-redshift | 15 | 4 | 3 | 3 |
| dfl-editor-sel-bw-tema-sprint | 15 | 4 | 3 | 3 |
| dfl-editor-sel-bw-tema-sunlit | 15 | 4 | 3 | 3 |
| dfl-editor-sel-bw-tema-trail | 15 | 4 | 3 | 3 |
| dfl-editor-sel-high-tema-aurora | 15 | 4 | 3 | 3 |
| dfl-editor-sel-high-tema-cathode | 15 | 4 | 3 | 3 |
| dfl-editor-sel-high-tema-redshift | 15 | 4 | 3 | 3 |
| dfl-editor-sel-high-tema-sprint | 15 | 4 | 3 | 3 |
| dfl-editor-sel-high-tema-sunlit | 15 | 4 | 3 | 3 |
| dfl-editor-sel-high-tema-trail | 15 | 4 | 3 | 3 |
| dfl-editor-tx | 15 | 4 | 3 | 3 |
| dfl-editor-usb-d | 15 | 4 | 3 | 3 |
| dfl-editor-usb-d-tema-aurora | 15 | 4 | 3 | 3 |
| dfl-editor-usb-d-tema-cathode | 15 | 4 | 3 | 3 |
| dfl-editor-usb-d-tema-redshift | 15 | 4 | 3 | 3 |
| dfl-editor-usb-d-tema-sprint | 15 | 4 | 3 | 3 |
| dfl-editor-usb-d-tema-sunlit | 15 | 4 | 3 | 3 |
| dfl-editor-usb-d-tema-trail | 15 | 4 | 3 | 3 |
| dfl-lsb-d | 15 | 4 | 3 | 3 |
| dfl-mirror-lsb | 15 | 4 | 3 | 3 |
| dfl-mirror-usb | 15 | 4 | 3 | 3 |
| dfl-page-tema-aurora | 15 | 4 | 3 | 3 |
| dfl-page-tema-cathode | 15 | 4 | 3 | 3 |
| dfl-page-tema-redshift | 15 | 4 | 3 | 3 |
| dfl-page-tema-sprint | 15 | 4 | 3 | 3 |
| dfl-page-tema-sunlit | 15 | 4 | 3 | 3 |
| dfl-page-tema-trail | 15 | 4 | 3 | 3 |
| dfl-presets-activo | 15 | 4 | 3 | 3 |
| dfl-presets-adj-tema-aurora | 15 | 4 | 3 | 3 |
| dfl-presets-adj-tema-cathode | 15 | 4 | 3 | 3 |
| dfl-presets-adj-tema-redshift | 15 | 4 | 3 | 3 |
| dfl-presets-adj-tema-sprint | 15 | 4 | 3 | 3 |
| dfl-presets-adj-tema-sunlit | 15 | 4 | 3 | 3 |
| dfl-presets-adj-tema-trail | 15 | 4 | 3 | 3 |
| dfl-presets-apply | 15 | 4 | 3 | 3 |
| dfl-presets-assign-cancel | 15 | 4 | 3 | 3 |
| dfl-presets-assign-ok | 15 | 4 | 3 | 3 |
| dfl-presets-assign-tema-aurora | 15 | 4 | 3 | 3 |
| dfl-presets-assign-tema-cathode | 15 | 4 | 3 | 3 |
| dfl-presets-assign-tema-redshift | 15 | 4 | 3 | 3 |
| dfl-presets-assign-tema-sprint | 15 | 4 | 3 | 3 |
| dfl-presets-assign-tema-sunlit | 15 | 4 | 3 | 3 |
| dfl-presets-assign-tema-trail | 15 | 4 | 3 | 3 |
| dfl-presets-confirm-tema-aurora | 15 | 4 | 3 | 3 |
| dfl-presets-confirm-tema-cathode | 15 | 4 | 3 | 3 |
| dfl-presets-confirm-tema-redshift | 15 | 4 | 3 | 3 |
| dfl-presets-confirm-tema-sprint | 15 | 4 | 3 | 3 |
| dfl-presets-confirm-tema-sunlit | 15 | 4 | 3 | 3 |
| dfl-presets-confirm-tema-trail | 15 | 4 | 3 | 3 |
| dfl-presets-custom | 15 | 4 | 3 | 3 |
| dfl-presets-cycles | 15 | 4 | 3 | 3 |
| dfl-presets-digi | 15 | 4 | 3 | 3 |
| dfl-presets-fav-tema-aurora | 15 | 4 | 3 | 3 |
| dfl-presets-fav-tema-cathode | 15 | 4 | 3 | 3 |
| dfl-presets-fav-tema-redshift | 15 | 4 | 3 | 3 |
| dfl-presets-fav-tema-sprint | 15 | 4 | 3 | 3 |
| dfl-presets-fav-tema-sunlit | 15 | 4 | 3 | 3 |
| dfl-presets-fav-tema-trail | 15 | 4 | 3 | 3 |
| dfl-presets-graph-tema-aurora | 15 | 4 | 3 | 3 |
| dfl-presets-graph-tema-cathode | 15 | 4 | 3 | 3 |
| dfl-presets-graph-tema-redshift | 15 | 4 | 3 | 3 |
| dfl-presets-graph-tema-sprint | 15 | 4 | 3 | 3 |
| dfl-presets-graph-tema-sunlit | 15 | 4 | 3 | 3 |
| dfl-presets-graph-tema-trail | 15 | 4 | 3 | 3 |
| dfl-presets-mode-change | 15 | 4 | 3 | 3 |
| dfl-presets-more-tema-aurora | 15 | 4 | 3 | 3 |
| dfl-presets-more-tema-cathode | 15 | 4 | 3 | 3 |
| dfl-presets-more-tema-redshift | 15 | 4 | 3 | 3 |
| dfl-presets-more-tema-sprint | 15 | 4 | 3 | 3 |
| dfl-presets-more-tema-sunlit | 15 | 4 | 3 | 3 |
| dfl-presets-more-tema-trail | 15 | 4 | 3 | 3 |
| dfl-presets-nav | 15 | 4 | 3 | 3 |
| dfl-presets-nfm | 15 | 4 | 3 | 3 |
| dfl-presets-rotary0 | 15 | 4 | 3 | 3 |
| dfl-presets-save | 15 | 4 | 3 | 3 |
| dfl-presets-save-cancel | 15 | 4 | 3 | 3 |
| dfl-presets-save-fail | 15 | 4 | 3 | 3 |
| dfl-presets-touch | 15 | 4 | 3 | 3 |
| dfl-presets-touch-hold | 15 | 4 | 3 | 3 |
| dfl-presets-tx | 15 | 4 | 3 | 3 |
| dfl-presets-vol | 15 | 4 | 3 | 3 |
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

- RTC / system clock (time())=SYNTHETIC, acquisition (fake backend)=PARTIAL, audio.c=SYNTHETIC, audio_enhancement.c=SYNTHETIC, backlight.c=SYNTHETIC, band_info.c=PARTIAL, bt_state.c=SYNTHETIC, btaudio.c=SYNTHETIC, cfg (catalogue)=SYNTHETIC, cfg.cw_decoder_auto=SYNTHETIC, cfg/transverter.c=SYNTHETIC, cw_tune_ui.c=PARTIAL, main_screen.c (ACTION_APP_BANDS)=PARTIAL, main_screen.c (ACTION_APP_RTTY close)=PARTIAL, main_screen.c (APP key -> buttons_group_app)=PARTIAL, main_screen.c (KEY key -> buttons_group_key)=PARTIAL, multipass TX gate=PARTIAL, params=SYNTHETIC, radio=SYNTHETIC, roe_curve.c=PARTIAL, rtty.c=SYNTHETIC, scheduler=SYNTHETIC, settle window / timeouts=SYNTHETIC, spectrum.c=PARTIAL, waterfall.c=PARTIAL, wfview_server.c=SYNTHETIC

## Known issues

| Issue | Fixture | Severity | Status | A | B | Classification |
|---|---|---|---|---|---|---|
| UI-001 | home-usb-20m | major | fixed | no | no | NOT_REPRODUCED |
| UI-009 | home-bateria-externa | major | fixed | no | no | NOT_REPRODUCED |
| UI-015 | settings-wfbat | minor | verified | — | — | UNVERIFIED |
| UI-016 | settings-home | minor | fixed | — | — | UNVERIFIED |
| UI-017 | main-frecuencia-dseg | minor | verified | — | — | UNVERIFIED |
| UI-018 | dfl-presets-rotary0 | minor | verified | — | — | UNVERIFIED |
| UI-002 | apps-menu-1 | major | fixed | — | — | UNVERIFIED |
| UI-003 | home-usb-20m | major | unverified | — | — | UNVERIFIED |
| UI-004 | settings-home | major | fixed | — | — | UNVERIFIED |
| UI-005 | bt-dialog | minor | fixed | — | — | UNVERIFIED |
| UI-006 | home-usb-20m | minor | unverified | — | — | UNVERIFIED |
| UI-008 | home-usb-20m | info | out-of-scope | — | — | UNVERIFIED |

10 issue(s) are unverified: recorded from the repository but with no detector yet, so they neither block nor pass. They stay in the annex until the harness reproduces them.

## Quality contracts

2058 contract checks, 0 failure(s), 0 unchanged baseline defect(s).

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
