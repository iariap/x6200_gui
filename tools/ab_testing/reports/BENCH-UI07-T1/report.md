# Render benchmark — BENCH-UI07-T1

- Status: **FAIL** (exit 1)
- Date: 2026-10-14
- a: `d9c63b2d92b5d685396276d877c1df79ecd46845`
- b: `d9c63b2d92b5d685396276d877c1df79ecd46845`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| bench-render | FAIL | 3.4165 | 4.9305 | 4.9995 | 292.7 | 38.73% | 0.0% | 0.0% |
| dialog-about | PASS | 6.0432 | 7.7793 | 7.8731 | 165.48 | 1.197% | 0.0% | 0.0% |
| home-bateria-externa | PASS | 3.2494 | 4.1245 | 4.2026 | 307.74 | -14.725% | 0.0% | 0.0% |
| home-frecuencia-extrema | PASS | 3.2175 | 3.3554 | 3.3889 | 310.81 | 1.818% | 0.0% | 0.0% |
| home-usb-20m | PASS | 3.2339 | 3.294 | 3.3346 | 309.23 | -2.412% | 0.0% | 0.0% |
| main-bateria-baja | PASS | 3.2096 | 3.4857 | 4.5011 | 311.56 | 6.459% | 0.0% | 0.0% |
| main-espectro-agua | FAIL | 3.4363 | 5.0786 | 5.1243 | 291.01 | 47.775% | 0.0% | 0.0% |
| main-foco | PASS | 3.2622 | 4.8459 | 4.935 | 306.54 | 20.798% | 0.0% | 0.0% |
| main-otro-tema | PASS | 3.1432 | 4.6544 | 4.727 | 318.15 | 22.878% | 0.0% | 0.0% |
| main-tema-classic | PASS | 3.2393 | 4.0508 | 4.0731 | 308.7 | 22.159% | 0.0% | 0.0% |
| main-tema-holo | PASS | 3.2743 | 3.4762 | 4.2156 | 305.41 | 1.81% | 0.0% | 0.0% |
| main-tema-material-3 | PASS | 3.3024 | 3.5308 | 3.6809 | 302.81 | -12.887% | 0.0% | 0.0% |
| main-tema-outdoor-negative | PASS | 3.2465 | 3.4156 | 3.5196 | 308.02 | -8.221% | 0.0% | 0.0% |
| main-tema-rs | PASS | 3.2115 | 3.3567 | 3.7433 | 311.38 | -5.098% | 0.0% | 0.0% |
| main-tema-vision | PASS | 3.315 | 3.4952 | 3.5209 | 301.66 | 1.218% | 0.0% | 0.0% |
| main-tema-vision-cwr | PASS | 3.3091 | 3.7383 | 4.2302 | 302.19 | 4.012% | 0.0% | 0.0% |
| main-tema-vision-usbd | PASS | 3.3106 | 3.4173 | 3.4371 | 302.06 | 1.508% | 0.0% | 0.0% |
| main-tx-simulado | PASS | 3.2862 | 3.4207 | 4.6497 | 304.3 | -5.325% | 0.0% | 0.0% |

- **bench-render** (FAIL):
  - p95 render +38.7% (budget 25.0%): 3.554 -> 4.931 ms

- **main-espectro-agua** (FAIL):
  - p95 render +47.8% (budget 25.0%): 3.437 -> 5.079 ms

- Environment: x86 host: relative regression proxy, not the Cortex-A7
