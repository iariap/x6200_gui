# Render benchmark — BENCH-CATHODE70

- Status: **PASS** (exit 0)
- Date: 2026-10-15
- revision: `e510791b81d1a181621a0e71e094038435312a8c`

> x86 host proxy (CLOCK_MONOTONIC): a measurement, not a battery figure.

| Scenario | Frames | p50 (ms) | p95 (ms) | p99 (ms) | FPS | Dispersion | flushes | redrawn area | RSS | PSS |
|---|---|---|---|---|---|---|---|---|---|---|
| main-tema-cathode | 20 | 3.3509 | 3.529 | 3.5438 | 298.43 | 1.655% | 1.0 | 384000.0 | 10674176 | 10727424 |
| main-tema-classic | 20 | 3.3707 | 3.6529 | 3.9335 | 296.67 | 2.595% | 1.0 | 384000.0 | 10477568 | 10465280 |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
