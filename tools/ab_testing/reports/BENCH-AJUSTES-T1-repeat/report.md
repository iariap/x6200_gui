# Render benchmark — BENCH-AJUSTES-T1-repeat

- Status: **PASS** (exit 0)
- Date: 2026-10-14
- a: `f360f9218e0814bbb4ed91ef79942b3f45864ba8`
- b: `63ecc8716859d420cd937a42e851fd912dc66658`

> x86 host proxy (CLOCK_MONOTONIC): relative regression detection only, not the Cortex-A7 frame budget nor battery life.

| Scenario | Status | p50 (ms) | p95 (ms) | p99 (ms) | FPS | p95 Δ | flushes Δ | area Δ |
|---|---|---|---|---|---|---|---|---|
| bench-render | PASS | 3.3959 | 3.5417 | 3.7034 | 294.47 | 3.206% | 0.0% | 0.0% |
| dialog-about | PASS | 6.1186 | 6.9818 | 7.784 | 163.43 | 4.362% | 0.0% | 0.0% |
| home-bateria-externa | PASS | 3.2544 | 3.3571 | 3.3892 | 307.28 | -13.175% | 0.0% | 0.0% |
| home-frecuencia-extrema | PASS | 3.1682 | 3.268 | 3.2901 | 315.64 | 0.912% | 0.0% | 0.0% |
| home-usb-20m | PASS | 3.2483 | 3.2813 | 3.3067 | 307.86 | -0.898% | 0.0% | 0.0% |
| main-bateria-baja | PASS | 3.192 | 3.2409 | 3.2471 | 313.29 | -9.647% | 0.0% | 0.0% |
| main-espectro-agua | PASS | 3.3971 | 3.535 | 3.895 | 294.37 | -7.238% | 0.0% | 0.0% |
| main-foco | PASS | 3.2538 | 3.3998 | 3.4145 | 307.34 | 2.124% | 0.0% | 0.0% |
| main-natural | PASS | 3.3085 | 3.5106 | 3.6368 | 302.25 | 6.375% | 0.0% | 0.0% |
| main-natural-eco | PASS | 3.2412 | 3.4658 | 4.2104 | 308.53 | 1.071% | 0.0% | 0.0% |
| main-otro-tema | PASS | 3.1549 | 3.3364 | 3.351 | 316.96 | 4.792% | 0.0% | 0.0% |
| main-tema-classic | PASS | 3.2138 | 3.3045 | 3.3159 | 311.16 | -4.239% | 0.0% | 0.0% |
| main-tema-holo | PASS | 3.2849 | 3.4618 | 3.5324 | 304.43 | 1.518% | 0.0% | 0.0% |
| main-tema-material-3 | PASS | 3.2454 | 3.3188 | 3.3747 | 308.13 | -4.613% | 0.0% | 0.0% |
| main-tema-outdoor-negative | PASS | 3.236 | 3.309 | 3.3339 | 309.02 | -3.165% | 0.0% | 0.0% |
| main-tema-rs | PASS | 3.237 | 3.2728 | 3.294 | 308.93 | -5.816% | 0.0% | 0.0% |
| main-tema-vision | PASS | 3.2565 | 4.1836 | 4.3809 | 307.08 | 20.584% | 0.0% | 0.0% |
| main-tema-vision-cwr | PASS | 3.2704 | 3.3104 | 3.3226 | 305.77 | -12.985% | 0.0% | 0.0% |
| main-tema-vision-usbd | PASS | 3.2591 | 3.3715 | 3.8221 | 306.84 | -11.215% | 0.0% | 0.0% |
| main-tx-simulado | PASS | 3.2725 | 3.3358 | 3.3524 | 305.58 | -0.65% | 0.0% | 0.0% |

- Environment: x86 host: relative regression proxy, not the Cortex-A7
