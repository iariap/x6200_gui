# Render benchmark — BENCH-AURORA72-CLASSIC-R2

- Status: **PASS** (exit 0)
- Date: 2026-10-15
- revision: `3ebe828aeaf23a90d3ec28a608778fb743f8456b`

> x86 host proxy (CLOCK_MONOTONIC): a measurement, not a battery figure.

| Scenario | Frames | p50 (ms) | p95 (ms) | p99 (ms) | FPS | Dispersion | flushes | redrawn area | RSS | PSS |
|---|---|---|---|---|---|---|---|---|---|---|
| main-tema-aurora | 20 | 3.4261 | 5.9701 | 5.995 | 291.88 | 3.31% | 1.0 | 384000.0 | 10641408 | 10629120 |
| main-tema-classic | 20 | 3.4212 | 3.5204 | 3.5452 | 292.29 | 1.125% | 1.0 | 384000.0 | 10473472 | 10461184 |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
