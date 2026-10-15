# Render benchmark — BENCH-AURORA72-CLASSIC-R1

- Status: **INCONCLUSIVE** (exit 3)
- Date: 2026-10-15
- revision: `3ebe828aeaf23a90d3ec28a608778fb743f8456b`

> x86 host proxy (CLOCK_MONOTONIC): a measurement, not a battery figure.

| Scenario | Frames | p50 (ms) | p95 (ms) | p99 (ms) | FPS | Dispersion | flushes | redrawn area | RSS | PSS |
|---|---|---|---|---|---|---|---|---|---|---|
| main-tema-aurora | 20 | 5.8048 | 7.1298 | 7.3082 | 172.27 | 6.639% | 1.0 | 384000.0 | 10334208 | 7606272 |
| main-tema-classic | 20 | 5.4482 | 6.3562 | 7.1538 | 183.55 | 37.562% | 1.0 | 384000.0 | 10412032 | 10465280 |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
