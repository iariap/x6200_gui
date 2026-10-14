# Render benchmark — AB-e2e-benchmark

- Status: **FAIL** (exit 1)
- Date: 2026-10-13
- a: `c3ce8fa5a7ce82c771c1fd544e557ee4d823bc66`
- b: `fd466dd0ffb8bc714b823ef110edadf7e2a697ea`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| bench-render | FAIL | 11.1143 | 11.951 | 11.9646 | 89.97 | 167.468% | 200.0% | 200.0% |

- **bench-render** (FAIL):
  - p95 render +167.5% (budget 25.0%): 4.468 -> 11.951 ms
  - flushes +200.0% (budget 20.0%): 1.0 -> 3.0
  - redrawn area +200.0% (budget 20.0%): 384000 -> 1152000 px

- Environment: x86 host: relative regression proxy, not the Cortex-A7
