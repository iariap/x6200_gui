# Render benchmark — BENCH-AURORA72-CLASSIC-R1

- Status: **PASS** (exit 0)
- Date: 2026-10-15
- revision: `5ff37274d8d413d2c92a1527ce204f36550fd651`

> x86 host proxy (CLOCK_MONOTONIC): a measurement, not a battery figure.

| Scenario | Frames | p50 (ms) | p95 (ms) | p99 (ms) | FPS | Dispersion | flushes | redrawn area | RSS | PSS |
|---|---|---|---|---|---|---|---|---|---|---|
| main-tema-aurora | 20 | 3.4382 | 3.9486 | 4.0627 | 290.85 | 2.426% | 1.0 | 384000.0 | 10612736 | 8507392 |
| main-tema-classic | 20 | 3.4483 | 3.6577 | 4.3381 | 290.0 | 0.621% | 1.0 | 384000.0 | 10481664 | 8249344 |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
