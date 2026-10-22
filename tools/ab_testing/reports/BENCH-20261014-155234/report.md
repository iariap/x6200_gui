# Render benchmark — BENCH-20261014-155234

- Status: **PASS** (exit 0)
- Date: 2026-10-14
- a: `e5f1010b5cae84c68c3d4c8cf1d24dda588b517a`
- b: `23ebe8e9e2f71400931ce9c29b191dcddc14486d`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| bench-render | PASS | 3.5006 | 3.7404 | 4.3038 | 285.67 | 4.091% | 0.0% | 0.0% |
| dialog-about | PASS | 6.3027 | 7.8123 | 7.9119 | 158.66 | 12.94% | 0.0% | 0.0% |
| home-bateria-externa | PASS | 3.2843 | 3.4177 | 4.4919 | 304.47 | -13.293% | 0.0% | 0.0% |
| home-frecuencia-extrema | PASS | 3.242 | 4.6752 | 5.0788 | 308.45 | -0.877% | 0.0% | 0.0% |
| home-usb-20m | PASS | 3.2751 | 3.5583 | 4.5713 | 305.34 | -23.682% | 0.0% | 0.0% |
| main-bateria-baja | PASS | 3.2418 | 4.6826 | 4.7197 | 308.47 | -0.246% | 0.0% | 0.0% |
| main-espectro-agua | PASS | 3.4642 | 5.0508 | 5.4583 | 288.67 | -0.75% | 0.0% | 0.0% |
| main-foco | PASS | 3.26 | 3.294 | 3.2962 | 306.75 | -30.194% | 0.0% | 0.0% |
| main-natural | PASS | 3.3003 | 3.9916 | 4.8958 | 303.0 | -17.406% | 0.0% | 0.0% |
| main-natural-eco | PASS | 3.2744 | 3.9036 | 4.5276 | 305.4 | 13.493% | 0.0% | 0.0% |
| main-otro-tema | PASS | 3.1465 | 3.5643 | 3.8018 | 317.81 | -1.209% | 0.0% | 0.0% |
| main-tema-classic | PASS | 3.2561 | 3.3868 | 4.5576 | 307.12 | -10.571% | 0.0% | 0.0% |
| main-tema-holo | PASS | 3.3046 | 4.14 | 4.666 | 302.61 | 16.053% | 0.0% | 0.0% |
| main-tema-material-3 | PASS | 3.2999 | 3.5405 | 4.5585 | 303.04 | 4.19% | 0.0% | 0.0% |
| main-tema-outdoor-negative | PASS | 3.255 | 3.568 | 4.5722 | 307.22 | -2.058% | 0.0% | 0.0% |
| main-tema-rs | PASS | 3.2728 | 4.0903 | 4.2225 | 305.55 | 21.395% | 0.0% | 0.0% |
| main-tema-vision | PASS | 3.2832 | 3.3304 | 3.3334 | 304.58 | -0.008% | 0.0% | 0.0% |
| main-tema-vision-cwr | PASS | 3.3075 | 3.3761 | 3.4118 | 302.35 | 0.963% | 0.0% | 0.0% |
| main-tema-vision-usbd | PASS | 3.3057 | 3.4453 | 4.4996 | 302.51 | 1.748% | 0.0% | 0.0% |
| main-tx-simulado | PASS | 3.2862 | 3.4566 | 4.5324 | 304.3 | -3.764% | 0.0% | 0.0% |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
