# Render benchmark — BENCH-CATHODE70-DSEG

- Status: **PASS** (exit 0)
- Date: 2026-10-15
- revision: `5ade4f999813400efdcb6a1850cb56c68f906a91`

> x86 host proxy (CLOCK_MONOTONIC): a measurement, not a battery figure.

| Scenario | Frames | p50 (ms) | p95 (ms) | p99 (ms) | FPS | Dispersion | flushes | redrawn area | RSS | PSS |
|---|---|---|---|---|---|---|---|---|---|---|
| main-foco | 20 | 3.3347 | 3.6584 | 4.2611 | 299.88 | 1.958% | 1.0 | 384000.0 | 10395648 | 10448896 |
| main-frecuencia-dseg | 20 | 3.3355 | 3.6872 | 3.7008 | 299.8 | 1.758% | 1.0 | 384000.0 | 10625024 | 10612736 |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
