# Render benchmark — BENCH-UI05-T2

- Status: **PASS** (exit 0)
- Date: 2026-10-14
- a: `41532e786bf0649d86860cf486d9da5c9b0e7947`
- b: `84c9115ccb20e0096480fc2119d4bed96250c8c6`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| bench-render | PASS | 3.4535 | 3.5995 | 3.6174 | 289.56 | -6.755% | 0.0% | 0.0% |
| dialog-about | PASS | 6.3057 | 6.976 | 6.9869 | 158.59 | -10.312% | 0.0% | 0.0% |
| home-bateria-externa | PASS | 3.2927 | 3.4501 | 3.4652 | 303.7 | -1.724% | 0.0% | 0.0% |
| home-frecuencia-extrema | PASS | 3.2352 | 3.6148 | 4.6728 | 309.1 | 7.789% | 0.0% | 0.0% |
| home-usb-20m | PASS | 3.3137 | 3.4628 | 3.4934 | 301.77 | -1.079% | 0.0% | 0.0% |
| main-bateria-baja | PASS | 3.2427 | 3.4093 | 3.4143 | 308.39 | -20.938% | 0.0% | 0.0% |
| main-espectro-agua | PASS | 3.3428 | 4.1555 | 4.3881 | 299.15 | -7.196% | 0.0% | 0.0% |
| main-foco | PASS | 3.1502 | 3.3488 | 3.3502 | 317.45 | -15.024% | 0.0% | 0.0% |
| main-otro-tema | PASS | 3.1528 | 3.3814 | 3.4173 | 317.17 | 3.102% | 0.0% | 0.0% |
| main-tema-classic | PASS | 3.1572 | 3.4803 | 3.4809 | 316.74 | -9.909% | 0.0% | 0.0% |
| main-tema-holo | PASS | 3.2031 | 3.7453 | 4.7251 | 312.2 | 8.641% | 0.0% | 0.0% |
| main-tema-material-3 | PASS | 3.1904 | 3.8067 | 4.4489 | 313.45 | 10.169% | 0.0% | 0.0% |
| main-tema-outdoor-negative | PASS | 3.1762 | 3.3354 | 3.394 | 314.84 | -3.43% | 0.0% | 0.0% |
| main-tema-rs | PASS | 3.1659 | 3.2954 | 3.3677 | 315.87 | -5.814% | 0.0% | 0.0% |
| main-tema-vision | PASS | 3.2649 | 3.4673 | 3.7914 | 306.29 | -5.038% | 0.0% | 0.0% |
| main-tx-simulado | PASS | 3.1599 | 3.3748 | 3.3967 | 316.46 | -2.261% | 0.0% | 0.0% |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
