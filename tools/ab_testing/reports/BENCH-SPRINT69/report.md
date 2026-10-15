# Render benchmark — BENCH-SPRINT69

- Status: **PASS** (exit 0)
- Date: 2026-10-15
- revision: `3d8a3ae9f9a733019c4e398d1bb47ea8756afd10`

> x86 host proxy (CLOCK_MONOTONIC): a measurement, not a battery figure.

| Scenario | Frames | p50 (ms) | p95 (ms) | p99 (ms) | FPS | Dispersion | flushes | redrawn area | RSS | PSS |
|---|---|---|---|---|---|---|---|---|---|---|
| main-tema-classic | 20 | 3.3112 | 3.4634 | 3.5948 | 302.01 | 1.919% | 1.0 | 384000.0 | 10522624 | 10575872 |
| main-tema-sprint | 20 | 3.3255 | 3.6783 | 4.3487 | 300.7 | 1.792% | 1.0 | 384000.0 | 10489856 | 10543104 |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
