# Render benchmark — BENCH-UI04-EXP1

- Status: **PASS** (exit 0)
- Date: 2026-10-14
- a: `b02990799fd9a974566f2520b361d2c4214632e2`
- b: `c471ecb6760e5ddb4297f287562a4633fed6fd3f`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| bench-render | PASS | 3.4634 | 3.6448 | 3.9063 | 288.73 | -0.405% | 0.0% | 0.0% |
| dialog-about | PASS | 6.2979 | 6.5328 | 6.7067 | 158.78 | -6.285% | 0.0% | 0.0% |
| home-bateria-externa | PASS | 3.2895 | 3.4905 | 3.5531 | 304.0 | -3.2% | 0.0% | 0.0% |
| home-frecuencia-extrema | PASS | 3.2146 | 3.3076 | 3.312 | 311.08 | -16.96% | 0.0% | 0.0% |
| home-usb-20m | PASS | 3.3228 | 3.5596 | 3.7848 | 300.96 | 3.164% | 0.0% | 0.0% |
| main-bateria-baja | PASS | 3.2361 | 3.2913 | 3.3495 | 309.01 | -2.158% | 0.0% | 0.0% |
| main-espectro-agua | PASS | 3.4877 | 3.6476 | 3.6582 | 286.72 | -9.495% | 0.0% | 0.0% |
| main-foco | PASS | 3.2949 | 3.4347 | 3.4425 | 303.5 | -17.925% | 0.0% | 0.0% |
| main-otro-tema | PASS | 3.1551 | 3.2748 | 3.301 | 316.94 | -1.598% | 0.0% | 0.0% |
| main-tema-classic | PASS | 3.2911 | 3.8075 | 4.9813 | 303.85 | 11.946% | 0.0% | 0.0% |
| main-tema-holo | PASS | 3.3371 | 3.4601 | 3.4697 | 299.66 | -1.348% | 0.0% | 0.0% |
| main-tema-material-3 | PASS | 3.347 | 3.5042 | 3.5919 | 298.78 | -20.847% | 0.0% | 0.0% |
| main-tema-outdoor-negative | PASS | 3.2636 | 3.4773 | 3.6843 | 306.41 | -3.266% | 0.0% | 0.0% |
| main-tema-rs | PASS | 3.2429 | 4.187 | 5.1523 | 308.37 | 16.924% | 0.0% | 0.0% |
| main-tema-vision | PASS | 3.3489 | 3.5692 | 3.586 | 298.61 | -9.577% | 0.0% | 0.0% |
| main-tx-simulado | PASS | 3.3427 | 3.5563 | 3.5802 | 299.15 | -2.09% | 0.0% | 0.0% |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
