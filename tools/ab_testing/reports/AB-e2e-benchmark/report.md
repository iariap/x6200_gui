# Render benchmark — AB-e2e-benchmark

- Status: **FAIL** (exit 1)
- Date: 2026-10-13
- a: `92d940529a9252d50b8eb4410611b957213ff6ac`
- b: `93187e179bce715106b7ae6ad87a38e4e98669ca`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| bench-render | FAIL | 12.8052 | 13.4877 | 13.6853 | 78.09 | 195.585% | 200.0% | 200.0% |

- **bench-render** (FAIL):
  - p95 render +195.6% (budget 25.0%): 4.563 -> 13.488 ms
  - flushes +200.0% (budget 20.0%): 1.0 -> 3.0
  - redrawn area +200.0% (budget 20.0%): 384000 -> 1152000 px

- Environment: x86 host: relative regression proxy, not the Cortex-A7
