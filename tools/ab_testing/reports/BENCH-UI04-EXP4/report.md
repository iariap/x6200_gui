# Render benchmark — BENCH-UI04-EXP4

- Status: **FAIL** (exit 1)
- Date: 2026-10-14
- a: `cc55931c9dd4bbb4c1c26e0b82837eb011416702`
- b: `7d4e89ad09c82e989ef0be5f5d38edf3b50453c5`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| bench-render | FAIL | 3.4045 | 5.1054 | 5.158 | 293.73 | 35.375% | 0.0% | 0.0% |
| dialog-about | PASS | 6.1408 | 6.6751 | 6.758 | 162.85 | -13.404% | 0.0% | 0.0% |
| home-bateria-externa | PASS | 3.2544 | 3.2847 | 3.3107 | 307.28 | -12.644% | 0.0% | 0.0% |
| home-frecuencia-extrema | PASS | 3.1764 | 3.4242 | 3.7618 | 314.82 | -1.985% | 0.0% | 0.0% |
| home-usb-20m | FAIL | 3.2569 | 4.7532 | 4.813 | 307.04 | 34.636% | 0.0% | 0.0% |
| main-bateria-baja | FAIL | 3.2086 | 4.8397 | 4.8763 | 311.66 | 45.697% | 0.0% | 0.0% |
| main-espectro-agua | PASS | 3.405 | 4.9684 | 5.0863 | 293.69 | 1.204% | 0.0% | 0.0% |
| main-foco | PASS | 3.2449 | 3.3777 | 4.527 | 308.18 | -9.908% | 0.0% | 0.0% |
| main-otro-tema | PASS | 3.1441 | 3.2715 | 3.279 | 318.06 | -31.539% | 0.0% | 0.0% |
| main-tema-classic | FAIL | 3.2373 | 4.7451 | 4.7597 | 308.9 | 39.181% | 0.0% | 0.0% |
| main-tema-holo | PASS | 3.2893 | 4.2479 | 4.7622 | 304.02 | -9.071% | 0.0% | 0.0% |
| main-tema-material-3 | PASS | 3.2927 | 3.5254 | 4.5325 | 303.7 | -28.367% | 0.0% | 0.0% |
| main-tema-outdoor-negative | PASS | 3.2325 | 3.6507 | 4.5941 | 309.35 | -6.078% | 0.0% | 0.0% |
| main-tema-rs | PASS | 3.2229 | 3.2711 | 3.293 | 310.27 | -32.674% | 0.0% | 0.0% |
| main-tema-vision | PASS | 3.3112 | 3.9936 | 4.0726 | 302.01 | 6.538% | 0.0% | 0.0% |
| main-tx-simulado | PASS | 3.2818 | 3.4351 | 4.5702 | 304.71 | 0.136% | 0.0% | 0.0% |

- **bench-render** (FAIL):
  - p95 render +35.4% (budget 25.0%): 3.771 -> 5.105 ms

- **home-usb-20m** (FAIL):
  - p95 render +34.6% (budget 25.0%): 3.530 -> 4.753 ms

- **main-bateria-baja** (FAIL):
  - p95 render +45.7% (budget 25.0%): 3.322 -> 4.840 ms

- **main-tema-classic** (FAIL):
  - p95 render +39.2% (budget 25.0%): 3.409 -> 4.745 ms

- Environment: x86 host: relative regression proxy, not the Cortex-A7
