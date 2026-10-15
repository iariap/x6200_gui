# Render benchmark — BENCH-SPRINT69

- Status: **PASS** (exit 0)
- Date: 2026-10-15
- revision: `16fae76b327cb9426c0ff575f03e8526e176f5f2`

> x86 host proxy (CLOCK_MONOTONIC): a measurement, not a battery figure.

| Scenario | Frames | p50 (ms) | p95 (ms) | p99 (ms) | FPS | Dispersion | flushes | redrawn area | RSS | PSS |
|---|---|---|---|---|---|---|---|---|---|---|
| main-tema-classic | 20 | 3.2769 | 3.3563 | 3.5853 | 305.17 | 1.921% | 1.0 | 384000.0 | 10694656 | 10682368 |
| main-tema-sprint | 20 | 3.1757 | 3.3289 | 3.5865 | 314.89 | 1.604% | 1.0 | 384000.0 | 10559488 | 10612736 |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
