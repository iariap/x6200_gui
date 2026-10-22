# Render benchmark — BENCH-CATHODE70-R1

- Status: **PASS** (exit 0)
- Date: 2026-10-15
- revision: `170ac892b4653c62e1c5ffac3c894997d2ab0e1e`

> x86 host proxy (CLOCK_MONOTONIC): a measurement, not a battery figure.

| Scenario | Frames | p50 (ms) | p95 (ms) | p99 (ms) | FPS | Dispersion | flushes | redrawn area | RSS | PSS |
|---|---|---|---|---|---|---|---|---|---|---|
| main-tema-cathode | 20 | 3.2836 | 3.3623 | 3.4113 | 304.54 | 0.594% | 1.0 | 384000.0 | 10452992 | 10506240 |
| main-tema-classic | 20 | 3.2737 | 3.3605 | 3.4399 | 305.46 | 1.399% | 1.0 | 384000.0 | 10547200 | 10534912 |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
