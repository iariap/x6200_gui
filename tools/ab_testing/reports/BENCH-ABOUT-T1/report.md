# Render benchmark — BENCH-ABOUT-T1

- Status: **PASS** (exit 0)
- Date: 2026-10-14
- a: `0684861033691b9a78dfd402b3b78cfc360656fd`
- b: `9224734864e877b2df716261e474d942946c4c67`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| app-bands | PASS | 3.0705 | 3.1264 | 3.1342 | 325.67 | 0.499% | 0.0% | 0.0% |
| bench-render | PASS | 3.3293 | 3.389 | 3.395 | 300.36 | -2.296% | 0.0% | 0.0% |
| dialog-about | PASS | 6.0098 | 6.6807 | 6.8379 | 166.4 | 9.835% | 0.0% | 0.0% |
| home-bateria-externa | PASS | 3.1683 | 3.2421 | 3.26 | 315.63 | 1.017% | 0.0% | 0.0% |
| home-frecuencia-extrema | PASS | 3.1154 | 3.2751 | 3.3945 | 320.99 | 4.199% | 0.0% | 0.0% |
| home-usb-20m | PASS | 3.1677 | 3.3494 | 3.3532 | 315.68 | 4.13% | 0.0% | 0.0% |
| main-bateria-baja | PASS | 3.1226 | 3.2467 | 4.1405 | 320.25 | -1.692% | 0.0% | 0.0% |
| main-espectro-agua | PASS | 3.3684 | 3.6654 | 3.8691 | 296.88 | 5.571% | 0.0% | 0.0% |
| main-foco | PASS | 3.1572 | 3.1866 | 3.19 | 316.73 | -6.411% | 0.0% | 0.0% |
| main-natural | PASS | 3.2005 | 3.4248 | 4.7539 | 312.45 | 4.774% | 0.0% | 0.0% |
| main-natural-eco | PASS | 3.1839 | 3.2596 | 3.3739 | 314.08 | 1.335% | 0.0% | 0.0% |
| main-otro-tema | PASS | 3.0796 | 3.3922 | 3.6752 | 324.72 | 5.42% | 0.0% | 0.0% |
| main-tema-classic | PASS | 3.1441 | 3.2686 | 3.7428 | 318.06 | -2.26% | 0.0% | 0.0% |
| main-tema-holo | PASS | 3.2129 | 3.5279 | 3.8237 | 311.25 | 9.181% | 0.0% | 0.0% |
| main-tema-material-3 | PASS | 3.204 | 3.2856 | 3.515 | 312.11 | 1.83% | 0.0% | 0.0% |
| main-tema-outdoor-negative | PASS | 3.1596 | 3.2022 | 3.2125 | 316.5 | -0.215% | 0.0% | 0.0% |
| main-tema-rs | PASS | 3.168 | 3.2386 | 3.2723 | 315.66 | 2.898% | 0.0% | 0.0% |
| main-tema-vision | PASS | 3.2737 | 3.5581 | 4.1335 | 305.46 | 8.705% | 0.0% | 0.0% |
| main-tema-vision-cwr | PASS | 3.2311 | 3.3848 | 3.3991 | 309.49 | -0.629% | 0.0% | 0.0% |
| main-tema-vision-usbd | PASS | 3.2256 | 3.3005 | 3.5166 | 310.02 | -4.563% | 0.0% | 0.0% |
| main-tx-simulado | PASS | 3.271 | 4.1457 | 4.5603 | 305.72 | 20.433% | 0.0% | 0.0% |
| settings-bands | PASS | 3.1065 | 3.2906 | 3.7034 | 321.91 | 4.484% | 0.0% | 0.0% |
| settings-home | PASS | 6.5216 | 6.6057 | 6.8033 | 153.34 | 0.925% | 0.0% | 0.0% |
| settings-wfbat | PASS | 6.5226 | 7.3194 | 7.754 | 153.31 | 11.943% | 0.0% | 0.0% |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
