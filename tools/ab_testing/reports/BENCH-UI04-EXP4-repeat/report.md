# Render benchmark — BENCH-UI04-EXP4-repeat

- Status: **PASS** (exit 0)
- Date: 2026-10-14
- a: `cc55931c9dd4bbb4c1c26e0b82837eb011416702`
- b: `7d4e89ad09c82e989ef0be5f5d38edf3b50453c5`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| bench-render | PASS | 3.4016 | 3.6205 | 3.6224 | 293.98 | 4.234% | 0.0% | 0.0% |
| dialog-about | PASS | 6.1742 | 7.7634 | 7.9444 | 161.96 | 3.291% | 0.0% | 0.0% |
| home-bateria-externa | PASS | 3.2418 | 3.5995 | 3.8663 | 308.47 | 9.49% | 0.0% | 0.0% |
| home-frecuencia-extrema | PASS | 3.1905 | 3.3803 | 3.4069 | 313.43 | 0.599% | 0.0% | 0.0% |
| home-usb-20m | PASS | 3.2112 | 3.8945 | 4.2993 | 311.41 | 17.942% | 0.0% | 0.0% |
| main-bateria-baja | PASS | 3.2119 | 3.4464 | 4.4698 | 311.34 | -26.885% | 0.0% | 0.0% |
| main-espectro-agua | PASS | 3.4365 | 4.8782 | 5.0148 | 290.99 | 0.244% | 0.0% | 0.0% |
| main-foco | PASS | 3.252 | 3.4574 | 3.4842 | 307.5 | -27.249% | 0.0% | 0.0% |
| main-otro-tema | PASS | 3.1157 | 3.5486 | 4.4076 | 320.96 | -24.332% | 0.0% | 0.0% |
| main-tema-classic | PASS | 3.2434 | 3.6521 | 4.4153 | 308.32 | 4.516% | 0.0% | 0.0% |
| main-tema-holo | PASS | 3.284 | 4.4524 | 4.8624 | 304.5 | 14.057% | 0.0% | 0.0% |
| main-tema-material-3 | PASS | 3.2756 | 3.7204 | 3.8646 | 305.29 | 4.033% | 0.0% | 0.0% |
| main-tema-outdoor-negative | PASS | 3.2195 | 3.3383 | 4.3523 | 310.61 | -29.493% | 0.0% | 0.0% |
| main-tema-rs | PASS | 3.1996 | 3.6592 | 4.6216 | 312.54 | 12.714% | 0.0% | 0.0% |
| main-tema-vision | PASS | 3.2691 | 3.9856 | 4.7185 | 305.89 | 19.379% | 0.0% | 0.0% |
| main-tx-simulado | PASS | 3.2298 | 3.3488 | 3.351 | 309.61 | -4.803% | 0.0% | 0.0% |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
