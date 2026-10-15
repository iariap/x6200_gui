# Render benchmark — BENCH-AURORA72-CLASSIC-R2

- Status: **PASS** (exit 0)
- Date: 2026-10-15
- revision: `5ff37274d8d413d2c92a1527ce204f36550fd651`

> x86 host proxy (CLOCK_MONOTONIC): a measurement, not a battery figure.

| Scenario | Frames | p50 (ms) | p95 (ms) | p99 (ms) | FPS | Dispersion | flushes | redrawn area | RSS | PSS |
|---|---|---|---|---|---|---|---|---|---|---|
| main-tema-aurora | 20 | 3.4379 | 3.6641 | 3.7393 | 290.88 | 1.059% | 1.0 | 384000.0 | 10436608 | 8220672 |
| main-tema-classic | 20 | 3.5238 | 3.9035 | 3.9849 | 283.78 | 1.982% | 1.0 | 384000.0 | 10432512 | 8249344 |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
