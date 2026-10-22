# Render benchmark — AB-FONTSE-BENCH-R1

- Status: **PASS** (exit 0)
- Date: 2026-10-15
- a: `11f8d689317a76f68cd4da5d6915e13e8b09450e`
- b: `562d3f212002a5379f448a1ee8eb9a16ffdd8f06`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| main-frecuencia-dseg | PASS | 3.3197 | 3.5768 | 3.8905 | 301.23 | -1.178% | 0.0% | 0.0% |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
