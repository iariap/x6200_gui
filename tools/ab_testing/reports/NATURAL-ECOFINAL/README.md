# Evidencia del banco natural — corrida eco-final

Salida cruda de `tests/check_harness_natural.sh` (fixture `main-natural`) más las
corridas que la spec de la corrida mide, generadas en host x86 con el harness de
`baf6164`:

| archivo | modo | qué fija |
|---|---|---|
| `natural.json` / `run1.json` | `--bench-natural` | el perfil QUALITY del corpus UI-07 (6745720 px, 45 rectángulos, 37/40 frames) y el instrumento `waterfall_offscreen_redraws = 24` (uno por fila) |
| `forced.json` | `--bench` | compatibilidad del modo forzado (3 frames x 384000 px) |
| `static.json` | `--bench-natural --natural-static` | la delimitación (sin drive no hay regiones de datos) |
| `cli.json` | `--bench-natural 10` | el override de frames por CLI |
| `off.json` | `--bench-natural --display-off` | el panel oscuro: 0 px y `waterfall_offscreen_redraws = 0` (el corte del cómputo de la paleta) |
| `main-natural-eco.json` | `--bench-natural` sobre `main-natural-eco` | la cadencia ECO: cascada 6/24 redibujos y espectro 1 de cada 4 (`VP_SPECTRUM_ECO_PERIOD`), totales 1758360 px |

Los PNG son byte a byte los del corpus previo (ver `sha256.txt`): la captura
común no mueve un píxel; lo que cambia es el trabajo de render por frame.

El checker es `tests/natural_evidence.py`; `tools/ab_testing/tests/test_natural.py`
lo corre sobre este directorio en `pytest -m "not e2e"`.
