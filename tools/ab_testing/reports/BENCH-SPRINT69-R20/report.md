# Render benchmark — BENCH-SPRINT69-R20

- Status: **INCONCLUSIVE** (exit 3)
- Date: 2026-10-15
- revision: `6a4b6ea0fd59a56a4f47e7453e7407542431e4c0`

> x86 host proxy (CLOCK_MONOTONIC): a measurement, not a battery figure.

| Scenario | Frames | p50 (ms) | p95 (ms) | p99 (ms) | FPS | Dispersion | flushes | redrawn area | RSS | PSS |
|---|---|---|---|---|---|---|---|---|---|---|
| main-tema-classic | 80 | 4.5256 | 5.6614 | 5.7336 | 220.97 | 36.56% | 1.0 | 384000.0 | 10551296 | 10539008 |
| main-tema-sprint | 80 | 4.9818 | 5.7438 | 5.8322 | 200.73 | 24.317% | 1.0 | 384000.0 | 10559488 | 10612736 |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
