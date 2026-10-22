# Render benchmark — BENCH-UI04-EXP5

- Status: **FAIL** (exit 1)
- Date: 2026-10-14
- a: `9c031c85d4eaef1572be81f8b037d14a4b9245da`
- b: `7b644a7714588a97e17289ee8cd48785bbc3c2c9`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| bench-render | PASS | 3.3959 | 3.4411 | 3.4588 | 294.48 | 0.162% | 0.0% | 0.0% |
| dialog-about | PASS | 6.0748 | 6.149 | 6.5445 | 164.61 | -3.328% | 0.0% | 0.0% |
| home-bateria-externa | PASS | 3.2147 | 3.2397 | 3.241 | 311.07 | -1.741% | 0.0% | 0.0% |
| home-frecuencia-extrema | PASS | 3.1708 | 3.2076 | 3.217 | 315.38 | -5.357% | 0.0% | 0.0% |
| home-usb-20m | PASS | 3.2358 | 3.3933 | 3.5655 | 309.04 | 3.42% | 0.0% | 0.0% |
| main-bateria-baja | PASS | 3.1755 | 3.275 | 4.2534 | 314.91 | 0.648% | 0.0% | 0.0% |
| main-espectro-agua | PASS | 3.3965 | 3.484 | 3.9996 | 294.42 | -2.454% | 0.0% | 0.0% |
| main-foco | PASS | 3.2771 | 3.331 | 3.3524 | 305.14 | 1.09% | 0.0% | 0.0% |
| main-otro-tema | PASS | 3.1326 | 3.2027 | 3.2232 | 319.22 | -2.11% | 0.0% | 0.0% |
| main-tema-classic | PASS | 3.1976 | 3.2783 | 3.3074 | 312.74 | -0.094% | 0.0% | 0.0% |
| main-tema-holo | PASS | 3.262 | 3.3308 | 3.4122 | 306.56 | -0.846% | 0.0% | 0.0% |
| main-tema-material-3 | PASS | 3.2759 | 3.426 | 3.494 | 305.25 | 3.283% | 0.0% | 0.0% |
| main-tema-outdoor-negative | PASS | 3.2331 | 3.3214 | 3.3428 | 309.3 | -0.694% | 0.0% | 0.0% |
| main-tema-rs | FAIL | 3.2157 | 4.6554 | 4.8812 | 310.97 | 38.557% | 0.0% | 0.0% |
| main-tema-vision | PASS | 3.2862 | 3.3805 | 3.5023 | 304.31 | -0.77% | 0.0% | 0.0% |
| main-tema-vision-cwr | PASS | 3.264 | 3.3031 | 3.3052 | 306.38 | -0.385% | 0.0% | 0.0% |
| main-tema-vision-usbd | PASS | 3.2849 | 3.4605 | 3.6901 | 304.42 | 5.008% | 0.0% | 0.0% |
| main-tx-simulado | PASS | 3.2872 | 3.5995 | 4.6124 | 304.21 | 8.873% | 0.0% | 0.0% |

- **main-tema-rs** (FAIL):
  - p95 render +38.6% (budget 25.0%): 3.360 -> 4.655 ms

- Environment: x86 host: relative regression proxy, not the Cortex-A7
