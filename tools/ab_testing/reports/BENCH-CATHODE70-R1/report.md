# Render benchmark — BENCH-CATHODE70-R1

- Status: **PASS** (exit 0)
- Date: 2026-10-15
- revision: `e510791b81d1a181621a0e71e094038435312a8c`

> x86 host proxy (CLOCK_MONOTONIC): a measurement, not a battery figure.

| Scenario | Frames | p50 (ms) | p95 (ms) | p99 (ms) | FPS | Dispersion | flushes | redrawn area | RSS | PSS |
|---|---|---|---|---|---|---|---|---|---|---|
| main-tema-cathode | 20 | 3.3475 | 3.4775 | 3.7196 | 298.73 | 1.692% | 1.0 | 384000.0 | 10641408 | 10629120 |
| main-tema-classic | 20 | 3.4093 | 3.7621 | 3.8532 | 293.32 | 2.924% | 1.0 | 384000.0 | 10518528 | 10506240 |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
