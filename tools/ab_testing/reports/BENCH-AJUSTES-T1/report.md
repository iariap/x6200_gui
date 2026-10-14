# Render benchmark — BENCH-AJUSTES-T1

- Status: **FAIL** (exit 1)
- Date: 2026-10-14
- a: `f360f9218e0814bbb4ed91ef79942b3f45864ba8`
- b: `63ecc8716859d420cd937a42e851fd912dc66658`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| bench-render | PASS | 3.5003 | 4.0485 | 4.8414 | 285.69 | 11.761% | 0.0% | 0.0% |
| dialog-about | PASS | 6.2414 | 7.6291 | 7.8692 | 160.22 | 2.911% | 0.0% | 0.0% |
| home-bateria-externa | PASS | 3.272 | 3.3405 | 3.4727 | 305.62 | -12.446% | 0.0% | 0.0% |
| home-frecuencia-extrema | PASS | 3.2409 | 3.3122 | 3.7374 | 308.55 | -29.255% | 0.0% | 0.0% |
| home-usb-20m | PASS | 3.2811 | 3.451 | 3.5284 | 304.78 | 1.726% | 0.0% | 0.0% |
| main-bateria-baja | FAIL | 3.2543 | 4.7272 | 4.851 | 307.29 | 40.006% | 0.0% | 0.0% |
| main-espectro-agua | PASS | 3.4707 | 3.8078 | 4.7903 | 288.12 | -8.628% | 0.0% | 0.0% |
| main-foco | FAIL | 3.2912 | 4.8079 | 5.4292 | 303.84 | 42.404% | 0.0% | 0.0% |
| main-natural | PASS | 3.3304 | 3.5629 | 4.5674 | 300.27 | 4.479% | 0.0% | 0.0% |
| main-natural-eco | PASS | 3.2969 | 3.4801 | 3.5112 | 303.32 | -26.764% | 0.0% | 0.0% |
| main-otro-tema | PASS | 3.1516 | 3.2427 | 3.3027 | 317.3 | -6.93% | 0.0% | 0.0% |
| main-tema-classic | PASS | 3.2828 | 3.6273 | 3.6735 | 304.61 | -5.598% | 0.0% | 0.0% |
| main-tema-holo | PASS | 3.3486 | 3.4685 | 3.4836 | 298.63 | 1.814% | 0.0% | 0.0% |
| main-tema-material-3 | PASS | 3.3415 | 3.7968 | 4.6952 | 299.26 | 11.469% | 0.0% | 0.0% |
| main-tema-outdoor-negative | PASS | 3.3104 | 3.9555 | 4.6477 | 302.08 | 19.187% | 0.0% | 0.0% |
| main-tema-rs | FAIL | 3.3417 | 4.6 | 4.8303 | 299.25 | 40.535% | 0.0% | 0.0% |
| main-tema-vision | FAIL | 3.3386 | 4.8168 | 4.8517 | 299.53 | 39.169% | 0.0% | 0.0% |
| main-tema-vision-cwr | FAIL | 3.3061 | 4.8071 | 4.8446 | 302.47 | 41.188% | 0.0% | 0.0% |
| main-tema-vision-usbd | PASS | 3.2891 | 3.3527 | 3.3577 | 304.03 | -15.028% | 0.0% | 0.0% |
| main-tx-simulado | PASS | 3.3071 | 3.5929 | 3.6293 | 302.38 | -12.758% | 0.0% | 0.0% |

- **main-bateria-baja** (FAIL):
  - p95 render +40.0% (budget 25.0%): 3.376 -> 4.727 ms

- **main-foco** (FAIL):
  - p95 render +42.4% (budget 25.0%): 3.376 -> 4.808 ms

- **main-tema-rs** (FAIL):
  - p95 render +40.5% (budget 25.0%): 3.273 -> 4.600 ms

- **main-tema-vision** (FAIL):
  - p95 render +39.2% (budget 25.0%): 3.461 -> 4.817 ms

- **main-tema-vision-cwr** (FAIL):
  - p95 render +41.2% (budget 25.0%): 3.405 -> 4.807 ms

- Environment: x86 host: relative regression proxy, not the Cortex-A7
