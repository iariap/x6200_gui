# Render benchmark — AB-e2e-benchmark

- Status: **FAIL** (exit 1)
- Date: 2026-10-13
- a: `ec1b0a58ec30270ece2a6b490ae41ed341133755`
- b: `d874c9842820ad2649b8d7fc73b1a89ceade18b0`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| bench-render | FAIL | 10.7022 | 11.318 | 12.097 | 93.44 | 199.916% | 200.0% | 200.0% |

- **bench-render** (FAIL):
  - p95 render +199.9% (budget 25.0%): 3.774 -> 11.318 ms
  - flushes +200.0% (budget 20.0%): 1.0 -> 3.0
  - redrawn area +200.0% (budget 20.0%): 384000 -> 1152000 px

- Environment: x86 host: relative regression proxy, not the Cortex-A7
