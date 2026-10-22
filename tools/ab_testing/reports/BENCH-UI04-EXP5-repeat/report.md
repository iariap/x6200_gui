# Render benchmark — BENCH-UI04-EXP5-repeat

- Status: **PASS** (exit 0)
- Date: 2026-10-14
- a: `9c031c85d4eaef1572be81f8b037d14a4b9245da`
- b: `7b644a7714588a97e17289ee8cd48785bbc3c2c9`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| bench-render | PASS | 3.4245 | 3.4974 | 3.5713 | 292.02 | -2.618% | 0.0% | 0.0% |
| dialog-about | PASS | 6.1282 | 6.2546 | 6.4003 | 163.18 | 0.223% | 0.0% | 0.0% |
| home-bateria-externa | PASS | 3.2306 | 3.2774 | 3.2825 | 309.54 | 0.379% | 0.0% | 0.0% |
| home-frecuencia-extrema | PASS | 3.1949 | 3.2417 | 3.3081 | 313.0 | -4.043% | 0.0% | 0.0% |
| home-usb-20m | PASS | 3.2848 | 3.4165 | 3.4294 | 304.43 | 3.384% | 0.0% | 0.0% |
| main-bateria-baja | PASS | 3.2035 | 3.2502 | 3.2537 | 312.16 | -6.517% | 0.0% | 0.0% |
| main-espectro-agua | PASS | 3.4286 | 3.6564 | 3.7926 | 291.66 | 4.297% | 0.0% | 0.0% |
| main-foco | PASS | 3.2653 | 3.3059 | 3.3457 | 306.25 | -6.817% | 0.0% | 0.0% |
| main-otro-tema | PASS | 3.1217 | 3.1682 | 3.1974 | 320.34 | -0.046% | 0.0% | 0.0% |
| main-tema-classic | PASS | 3.2944 | 3.5486 | 3.5528 | 303.55 | 9.051% | 0.0% | 0.0% |
| main-tema-holo | PASS | 3.286 | 3.3551 | 3.4845 | 304.32 | 2.161% | 0.0% | 0.0% |
| main-tema-material-3 | PASS | 3.2907 | 3.384 | 3.4494 | 303.89 | -0.315% | 0.0% | 0.0% |
| main-tema-outdoor-negative | PASS | 3.2411 | 3.3906 | 3.4817 | 308.54 | 0.246% | 0.0% | 0.0% |
| main-tema-rs | PASS | 3.2584 | 3.3227 | 3.3312 | 306.9 | -2.747% | 0.0% | 0.0% |
| main-tema-vision | PASS | 3.2923 | 3.459 | 3.7083 | 303.74 | -1.333% | 0.0% | 0.0% |
| main-tema-vision-cwr | PASS | 3.2862 | 3.3273 | 3.3692 | 304.31 | -1.671% | 0.0% | 0.0% |
| main-tema-vision-usbd | PASS | 3.3269 | 3.4444 | 3.453 | 300.58 | 2.787% | 0.0% | 0.0% |
| main-tx-simulado | PASS | 3.2729 | 3.3951 | 3.4017 | 305.54 | -10.72% | 0.0% | 0.0% |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
