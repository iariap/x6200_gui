# Render benchmark — BENCH-SPRINT69

- Status: **PASS** (exit 0)
- Date: 2026-10-15
- revision: `46195bffe31baf790fce23f79698941b6504cbe6`

> x86 host proxy (CLOCK_MONOTONIC): a measurement, not a battery figure.

| Scenario | Frames | p50 (ms) | p95 (ms) | p99 (ms) | FPS | Dispersion | flushes | redrawn area | RSS | PSS |
|---|---|---|---|---|---|---|---|---|---|---|
| main-tema-classic | 20 | 3.3104 | 3.4198 | 3.6928 | 302.08 | 1.653% | 1.0 | 384000.0 | 10301440 | 10354688 |
| main-tema-sprint | 20 | 3.2622 | 3.399 | 4.3464 | 306.54 | 1.407% | 1.0 | 384000.0 | 10686464 | 10674176 |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
