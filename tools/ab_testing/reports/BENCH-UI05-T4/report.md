# Render benchmark — BENCH-UI05-T4

- Status: **PASS** (exit 0)
- Date: 2026-10-14
- a: `fad0b42aa8032502ce2edc509efeb8aab10b11c9`
- b: `c52ca22aa4c39c2ebd0e3c26f9351665db4b689d`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| bench-render | PASS | 3.413 | 3.4873 | 3.498 | 293.0 | -1.286% | 0.0% | 0.0% |
| dialog-about | PASS | 6.1804 | 6.3106 | 6.7769 | 161.8 | 1.821% | 0.0% | 0.0% |
| home-bateria-externa | PASS | 3.2565 | 3.7199 | 3.9714 | 307.07 | 12.147% | 0.0% | 0.0% |
| home-frecuencia-extrema | PASS | 3.1821 | 3.3549 | 3.5742 | 314.26 | -3.033% | 0.0% | 0.0% |
| home-usb-20m | PASS | 3.2466 | 3.3164 | 3.3603 | 308.01 | -3.438% | 0.0% | 0.0% |
| main-bateria-baja | PASS | 3.2399 | 3.2907 | 3.3288 | 308.65 | -2.82% | 0.0% | 0.0% |
| main-espectro-agua | PASS | 3.4078 | 4.1322 | 4.1497 | 293.44 | 17.649% | 0.0% | 0.0% |
| main-foco | PASS | 3.2786 | 3.4156 | 3.4373 | 305.01 | -0.846% | 0.0% | 0.0% |
| main-otro-tema | PASS | 3.1841 | 3.2947 | 3.3883 | 314.06 | 1.035% | 0.0% | 0.0% |
| main-tema-classic | PASS | 3.2377 | 3.3443 | 3.4406 | 308.87 | 1.059% | 0.0% | 0.0% |
| main-tema-holo | PASS | 3.2643 | 3.3694 | 3.4025 | 306.35 | -3.218% | 0.0% | 0.0% |
| main-tema-material-3 | PASS | 3.2946 | 3.3493 | 3.4579 | 303.53 | -1.78% | 0.0% | 0.0% |
| main-tema-outdoor-negative | PASS | 3.2402 | 3.3307 | 3.4341 | 308.62 | 0.238% | 0.0% | 0.0% |
| main-tema-rs | PASS | 3.2503 | 3.3698 | 3.4598 | 307.66 | -1.154% | 0.0% | 0.0% |
| main-tema-vision | PASS | 3.2812 | 3.5942 | 4.7224 | 304.77 | 5.688% | 0.0% | 0.0% |
| main-tx-simulado | PASS | 3.2672 | 3.3442 | 3.4121 | 306.07 | 0.971% | 0.0% | 0.0% |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
