# Evidencia del banco natural — corrida eco-final

Salida cruda de `tests/check_harness_natural.sh` (fixture `main-natural`) más las
corridas que la spec de la corrida mide, generadas en host x86 con el harness de
`02c1311`.

| archivo | modo | qué fija |
|---|---|---|
| `natural.json` / `run1.json` | `--bench-natural` | el perfil del corpus UI-07 (12580800 px, 66 rectángulos, 39/40 frames) y el instrumento `waterfall_offscreen_redraws = 40` (uno por fila) |
| `forced.json` | `--bench` | compatibilidad del modo forzado (3 frames x 384000 px) |
| `static.json` | `--bench-natural --natural-static` | la delimitación (sin drive no hay regiones de datos; quedan sólo los band-info: las guardas de OPT-4 #150 apagaron los ticks idénticos de reloj/BT) |
| `cli.json` | `--bench-natural 10` | el override de frames por CLI |
| `off.json` | `--bench-natural --display-off` | el panel oscuro: 0 px y `waterfall_offscreen_redraws = 0` (el corte del cómputo de la paleta) |
| `main-natural-eco.json` | `--bench-natural` sobre `main-natural-eco` | la cadencia ECO: cascada 10/40 redibujos y espectro 1 de cada 4 (`VP_SPECTRUM_ECO_PERIOD`), totales 3113600 px |

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

## Re-baseline 2026-10-19 — rasterizado incremental del waterfall (OPT-2 #148)

El anillo de filas de `src/waterfall.c` (576d2df3) agrega dos instrumentos al
bloque `natural` que `tests/natural_evidence.py` exige y esta evidencia se
re-generó con el binario de la rama (HEAD `d2beb6c6`):

| campo | `main-natural` (QUALITY) | `main-natural-eco` (ECO) | `--display-off` |
|---|---|---|---|
| `waterfall_rasterized_px` | 32000 (40 filas x 800 px) | 32000 | 0 |
| `waterfall_full_rebuilds` | 0 | 0 | 0 |

Una fila nueva en reposo rasteriza **800 px** en vez de las 245 filas (196000
px) del baseline; no hay ninguna reconstrucción completa porque el fixture no
cambia centro, zoom ni paleta. El **área invalidada/flusheada no cambia** (el
scroll mueve todos los píxeles visibles): los totales del bloque `natural`
siguen siendo 11223840 px / 67 rectángulos en QUALITY. Con el panel oscuro el
anillo saltea el render entero (`waterfall_rasterized_px = 0`).

Los PNG de la captura común también cambiaron respecto de la corrida anterior:
entre `02c1311` y esta base entraron cambios de UI ajenos a OPT-2 (el A/B
OPT-2 `02090454` vs `576d2df3` es pixel-identical sobre 89 escenarios). El
bloque `natural` no cambió; sólo la imagen (`framebuffer_fnv1a`/`png_bytes`) y
los dos campos nuevos. Ver `sha256.txt`.

## Re-baseline 2026-10-19 — guardas de igualdad de OPT-4 #150

Las guardas de igualdad de los updaters (`923e4dea`: `src/clock.c`, `src/info.c`,
`src/meter.c`, `src/tx_info.c`, `src/main_screen.c`, `src/ui_set.h`) evitan
`lv_label_set_text` / `lv_obj_set_style_*` cuando el valor **vivo** del objeto no
cambia (LVGL 8.3.11 no compara: cada escritura idéntica invalidaba igual). En el
arnés `time()` está fijo (`mocks/mock_ui_deps.c`), así que el reloj y el icono BT
no cambian de texto/color en toda la corrida y dejan de invalidar.

Esta evidencia se re-generó con el binario de la rama (`tests/check_harness_natural.sh`
más `--bench-natural` sobre `main-natural-eco`). El **histograma de rectángulos
del bloque `natural` pierde el rectángulo del reloj** `(580,0,220,72)`
(15840 px) y el control de delimitación queda sólo con los band-info: es el
efecto buscado por las guardas, no una regresión. Ningún PNG se mueve (ver
`sha256.txt`: las guardas sólo suprimen invalidaciones que no cambiaban un
píxel), y las regiones perfiladas tampoco (son geometría): el espectro sigue con
132000 px x 27 frames y la cascada con 196000 px x 39 filas.

| campo | antes (OPT-2 `d2beb6c6`) | después (OPT-4 `cbaaf72f`) |
|---|---|---|
| `natural.totals.invalidated_px` | 11223840 | **11208000** |
| `natural.totals.invalidated_rects` / `flushes` | 67 / 67 | **66 / 66** |
| histograma: reloj `(580,0,220,72)` | 1 | **0** |
| `--natural-static` `invalidated_px` (sólo band-info) | 254840 | **190400** |
| captura común `metrics.flushes` / `flushed_pixels` | 6 / 1127680 | **5 / 1111840** |
| `--bench-natural --display-off` `metrics.flushes` / `flushed_pixels` | 5 / 743680 | **4 / 727840** |
| `main-natural-eco` `invalidated_px` / `rects` | 2861240 / 25 | **2796800 / 20** |
| PNG de todos los modos | — | **sin cambio** (mismos sha256) |

En la radio el texto del reloj cambia una vez por segundo, así que la guarda no
lo apaga del todo: sólo evita el tick redundante (2 ticks/s → 1). En el arnés,
con `time()` congelado, los suprime todos. El perfil natural, el histograma y los
totales quedan fijados por `tests/natural_evidence.py`; el contrato de
`tools/ab_testing/tests/test_natural.py` se actualizó con los mismos números.

## Re-baseline 2026-10-23 — reloj permanente + banda de estado (AB-HOME250, issue #250)

El reloj de la barra superior deja de alternar hora y alimentación y pasa a ser
permanente, con una banda de estado compacta debajo (batería y fuente externa
opcional) en la misma caja de 206/209 x 61 (`src/clock.c`); la fila `Clock view` y
la de sus tres timeouts salen de Ajustes (`src/dialog_settings.c`). La evidencia
se re-generó con el binario de esta revisión (`tests/check_harness_natural.sh`
más `--bench-natural` sobre `main-natural-eco`).

Lo que **no** se mueve: el perfil de los 40 frames naturales (espectro 132000 px
x 27, cascada 196000 px x 39, `natural.totals` 11208000 px en 66 rectángulos, el
control `--natural-static` en 190400 px con sólo band-info) y la idempotencia
(`run1` y `natural` byte a byte). Con `time()` fijo el reloj no cambia su texto y
la guarda de igualdad del #150 lo apaga, así que sus rectángulos no aparecen en
el histograma; el histograma tampoco gana rectángulos.

Lo que sí cambia es el **settle de la captura** (el `metrics` de la captura
común), porque el reloj pasa de una etiqueta de dos líneas (potencia) a un
contenedor con la línea del reloj (209 x 31) y la banda de estado: su
invalidación de arranque es más chica y se une distinto con la del espectro
(0,0,800,165, que contiene al panel). El bloque `metrics` no expone los
rectángulos del settle, así que la descomposición exacta no se puede citar; el
número está medido sobre el binario de esta revisión y el control
`--display-off` lo verifica por diferencia (`SETTLE_PX - FULL_SCREEN_PX`).

| campo | antes (#150 `cbaaf72f`) | después (#250) |
|---|---|---|
| `metrics.flushes` (captura `main-natural`) | 5 | **6** |
| `metrics.flushed_pixels` (captura `main-natural`) | 1111840 | **1100352** |
| `--display-off` `metrics.flushes` / `flushed_pixels` | 4 / 727840 | **5 / 716352** |
| `main-natural-eco` `metrics.flushes` / `flushed_pixels` | 24 / 3776640 | **24 / 3762602** |
| `main-natural-eco` `natural.totals.invalidated_px` / rects | 2796800 / 20 | 2796800 / 20 |
| PNG de todos los modos | — | **cambian** (la tinta del panel del reloj) |

El PNG cambia en los cuatro modos porque la captura incluye el panel del reloj
(la hora grande arriba y el estado abajo, dentro de los mismos 206/209 x 61); el
fondo del panel (el bitmap de `clock_style`) queda byte a byte igual. Los sha256
nuevos están en `sha256.txt`.

Medición de costo del reloj, con el reloj de pared avanzando con el reloj virtual
(un parche local del `__wrap_time` del harness, no versionado) y el mismo camino
para las dos revisiones: el rectángulo que invalida el reloj cuando cambia el
segundo pasa de **15549 px** (`581,0,219,71`, el reloj de potencia de `main` en
vista hora) a **10272 px** (`586,0,214,48`, la línea del reloj del #250) en
`main-natural`, y a 7490 px (`586,0,214,35`) en `main-frecuencia-dseg` (dígitos
segmentados). El reloj permanente invalida menos píxeles por segundo que el viejo.

## Re-baseline 2026-10-23 — UI POLISH 1 HOME (#285, épica #284)

El reloj sube a `font_digit(&sony_44)` (la etiqueta de la hora pasa de 209×31 a
209×33), el porcentaje de batería sube a `&sony_22` y la fila de estado estrena
una **banda opaca full-width** del rol `TC(bar)` debajo de las etiquetas
(`src/clock.c`), y la runa de BT pasa de un zigzag a **dos trazos** (brazos +
espina, `src/info.c`). La evidencia se re-generó con el binario de esta revisión
(`tests/check_harness_natural.sh` más `--bench-natural` sobre `main-natural-eco`),
como las veces anteriores.

Lo que **no** se mueve: el perfil de los 40 frames naturales (espectro 132000 px
× 27, cascada 196000 px × 39, `natural.totals` 11208000 px en 66 rectángulos / 66
flushes, el control `--natural-static` en 190400 px con sus 7 band-info y
`--display-off` en 0 px), la idempotencia (`run1` y `natural` byte a byte) y el
histograma de rectángulos. Con `time()` fijo el reloj no cambia su texto y la
guarda de igualdad de OPT-4 #150 lo apaga, así que los frames naturales no
dependen del reloj: **el costo por tick no entra en esta medición**.

Lo que sí cambia es el **settle de la captura** (el bloque `metrics`) de los
siete modos: la invalidación de arranque del panel del reloj es 2 px más alta y
suma la superficie nueva de la banda, así que el settle sube **+544 px** con los
mismos flushes. El bloque `metrics` no expone los rectángulos del settle, así que
la descomposición exacta no se puede citar; el número está medido sobre el binario
de esta revisión y el control `--display-off` lo verifica por diferencia
(`SETTLE_PX − FULL_SCREEN_PX`).

| campo | antes (#250 `65f7842c`) | después (#285 `3cc409e9`) |
|---|---|---|
| `metrics.flushes` (captura `main-natural`) | 72 | **72** (6 + 66, sin cambio) |
| `metrics.flushed_pixels` (captura `main-natural`) | 12308352 | **12308896** (+544) |
| `--display-off` `metrics.flushes` / `flushed_pixels` | 5 / 716352 | **5 / 716896** (+544) |
| `static.json` `metrics.flushes` / `flushed_pixels` | 13 / 1290752 | **13 / 1291296** (+544) |
| `cli.json` `metrics.flushes` / `flushed_pixels` | 21 / 3656352 | **21 / 3656896** (+544) |
| `forced.json` `metrics.flushes` / `flushed_pixels` | 9 / 2252352 | **9 / 2252896** (+544) |
| `main-natural-eco` `metrics.flushes` / `flushed_pixels` | 24 / 3762602 | **24 / 3763146** (+544) |
| PNG de todos los modos | — | **cambian** (la tinta del panel del reloj) |

El +544 px es el settle de **arranque** de la captura, no costo de operación: con
el reloj de pared avanzando con el reloj virtual (parche local de `__wrap_time`,
no versionado) el rectángulo que invalida el reloj cuando cambia el segundo pasa
de **10272 px** (`586,0,214,48`) a **10486 px** (`586,0,214,49`), o sea **+214 px
por tick de 500 ms** (+2.1%; son los 2 px de alto que creció la etiqueta de la
hora), y el reloj invalida una vez por segundo en la radio. Los sha256 nuevos de
los siete PNG están en `sha256.txt`, junto con este bloque de re-baseline.

## Re-baseline 2026-10-24 — #306: los 44 px del waterfall de #298 (costo aceptado)

`#298` (`7a1795da`) dejó de reservar los `SPOT_MARKS_H = 44` px de la franja de
spots arriba del waterfall, así que la cascada subió 44 px y es 44 px más alta:
su caja pasó de `0,240 800×240` a `0,196 800×284` y cada fila invalida **231200
px** en vez de 196000 (`0,235 800×245` → `0,191 800×289`). El band-info, que es
hijo del waterfall (`y + 6`), se movió con él: `0,241 800×34` → `0,197 800×34`
(mismo tamaño). El owner **aceptó el costo** (opción A de #306): el waterfall se
queda con los 44 px y esta evidencia se re-generó con el binario de la revisión
`97229004` (`tests/check_harness_natural.sh` más `--bench-natural` sobre
`main-natural-eco`), sin tocar código de producto.

Lo que **no** se mueve: el perfil del espectro (132000 px × 27), el del reloj
(10486 px, 0 ticks con `time()` fijo), el histograma (cascada 39 / espectro 27),
la idempotencia (`run1` y `natural` byte a byte), el control `--natural-static`
en 190400 px con sus 7 band-info (se movió, no creció), `--display-off` en 0 px,
`waterfall_rasterized_px = 32000` (una fila de 800 px por fila nueva: el área
invalidada crece, el trabajo de software no) y `waterfall_full_rebuilds = 0`.

Lo que sí cambia es el **área invalidada**: +35200 px (800 × 44) por cada
rectángulo de cascada y +35200 px en el **settle** de los cinco modos (es la
invalidación de arranque que une la cascada con el espectro, una vez por
captura).

| campo | antes (`3cc409e9`, #285) | después (`97229004`, #306) | delta |
|---|---|---|---|
| `natural.totals.invalidated_px` / rects | 11208000 / 66 | **12580800 / 66** | +1372800 (**+12.2%**), 0 rects |
| rect de la cascada | `0,235 800x245` = 196000 | **`0,191 800x289` = 231200** | +35200 por fila (+18.0%) |
| `natural.totals.joined_px` | 1372000 | **1618400** | +246400 (7 × 35200) |
| `metrics` (`natural`/`run1`) | 72 / 12308896 | **72 / 13716896** | +1372800 (settle +35200) |
| `metrics` (`forced`) | 9 / 2252896 | **9 / 2288096** | +35200 |
| `metrics` (`static`) | 13 / 1291296 | **13 / 1326496** | +35200 |
| `metrics` (`cli`, `--bench-natural 10`) | 21 / 3656896 | **21 / 4008896** | +35200 settle, +316800 frames |
| `metrics` (`off`, `--display-off`) | 5 / 716896 | **5 / 752096** | +35200 |
| `main-natural-eco` invalidado / rects | 2796800 / 20 | **3113600 / 20** | +316800 (9 filas) |
| `main-natural-eco` `metrics` | 24 / 3763146 | **24 / 4115146** | +35200 |
| PNG de los siete modos | — | **cambian** | ver abajo |

El settle se puede citar exacto en esta re-baseline porque es **una sola
invalidación** de la cascada (800 × 44 = 35200 px): el bloque `metrics` no expone
los rectángulos, pero el delta es el mismo en los cinco modos y el control
`--display-off` lo verifica por diferencia (`1136096 − 384000 = 752096`).

Los PNG cambian por dos motivos, ninguno de este cambio (que no toca `src/`): el
waterfall está 44 px más arriba en la captura (`#298`) y esta evidencia estaba
commiteada desde `#285`, así que también recoge lo que `src/` se movió después
(`#294`, `#295`, `#298`, `#301`, `#292`). Los sha256 nuevos están arriba y en
`sha256.txt`; el PNG no depende del string de `HARNESS_REVISION` (la misma
revisión recompilada da el mismo sha256).

El **+12.2 %** es área invalidada, no trabajo por frame: el anillo de OPT-2
(`#148`) sigue rasterizando una fila de 800 px por fila nueva. El impacto real en
la Cortex-A7 (costo del flush por frame) lo mide el **QA físico**; este banco es
host x86 (`natural.environment`).

### Cómo se declara el perfil (por componente)

`tests/natural_evidence.py` ya no teclea rectángulos: declara la **caja** de cada
componente medida con `--inspect` (`COMPONENTS`) y **deriva** el rect que LVGL
invalida (caja + `ext_draw_size` de 5 px, recortada al panel). El band-info se
verifica en el control `--natural-static` contra su caja declarada (histograma de
7) y el settle se verifica ahora en los cinco modos. Control negativo (inyectado
y revertido): un rect no declarado en el JSON falla (`unprofiled invalidated
rectangle`) y mover un componente declarado también (`band_info_height 24 → 28`
→ `the control run invalidated (0, 195, 800, 38) ... not the declared band-info
rectangle (0, 197, 800, 34)`).

