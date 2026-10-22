# Render benchmark — BENCH-UI08-T1

- Status: **PASS** (exit 0)
- Date: 2026-10-14
- a: `c65d2046b289c4f182bb51ccbe75b5dd942bef86`
- b: `07ac3bc3c2d9cf369a85642632a0f2dd5b54a80e`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| bench-render | PASS | 3.3195 | 3.4629 | 3.5024 | 301.25 | -3.774% | 0.0% | 0.0% |
| dialog-about | PASS | 5.9113 | 6.7539 | 7.0311 | 169.17 | 8.405% | 0.0% | 0.0% |
| home-bateria-externa | PASS | 3.1303 | 3.1886 | 3.2016 | 319.46 | -0.142% | 0.0% | 0.0% |
| home-frecuencia-extrema | PASS | 3.1008 | 3.6807 | 3.7134 | 322.5 | 15.201% | 0.0% | 0.0% |
| home-usb-20m | PASS | 3.1313 | 3.2068 | 3.2087 | 319.36 | -2.22% | 0.0% | 0.0% |
| main-bateria-baja | PASS | 3.1404 | 3.3227 | 3.3309 | 318.43 | 3.134% | 0.0% | 0.0% |
| main-espectro-agua | PASS | 3.4066 | 3.6564 | 4.4906 | 293.55 | 9.788% | 0.0% | 0.0% |
| main-foco | PASS | 3.1442 | 3.1722 | 3.1833 | 318.05 | -3.805% | 0.0% | 0.0% |
| main-natural | PASS | 3.173 | 3.5507 | 4.6449 | 315.16 | 10.153% | 0.0% | 0.0% |
| main-otro-tema | PASS | 3.0339 | 3.0666 | 3.0804 | 329.6 | -1.551% | 0.0% | 0.0% |
| main-tema-classic | PASS | 3.1251 | 3.2155 | 3.3317 | 319.99 | 1.345% | 0.0% | 0.0% |
| main-tema-holo | PASS | 3.1817 | 3.3616 | 3.7229 | 314.3 | 1.41% | 0.0% | 0.0% |
| main-tema-material-3 | PASS | 3.1886 | 3.2557 | 3.3238 | 313.61 | 0.966% | 0.0% | 0.0% |
| main-tema-outdoor-negative | PASS | 3.1414 | 3.2541 | 3.3477 | 318.33 | 1.551% | 0.0% | 0.0% |
| main-tema-rs | PASS | 3.158 | 3.297 | 3.9236 | 316.66 | 3.853% | 0.0% | 0.0% |
| main-tema-vision | PASS | 3.1983 | 3.3809 | 3.7891 | 312.66 | 4.5% | 0.0% | 0.0% |
| main-tema-vision-cwr | PASS | 3.19 | 3.2365 | 3.2964 | 313.48 | -5.913% | 0.0% | 0.0% |
| main-tema-vision-usbd | PASS | 3.1856 | 3.357 | 3.5345 | 313.91 | 1.612% | 0.0% | 0.0% |
| main-tx-simulado | PASS | 3.1802 | 3.235 | 3.2388 | 314.45 | 0.399% | 0.0% | 0.0% |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
