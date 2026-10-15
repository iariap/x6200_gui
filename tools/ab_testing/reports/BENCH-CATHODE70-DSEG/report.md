# Render benchmark — BENCH-CATHODE70-DSEG

- Status: **PASS** (exit 0)
- Date: 2026-10-15
- revision: `170ac892b4653c62e1c5ffac3c894997d2ab0e1e`

> x86 host proxy (CLOCK_MONOTONIC): a measurement, not a battery figure.

| Scenario | Frames | p50 (ms) | p95 (ms) | p99 (ms) | FPS | Dispersion | flushes | redrawn area | RSS | PSS |
|---|---|---|---|---|---|---|---|---|---|---|
| main-foco | 20 | 3.2435 | 3.7311 | 4.0997 | 308.31 | 1.456% | 1.0 | 384000.0 | 10473472 | 10461184 |
| main-frecuencia-dseg | 20 | 3.2471 | 3.2885 | 3.377 | 307.96 | 0.712% | 1.0 | 384000.0 | 10764288 | 10752000 |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
