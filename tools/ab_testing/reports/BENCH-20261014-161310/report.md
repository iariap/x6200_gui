# Render benchmark — BENCH-20261014-161310

- Status: **PASS** (exit 0)
- Date: 2026-10-14
- a: `4c711f60b3789372f9e1c117e514418f0eee00c8`
- b: `6508c139ab5200de2196c26fb3439d5e82201fb2`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| bench-render | PASS | 3.4187 | 4.6153 | 4.9656 | 292.51 | 20.663% | 0.0% | 0.0% |
| dialog-about | PASS | 6.1369 | 6.7887 | 8.0081 | 162.95 | -12.339% | 0.0% | 0.0% |
| home-bateria-externa | PASS | 3.2769 | 4.8864 | 5.5013 | 305.17 | 1.665% | 0.0% | 0.0% |
| home-frecuencia-extrema | PASS | 3.1902 | 3.3623 | 4.457 | 313.46 | -30.107% | 0.0% | 0.0% |
| home-usb-20m | PASS | 3.2703 | 3.451 | 3.7544 | 305.78 | -3.422% | 0.0% | 0.0% |
| main-bateria-baja | PASS | 3.2119 | 3.4097 | 4.5711 | 311.35 | -21.058% | 0.0% | 0.0% |
| main-espectro-agua | PASS | 3.4204 | 3.4622 | 3.475 | 292.36 | -15.593% | 0.0% | 0.0% |
| main-foco | PASS | 3.2496 | 3.3096 | 3.3217 | 307.73 | -35.178% | 0.0% | 0.0% |
| main-natural | PASS | 3.2887 | 3.4675 | 3.532 | 304.07 | -3.343% | 0.0% | 0.0% |
| main-natural-eco | PASS | 3.2974 | 4.3727 | 4.7686 | 303.26 | -10.342% | 0.0% | 0.0% |
| main-otro-tema | PASS | 3.1424 | 4.7322 | 4.7794 | 318.23 | 1.7% | 0.0% | 0.0% |
| main-tema-classic | PASS | 3.2127 | 4.8467 | 5.0638 | 311.27 | 15.091% | 0.0% | 0.0% |
| main-tema-holo | PASS | 3.3029 | 3.383 | 3.402 | 302.77 | -9.417% | 0.0% | 0.0% |
| main-tema-material-3 | PASS | 3.3312 | 3.814 | 4.4248 | 300.19 | 10.184% | 0.0% | 0.0% |
| main-tema-outdoor-negative | PASS | 3.2826 | 3.4415 | 4.586 | 304.64 | -14.727% | 0.0% | 0.0% |
| main-tema-rs | PASS | 3.2439 | 3.3778 | 4.4756 | 308.28 | 0.062% | 0.0% | 0.0% |
| main-tema-vision | PASS | 3.3066 | 3.5818 | 3.5888 | 302.43 | 3.303% | 0.0% | 0.0% |
| main-tema-vision-cwr | PASS | 3.3186 | 3.4982 | 4.5319 | 301.34 | -29.03% | 0.0% | 0.0% |
| main-tema-vision-usbd | PASS | 3.3384 | 4.0356 | 4.7775 | 299.54 | -15.568% | 0.0% | 0.0% |
| main-tx-simulado | PASS | 3.2992 | 4.8231 | 4.8325 | 303.1 | 2.146% | 0.0% | 0.0% |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
