# Render benchmark — BENCH-OPT4-T1

- Status: **INCONCLUSIVE** (exit 3)
- Date: 2026-10-19
- a: `1c599025a96e3f9053a673a0b9f2aa5546810325`
- b: `923e4dea35bf5e7f0e42514dbd0bcbc7365dc780`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| app-bands | INCONCLUSIVE | 4.937 | 7.1367 | 8.9164 | 202.55 | 85.905% | 0.0% | 0.0% |
| app-close-home | INCONCLUSIVE | 5.1892 | 7.2109 | 7.3667 | 192.71 | 81.639% | 0.0% | 0.0% |
| apps-menu-1 | INCONCLUSIVE | 4.8493 | 7.2072 | 7.3234 | 206.22 | 63.47% | 0.0% | 0.0% |
| apps-menu-2 | INCONCLUSIVE | 4.8445 | 6.7227 | 7.0129 | 206.42 | 59.633% | 0.0% | 0.0% |
| apps-menu-3 | INCONCLUSIVE | 5.0261 | 6.6239 | 6.7245 | 198.96 | 44.552% | 0.0% | 0.0% |
| apps-menu-4 | INCONCLUSIVE | 5.0274 | 6.9471 | 6.9821 | 198.91 | 20.391% | 0.0% | 0.0% |
| apps-menu-5 | INCONCLUSIVE | 4.7034 | 6.971 | 7.0022 | 212.61 | 76.303% | 0.0% | 0.0% |
| apps-menu-6 | PASS | 3.91 | 6.1389 | 6.4071 | 255.76 | 16.599% | 0.0% | 0.0% |
| autotune | INCONCLUSIVE | 3.8053 | 6.1458 | 6.1765 | 262.79 | 51.65% | 0.0% | 0.0% |
| autotune-found | INCONCLUSIVE | 4.0851 | 6.5901 | 6.5986 | 244.79 | 35.945% | 0.0% | 0.0% |
| autotune-progress | INCONCLUSIVE | 4.3219 | 6.5134 | 6.9088 | 231.38 | 52.01% | 0.0% | 0.0% |
| bench-render | INCONCLUSIVE | 4.1635 | 7.0553 | 7.2215 | 240.18 | 40.705% | 0.0% | 0.0% |
| bt-dialog | INCONCLUSIVE | 8.9544 | 12.5213 | 12.5241 | 111.68 | 44.201% | 0.0% | 0.0% |
| bt-dialog-tema-aurora | INCONCLUSIVE | 7.2658 | 12.7011 | 13.6626 | 137.63 | 52.045% | 0.0% | 0.0% |
| bt-dialog-tema-cathode | INCONCLUSIVE | 7.8393 | 13.6 | 13.9685 | 127.56 | 75.154% | 0.0% | 0.0% |
| bt-dialog-tema-redshift | INCONCLUSIVE | 7.3189 | 12.8913 | 13.0571 | 136.63 | 49.525% | 0.0% | 0.0% |
| bt-dialog-tema-sprint | INCONCLUSIVE | 7.7943 | 12.1538 | 12.2975 | 128.3 | 46.946% | 0.0% | 0.0% |
| bt-dialog-tema-sunlit | INCONCLUSIVE | 7.393 | 13.7767 | 13.8377 | 135.26 | 53.977% | 0.0% | 0.0% |
| bt-dialog-tema-trail | INCONCLUSIVE | 7.0983 | 13.0398 | 13.2947 | 140.88 | 49.959% | 0.0% | 0.0% |
| cw-autotune-off | INCONCLUSIVE | 3.6944 | 6.3093 | 6.3192 | 270.68 | 49.677% | 0.0% | 0.0% |
| cw-autotune-on | INCONCLUSIVE | 3.8175 | 7.0175 | 7.0747 | 261.95 | 49.48% | 0.0% | 0.0% |
| cw-tune-default | INCONCLUSIVE | 3.5181 | 6.7522 | 7.0655 | 284.24 | 69.562% | 0.0% | 0.0% |
| cw-tune-ssb | INCONCLUSIVE | 3.4482 | 7.3743 | 7.4352 | 290.0 | 64.575% | 0.0% | 0.0% |
| cw-tune-tema-sunlit | INCONCLUSIVE | 3.7611 | 6.8446 | 7.3625 | 265.88 | 44.128% | 0.0% | 0.0% |
| cw-tune-tuner | INCONCLUSIVE | 3.7275 | 6.8296 | 6.8463 | 268.27 | 49.978% | 0.0% | 0.0% |
| home-bateria-externa | INCONCLUSIVE | 4.268 | 7.1883 | 7.1973 | 234.3 | 43.928% | 0.0% | 0.0% |
| home-cb | INCONCLUSIVE | 3.6006 | 6.6534 | 6.6882 | 277.73 | 62.879% | 0.0% | 0.0% |
| home-cb-offchannel | INCONCLUSIVE | 3.471 | 6.645 | 6.6955 | 288.1 | 55.535% | 0.0% | 0.0% |
| home-frecuencia-extrema | INCONCLUSIVE | 3.6597 | 7.2743 | 7.3615 | 273.25 | 61.859% | 0.0% | 0.0% |
| home-usb-20m | INCONCLUSIVE | 3.5894 | 6.9303 | 7.0824 | 278.6 | 52.011% | 0.0% | 0.0% |
| main-bateria-baja | PASS | 3.5686 | 6.4217 | 6.8757 | 280.22 | -6.606% | 0.0% | 0.0% |
| main-espectro-agua | PASS | 3.6315 | 7.36 | 7.6582 | 275.36 | -4.37% | 0.0% | 0.0% |
| main-foco | PASS | 3.6557 | 6.7333 | 6.8184 | 273.55 | 2.562% | 0.0% | 0.0% |
| main-frecuencia-dseg | PASS | 4.1757 | 7.4913 | 9.3323 | 239.48 | 9.465% | 0.0% | 0.0% |
| main-natural | PASS | 3.5747 | 6.7547 | 6.7807 | 279.74 | 1.299% | 0.0% | 0.0% |
| main-natural-eco | PASS | 3.9104 | 6.6851 | 6.8174 | 255.73 | -2.96% | 0.0% | 0.0% |
| main-tema-aurora | PASS | 3.8346 | 6.6529 | 6.7363 | 260.78 | -3.225% | 0.0% | 0.0% |
| main-tema-aurora-agua | PASS | 4.1098 | 5.6879 | 5.9318 | 243.32 | -19.849% | 0.0% | 0.0% |
| main-tema-aurora-eco | PASS | 4.514 | 6.0045 | 6.1797 | 221.54 | -7.652% | 0.0% | 0.0% |
| main-tema-aurora-tx | PASS | 3.8411 | 4.7316 | 4.7706 | 260.35 | -30.36% | 0.0% | 0.0% |
| main-tema-cathode | PASS | 3.956 | 5.1436 | 5.6344 | 252.78 | -26.921% | 0.0% | 0.0% |
| main-tema-cathode-agua | PASS | 4.138 | 5.1486 | 5.2413 | 241.66 | -25.695% | 0.0% | 0.0% |
| main-tema-cathode-eco | PASS | 4.0038 | 4.9169 | 5.0542 | 249.76 | -28.311% | 0.0% | 0.0% |
| main-tema-cathode-tx | PASS | 4.1635 | 6.8492 | 6.9815 | 240.18 | 3.712% | 0.0% | 0.0% |
| main-tema-redshift | PASS | 3.9704 | 6.2826 | 6.3414 | 251.86 | -1.252% | 0.0% | 0.0% |
| main-tema-redshift-agua | PASS | 4.5711 | 8.1182 | 8.5922 | 218.77 | 15.924% | 0.0% | 0.0% |
| main-tema-redshift-eco | INCONCLUSIVE | 4.3311 | 6.6648 | 6.7638 | 230.89 | -7.255% | 0.0% | 0.0% |
| main-tema-redshift-tx | PASS | 4.0194 | 6.7189 | 6.9383 | 248.79 | -7.304% | 0.0% | 0.0% |
| main-tema-sprint | INCONCLUSIVE | 4.2392 | 7.0675 | 7.917 | 235.9 | 6.849% | 0.0% | 0.0% |
| main-tema-sprint-agua | INCONCLUSIVE | 5.4395 | 7.4967 | 7.5272 | 183.84 | 2.516% | 0.0% | 0.0% |
| main-tema-sprint-eco | PASS | 3.7539 | 4.9485 | 5.0194 | 266.39 | -29.332% | 0.0% | 0.0% |
| main-tema-sprint-tx | PASS | 3.8806 | 4.54 | 5.0619 | 257.69 | -31.977% | 0.0% | 0.0% |
| main-tema-sunlit | PASS | 3.7706 | 4.5875 | 4.7081 | 265.21 | -29.639% | 0.0% | 0.0% |
| main-tema-sunlit-agua | PASS | 4.2721 | 4.522 | 4.5572 | 234.08 | -38.667% | 0.0% | 0.0% |
| main-tema-sunlit-eco | PASS | 4.1729 | 4.5838 | 4.6217 | 239.64 | -35.238% | 0.0% | 0.0% |
| main-tema-sunlit-tx | PASS | 4.0408 | 4.8623 | 5.4842 | 247.48 | -28.183% | 0.0% | 0.0% |
| main-tema-trail | PASS | 4.2123 | 5.0329 | 6.0184 | 237.4 | -24.004% | 0.0% | 0.0% |
| main-tema-trail-agua | PASS | 4.1555 | 6.1207 | 6.8449 | 240.65 | -13.162% | 0.0% | 0.0% |
| main-tema-trail-eco | PASS | 3.8909 | 4.5276 | 4.8609 | 257.01 | -33.812% | 0.0% | 0.0% |
| main-tema-trail-tx | PASS | 3.7583 | 4.5693 | 5.4755 | 266.08 | -33.044% | 0.0% | 0.0% |
| main-tx-simulado | PASS | 3.9587 | 4.7153 | 5.3407 | 252.61 | -32.165% | 0.0% | 0.0% |
| settings-audio | INCONCLUSIVE | 7.9908 | 9.495 | 9.8789 | 125.14 | -25.792% | 0.0% | 0.0% |
| settings-bands | INCONCLUSIVE | 4.1718 | 4.5273 | 4.5321 | 239.7 | -27.818% | 0.0% | 0.0% |
| settings-home | INCONCLUSIVE | 7.7096 | 8.4014 | 9.0007 | 129.71 | -38.293% | 0.0% | 0.0% |
| settings-perfil | PASS | 7.896 | 14.2857 | 14.8625 | 126.65 | 11.842% | 0.0% | 0.0% |
| settings-tema-aurora | INCONCLUSIVE | 8.6075 | 13.1848 | 14.9755 | 116.18 | -1.346% | 0.0% | 0.0% |
| settings-tema-cathode | INCONCLUSIVE | 8.705 | 13.6282 | 14.124 | 114.88 | -6.027% | 0.0% | 0.0% |
| settings-tema-redshift | PASS | 7.7431 | 13.4116 | 13.5379 | 129.15 | 5.447% | 0.0% | 0.0% |
| settings-tema-sprint | INCONCLUSIVE | 7.724 | 12.2546 | 12.6272 | 129.47 | -3.08% | 0.0% | 0.0% |
| settings-tema-sunlit | INCONCLUSIVE | 7.0914 | 9.8236 | 9.9612 | 141.02 | -27.631% | 0.0% | 0.0% |
| settings-tema-trail | INCONCLUSIVE | 7.4033 | 8.5553 | 8.6978 | 135.07 | -32.913% | 0.0% | 0.0% |
| settings-wfbat | INCONCLUSIVE | 7.3903 | 8.3989 | 8.7998 | 135.31 | -34.414% | 0.0% | 0.0% |
| swrscan-3pass | INCONCLUSIVE | 7.8443 | 8.9646 | 9.039 | 127.48 | -35.172% | 0.0% | 0.0% |
| swrscan-3pass-p1 | INCONCLUSIVE | 7.674 | 8.5022 | 8.7656 | 130.31 | -37.36% | 0.0% | 0.0% |
| swrscan-3pass-p2 | INCONCLUSIVE | 7.4803 | 8.8761 | 9.5375 | 133.68 | -35.082% | 0.0% | 0.0% |
| swrscan-3pass-p3 | INCONCLUSIVE | 7.4656 | 9.8915 | 10.9976 | 133.95 | -28.021% | 0.0% | 0.0% |
| swrscan-3pass-stop | INCONCLUSIVE | 7.3495 | 10.5651 | 10.7713 | 136.06 | -21.228% | 0.0% | 0.0% |
| swrscan-3pass-tema-aurora | INCONCLUSIVE | 8.1054 | 9.8399 | 10.061 | 123.37 | -26.677% | 0.0% | 0.0% |
| swrscan-3pass-tema-cathode | INCONCLUSIVE | 7.3493 | 9.8471 | 10.3631 | 136.07 | -30.79% | 0.0% | 0.0% |
| swrscan-5pass-moving | INCONCLUSIVE | 7.6095 | 9.8642 | 10.4211 | 131.41 | -26.007% | 0.0% | 0.0% |
| swrscan-5pass-outliers | INCONCLUSIVE | 7.5135 | 11.3182 | 11.3878 | 133.09 | -10.355% | 0.0% | 0.0% |
| swrscan-double-valley | INCONCLUSIVE | 7.6648 | 9.0629 | 9.2244 | 130.47 | -32.902% | 0.0% | 0.0% |
| swrscan-flat | INCONCLUSIVE | 7.6518 | 8.9009 | 9.0018 | 130.69 | -29.31% | 0.0% | 0.0% |
| swrscan-narrow-zoom | INCONCLUSIVE | 7.3173 | 8.232 | 8.3861 | 136.66 | -35.923% | 0.0% | 0.0% |
| swrscan-owner-40m | INCONCLUSIVE | 7.3762 | 8.9987 | 9.1602 | 135.57 | -28.871% | 0.0% | 0.0% |
| swrscan-owner-40m-domo | INCONCLUSIVE | 7.9853 | 8.5113 | 8.8856 | 125.23 | -35.315% | 0.0% | 0.0% |
| swrscan-owner-40m-sweep | INCONCLUSIVE | 7.4287 | 7.7178 | 8.622 | 134.61 | -44.36% | 0.0% | 0.0% |
| swrscan-quick | INCONCLUSIVE | 7.3113 | 7.7658 | 8.0433 | 136.78 | -42.389% | 0.0% | 0.0% |
| swrscan-wide-zoom | INCONCLUSIVE | 7.1107 | 8.1367 | 8.2349 | 140.63 | -35.518% | 0.0% | 0.0% |

