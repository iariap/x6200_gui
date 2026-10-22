# Render benchmark — BENCH-AURORA72-CLASSIC

- Status: **PASS** (exit 0)
- Date: 2026-10-15
- revision: `5ff37274d8d413d2c92a1527ce204f36550fd651`

> x86 host proxy (CLOCK_MONOTONIC): a measurement, not a battery figure.

| Scenario | Frames | p50 (ms) | p95 (ms) | p99 (ms) | FPS | Dispersion | flushes | redrawn area | RSS | PSS |
|---|---|---|---|---|---|---|---|---|---|---|
| main-tema-aurora | 20 | 3.4506 | 3.6261 | 3.6581 | 289.8 | 3.663% | 1.0 | 384000.0 | 10264576 | 8060928 |
| main-tema-classic | 20 | 3.6846 | 4.4337 | 4.504 | 271.4 | 8.19% | 1.0 | 384000.0 | 10428416 | 8351744 |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
