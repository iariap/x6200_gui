# Render benchmark — BENCH-SPRINT69-DSEG

- Status: **PASS** (exit 0)
- Date: 2026-10-15
- revision: `16fae76b327cb9426c0ff575f03e8526e176f5f2`

> x86 host proxy (CLOCK_MONOTONIC): a measurement, not a battery figure.

| Scenario | Frames | p50 (ms) | p95 (ms) | p99 (ms) | FPS | Dispersion | flushes | redrawn area | RSS | PSS |
|---|---|---|---|---|---|---|---|---|---|---|
| main-foco | 20 | 3.1975 | 3.3196 | 3.3346 | 312.74 | 2.133% | 1.0 | 384000.0 | 10424320 | 10412032 |
| main-frecuencia-dseg | 20 | 3.2193 | 3.271 | 3.2797 | 310.63 | 1.105% | 1.0 | 384000.0 | 10526720 | 10579968 |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
