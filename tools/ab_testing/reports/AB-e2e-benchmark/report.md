# Render benchmark — AB-e2e-benchmark

- Status: **FAIL** (exit 1)
- Date: 2026-10-13
- a: `667fad4b2cf96e958464b88dd992ea496c063bc8`
- b: `6b66383e500ff8702045c14a44c95ce9fcf5df6b`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| bench-render | FAIL | 13.821 | 14.5219 | 14.7556 | 72.35 | 236.676% | 200.0% | 200.0% |

- **bench-render** (FAIL):
  - p95 render +236.7% (budget 25.0%): 4.313 -> 14.522 ms
  - flushes +200.0% (budget 20.0%): 1.0 -> 3.0
  - redrawn area +200.0% (budget 20.0%): 384000 -> 1152000 px

- Environment: x86 host: relative regression proxy, not the Cortex-A7
