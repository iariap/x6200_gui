# Render benchmark — BENCH-OPT3-149

- Status: **INCONCLUSIVE** (exit 3)
- Date: 2026-10-19
- a: `a69ea2883328dc4df4c993e3e83d48f473e9617d`
- b: `75a48b8966f31db3bdd5cdf3a98a09080053cc9b`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| app-bands | PASS | 3.1936 | 3.4204 | 3.4389 | 313.13 | -15.765% | 0.0% | 0.0% |
| app-close-home | PASS | 3.2873 | 3.4082 | 3.4118 | 304.2 | -5.067% | 0.0% | 0.0% |
| apps-menu-1 | PASS | 3.2807 | 3.6444 | 3.8423 | 304.81 | 6.423% | 0.0% | 0.0% |
| apps-menu-2 | PASS | 3.2649 | 3.4238 | 3.5192 | 306.28 | -24.465% | 0.0% | 0.0% |
| apps-menu-3 | PASS | 3.296 | 3.7737 | 4.0171 | 303.4 | 1.801% | 0.0% | 0.0% |
| apps-menu-4 | PASS | 3.2788 | 3.3579 | 3.4437 | 304.99 | -17.991% | 0.0% | 0.0% |
| apps-menu-5 | PASS | 3.2361 | 3.3861 | 4.5268 | 309.02 | 1.21% | 0.0% | 0.0% |
| apps-menu-6 | PASS | 3.303 | 3.66 | 3.8926 | 302.76 | 5.832% | 0.0% | 0.0% |
| autotune | INCONCLUSIVE | 3.3537 | 4.3593 | 4.7408 | 298.18 | 26.895% | 0.0% | 0.0% |
| autotune-found | PASS | 3.3136 | 4.0186 | 5.0732 | 301.79 | 18.9% | 0.0% | 0.0% |
| autotune-progress | PASS | 3.3434 | 3.5857 | 3.5987 | 299.1 | -3.177% | 0.0% | 0.0% |
| bench-render | INCONCLUSIVE | 3.5847 | 4.6415 | 5.2071 | 278.96 | 27.566% | 0.0% | 0.0% |
| bt-dialog | PASS | 6.6789 | 7.0558 | 7.2694 | 149.73 | -12.822% | 0.0% | 0.0% |
| bt-dialog-tema-aurora | PASS | 6.6341 | 7.0384 | 7.0787 | 150.74 | -7.027% | 0.0% | 0.0% |
| bt-dialog-tema-cathode | PASS | 6.5499 | 9.8249 | 9.9975 | 152.67 | 18.9% | 0.0% | 0.0% |
| bt-dialog-tema-redshift | INCONCLUSIVE | 6.5737 | 11.4354 | 12.4983 | 152.12 | 59.157% | 0.0% | 0.0% |
| bt-dialog-tema-sprint | INCONCLUSIVE | 6.309 | 10.845 | 11.3296 | 158.5 | 54.846% | 0.0% | 0.0% |
| bt-dialog-tema-sunlit | INCONCLUSIVE | 6.7662 | 10.2155 | 10.6484 | 147.79 | 31.612% | 0.0% | 0.0% |
| bt-dialog-tema-trail | INCONCLUSIVE | 6.676 | 10.2271 | 10.6729 | 149.79 | 53.183% | 0.0% | 0.0% |
| cw-autotune-off | INCONCLUSIVE | 3.4177 | 5.0235 | 5.0547 | 292.59 | 43.815% | 0.0% | 0.0% |
| cw-autotune-on | INCONCLUSIVE | 3.2687 | 5.4962 | 5.5791 | 305.93 | 56.579% | 0.0% | 0.0% |
| cw-tune-default | INCONCLUSIVE | 3.3435 | 5.5079 | 5.5625 | 299.09 | 57.349% | 0.0% | 0.0% |
| cw-tune-ssb | PASS | 3.3462 | 5.0965 | 5.1573 | 298.84 | 23.526% | 0.0% | 0.0% |
| cw-tune-tema-sunlit | INCONCLUSIVE | 3.3557 | 5.5981 | 5.7574 | 298.0 | 65.828% | 0.0% | 0.0% |
| cw-tune-tuner | INCONCLUSIVE | 3.2641 | 5.8161 | 5.8646 | 306.36 | 69.495% | 0.0% | 0.0% |
| home-bateria-externa | INCONCLUSIVE | 3.3677 | 5.6886 | 5.8444 | 296.94 | 64.59% | 0.0% | 0.0% |
| home-frecuencia-extrema | INCONCLUSIVE | 3.2778 | 5.112 | 6.1514 | 305.09 | 32.836% | 0.0% | 0.0% |
| home-usb-20m | INCONCLUSIVE | 3.351 | 5.4369 | 5.482 | 298.42 | 42.333% | 0.0% | 0.0% |
| main-bateria-baja | INCONCLUSIVE | 3.3188 | 5.4579 | 5.5136 | 301.31 | 53.559% | 0.0% | 0.0% |
| main-espectro-agua | INCONCLUSIVE | 3.4838 | 5.976 | 6.0767 | 287.04 | 66.925% | 0.0% | 0.0% |
| main-foco | INCONCLUSIVE | 3.3548 | 5.0944 | 5.1989 | 298.08 | 33.819% | 0.0% | 0.0% |
| main-frecuencia-dseg | INCONCLUSIVE | 3.6235 | 6.8428 | 6.8688 | 275.98 | 61.043% | 0.0% | 0.0% |
| main-natural | INCONCLUSIVE | 3.3773 | 5.4422 | 5.7876 | 296.09 | 59.994% | 0.0% | 0.0% |
| main-natural-eco | INCONCLUSIVE | 3.3714 | 5.512 | 6.4765 | 296.61 | 54.171% | 0.0% | 0.0% |
| main-tema-aurora | INCONCLUSIVE | 3.3426 | 5.2873 | 5.3438 | 299.16 | 51.814% | 0.0% | 0.0% |
| main-tema-aurora-agua | INCONCLUSIVE | 3.7704 | 6.1606 | 6.4582 | 265.22 | 67.801% | 0.0% | 0.0% |
| main-tema-aurora-eco | INCONCLUSIVE | 3.3094 | 6.1109 | 6.9388 | 302.17 | 66.835% | 0.0% | 0.0% |
| main-tema-aurora-tx | INCONCLUSIVE | 3.3699 | 6.1243 | 6.3261 | 296.75 | 76.891% | 0.0% | 0.0% |
| main-tema-cathode | INCONCLUSIVE | 3.4549 | 5.8154 | 5.836 | 289.44 | 64.921% | 0.0% | 0.0% |
| main-tema-cathode-agua | INCONCLUSIVE | 3.5297 | 6.0091 | 6.1277 | 283.31 | 62.686% | 0.0% | 0.0% |
| main-tema-cathode-eco | INCONCLUSIVE | 3.4063 | 4.7164 | 4.7346 | 293.57 | 31.356% | 0.0% | 0.0% |
| main-tema-cathode-tx | INCONCLUSIVE | 3.4617 | 4.9119 | 4.9141 | 288.88 | 34.852% | 0.0% | 0.0% |
| main-tema-redshift | INCONCLUSIVE | 3.419 | 4.6011 | 4.6466 | 292.48 | 30.583% | 0.0% | 0.0% |
| main-tema-redshift-agua | INCONCLUSIVE | 3.5163 | 4.7379 | 4.8346 | 284.39 | 30.463% | 0.0% | 0.0% |
| main-tema-redshift-eco | PASS | 3.3933 | 3.8032 | 4.0633 | 294.69 | 2.543% | 0.0% | 0.0% |
| main-tema-redshift-tx | PASS | 3.2972 | 3.8307 | 3.9006 | 303.29 | 6.851% | 0.0% | 0.0% |
| main-tema-sprint | PASS | 3.3304 | 3.9213 | 3.9404 | 300.26 | 4.057% | 0.0% | 0.0% |
| main-tema-sprint-agua | INCONCLUSIVE | 3.6055 | 5.0353 | 5.2913 | 277.35 | 28.345% | 0.0% | 0.0% |
| main-tema-sprint-eco | PASS | 3.2828 | 3.6236 | 4.9847 | 304.62 | -0.598% | 0.0% | 0.0% |
| main-tema-sprint-tx | PASS | 3.3738 | 3.7867 | 4.0891 | 296.4 | 3.938% | 0.0% | 0.0% |
| main-tema-sunlit | PASS | 3.3391 | 3.7832 | 4.6631 | 299.48 | -3.773% | 0.0% | 0.0% |
| main-tema-sunlit-agua | PASS | 3.5112 | 4.6213 | 4.6615 | 284.8 | 19.044% | 0.0% | 0.0% |
| main-tema-sunlit-eco | PASS | 3.4455 | 4.8793 | 5.5411 | 290.23 | 24.5% | 0.0% | 0.0% |
| main-tema-sunlit-tx | PASS | 3.4387 | 3.8303 | 3.9687 | 290.81 | 5.371% | 0.0% | 0.0% |
| main-tema-trail | PASS | 3.4279 | 3.5503 | 3.5893 | 291.72 | -27.931% | 0.0% | 0.0% |
| main-tema-trail-agua | PASS | 3.5162 | 4.0866 | 5.1937 | 284.4 | -1.529% | 0.0% | 0.0% |
| main-tema-trail-eco | PASS | 3.4385 | 3.8532 | 4.297 | 290.83 | 8.904% | 0.0% | 0.0% |
| main-tema-trail-tx | PASS | 3.385 | 3.8757 | 4.1611 | 295.42 | -0.341% | 0.0% | 0.0% |
| main-tx-simulado | PASS | 3.3986 | 3.4631 | 3.493 | 294.24 | -3.145% | 0.0% | 0.0% |
| settings-audio | PASS | 6.9339 | 7.2829 | 8.1621 | 144.22 | -6.894% | 0.0% | 0.0% |
| settings-bands | PASS | 3.2165 | 3.6079 | 3.6797 | 310.9 | -16.281% | 0.0% | 0.0% |
| settings-home | PASS | 6.975 | 7.5027 | 8.6608 | 143.37 | -4.997% | 0.0% | 0.0% |
| settings-perfil | PASS | 6.5293 | 7.8241 | 7.9656 | 153.16 | 3.374% | 0.0% | 0.0% |
| settings-tema-aurora | PASS | 6.6631 | 6.9698 | 7.0254 | 150.08 | -7.335% | 0.0% | 0.0% |
| settings-tema-cathode | PASS | 6.8051 | 7.9178 | 9.0734 | 146.95 | 1.974% | 0.0% | 0.0% |
| settings-tema-redshift | PASS | 6.5108 | 6.7949 | 7.7074 | 153.59 | -17.424% | 0.0% | 0.0% |
| settings-tema-sprint | PASS | 6.5291 | 6.8541 | 7.9591 | 153.16 | -10.596% | 0.0% | 0.0% |
| settings-tema-sunlit | PASS | 6.6409 | 8.5713 | 9.077 | 150.58 | 2.642% | 0.0% | 0.0% |
| settings-tema-trail | PASS | 6.8789 | 8.6912 | 8.7226 | 145.37 | -26.738% | 0.0% | 0.0% |
| settings-wfbat | PASS | 6.9366 | 7.3007 | 8.4536 | 144.16 | -26.011% | 0.0% | 0.0% |
| swrscan-3pass | PASS | 6.7791 | 7.7489 | 8.6977 | 147.51 | -34.442% | 0.0% | 0.0% |
| swrscan-3pass-p1 | PASS | 6.9094 | 7.3612 | 8.0527 | 144.73 | -7.634% | 0.0% | 0.0% |
| swrscan-3pass-p2 | PASS | 6.9978 | 7.4704 | 7.6982 | 142.9 | -7.776% | 0.0% | 0.0% |
| swrscan-3pass-p3 | PASS | 6.9855 | 8.4673 | 9.2915 | 143.15 | 11.371% | 0.0% | 0.0% |
| swrscan-3pass-stop | PASS | 6.9277 | 7.6215 | 7.9304 | 144.35 | 5.823% | 0.0% | 0.0% |
| swrscan-3pass-tema-aurora | PASS | 7.2641 | 8.0299 | 8.2923 | 137.66 | 7.052% | 0.0% | 0.0% |
| swrscan-3pass-tema-cathode | PASS | 6.8569 | 7.3725 | 7.478 | 145.84 | -9.911% | 0.0% | 0.0% |
| swrscan-5pass-moving | PASS | 6.8393 | 7.6452 | 8.2354 | 146.21 | -7.033% | 0.0% | 0.0% |
| swrscan-5pass-outliers | PASS | 6.8066 | 8.397 | 8.8175 | 146.92 | 16.388% | 0.0% | 0.0% |
| swrscan-double-valley | PASS | 6.7765 | 7.8681 | 8.1681 | 147.57 | 10.873% | 0.0% | 0.0% |
| swrscan-flat | PASS | 6.5816 | 9.2595 | 10.4193 | 151.94 | 12.257% | 0.0% | 0.0% |
| swrscan-narrow-zoom | INCONCLUSIVE | 6.7531 | 10.5816 | 11.3301 | 148.08 | 52.487% | 0.0% | 0.0% |
| swrscan-owner-40m | PASS | 6.7352 | 7.0078 | 7.0849 | 148.47 | -0.858% | 0.0% | 0.0% |
| swrscan-owner-40m-domo | PASS | 6.7168 | 6.8947 | 6.9796 | 148.88 | -6.551% | 0.0% | 0.0% |
| swrscan-owner-40m-sweep | PASS | 6.8222 | 7.7361 | 8.2248 | 146.58 | 6.002% | 0.0% | 0.0% |
| swrscan-quick | PASS | 6.7031 | 7.1409 | 7.1726 | 149.18 | -6.945% | 0.0% | 0.0% |
| swrscan-wide-zoom | PASS | 6.5695 | 7.0853 | 7.1346 | 152.22 | 2.152% | 0.0% | 0.0% |

