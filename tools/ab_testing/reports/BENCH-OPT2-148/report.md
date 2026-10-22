# Render benchmark — BENCH-OPT2-148

- Status: **INCONCLUSIVE** (exit 3)
- Date: 2026-10-19
- a: `020904541eed5228efbcb741bc6492681475e2a4`
- b: `576d2df3fbc2a499054c14985f2baf776c6e43b3`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| app-bands | INCONCLUSIVE | 3.403 | 6.4247 | 6.5385 | 293.86 | 47.678% | 0.0% | 0.0% |
| app-close-home | INCONCLUSIVE | 3.5934 | 7.0153 | 7.0463 | 278.28 | 68.181% | 0.0% | 0.0% |
| apps-menu-1 | INCONCLUSIVE | 3.4169 | 6.8882 | 7.3284 | 292.66 | 80.34% | 0.0% | 0.0% |
| apps-menu-2 | INCONCLUSIVE | 3.7817 | 6.5492 | 7.1632 | 264.43 | 41.428% | 0.0% | 0.0% |
| apps-menu-3 | INCONCLUSIVE | 3.3914 | 7.2211 | 8.0929 | 294.86 | 44.558% | 0.0% | 0.0% |
| apps-menu-4 | INCONCLUSIVE | 3.4975 | 7.2721 | 7.4779 | 285.91 | 98.987% | 0.0% | 0.0% |
| apps-menu-5 | INCONCLUSIVE | 3.4593 | 6.6625 | 6.8208 | 289.08 | 48.191% | 0.0% | 0.0% |
| apps-menu-6 | INCONCLUSIVE | 3.4665 | 6.94 | 7.5127 | 288.47 | 96.378% | 0.0% | 0.0% |
| autotune | INCONCLUSIVE | 3.4808 | 6.2863 | 6.3893 | 287.29 | 71.19% | 0.0% | 0.0% |
| autotune-found | INCONCLUSIVE | 3.5088 | 6.8563 | 7.0531 | 285.0 | 69.444% | 0.0% | 0.0% |
| autotune-progress | INCONCLUSIVE | 3.4826 | 6.8991 | 6.9055 | 287.14 | 86.618% | 0.0% | 0.0% |
| bench-render | INCONCLUSIVE | 3.7617 | 7.2638 | 8.1468 | 265.84 | 87.217% | 0.0% | 0.0% |
| bt-dialog | INCONCLUSIVE | 6.7359 | 12.5811 | 13.4877 | 148.46 | 68.67% | 0.0% | 0.0% |
| bt-dialog-tema-aurora | INCONCLUSIVE | 7.1086 | 11.9358 | 12.3306 | 140.67 | 47.107% | 0.0% | 0.0% |
| bt-dialog-tema-cathode | INCONCLUSIVE | 7.4066 | 12.015 | 13.1536 | 135.01 | 74.777% | 0.0% | 0.0% |
| bt-dialog-tema-redshift | INCONCLUSIVE | 6.8444 | 13.248 | 13.4003 | 146.11 | 84.032% | 0.0% | 0.0% |
| bt-dialog-tema-sprint | INCONCLUSIVE | 6.8532 | 12.475 | 13.544 | 145.92 | 51.294% | 0.0% | 0.0% |
| bt-dialog-tema-sunlit | INCONCLUSIVE | 6.9326 | 12.5227 | 12.5528 | 144.25 | 54.256% | 0.0% | 0.0% |
| bt-dialog-tema-trail | INCONCLUSIVE | 7.365 | 12.1014 | 12.1401 | 135.78 | 45.228% | 0.0% | 0.0% |
| cw-autotune-off | INCONCLUSIVE | 3.4386 | 6.5138 | 6.5195 | 290.81 | 71.461% | 0.0% | 0.0% |
| cw-autotune-on | INCONCLUSIVE | 4.1084 | 6.9891 | 7.9249 | 243.4 | 58.199% | 0.0% | 0.0% |
| cw-tune-default | INCONCLUSIVE | 3.5375 | 7.9608 | 10.5058 | 282.69 | 57.01% | 0.0% | 0.0% |
| cw-tune-ssb | INCONCLUSIVE | 3.3974 | 7.3354 | 8.0322 | 294.34 | 62.33% | 0.0% | 0.0% |
| cw-tune-tema-sunlit | INCONCLUSIVE | 3.702 | 6.6794 | 7.3765 | 270.12 | 70.901% | 0.0% | 0.0% |
| cw-tune-tuner | INCONCLUSIVE | 3.5973 | 6.5172 | 6.995 | 277.99 | 74.357% | 0.0% | 0.0% |
| home-bateria-externa | INCONCLUSIVE | 3.4833 | 6.8587 | 7.8648 | 287.08 | 70.476% | 0.0% | 0.0% |
| home-cb | INCONCLUSIVE | 3.5509 | 6.8264 | 7.3074 | 281.62 | 79.486% | 0.0% | 0.0% |
| home-cb-offchannel | INCONCLUSIVE | 3.3944 | 7.1179 | 7.8163 | 294.6 | 47.903% | 0.0% | 0.0% |
| home-frecuencia-extrema | INCONCLUSIVE | 3.4002 | 6.8559 | 7.1849 | 294.1 | 66.506% | 0.0% | 0.0% |
| home-usb-20m | INCONCLUSIVE | 3.5002 | 6.6433 | 6.6795 | 285.7 | 74.029% | 0.0% | 0.0% |
| main-bateria-baja | INCONCLUSIVE | 3.8204 | 7.2947 | 7.4449 | 261.75 | 106.1% | 0.0% | 0.0% |
| main-espectro-agua | INCONCLUSIVE | 4.2113 | 7.4608 | 7.5802 | 237.46 | 74.152% | 0.0% | 0.0% |
| main-foco | INCONCLUSIVE | 3.5086 | 7.0026 | 7.1399 | 285.01 | 90.682% | 0.0% | 0.0% |
| main-frecuencia-dseg | INCONCLUSIVE | 3.7462 | 6.5138 | 6.7349 | 266.94 | 31.316% | 0.0% | 0.0% |
| main-natural | INCONCLUSIVE | 3.4864 | 6.9054 | 8.293 | 286.83 | 54.397% | 0.0% | 0.0% |
| main-natural-eco | INCONCLUSIVE | 3.7307 | 7.0056 | 7.8409 | 268.05 | 63.496% | 0.0% | 0.0% |
| main-tema-aurora | INCONCLUSIVE | 3.5659 | 6.9629 | 7.726 | 280.43 | 91.117% | 0.0% | 0.0% |
| main-tema-aurora-agua | INCONCLUSIVE | 3.7432 | 6.6208 | 6.714 | 267.15 | 57.364% | 0.0% | 0.0% |
| main-tema-aurora-eco | INCONCLUSIVE | 3.4449 | 6.8985 | 7.9907 | 290.29 | 73.762% | 0.0% | 0.0% |
| main-tema-aurora-tx | PASS | 3.4985 | 4.9101 | 5.3523 | 285.84 | 22.688% | 0.0% | 0.0% |
| main-tema-cathode | PASS | 3.7149 | 4.5596 | 5.4262 | 269.18 | -10.269% | 0.0% | 0.0% |
| main-tema-cathode-agua | PASS | 3.8532 | 4.7001 | 4.9683 | 259.52 | -4.238% | 0.0% | 0.0% |
| main-tema-cathode-eco | PASS | 3.6149 | 4.0923 | 4.2633 | 276.64 | -14.245% | 0.0% | 0.0% |
| main-tema-cathode-tx | INCONCLUSIVE | 3.4739 | 4.8571 | 5.4268 | 287.87 | 25.322% | 0.0% | 0.0% |
| main-tema-redshift | PASS | 3.6969 | 4.2492 | 4.7072 | 270.5 | 0.153% | 0.0% | 0.0% |
| main-tema-redshift-agua | PASS | 3.7523 | 4.4469 | 4.9086 | 266.5 | -12.082% | 0.0% | 0.0% |
| main-tema-redshift-eco | PASS | 3.4219 | 4.0806 | 4.0885 | 292.24 | 14.564% | 0.0% | 0.0% |
| main-tema-redshift-tx | PASS | 3.6429 | 4.0254 | 4.0323 | 274.51 | -16.962% | 0.0% | 0.0% |
| main-tema-sprint | PASS | 3.4619 | 5.2084 | 5.2624 | 288.85 | 6.6% | 0.0% | 0.0% |
| main-tema-sprint-agua | PASS | 3.867 | 4.3864 | 5.2519 | 258.6 | 2.112% | 0.0% | 0.0% |
| main-tema-sprint-eco | PASS | 3.3971 | 4.8817 | 4.8913 | 294.37 | 15.377% | 0.0% | 0.0% |
| main-tema-sprint-tx | PASS | 3.6017 | 4.0522 | 4.212 | 277.65 | 8.101% | 0.0% | 0.0% |
| main-tema-sunlit | PASS | 3.5163 | 4.6142 | 4.8175 | 284.39 | 23.434% | 0.0% | 0.0% |
| main-tema-sunlit-agua | PASS | 3.5364 | 3.7433 | 3.7682 | 282.77 | -1.144% | 0.0% | 0.0% |
| main-tema-sunlit-eco | PASS | 3.6527 | 4.928 | 4.977 | 273.77 | 19.014% | 0.0% | 0.0% |
| main-tema-sunlit-tx | PASS | 3.4855 | 4.8442 | 4.9136 | 286.9 | 24.149% | 0.0% | 0.0% |
| main-tema-trail | INCONCLUSIVE | 3.4211 | 6.6132 | 6.6546 | 292.3 | 36.092% | 0.0% | 0.0% |
| main-tema-trail-agua | INCONCLUSIVE | 3.6388 | 6.8644 | 6.8793 | 274.82 | 46.744% | 0.0% | 0.0% |
| main-tema-trail-eco | INCONCLUSIVE | 3.8121 | 6.408 | 6.4559 | 262.33 | 69.327% | 0.0% | 0.0% |
| main-tema-trail-tx | PASS | 3.5482 | 3.9335 | 4.1295 | 281.83 | 0.734% | 0.0% | 0.0% |
| main-tx-simulado | PASS | 3.5547 | 4.1026 | 4.2104 | 281.32 | 10.539% | 0.0% | 0.0% |
| settings-audio | PASS | 7.1843 | 8.4787 | 8.6021 | 139.19 | 10.937% | 0.0% | 0.0% |
| settings-bands | PASS | 3.309 | 3.3954 | 3.5335 | 302.21 | -2.934% | 0.0% | 0.0% |
| settings-home | PASS | 7.2186 | 8.9296 | 9.8648 | 138.53 | 15.308% | 0.0% | 0.0% |
| settings-perfil | PASS | 6.8787 | 8.3664 | 8.4814 | 145.38 | 6.821% | 0.0% | 0.0% |
| settings-tema-aurora | PASS | 6.9798 | 7.9374 | 8.92 | 143.27 | 8.739% | 0.0% | 0.0% |
| settings-tema-cathode | PASS | 6.8666 | 7.4854 | 8.6292 | 145.63 | -11.36% | 0.0% | 0.0% |
| settings-tema-redshift | PASS | 7.1397 | 8.326 | 8.393 | 140.06 | 6.269% | 0.0% | 0.0% |
| settings-tema-sprint | PASS | 6.8769 | 8.1983 | 8.6607 | 145.41 | -5.896% | 0.0% | 0.0% |
| settings-tema-sunlit | PASS | 6.7693 | 7.4278 | 8.4599 | 147.73 | -9.866% | 0.0% | 0.0% |
| settings-tema-trail | PASS | 6.8861 | 8.304 | 8.355 | 145.22 | -6.86% | 0.0% | 0.0% |
| settings-wfbat | PASS | 7.5834 | 8.0455 | 8.674 | 131.87 | 2.573% | 0.0% | 0.0% |
| swrscan-3pass | PASS | 6.9935 | 7.6876 | 8.2618 | 142.99 | -11.325% | 0.0% | 0.0% |
| swrscan-3pass-p1 | PASS | 7.3756 | 8.0166 | 8.5913 | 135.58 | 1.539% | 0.0% | 0.0% |
| swrscan-3pass-p2 | PASS | 7.518 | 8.9797 | 10.8185 | 133.01 | 3.199% | 0.0% | 0.0% |
| swrscan-3pass-p3 | PASS | 7.1314 | 7.7507 | 7.9467 | 140.22 | -7.107% | 0.0% | 0.0% |
| swrscan-3pass-stop | PASS | 7.2566 | 7.9678 | 8.3693 | 137.81 | -11.956% | 0.0% | 0.0% |
| swrscan-3pass-tema-aurora | PASS | 7.1648 | 9.1838 | 9.4756 | 139.57 | 10.881% | 0.0% | 0.0% |
| swrscan-3pass-tema-cathode | PASS | 7.2011 | 7.8036 | 9.6843 | 138.87 | -14.585% | 0.0% | 0.0% |
| swrscan-5pass-moving | PASS | 6.9882 | 7.4589 | 7.8531 | 143.1 | -16.392% | 0.0% | 0.0% |
| swrscan-5pass-outliers | PASS | 7.3956 | 8.4265 | 8.5575 | 135.22 | -37.712% | 0.0% | 0.0% |
| swrscan-double-valley | PASS | 7.3733 | 9.2118 | 9.8141 | 135.62 | -27.331% | 0.0% | 0.0% |
| swrscan-flat | PASS | 6.9062 | 7.2624 | 8.0637 | 144.8 | -42.072% | 0.0% | 0.0% |
| swrscan-narrow-zoom | PASS | 6.9436 | 8.607 | 8.9964 | 144.02 | -35.515% | 0.0% | 0.0% |
| swrscan-owner-40m | PASS | 6.8331 | 8.0603 | 8.6551 | 146.35 | -29.227% | 0.0% | 0.0% |
| swrscan-owner-40m-domo | PASS | 7.1165 | 8.1921 | 8.3623 | 140.52 | -30.258% | 0.0% | 0.0% |
| swrscan-owner-40m-sweep | PASS | 7.0267 | 8.4586 | 8.7266 | 142.31 | -29.412% | 0.0% | 0.0% |
| swrscan-quick | PASS | 7.0705 | 8.6885 | 9.4227 | 141.43 | -30.638% | 0.0% | 0.0% |
| swrscan-wide-zoom | PASS | 7.0779 | 9.014 | 9.7323 | 141.28 | -24.35% | 0.0% | 0.0% |

