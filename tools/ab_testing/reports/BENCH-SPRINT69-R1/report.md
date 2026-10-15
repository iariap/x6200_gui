# Render benchmark — BENCH-SPRINT69-R1

- Status: **PASS** (exit 0)
- Date: 2026-10-15
- revision: `16fae76b327cb9426c0ff575f03e8526e176f5f2`

> x86 host proxy (CLOCK_MONOTONIC): a measurement, not a battery figure.

| Scenario | Frames | p50 (ms) | p95 (ms) | p99 (ms) | FPS | Dispersion | flushes | redrawn area | RSS | PSS |
|---|---|---|---|---|---|---|---|---|---|---|
| main-tema-classic | 20 | 3.3492 | 4.0455 | 4.3001 | 298.57 | 2.574% | 1.0 | 384000.0 | 10539008 | 10526720 |
| main-tema-sprint | 20 | 3.2907 | 3.822 | 4.9706 | 303.89 | 1.831% | 1.0 | 384000.0 | 10330112 | 10383360 |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
