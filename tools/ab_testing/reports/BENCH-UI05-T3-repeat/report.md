# Render benchmark — BENCH-UI05-T3-retry2

- Status: **FAIL** (exit 1)
- Date: 2026-10-14
- a: `19aa6182d20998930c6ba9939d7e4ea7da43c304`
- b: `644e55dfb1b6572029c1df3951f01763271a60f6`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| bench-render | PASS | 3.4284 | 3.5357 | 3.6595 | 291.69 | -5.053% | 0.0% | 0.0% |
| dialog-about | PASS | 6.202 | 6.5177 | 6.6934 | 161.24 | -15.793% | 0.0% | 0.0% |
| home-bateria-externa | FAIL | 3.285 | 4.261 | 4.3161 | 304.41 | 26.437% | 0.0% | 0.0% |
| home-frecuencia-extrema | PASS | 3.2153 | 3.6602 | 3.6995 | 311.01 | 6.175% | 0.0% | 0.0% |
| home-usb-20m | PASS | 3.2496 | 3.5926 | 4.5524 | 307.73 | -8.803% | 0.0% | 0.0% |
| main-bateria-baja | PASS | 3.1688 | 3.285 | 3.3155 | 315.57 | -10.255% | 0.0% | 0.0% |
| main-espectro-agua | PASS | 3.4275 | 3.6137 | 3.6598 | 291.76 | -5.558% | 0.0% | 0.0% |
| main-foco | PASS | 3.2119 | 3.4197 | 3.5012 | 311.35 | -8.189% | 0.0% | 0.0% |
| main-otro-tema | PASS | 3.1611 | 3.408 | 3.5286 | 316.35 | 4.281% | 0.0% | 0.0% |
| main-tema-classic | PASS | 3.2792 | 3.6888 | 4.7109 | 304.95 | 12.384% | 0.0% | 0.0% |
| main-tema-holo | PASS | 3.262 | 3.5952 | 3.6174 | 306.56 | 2.47% | 0.0% | 0.0% |
| main-tema-material-3 | PASS | 3.2361 | 3.3849 | 3.4099 | 309.02 | -9.363% | 0.0% | 0.0% |
| main-tema-outdoor-negative | PASS | 3.259 | 3.3706 | 3.6754 | 306.84 | 3.469% | 0.0% | 0.0% |
| main-tema-rs | FAIL | 3.2829 | 4.5644 | 4.7292 | 304.61 | 31.964% | 0.0% | 0.0% |
| main-tema-vision | PASS | 3.2375 | 3.3799 | 3.4599 | 308.88 | -3.523% | 0.0% | 0.0% |
| main-tx-simulado | PASS | 3.1944 | 3.7993 | 4.0679 | 313.04 | -11.681% | 0.0% | 0.0% |

- **home-bateria-externa** (FAIL):
  - p95 render +26.4% (budget 25.0%): 3.370 -> 4.261 ms

- **main-tema-rs** (FAIL):
  - p95 render +32.0% (budget 25.0%): 3.459 -> 4.564 ms

- Environment: x86 host: relative regression proxy, not the Cortex-A7
