# Render benchmark — BENCH-20261014-162449

- Status: **PASS** (exit 0)
- Date: 2026-10-14
- a: `4c711f60b3789372f9e1c117e514418f0eee00c8`
- b: `625f254b7b1ba359ebcd4ea0f7c29859ceaa082e`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| bench-render | PASS | 3.4112 | 4.9905 | 5.0103 | 293.16 | 19.341% | 0.0% | 0.0% |
| dialog-about | PASS | 6.2472 | 7.8896 | 8.3386 | 160.07 | 20.652% | 0.0% | 0.0% |
| home-bateria-externa | PASS | 3.2592 | 4.1139 | 4.7249 | 306.82 | 18.049% | 0.0% | 0.0% |
| home-frecuencia-extrema | PASS | 3.2355 | 3.4121 | 4.5395 | 309.07 | -14.725% | 0.0% | 0.0% |
| home-usb-20m | PASS | 3.2809 | 4.4495 | 4.8127 | 304.79 | 23.867% | 0.0% | 0.0% |
| main-bateria-baja | PASS | 3.2389 | 4.0538 | 4.5485 | 308.75 | -14.72% | 0.0% | 0.0% |
| main-espectro-agua | PASS | 3.4141 | 3.7933 | 4.7318 | 292.91 | -23.351% | 0.0% | 0.0% |
| main-foco | PASS | 3.2483 | 3.4485 | 4.9657 | 307.85 | -11.342% | 0.0% | 0.0% |
| main-natural | PASS | 3.267 | 3.5705 | 4.8845 | 306.1 | -17.227% | 0.0% | 0.0% |
| main-natural-eco | PASS | 3.2715 | 3.4118 | 3.4585 | 305.67 | -3.652% | 0.0% | 0.0% |
| main-otro-tema | PASS | 3.1626 | 3.2529 | 3.4919 | 316.2 | 0.648% | 0.0% | 0.0% |
| main-tema-classic | PASS | 3.2376 | 4.1892 | 4.5449 | 308.88 | -16.066% | 0.0% | 0.0% |
| main-tema-holo | PASS | 3.2965 | 3.3561 | 3.3699 | 303.36 | -6.534% | 0.0% | 0.0% |
| main-tema-material-3 | PASS | 3.3082 | 3.3431 | 3.3635 | 302.28 | -5.164% | 0.0% | 0.0% |
| main-tema-outdoor-negative | PASS | 3.2749 | 3.594 | 4.36 | 305.35 | 5.674% | 0.0% | 0.0% |
| main-tema-rs | PASS | 3.2623 | 3.5029 | 3.6875 | 306.53 | 4.012% | 0.0% | 0.0% |
| main-tema-vision | PASS | 3.3754 | 3.5074 | 3.5084 | 296.26 | 5.418% | 0.0% | 0.0% |
| main-tema-vision-cwr | PASS | 3.3199 | 3.5565 | 3.5769 | 301.21 | -26.916% | 0.0% | 0.0% |
| main-tema-vision-usbd | PASS | 3.3283 | 3.9025 | 5.0511 | 300.45 | -19.449% | 0.0% | 0.0% |
| main-tx-simulado | PASS | 3.3037 | 3.3533 | 3.3631 | 302.69 | -30.331% | 0.0% | 0.0% |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