- **app-bands** (INCONCLUSIVE):
  - p95 render +47.7% (budget 25.0%): 4.350 -> 6.425 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **app-close-home** (INCONCLUSIVE):
  - p95 render +68.2% (budget 25.0%): 4.171 -> 7.015 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **apps-menu-1** (INCONCLUSIVE):
  - p95 render +80.3% (budget 25.0%): 3.820 -> 6.888 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **apps-menu-2** (INCONCLUSIVE):
  - p95 render +41.4% (budget 25.0%): 4.631 -> 6.549 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **apps-menu-3** (INCONCLUSIVE):
  - p95 render +44.6% (budget 25.0%): 4.995 -> 7.221 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **apps-menu-4** (INCONCLUSIVE):
  - p95 render +99.0% (budget 25.0%): 3.655 -> 7.272 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **apps-menu-5** (INCONCLUSIVE):
  - p95 render +48.2% (budget 25.0%): 4.496 -> 6.663 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **apps-menu-6** (INCONCLUSIVE):
  - p95 render +96.4% (budget 25.0%): 3.534 -> 6.940 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **autotune** (INCONCLUSIVE):
  - p95 render +71.2% (budget 25.0%): 3.672 -> 6.286 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **autotune-found** (INCONCLUSIVE):
  - p95 render +69.4% (budget 25.0%): 4.046 -> 6.856 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **autotune-progress** (INCONCLUSIVE):
  - p95 render +86.6% (budget 25.0%): 3.697 -> 6.899 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **bench-render** (INCONCLUSIVE):
  - p95 render +87.2% (budget 25.0%): 3.880 -> 7.264 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **bt-dialog** (INCONCLUSIVE):
  - p95 render +68.7% (budget 25.0%): 7.459 -> 12.581 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **bt-dialog-tema-aurora** (INCONCLUSIVE):
  - p95 render +47.1% (budget 25.0%): 8.114 -> 11.936 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **bt-dialog-tema-cathode** (INCONCLUSIVE):
  - p95 render +74.8% (budget 25.0%): 6.874 -> 12.015 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **bt-dialog-tema-redshift** (INCONCLUSIVE):
  - p95 render +84.0% (budget 25.0%): 7.199 -> 13.248 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **bt-dialog-tema-sprint** (INCONCLUSIVE):
  - p95 render +51.3% (budget 25.0%): 8.246 -> 12.475 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **bt-dialog-tema-sunlit** (INCONCLUSIVE):
  - p95 render +54.3% (budget 25.0%): 8.118 -> 12.523 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **bt-dialog-tema-trail** (INCONCLUSIVE):
  - p95 render +45.2% (budget 25.0%): 8.333 -> 12.101 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **cw-autotune-off** (INCONCLUSIVE):
  - p95 render +71.5% (budget 25.0%): 3.799 -> 6.514 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **cw-autotune-on** (INCONCLUSIVE):
  - B dispersion 29.6% > 25.0%

