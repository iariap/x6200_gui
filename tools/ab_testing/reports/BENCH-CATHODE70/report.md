# Render benchmark — BENCH-CATHODE70

- Status: **PASS** (exit 0)
- Date: 2026-10-15
- revision: `5ade4f999813400efdcb6a1850cb56c68f906a91`

> x86 host proxy (CLOCK_MONOTONIC): a measurement, not a battery figure.

| Scenario | Frames | p50 (ms) | p95 (ms) | p99 (ms) | FPS | Dispersion | flushes | redrawn area | RSS | PSS |
|---|---|---|---|---|---|---|---|---|---|---|
| main-tema-cathode | 20 | 3.3601 | 3.6086 | 4.0232 | 297.61 | 2.178% | 1.0 | 384000.0 | 10543104 | 10596352 |
| main-tema-classic | 20 | 3.3564 | 3.4494 | 3.5982 | 297.94 | 1.054% | 1.0 | 384000.0 | 10584064 | 10571776 |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
