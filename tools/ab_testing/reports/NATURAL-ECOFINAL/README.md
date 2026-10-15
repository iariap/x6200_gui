# Evidencia del banco natural — corrida eco-final

Salida cruda de `tests/check_harness_natural.sh` (fixture `main-natural`) más las
corridas que la spec de la corrida mide, generadas en host x86 con el harness de
`02c1311`.

| archivo | modo | qué fija |
|---|---|---|
| `natural.json` / `run1.json` | `--bench-natural` | el perfil del corpus UI-07 (11223840 px, 67 rectángulos, 39/40 frames) y el instrumento `waterfall_offscreen_redraws = 40` (uno por fila) |
| `forced.json` | `--bench` | compatibilidad del modo forzado (3 frames x 384000 px) |
| `static.json` | `--bench-natural --natural-static` | la delimitación (sin drive no hay regiones de datos; quedan reloj/band info/BT) |
| `cli.json` | `--bench-natural 10` | el override de frames por CLI |
| `off.json` | `--bench-natural --display-off` | el panel oscuro: 0 px y `waterfall_offscreen_redraws = 0` (el corte del cómputo de la paleta) |
| `main-natural-eco.json` | `--bench-natural` sobre `main-natural-eco` | la cadencia ECO: cascada 10/40 redibujos y espectro 1 de cada 4 (`VP_SPECTRUM_ECO_PERIOD`), totales 2861240 px |

Los PNG de la captura común (`forced`, `static`, `off`) son byte a byte los del
corpus previo (ver `sha256.txt`): poner el periodo de la ROM en el refr_timer no
mueve un píxel de la captura; lo que cambia es el trabajo de render por frame.

El checker es `tests/natural_evidence.py`; `tools/ab_testing/tests/test_natural.py`
lo corre sobre este directorio en `pytest -m "not e2e"`.

## Re-baseline 2026-10-15 — el frame natural es un frame de la ROM (decisión 8)

Un frame natural es *un periodo de refresco de LVGL*, y el arnés tiene su propio
`lv_conf.h`, que no define `LV_DISP_DEF_REFR_PERIOD`: LVGL le quedaba en su
default (**30 ms**) y el banco natural medía ventanas de la ROM que la ROM no
tiene. La ROM configura **50 ms** (`lv_conf.h`, `LV_DISP_DEF_REFR_PERIOD`), y
ahora el CMake del arnés lee ese número y lo pasa como `HARNESS_ROM_REFR_PERIOD`;
`natural_bench_run()` pone ese periodo en el `refr_timer` del display antes de
medir (la misma pieza que ya usaba el perfil de wakeups, `wakeup_bench.c`, #56,
que ahora toma el valor de esa única fuente). El checker re-lee `lv_conf.h` y
exige que el JSON lo declare en `frame_ms`: los dos no pueden driftear.

Los números de esta tabla son los medidos **después** del cambio; entre
paréntesis, los de antes (harness `baf6164`, 30 ms):

| | antes (30 ms) | después (50 ms) |
|---|---|---|
| `frame_ms` | 30 | 50 |
| ventana de la corrida (40 frames) | 1200 ms | 2000 ms |
| `spectrum_frames` / `waterfall_rows` | 17 / 24 | 28 / 40 |
| `natural.totals.invalidated_px` | 6745720 | 11223840 |
| `natural.totals.invalidated_rects` | 45 | 67 |
| frames con flush | 37/40 | 39/40 |
| `waterfall_offscreen_redraws` | 24 | 40 |
| `metrics.flushed_pixels` (captura + natural) | 1127680 + 6745720 | 1127680 + 11223840 |
| `main-natural-eco` invalidated px | 1758360 | 2861240 |

Ninguna región perfilada se movió (las cajas son geometría, no cadencia): el
espectro sigue invalidando 132000 px por frame de FFT, la cascada 196000 px por
fila y el reloj 15840 px por tick de 500 ms. Lo que cambió es **cuántos** de esos
frames entran en la ventana y, con ello, el histograma de rectángulos: con el
periodo de la ROM la cascada dispara exactamente una vez por frame y el espectro
una de cada dos, así que casi todos los frames llevan una región de datos y los
dos rectángulos de los timers de 2 s (band info, BT) llegan dentro de ellas y
LVGL los une (`joined_px = 1372000`). Siguen apareciendo —y son los que el
control de delimitación (`--natural-static`) reporta—: 7 band info y 1 BT.
