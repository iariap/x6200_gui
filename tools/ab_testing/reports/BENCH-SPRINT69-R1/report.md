# Render benchmark — BENCH-SPRINT69-R1

- Status: **PASS** (exit 0)
- Date: 2026-10-15
- revision: `6a4b6ea0fd59a56a4f47e7453e7407542431e4c0`

> x86 host proxy (CLOCK_MONOTONIC): a measurement, not a battery figure.

| Scenario | Frames | p50 (ms) | p95 (ms) | p99 (ms) | FPS | Dispersion | flushes | redrawn area | RSS | PSS |
|---|---|---|---|---|---|---|---|---|---|---|
| main-tema-classic | 20 | 5.9951 | 6.2861 | 6.5047 | 166.8 | 2.55% | 1.0 | 384000.0 | 10407936 | 7994368 |
| main-tema-sprint | 20 | 5.9571 | 6.1505 | 6.213 | 167.87 | 2.999% | 1.0 | 384000.0 | 10616832 | 7572480 |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
