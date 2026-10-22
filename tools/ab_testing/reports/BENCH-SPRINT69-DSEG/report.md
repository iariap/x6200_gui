# Render benchmark — BENCH-SPRINT69-DSEG

- Status: **PASS** (exit 0)
- Date: 2026-10-15
- revision: `3d8a3ae9f9a733019c4e398d1bb47ea8756afd10`

> x86 host proxy (CLOCK_MONOTONIC): a measurement, not a battery figure.

| Scenario | Frames | p50 (ms) | p95 (ms) | p99 (ms) | FPS | Dispersion | flushes | redrawn area | RSS | PSS |
|---|---|---|---|---|---|---|---|---|---|---|
| main-foco | 20 | 3.3312 | 3.5236 | 3.6067 | 300.2 | 1.584% | 1.0 | 384000.0 | 10424320 | 10412032 |
| main-frecuencia-dseg | 20 | 3.3131 | 3.5336 | 4.6167 | 301.83 | 2.016% | 1.0 | 384000.0 | 10481664 | 10534912 |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
