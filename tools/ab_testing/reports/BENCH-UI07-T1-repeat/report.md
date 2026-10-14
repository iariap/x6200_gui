# Render benchmark — BENCH-UI07-T1-repeat

- Status: **FAIL** (exit 1)
- Date: 2026-10-14
- a: `d9c63b2d92b5d685396276d877c1df79ecd46845`
- b: `d9c63b2d92b5d685396276d877c1df79ecd46845`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| bench-render | PASS | 3.3758 | 3.509 | 4.6974 | 296.23 | -5.145% | 0.0% | 0.0% |
| dialog-about | PASS | 6.1993 | 7.616 | 7.9078 | 161.31 | 15.791% | 0.0% | 0.0% |
| home-bateria-externa | PASS | 3.2349 | 3.3874 | 4.445 | 309.13 | -2.766% | 0.0% | 0.0% |
| home-frecuencia-extrema | PASS | 3.1921 | 4.0143 | 4.5529 | 313.28 | 17.584% | 0.0% | 0.0% |
| home-usb-20m | PASS | 3.2304 | 3.3941 | 4.5718 | 309.56 | -7.321% | 0.0% | 0.0% |
| main-bateria-baja | PASS | 3.2058 | 3.5049 | 4.456 | 311.94 | 0.993% | 0.0% | 0.0% |
| main-espectro-agua | PASS | 3.372 | 3.8749 | 4.7408 | 296.56 | 6.134% | 0.0% | 0.0% |
| main-foco | PASS | 3.2437 | 3.6433 | 4.4951 | 308.29 | 11.939% | 0.0% | 0.0% |
| main-otro-tema | PASS | 3.121 | 3.2775 | 3.507 | 320.4 | 2.875% | 0.0% | 0.0% |
| main-tema-classic | FAIL | 3.2429 | 4.5395 | 4.7348 | 308.36 | 39.774% | 0.0% | 0.0% |
| main-tema-holo | PASS | 3.297 | 3.4248 | 4.643 | 303.31 | 1.146% | 0.0% | 0.0% |
| main-tema-material-3 | PASS | 3.2601 | 3.2981 | 3.4967 | 306.74 | -5.236% | 0.0% | 0.0% |
| main-tema-outdoor-negative | PASS | 3.2179 | 4.0923 | 4.5841 | 310.77 | 14.397% | 0.0% | 0.0% |
| main-tema-rs | PASS | 3.227 | 3.6053 | 4.6893 | 309.89 | -26.302% | 0.0% | 0.0% |
| main-tema-vision | PASS | 3.2654 | 3.9011 | 4.2022 | 306.24 | 15.431% | 0.0% | 0.0% |
| main-tema-vision-cwr | PASS | 3.2558 | 3.2989 | 3.4504 | 307.14 | -13.431% | 0.0% | 0.0% |
| main-tema-vision-usbd | PASS | 3.2686 | 4.5551 | 4.7838 | 305.94 | 20.721% | 0.0% | 0.0% |
| main-tx-simulado | PASS | 3.2713 | 3.6527 | 4.5949 | 305.69 | 3.496% | 0.0% | 0.0% |

- **main-tema-classic** (FAIL):
  - p95 render +39.8% (budget 25.0%): 3.248 -> 4.539 ms

- Environment: x86 host: relative regression proxy, not the Cortex-A7
