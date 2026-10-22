# Render benchmark — BENCH-20261014-162612

- Status: **FAIL** (exit 1)
- Date: 2026-10-14
- a: `4c711f60b3789372f9e1c117e514418f0eee00c8`
- b: `625f254b7b1ba359ebcd4ea0f7c29859ceaa082e`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| bench-render | PASS | 3.4132 | 3.4748 | 3.5447 | 292.98 | -18.629% | 0.0% | 0.0% |
| dialog-about | PASS | 6.1776 | 7.9139 | 8.2972 | 161.88 | 16.262% | 0.0% | 0.0% |
| home-bateria-externa | PASS | 3.2518 | 4.7303 | 4.7822 | 307.53 | -1.259% | 0.0% | 0.0% |
| home-frecuencia-extrema | PASS | 3.2112 | 4.7247 | 4.7284 | 311.41 | -1.009% | 0.0% | 0.0% |
| home-usb-20m | PASS | 3.2558 | 3.5495 | 4.6101 | 307.14 | 4.34% | 0.0% | 0.0% |
| main-bateria-baja | PASS | 3.263 | 3.5182 | 4.8629 | 306.46 | 7.141% | 0.0% | 0.0% |
| main-espectro-agua | PASS | 3.4194 | 3.5012 | 3.5976 | 292.45 | -4.119% | 0.0% | 0.0% |
| main-foco | PASS | 3.2481 | 3.4169 | 3.48 | 307.88 | -27.606% | 0.0% | 0.0% |
| main-natural | PASS | 3.2795 | 3.4164 | 3.5873 | 304.92 | -30.619% | 0.0% | 0.0% |
| main-natural-eco | PASS | 3.2538 | 3.468 | 4.4795 | 307.34 | -9.945% | 0.0% | 0.0% |
| main-otro-tema | PASS | 3.1934 | 3.9009 | 4.4526 | 313.14 | -15.511% | 0.0% | 0.0% |
| main-tema-classic | FAIL | 3.2689 | 4.7317 | 4.7387 | 305.92 | 29.226% | 0.0% | 0.0% |
| main-tema-holo | PASS | 3.291 | 3.4306 | 4.4915 | 303.86 | -3.225% | 0.0% | 0.0% |
| main-tema-material-3 | PASS | 3.3224 | 3.4645 | 3.4945 | 300.99 | -27.546% | 0.0% | 0.0% |
| main-tema-outdoor-negative | PASS | 3.2793 | 4.9819 | 5.4534 | 304.94 | 17.833% | 0.0% | 0.0% |
| main-tema-rs | PASS | 3.2283 | 3.528 | 4.5691 | 309.76 | 3.227% | 0.0% | 0.0% |
| main-tema-vision | FAIL | 3.3418 | 4.8543 | 5.1634 | 299.24 | 44.265% | 0.0% | 0.0% |
| main-tema-vision-cwr | FAIL | 3.3148 | 4.5339 | 5.0289 | 301.68 | 34.351% | 0.0% | 0.0% |
| main-tema-vision-usbd | FAIL | 3.339 | 4.7851 | 4.8337 | 299.49 | 33.556% | 0.0% | 0.0% |
| main-tx-simulado | PASS | 3.3256 | 3.7342 | 4.604 | 300.69 | 11.203% | 0.0% | 0.0% |

- **main-tema-classic** (FAIL):
  - p95 render +29.2% (budget 25.0%): 3.662 -> 4.732 ms

- **main-tema-vision** (FAIL):
  - p95 render +44.3% (budget 25.0%): 3.365 -> 4.854 ms

- **main-tema-vision-cwr** (FAIL):
  - p95 render +34.4% (budget 25.0%): 3.375 -> 4.534 ms

- **main-tema-vision-usbd** (FAIL):
  - p95 render +33.6% (budget 25.0%): 3.583 -> 4.785 ms

- Environment: x86 host: relative regression proxy, not the Cortex-A7
