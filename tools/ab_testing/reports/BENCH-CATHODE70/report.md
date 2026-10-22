# Render benchmark — BENCH-CATHODE70

- Status: **PASS** (exit 0)
- Date: 2026-10-15
- revision: `170ac892b4653c62e1c5ffac3c894997d2ab0e1e`

> x86 host proxy (CLOCK_MONOTONIC): a measurement, not a battery figure.

| Scenario | Frames | p50 (ms) | p95 (ms) | p99 (ms) | FPS | Dispersion | flushes | redrawn area | RSS | PSS |
|---|---|---|---|---|---|---|---|---|---|---|
| main-tema-cathode | 20 | 3.2485 | 3.3068 | 3.3102 | 307.84 | 1.358% | 1.0 | 384000.0 | 10682368 | 10670080 |
| main-tema-classic | 20 | 3.2485 | 3.2807 | 3.2892 | 307.83 | 0.404% | 1.0 | 384000.0 | 10301440 | 10354688 |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
