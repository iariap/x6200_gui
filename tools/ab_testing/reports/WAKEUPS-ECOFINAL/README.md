# Evidencia del perfil de wakeups — corrida eco-final

Salida cruda de `tests/check_harness_wakeups.sh` (fixture `main-natural`, ventana
1000 ms), generada en host x86 con el harness de `baf6164`:

| archivo | modo | qué fija |
|---|---|---|
| `wakeups.json` / `run1.json` | `--bench-wakeups` (dos corridas, byte a byte) | `tick_custom: true`, `tick_thread_wakeups: 0`; lazo 14 wakeups (11 sin el camino de datos) |
| `window.json` | `--bench-wakeups 3000` | la ventana es un knob de CLI (31 wakeups de lazo, 0 del tick) |
| `plain.json` | captura común | el bloque `wakeups` no aparece (el modo es aditivo) |

`LV_TICK_CUSTOM 1` (lv_conf.h de la ROM, `CLOCK_MONOTONIC`) elimina el hilo del
tick, que en la corrida base despertaba 200 veces por segundo (93.5% de los
wakeups del hilo de GUI). El checker es `tests/wakeup_evidence.py`;
`tools/ab_testing/tests/test_wakeups.py` lo corre sobre este directorio en
`pytest -m "not e2e"`.
