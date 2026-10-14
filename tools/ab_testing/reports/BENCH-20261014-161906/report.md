# Render benchmark — BENCH-20261014-161906

- Status: **FAIL** (exit 1)
- Date: 2026-10-14
- a: `4c711f60b3789372f9e1c117e514418f0eee00c8`
- b: `4c711f60b3789372f9e1c117e514418f0eee00c8`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| bench-render | PASS | 3.4268 | 3.5868 | 3.597 | 291.82 | 2.225% | 0.0% | 0.0% |
| dialog-about | PASS | 6.1715 | 7.6608 | 7.9545 | 162.04 | 15.425% | 0.0% | 0.0% |
| home-bateria-externa | PASS | 3.279 | 3.8473 | 4.6403 | 304.98 | -18.505% | 0.0% | 0.0% |
| home-frecuencia-extrema | PASS | 3.2311 | 3.7531 | 6.1741 | 309.49 | -19.735% | 0.0% | 0.0% |
| home-usb-20m | PASS | 3.2725 | 4.7284 | 4.8458 | 305.57 | -1.195% | 0.0% | 0.0% |
| main-bateria-baja | PASS | 3.2279 | 3.3803 | 3.6093 | 309.79 | -27.492% | 0.0% | 0.0% |
| main-espectro-agua | PASS | 3.4474 | 3.7696 | 4.7875 | 290.07 | 3.146% | 0.0% | 0.0% |
| main-foco | PASS | 3.3114 | 3.8537 | 4.6113 | 301.99 | 16.094% | 0.0% | 0.0% |
| main-natural | PASS | 3.2509 | 3.5193 | 5.7098 | 307.61 | 2.988% | 0.0% | 0.0% |
| main-natural-eco | PASS | 3.2663 | 4.1045 | 4.3969 | 306.16 | 14.993% | 0.0% | 0.0% |
| main-otro-tema | PASS | 3.1448 | 3.2806 | 3.3052 | 317.99 | 0.461% | 0.0% | 0.0% |
| main-tema-classic | PASS | 3.2469 | 3.2952 | 3.2986 | 307.99 | -2.496% | 0.0% | 0.0% |
| main-tema-holo | FAIL | 3.3333 | 4.8511 | 4.8946 | 300.0 | 36.16% | 0.0% | 0.0% |
| main-tema-material-3 | PASS | 3.3029 | 3.5623 | 4.6229 | 302.76 | 4.136% | 0.0% | 0.0% |
| main-tema-outdoor-negative | PASS | 3.2977 | 3.4332 | 3.4844 | 303.24 | 1.194% | 0.0% | 0.0% |
| main-tema-rs | FAIL | 3.2395 | 4.8075 | 4.8257 | 308.69 | 34.532% | 0.0% | 0.0% |
| main-tema-vision | PASS | 3.2886 | 3.6644 | 4.6322 | 304.08 | 6.143% | 0.0% | 0.0% |
| main-tema-vision-cwr | PASS | 3.2892 | 3.4002 | 3.9745 | 304.03 | 1.619% | 0.0% | 0.0% |
| main-tema-vision-usbd | PASS | 3.3053 | 4.0942 | 4.6729 | 302.54 | 17.774% | 0.0% | 0.0% |
| main-tx-simulado | PASS | 3.2804 | 3.3245 | 3.3617 | 304.85 | -4.109% | 0.0% | 0.0% |

- **main-tema-holo** (FAIL):
  - p95 render +36.2% (budget 25.0%): 3.563 -> 4.851 ms

- **main-tema-rs** (FAIL):
  - p95 render +34.5% (budget 25.0%): 3.573 -> 4.808 ms

- Environment: x86 host: relative regression proxy, not the Cortex-A7
