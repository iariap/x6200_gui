# Render benchmark — BENCH-UI04-EXP3-repeat

- Status: **FAIL** (exit 1)
- Date: 2026-10-14
- a: `4d24990a9892c4587b9005b3ace17638e25ebeda`
- b: `fb2e5a8ff8ec7facb2aa1190728eede18ad6d1c6`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| bench-render | FAIL | 3.4385 | 4.942 | 4.9536 | 290.82 | 44.035% | 0.0% | 0.0% |
| dialog-about | PASS | 6.1094 | 7.6518 | 7.7304 | 163.68 | -0.186% | 0.0% | 0.0% |
| home-bateria-externa | PASS | 3.2301 | 3.384 | 4.4129 | 309.59 | 0.229% | 0.0% | 0.0% |
| home-frecuencia-extrema | PASS | 3.1542 | 3.4092 | 4.5084 | 317.04 | 1.375% | 0.0% | 0.0% |
| home-usb-20m | PASS | 3.2487 | 3.4627 | 3.6109 | 307.82 | 5.203% | 0.0% | 0.0% |
| main-bateria-baja | PASS | 3.191 | 3.411 | 4.4882 | 313.38 | 0.833% | 0.0% | 0.0% |
| main-espectro-agua | PASS | 3.3969 | 3.7056 | 4.8101 | 294.39 | 3.643% | 0.0% | 0.0% |
| main-foco | PASS | 3.213 | 3.5592 | 3.7666 | 311.24 | 8.323% | 0.0% | 0.0% |
| main-otro-tema | PASS | 3.1049 | 3.6267 | 4.45 | 322.07 | 9.503% | 0.0% | 0.0% |
| main-tema-classic | PASS | 3.2157 | 3.4809 | 4.6512 | 310.97 | -26.529% | 0.0% | 0.0% |
| main-tema-holo | PASS | 3.2645 | 3.4031 | 4.5109 | 306.33 | -28.143% | 0.0% | 0.0% |
| main-tema-material-3 | PASS | 3.2565 | 3.4017 | 4.0627 | 307.07 | -23.038% | 0.0% | 0.0% |
| main-tema-outdoor-negative | PASS | 3.2252 | 3.2718 | 3.2759 | 310.05 | -3.226% | 0.0% | 0.0% |
| main-tema-rs | PASS | 3.2171 | 3.6912 | 4.6246 | 310.84 | 13.444% | 0.0% | 0.0% |
| main-tema-vision | PASS | 3.2288 | 3.3707 | 3.3948 | 309.71 | -8.075% | 0.0% | 0.0% |
| main-tx-simulado | PASS | 3.2492 | 3.3775 | 3.6252 | 307.76 | -21.317% | 0.0% | 0.0% |

- **bench-render** (FAIL):
  - p95 render +44.0% (budget 25.0%): 3.431 -> 4.942 ms

- Environment: x86 host: relative regression proxy, not the Cortex-A7
