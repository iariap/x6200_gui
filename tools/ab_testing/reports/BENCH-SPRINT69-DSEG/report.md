# Render benchmark — BENCH-SPRINT69-DSEG

- Status: **PASS** (exit 0)
- Date: 2026-10-15
- revision: `6a4b6ea0fd59a56a4f47e7453e7407542431e4c0`

> x86 host proxy (CLOCK_MONOTONIC): a measurement, not a battery figure.

| Scenario | Frames | p50 (ms) | p95 (ms) | p99 (ms) | FPS | Dispersion | flushes | redrawn area | RSS | PSS |
|---|---|---|---|---|---|---|---|---|---|---|
| main-foco | 80 | 5.7431 | 6.2024 | 6.6209 | 174.12 | 17.172% | 1.0 | 384000.0 | 10457088 | 9086976 |
| main-frecuencia-dseg | 80 | 5.8761 | 6.1773 | 6.4732 | 170.18 | 3.91% | 1.0 | 384000.0 | 10432512 | 8041472 |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
