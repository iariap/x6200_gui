# Render benchmark — BENCH-20261014-161143

- Status: **FAIL** (exit 1)
- Date: 2026-10-14
- a: `4c711f60b3789372f9e1c117e514418f0eee00c8`
- b: `6508c139ab5200de2196c26fb3439d5e82201fb2`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| bench-render | PASS | 3.4555 | 4.2865 | 4.3371 | 289.39 | 23.94% | 0.0% | 0.0% |
| dialog-about | PASS | 6.2649 | 7.8359 | 7.9691 | 159.62 | 0.128% | 0.0% | 0.0% |
| home-bateria-externa | PASS | 3.261 | 3.3867 | 4.4601 | 306.65 | -0.898% | 0.0% | 0.0% |
| home-frecuencia-extrema | FAIL | 3.1836 | 4.7858 | 4.9994 | 314.11 | 45.717% | 0.0% | 0.0% |
| home-usb-20m | PASS | 3.2422 | 3.391 | 3.4152 | 308.43 | -28.209% | 0.0% | 0.0% |
| main-bateria-baja | PASS | 3.2069 | 3.545 | 4.4381 | 311.83 | -24.95% | 0.0% | 0.0% |
| main-espectro-agua | PASS | 3.4537 | 3.6305 | 3.6605 | 289.54 | 1.318% | 0.0% | 0.0% |
| main-foco | PASS | 3.2649 | 3.7847 | 4.0983 | 306.28 | -15.28% | 0.0% | 0.0% |
| main-natural | PASS | 3.2873 | 4.7883 | 5.3237 | 304.21 | 0.665% | 0.0% | 0.0% |
| main-natural-eco | PASS | 3.2778 | 3.5139 | 3.7882 | 305.09 | 2.618% | 0.0% | 0.0% |
| main-otro-tema | PASS | 3.1631 | 3.2429 | 3.6183 | 316.15 | -12.093% | 0.0% | 0.0% |
| main-tema-classic | PASS | 3.2666 | 4.1083 | 4.6612 | 306.13 | -17.75% | 0.0% | 0.0% |
| main-tema-holo | PASS | 3.2938 | 3.4188 | 4.4557 | 303.6 | -29.017% | 0.0% | 0.0% |
| main-tema-material-3 | PASS | 3.306 | 4.8229 | 4.9179 | 302.48 | 17.842% | 0.0% | 0.0% |
| main-tema-outdoor-negative | PASS | 3.2513 | 4.9768 | 5.3675 | 307.57 | 5.244% | 0.0% | 0.0% |
| main-tema-rs | PASS | 3.217 | 3.2488 | 3.2515 | 310.84 | -25.613% | 0.0% | 0.0% |
| main-tema-vision | PASS | 3.2929 | 3.7383 | 3.9193 | 303.68 | 7.484% | 0.0% | 0.0% |
| main-tema-vision-cwr | FAIL | 3.3352 | 4.3328 | 4.8048 | 299.84 | 27.067% | 0.0% | 0.0% |
| main-tema-vision-usbd | PASS | 3.3106 | 3.3461 | 3.363 | 302.06 | -31.103% | 0.0% | 0.0% |
| main-tx-simulado | PASS | 3.3093 | 3.3732 | 3.4438 | 302.18 | -2.339% | 0.0% | 0.0% |

- **home-frecuencia-extrema** (FAIL):
  - p95 render +45.7% (budget 25.0%): 3.284 -> 4.786 ms

- **main-tema-vision-cwr** (FAIL):
  - p95 render +27.1% (budget 25.0%): 3.410 -> 4.333 ms

- Environment: x86 host: relative regression proxy, not the Cortex-A7