- **app-bands** (INCONCLUSIVE):
  - B dispersion 49.6% > 25.0%

- **app-close-home** (INCONCLUSIVE):
  - B dispersion 49.1% > 25.0%

- **apps-menu-1** (INCONCLUSIVE):
  - B dispersion 46.2% > 25.0%

- **apps-menu-2** (INCONCLUSIVE):
  - B dispersion 42.5% > 25.0%

- **apps-menu-3** (INCONCLUSIVE):
  - B dispersion 39.4% > 25.0%

- **apps-menu-4** (INCONCLUSIVE):
  - B dispersion 47.3% > 25.0%

- **apps-menu-5** (INCONCLUSIVE):
  - p95 render +76.3% (budget 25.0%): 3.954 -> 6.971 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **autotune** (INCONCLUSIVE):
  - p95 render +51.6% (budget 25.0%): 4.053 -> 6.146 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **autotune-found** (INCONCLUSIVE):
  - p95 render +35.9% (budget 25.0%): 4.848 -> 6.590 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **autotune-progress** (INCONCLUSIVE):
  - p95 render +52.0% (budget 25.0%): 4.285 -> 6.513 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **bench-render** (INCONCLUSIVE):
  - p95 render +40.7% (budget 25.0%): 5.014 -> 7.055 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **bt-dialog** (INCONCLUSIVE):
  - B dispersion 25.2% > 25.0%

