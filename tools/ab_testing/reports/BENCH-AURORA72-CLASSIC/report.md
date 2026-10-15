# Render benchmark — BENCH-AURORA72-CLASSIC

- Status: **INCONCLUSIVE** (exit 3)
- Date: 2026-10-15
- revision: `3ebe828aeaf23a90d3ec28a608778fb743f8456b`

> x86 host proxy (CLOCK_MONOTONIC): a measurement, not a battery figure.

| Scenario | Frames | p50 (ms) | p95 (ms) | p99 (ms) | FPS | Dispersion | flushes | redrawn area | RSS | PSS |
|---|---|---|---|---|---|---|---|---|---|---|
| main-tema-aurora | 20 | 5.0623 | 6.4975 | 7.4973 | 197.54 | 39.767% | 1.0 | 384000.0 | 10616832 | 10604544 |
| main-tema-classic | 20 | 3.4641 | 6.2497 | 6.3003 | 288.68 | 5.189% | 1.0 | 384000.0 | 10420224 | 8127488 |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