- **autotune** (INCONCLUSIVE):
  - p95 render +26.9% (budget 25.0%): 3.435 -> 4.359 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **bench-render** (INCONCLUSIVE):
  - p95 render +27.6% (budget 25.0%): 3.639 -> 4.642 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **bt-dialog-tema-redshift** (INCONCLUSIVE):
  - p95 render +59.2% (budget 25.0%): 7.185 -> 11.435 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **bt-dialog-tema-sprint** (INCONCLUSIVE):
  - p95 render +54.8% (budget 25.0%): 7.004 -> 10.845 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **bt-dialog-tema-sunlit** (INCONCLUSIVE):
  - p95 render +31.6% (budget 25.0%): 7.762 -> 10.215 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **bt-dialog-tema-trail** (INCONCLUSIVE):
  - p95 render +53.2% (budget 25.0%): 6.676 -> 10.227 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **cw-autotune-off** (INCONCLUSIVE):
  - p95 render +43.8% (budget 25.0%): 3.493 -> 5.023 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **cw-autotune-on** (INCONCLUSIVE):
  - p95 render +56.6% (budget 25.0%): 3.510 -> 5.496 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **cw-tune-default** (INCONCLUSIVE):
  - p95 render +57.3% (budget 25.0%): 3.500 -> 5.508 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **cw-tune-tema-sunlit** (INCONCLUSIVE):
  - p95 render +65.8% (budget 25.0%): 3.376 -> 5.598 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **cw-tune-tuner** (INCONCLUSIVE):
  - p95 render +69.5% (budget 25.0%): 3.431 -> 5.816 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **home-bateria-externa** (INCONCLUSIVE):
  - p95 render +64.6% (budget 25.0%): 3.456 -> 5.689 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **home-frecuencia-extrema** (INCONCLUSIVE):
  - p95 render +32.8% (budget 25.0%): 3.848 -> 5.112 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **home-usb-20m** (INCONCLUSIVE):
  - p95 render +42.3% (budget 25.0%): 3.820 -> 5.437 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **main-bateria-baja** (INCONCLUSIVE):
  - p95 render +53.6% (budget 25.0%): 3.554 -> 5.458 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **main-espectro-agua** (INCONCLUSIVE):
  - p95 render +66.9% (budget 25.0%): 3.580 -> 5.976 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **main-foco** (INCONCLUSIVE):
  - p95 render +33.8% (budget 25.0%): 3.807 -> 5.094 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **main-frecuencia-dseg** (INCONCLUSIVE):
  - p95 render +61.0% (budget 25.0%): 4.249 -> 6.843 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **main-natural** (INCONCLUSIVE):
  - p95 render +60.0% (budget 25.0%): 3.402 -> 5.442 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **main-natural-eco** (INCONCLUSIVE):
  - p95 render +54.2% (budget 25.0%): 3.575 -> 5.512 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **main-tema-aurora** (INCONCLUSIVE):
  - p95 render +51.8% (budget 25.0%): 3.483 -> 5.287 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **main-tema-aurora-agua** (INCONCLUSIVE):
  - p95 render +67.8% (budget 25.0%): 3.671 -> 6.161 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **main-tema-aurora-eco** (INCONCLUSIVE):
  - p95 render +66.8% (budget 25.0%): 3.663 -> 6.111 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **main-tema-aurora-tx** (INCONCLUSIVE):
  - p95 render +76.9% (budget 25.0%): 3.462 -> 6.124 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **main-tema-cathode** (INCONCLUSIVE):
  - p95 render +64.9% (budget 25.0%): 3.526 -> 5.815 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **main-tema-cathode-agua** (INCONCLUSIVE):
  - p95 render +62.7% (budget 25.0%): 3.694 -> 6.009 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **main-tema-cathode-eco** (INCONCLUSIVE):
  - p95 render +31.4% (budget 25.0%): 3.591 -> 4.716 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **main-tema-cathode-tx** (INCONCLUSIVE):
  - p95 render +34.9% (budget 25.0%): 3.642 -> 4.912 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **main-tema-redshift** (INCONCLUSIVE):
  - p95 render +30.6% (budget 25.0%): 3.524 -> 4.601 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **main-tema-redshift-agua** (INCONCLUSIVE):
  - p95 render +30.5% (budget 25.0%): 3.632 -> 4.738 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **main-tema-sprint-agua** (INCONCLUSIVE):
  - p95 render +28.3% (budget 25.0%): 3.923 -> 5.035 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **swrscan-narrow-zoom** (INCONCLUSIVE):
  - p95 render +52.5% (budget 25.0%): 6.939 -> 10.582 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- Environment: x86 host: relative regression proxy, not the Cortex-A7