- **bt-dialog-tema-aurora** (INCONCLUSIVE):
  - p95 render +52.0% (budget 25.0%): 8.353 -> 12.701 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **bt-dialog-tema-cathode** (INCONCLUSIVE):
  - p95 render +75.2% (budget 25.0%): 7.765 -> 13.600 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **bt-dialog-tema-redshift** (INCONCLUSIVE):
  - p95 render +49.5% (budget 25.0%): 8.622 -> 12.891 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **bt-dialog-tema-sprint** (INCONCLUSIVE):
  - p95 render +46.9% (budget 25.0%): 8.271 -> 12.154 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **bt-dialog-tema-sunlit** (INCONCLUSIVE):
  - p95 render +54.0% (budget 25.0%): 8.947 -> 13.777 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **bt-dialog-tema-trail** (INCONCLUSIVE):
  - p95 render +50.0% (budget 25.0%): 8.696 -> 13.040 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **cw-autotune-off** (INCONCLUSIVE):
  - p95 render +49.7% (budget 25.0%): 4.215 -> 6.309 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **cw-autotune-on** (INCONCLUSIVE):
  - p95 render +49.5% (budget 25.0%): 4.695 -> 7.017 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **cw-tune-default** (INCONCLUSIVE):
  - p95 render +69.6% (budget 25.0%): 3.982 -> 6.752 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **cw-tune-ssb** (INCONCLUSIVE):
  - p95 render +64.6% (budget 25.0%): 4.481 -> 7.374 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **cw-tune-tema-sunlit** (INCONCLUSIVE):
  - p95 render +44.1% (budget 25.0%): 4.749 -> 6.845 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **cw-tune-tuner** (INCONCLUSIVE):
  - p95 render +50.0% (budget 25.0%): 4.554 -> 6.830 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **home-bateria-externa** (INCONCLUSIVE):
  - p95 render +43.9% (budget 25.0%): 4.994 -> 7.188 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **home-cb** (INCONCLUSIVE):
  - p95 render +62.9% (budget 25.0%): 4.085 -> 6.653 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **home-cb-offchannel** (INCONCLUSIVE):
  - p95 render +55.5% (budget 25.0%): 4.272 -> 6.645 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **home-frecuencia-extrema** (INCONCLUSIVE):
  - p95 render +61.9% (budget 25.0%): 4.494 -> 7.274 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **home-usb-20m** (INCONCLUSIVE):
  - p95 render +52.0% (budget 25.0%): 4.559 -> 6.930 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- **main-tema-redshift-eco** (INCONCLUSIVE):
  - B dispersion 32.7% > 25.0%

