# Render benchmark — BENCH-CATHODE70-DSEG

- Status: **PASS** (exit 0)
- Date: 2026-10-15
- revision: `e510791b81d1a181621a0e71e094038435312a8c`

> x86 host proxy (CLOCK_MONOTONIC): a measurement, not a battery figure.

| Scenario | Frames | p50 (ms) | p95 (ms) | p99 (ms) | FPS | Dispersion | flushes | redrawn area | RSS | PSS |
|---|---|---|---|---|---|---|---|---|---|---|
| main-foco | 20 | 3.3986 | 3.7133 | 3.8309 | 294.24 | 3.765% | 1.0 | 384000.0 | 10387456 | 10440704 |
| main-frecuencia-dseg | 20 | 3.4059 | 3.8408 | 4.5434 | 293.61 | 2.49% | 1.0 | 384000.0 | 10477568 | 10465280 |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
