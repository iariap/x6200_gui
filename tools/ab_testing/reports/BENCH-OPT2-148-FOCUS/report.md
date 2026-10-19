# Render benchmark — BENCH-OPT2-148-FOCUS

- Status: **INCONCLUSIVE** (exit 3)
- Date: 2026-10-19
- a: `020904541eed5228efbcb741bc6492681475e2a4`
- b: `576d2df3fbc2a499054c14985f2baf776c6e43b3`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| bench-render | PASS | 3.7754 | 6.0329 | 6.159 | 264.87 | -6.912% | 0.0% | 0.0% |
| main-espectro-agua | PASS | 3.7658 | 7.5431 | 8.2092 | 265.55 | 2.229% | 0.0% | 0.0% |
| main-natural | INCONCLUSIVE | 3.8052 | 6.7511 | 7.2042 | 262.8 | 61.51% | 0.0% | 0.0% |

- **main-natural** (INCONCLUSIVE):
  - p95 render +61.5% (budget 25.0%): 4.180 -> 6.751 ms — host noise: the deterministic proxies are unchanged (flushes 0.0%, redrawn area 0.0%), so this is runner preemption, not a render change

- Environment: x86 host: relative regression proxy, not the Cortex-A7
