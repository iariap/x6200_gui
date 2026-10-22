# Render benchmark — BENCH-UI04-EXP3

- Status: **PASS** (exit 0)
- Date: 2026-10-14
- a: `4d24990a9892c4587b9005b3ace17638e25ebeda`
- b: `fb2e5a8ff8ec7facb2aa1190728eede18ad6d1c6`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| bench-render | PASS | 3.3843 | 3.5154 | 3.5751 | 295.48 | -28.289% | 0.0% | 0.0% |
| dialog-about | PASS | 6.0758 | 6.1909 | 6.3315 | 164.59 | -22.629% | 0.0% | 0.0% |
| home-bateria-externa | PASS | 3.2087 | 3.3059 | 3.5154 | 311.66 | 1.318% | 0.0% | 0.0% |
| home-frecuencia-extrema | PASS | 3.1749 | 3.3368 | 4.4484 | 314.97 | 2.717% | 0.0% | 0.0% |
| home-usb-20m | PASS | 3.2165 | 3.3718 | 4.5088 | 310.9 | 3.037% | 0.0% | 0.0% |
| main-bateria-baja | PASS | 3.186 | 3.8054 | 4.3916 | 313.87 | -20.405% | 0.0% | 0.0% |
| main-espectro-agua | PASS | 3.3962 | 3.5028 | 3.5075 | 294.45 | -28.726% | 0.0% | 0.0% |
| main-foco | PASS | 3.2256 | 3.3497 | 3.4212 | 310.02 | -3.842% | 0.0% | 0.0% |
| main-otro-tema | PASS | 3.1089 | 3.4095 | 4.3197 | 321.66 | -4.007% | 0.0% | 0.0% |
| main-tema-classic | PASS | 3.2027 | 3.2755 | 3.3121 | 312.23 | -8.664% | 0.0% | 0.0% |
| main-tema-holo | PASS | 3.2639 | 3.3165 | 3.3186 | 306.39 | -29.952% | 0.0% | 0.0% |
| main-tema-material-3 | PASS | 3.2804 | 3.3741 | 3.5146 | 304.85 | -29.133% | 0.0% | 0.0% |
| main-tema-outdoor-negative | PASS | 3.2515 | 4.7496 | 4.819 | 307.55 | 13.479% | 0.0% | 0.0% |
| main-tema-rs | PASS | 3.1998 | 3.4616 | 4.4427 | 312.51 | -3.055% | 0.0% | 0.0% |
| main-tema-vision | PASS | 3.2887 | 3.9756 | 4.7403 | 304.08 | -4.2% | 0.0% | 0.0% |
| main-tx-simulado | PASS | 3.2893 | 3.4111 | 3.8966 | 304.02 | -14.638% | 0.0% | 0.0% |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
