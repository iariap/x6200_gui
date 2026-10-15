# Render benchmark — BENCH-CATHODE70-R1

- Status: **PASS** (exit 0)
- Date: 2026-10-15
- revision: `5ade4f999813400efdcb6a1850cb56c68f906a91`

> x86 host proxy (CLOCK_MONOTONIC): a measurement, not a battery figure.

| Scenario | Frames | p50 (ms) | p95 (ms) | p99 (ms) | FPS | Dispersion | flushes | redrawn area | RSS | PSS |
|---|---|---|---|---|---|---|---|---|---|---|
| main-tema-cathode | 20 | 3.3408 | 3.676 | 3.8576 | 299.33 | 2.055% | 1.0 | 384000.0 | 10637312 | 10690560 |
| main-tema-classic | 20 | 3.4551 | 3.6635 | 3.6977 | 289.43 | 2.667% | 1.0 | 384000.0 | 10473472 | 10461184 |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