- **cw-tune-default** (INCONCLUSIVE):
  - p95 render +57.0% (budget 25.0%): 5.070 -> 7.961 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **cw-tune-ssb** (INCONCLUSIVE):
  - p95 render +62.3% (budget 25.0%): 4.519 -> 7.335 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **cw-tune-tema-sunlit** (INCONCLUSIVE):
  - p95 render +70.9% (budget 25.0%): 3.908 -> 6.679 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **cw-tune-tuner** (INCONCLUSIVE):
  - p95 render +74.4% (budget 25.0%): 3.738 -> 6.517 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **home-bateria-externa** (INCONCLUSIVE):
  - p95 render +70.5% (budget 25.0%): 4.023 -> 6.859 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **home-cb** (INCONCLUSIVE):
  - p95 render +79.5% (budget 25.0%): 3.803 -> 6.826 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **home-cb-offchannel** (INCONCLUSIVE):
  - p95 render +47.9% (budget 25.0%): 4.813 -> 7.118 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **home-frecuencia-extrema** (INCONCLUSIVE):
  - p95 render +66.5% (budget 25.0%): 4.117 -> 6.856 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **home-usb-20m** (INCONCLUSIVE):
  - p95 render +74.0% (budget 25.0%): 3.817 -> 6.643 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **main-bateria-baja** (INCONCLUSIVE):
  - p95 render +106.1% (budget 25.0%): 3.539 -> 7.295 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **main-espectro-agua** (INCONCLUSIVE):
  - p95 render +74.2% (budget 25.0%): 4.284 -> 7.461 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **main-foco** (INCONCLUSIVE):
  - p95 render +90.7% (budget 25.0%): 3.672 -> 7.003 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **main-frecuencia-dseg** (INCONCLUSIVE):
  - p95 render +31.3% (budget 25.0%): 4.960 -> 6.514 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **main-natural** (INCONCLUSIVE):
  - p95 render +54.4% (budget 25.0%): 4.472 -> 6.905 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **main-natural-eco** (INCONCLUSIVE):
  - p95 render +63.5% (budget 25.0%): 4.285 -> 7.006 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **main-tema-aurora** (INCONCLUSIVE):
  - p95 render +91.1% (budget 25.0%): 3.643 -> 6.963 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **main-tema-aurora-agua** (INCONCLUSIVE):
  - p95 render +57.4% (budget 25.0%): 4.207 -> 6.621 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **main-tema-aurora-eco** (INCONCLUSIVE):
  - p95 render +73.8% (budget 25.0%): 3.970 -> 6.898 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **main-tema-cathode-tx** (INCONCLUSIVE):
  - p95 render +25.3% (budget 25.0%): 3.876 -> 4.857 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **main-tema-trail** (INCONCLUSIVE):
  - p95 render +36.1% (budget 25.0%): 4.859 -> 6.613 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **main-tema-trail-agua** (INCONCLUSIVE):
  - p95 render +46.7% (budget 25.0%): 4.678 -> 6.864 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **main-tema-trail-eco** (INCONCLUSIVE):
  - p95 render +69.3% (budget 25.0%): 3.784 -> 6.408 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- Environment: x86 host: relative regression proxy, not the Cortex-A7
