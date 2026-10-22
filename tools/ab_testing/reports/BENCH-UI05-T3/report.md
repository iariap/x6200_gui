# Render benchmark — BENCH-UI05-T3

- Status: **PASS** (exit 0)
- Date: 2026-10-14
- a: `19aa6182d20998930c6ba9939d7e4ea7da43c304`
- b: `644e55dfb1b6572029c1df3951f01763271a60f6`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| bench-render | PASS | 3.347 | 4.0887 | 4.1345 | 298.77 | 16.196% | 0.0% | 0.0% |
| dialog-about | PASS | 5.9916 | 6.1943 | 6.2216 | 166.9 | -2.13% | 0.0% | 0.0% |
| home-bateria-externa | PASS | 3.2156 | 3.2653 | 3.2653 | 310.98 | 3.026% | 0.0% | 0.0% |
| home-frecuencia-extrema | PASS | 3.0827 | 3.111 | 3.1324 | 324.39 | -13.77% | 0.0% | 0.0% |
| home-usb-20m | PASS | 3.1642 | 3.3043 | 3.3402 | 316.04 | -2.012% | 0.0% | 0.0% |
| main-bateria-baja | PASS | 3.0856 | 3.1126 | 3.1236 | 324.09 | -3.466% | 0.0% | 0.0% |
| main-espectro-agua | PASS | 3.324 | 3.538 | 3.5661 | 300.84 | 1.545% | 0.0% | 0.0% |
| main-foco | PASS | 3.1601 | 3.9048 | 5.179 | 316.45 | 22.707% | 0.0% | 0.0% |
| main-otro-tema | PASS | 3.0476 | 3.2211 | 3.3072 | 328.13 | -0.377% | 0.0% | 0.0% |
| main-tema-classic | PASS | 3.1066 | 3.188 | 3.1884 | 321.9 | -34.174% | 0.0% | 0.0% |
| main-tema-holo | PASS | 3.1868 | 3.2826 | 3.7817 | 313.79 | 0.709% | 0.0% | 0.0% |
| main-tema-material-3 | PASS | 3.2437 | 3.3151 | 3.3284 | 308.29 | -4.852% | 0.0% | 0.0% |
| main-tema-outdoor-negative | PASS | 3.1925 | 3.4252 | 3.4395 | 313.23 | 5.944% | 0.0% | 0.0% |
| main-tema-rs | PASS | 3.1099 | 3.1854 | 3.4567 | 321.55 | -1.425% | 0.0% | 0.0% |
| main-tema-vision | PASS | 3.1726 | 3.2887 | 3.9059 | 315.19 | -3.862% | 0.0% | 0.0% |
| main-tx-simulado | PASS | 3.1867 | 3.8049 | 4.0219 | 313.81 | 14.687% | 0.0% | 0.0% |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