- **main-tema-sprint** (INCONCLUSIVE):
  - B dispersion 27.6% > 25.0%

- **main-tema-sprint-agua** (INCONCLUSIVE):
  - B dispersion 42.4% > 25.0%

- **settings-audio** (INCONCLUSIVE):
  - A dispersion 28.3% > 25.0%

- **settings-bands** (INCONCLUSIVE):
  - A dispersion 36.4% > 25.0%

- **settings-home** (INCONCLUSIVE):
  - A dispersion 41.9% > 25.0%

- **settings-tema-aurora** (INCONCLUSIVE):
  - B dispersion 26.8% > 25.0%

- **settings-tema-cathode** (INCONCLUSIVE):
  - B dispersion 27.7% > 25.0%

- **settings-tema-sprint** (INCONCLUSIVE):
  - A dispersion 38.3% > 25.0%

- **settings-tema-sunlit** (INCONCLUSIVE):
  - A dispersion 39.4% > 25.0%

- **settings-tema-trail** (INCONCLUSIVE):
  - A dispersion 26.7% > 25.0%

- **settings-wfbat** (INCONCLUSIVE):
  - A dispersion 34.7% > 25.0%

- **swrscan-3pass** (INCONCLUSIVE):
  - A dispersion 38.0% > 25.0%

