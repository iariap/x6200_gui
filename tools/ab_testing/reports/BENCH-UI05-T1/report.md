# Render benchmark — BENCH-UI05-T1

- Status: **PASS** (exit 0)
- Date: 2026-10-14
- a: `135f2970eb86fa580476dad3addbd72ec5f4baa7`
- b: `ccaa95fdd9e7796d27ccab39889f7700547b5abd`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| bench-render | PASS | 3.333 | 3.4377 | 4.0293 | 300.03 | -3.345% | 0.0% | 0.0% |
| dialog-about | PASS | 5.9609 | 6.2591 | 6.453 | 167.76 | -2.044% | 0.0% | 0.0% |
| home-bateria-externa | PASS | 3.1502 | 4.1438 | 4.5461 | 317.44 | 12.812% | 0.0% | 0.0% |
| home-frecuencia-extrema | PASS | 3.1803 | 3.4862 | 4.4526 | 314.44 | -11.682% | 0.0% | 0.0% |
| home-usb-20m | PASS | 3.1214 | 3.187 | 3.2218 | 320.37 | -1.431% | 0.0% | 0.0% |
| main-bateria-baja | PASS | 3.0859 | 3.1376 | 3.1404 | 324.05 | -0.833% | 0.0% | 0.0% |
| main-espectro-agua | PASS | 3.3155 | 3.3433 | 3.4409 | 301.61 | -2.709% | 0.0% | 0.0% |
| main-foco | PASS | 3.1395 | 3.1955 | 3.2107 | 318.53 | -5.759% | 0.0% | 0.0% |
| main-otro-tema | PASS | 3.0311 | 3.1137 | 3.1747 | 329.92 | -10.072% | 0.0% | 0.0% |
| main-tema-classic | PASS | 3.1129 | 3.2413 | 3.3251 | 321.24 | -0.674% | 0.0% | 0.0% |
| main-tema-holo | PASS | 3.1923 | 3.4416 | 4.0556 | 313.25 | 1.559% | 0.0% | 0.0% |
| main-tema-material-3 | PASS | 3.165 | 3.2102 | 3.2137 | 315.95 | -2.031% | 0.0% | 0.0% |
| main-tema-outdoor-negative | PASS | 3.1692 | 3.2862 | 3.3411 | 315.54 | 2.101% | 0.0% | 0.0% |
| main-tema-rs | PASS | 3.1104 | 3.1361 | 3.1521 | 321.5 | -4.347% | 0.0% | 0.0% |
| main-tema-vision | PASS | 3.2534 | 3.342 | 3.3567 | 307.38 | 1.915% | 0.0% | 0.0% |
| main-tx-simulado | PASS | 3.1984 | 3.2717 | 3.3103 | 312.66 | -0.26% | 0.0% | 0.0% |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
