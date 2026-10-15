# Render benchmark — BENCH-SPRINT69-R2

- Status: **INCONCLUSIVE** (exit 3)
- Date: 2026-10-15
- revision: `6a4b6ea0fd59a56a4f47e7453e7407542431e4c0`

> x86 host proxy (CLOCK_MONOTONIC): a measurement, not a battery figure.

| Scenario | Frames | p50 (ms) | p95 (ms) | p99 (ms) | FPS | Dispersion | flushes | redrawn area | RSS | PSS |
|---|---|---|---|---|---|---|---|---|---|---|
| main-tema-classic | 20 | 4.8745 | 6.1447 | 6.3269 | 205.15 | 40.218% | 1.0 | 384000.0 | 10485760 | 10473472 |
| main-tema-sprint | 20 | 4.6205 | 6.1637 | 6.2367 | 216.43 | 39.479% | 1.0 | 384000.0 | 10686464 | 7701504 |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