- **swrscan-3pass-p1** (INCONCLUSIVE):
  - A dispersion 41.8% > 25.0%

- **swrscan-3pass-p2** (INCONCLUSIVE):
  - A dispersion 37.3% > 25.0%

- **swrscan-3pass-p3** (INCONCLUSIVE):
  - A dispersion 42.8% > 25.0%

- **swrscan-3pass-stop** (INCONCLUSIVE):
  - A dispersion 32.9% > 25.0%

- **swrscan-3pass-tema-aurora** (INCONCLUSIVE):
  - A dispersion 40.1% > 25.0%

- **swrscan-3pass-tema-cathode** (INCONCLUSIVE):
  - A dispersion 40.8% > 25.0%

- **swrscan-5pass-moving** (INCONCLUSIVE):
  - A dispersion 40.1% > 25.0%

- **swrscan-5pass-outliers** (INCONCLUSIVE):
  - A dispersion 35.1% > 25.0%

- **swrscan-double-valley** (INCONCLUSIVE):
  - A dispersion 41.9% > 25.0%

- **swrscan-flat** (INCONCLUSIVE):
  - A dispersion 40.5% > 25.0%

- **swrscan-narrow-zoom** (INCONCLUSIVE):
  - A dispersion 41.0% > 25.0%

- **swrscan-owner-40m** (INCONCLUSIVE):
  - A dispersion 40.4% > 25.0%

- **swrscan-owner-40m-domo** (INCONCLUSIVE):
  - A dispersion 39.5% > 25.0%

- **swrscan-owner-40m-sweep** (INCONCLUSIVE):
  - A dispersion 43.2% > 25.0%

- **swrscan-quick** (INCONCLUSIVE):
  - A dispersion 41.4% > 25.0%

- **swrscan-wide-zoom** (INCONCLUSIVE):
  - A dispersion 38.2% > 25.0%

- Environment: x86 host: relative regression proxy, not the Cortex-A7
