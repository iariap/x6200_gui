# Render benchmark — BENCH-SPRINT69-R1

- Status: **PASS** (exit 0)
- Date: 2026-10-15
- revision: `3d8a3ae9f9a733019c4e398d1bb47ea8756afd10`

> x86 host proxy (CLOCK_MONOTONIC): a measurement, not a battery figure.

| Scenario | Frames | p50 (ms) | p95 (ms) | p99 (ms) | FPS | Dispersion | flushes | redrawn area | RSS | PSS |
|---|---|---|---|---|---|---|---|---|---|---|
| main-tema-classic | 20 | 3.3508 | 3.582 | 3.6381 | 298.43 | 2.024% | 1.0 | 384000.0 | 10391552 | 10379264 |
| main-tema-sprint | 20 | 3.3122 | 3.4609 | 3.4749 | 301.91 | 2.081% | 1.0 | 384000.0 | 10469376 | 10457088 |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
