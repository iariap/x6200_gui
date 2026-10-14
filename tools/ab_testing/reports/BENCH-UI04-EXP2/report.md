# Render benchmark — BENCH-UI04-EXP2

- Status: **PASS** (exit 0)
- Date: 2026-10-14
- a: `8512d86845162533273648b3e45a514a2ea39aea`
- b: `6a17b83bed4a0228ae1871dc3262b61584df2f5e`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| bench-render | PASS | 3.3114 | 3.3583 | 3.3672 | 301.99 | -4.653% | 0.0% | 0.0% |
| dialog-about | PASS | 5.8841 | 6.0544 | 6.0758 | 169.95 | -2.067% | 0.0% | 0.0% |
| home-bateria-externa | PASS | 3.171 | 3.2751 | 3.7016 | 315.36 | -2.706% | 0.0% | 0.0% |
| home-frecuencia-extrema | PASS | 3.0853 | 3.1343 | 3.1489 | 324.12 | -3.977% | 0.0% | 0.0% |
| home-usb-20m | PASS | 3.1527 | 3.2684 | 3.419 | 317.19 | 1.199% | 0.0% | 0.0% |
| main-bateria-baja | PASS | 3.1057 | 3.3088 | 3.3586 | 321.99 | 3.101% | 0.0% | 0.0% |
| main-espectro-agua | PASS | 3.306 | 3.7627 | 4.0183 | 302.48 | 7.933% | 0.0% | 0.0% |
| main-foco | PASS | 3.1529 | 3.2091 | 3.4669 | 317.16 | -0.225% | 0.0% | 0.0% |
| main-otro-tema | PASS | 3.0434 | 3.079 | 3.0827 | 328.58 | -0.019% | 0.0% | 0.0% |
| main-tema-classic | PASS | 3.1163 | 3.1659 | 3.1708 | 320.89 | -16.664% | 0.0% | 0.0% |
| main-tema-holo | PASS | 3.1854 | 3.2482 | 3.2627 | 313.93 | 0.278% | 0.0% | 0.0% |
| main-tema-material-3 | PASS | 3.1927 | 3.3431 | 3.376 | 313.21 | -0.117% | 0.0% | 0.0% |
| main-tema-outdoor-negative | PASS | 3.129 | 3.2524 | 3.2545 | 319.59 | -14.85% | 0.0% | 0.0% |
| main-tema-rs | PASS | 3.243 | 3.3347 | 3.3486 | 308.36 | 4.594% | 0.0% | 0.0% |
| main-tema-vision | PASS | 3.33 | 3.7397 | 3.8255 | 300.3 | 9.511% | 0.0% | 0.0% |
| main-tx-simulado | PASS | 3.2097 | 3.3442 | 3.386 | 311.56 | 0.436% | 0.0% | 0.0% |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
