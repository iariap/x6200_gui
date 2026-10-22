# OPT-4 — Invalidaciones, flushes y wakeups innecesarios (issue #150)

Cuarto y último experimento de la tanda LVGL 8.3.11. **LVGL y el gitlink quedan
intactos** (`lvgl/` en `74d0a816`, v8.3.11): no se toca el submódulo, ni el
kernel, DTS, GPU, DSP, FFT, RF ni TX, ni los buffers DRM/fbdev.

La spec del issue es el contrato; este documento fija la **hipótesis y el
presupuesto antes de medir** (primer commit de la rama) y agrega abajo los
resultados, sin reescribir la hipótesis.

## Estado actual (hecho verificable)

Baseline ya en el trunk, **no se reimplementa**:

- **#147** draw buffer de 128 líneas (`src/main.c`: `DISP_BUF_LINES 128`).
- **#148** waterfall incremental (anillo de filas, `waterfall_rasterized_px` /
  `waterfall_full_rebuilds`). El scroll mueve *todos* los píxeles visibles: el
  área invalidada/flusheada por fila **no** baja, baja el rasterizado.
- **#149** snapshots del espectro + coalescing del scheduler.
- **#131b** LVGL sólo desde el hilo de GUI.

Se conservan además `LV_TICK_CUSTOM` con `CLOCK_MONOTONIC`, los 20 FPS base
(`LV_DISP_DEF_REFR_PERIOD 50`), el skip offscreen (`backlight_is_on()` en la
cascada) y la política ECO.

Perfil natural medido con el banco de UI-07 (`tools/ui_harness`, modo
`--bench-natural`), que corre el lazo natural de LVGL sobre el reloj virtual
(sin forzar pantalla completa) y reporta la invalidación real por fuente:

| Región | Rectángulo | px | Causa |
|---|---|---|---|
| spectrum | `0,0 800x165` | 132000 | `spectrum_refresh()` (src/spectrum.c), cada FFT |
| waterfall | `0,235 800x245` | 196000 | `refresh_waterfall()` (src/waterfall.c), cada fila |
| clock | `580,0 220x72` | 15840 | `show_time()`, cada 500 ms (src/clock.c) |
| band info | `0,241 800x34` | 27200 | fade de 2 s (src/band_info.c) |
| BT | `152,23 30x36` | 1080 | timer de 2 s (src/info.c) |

El espectro y la cascada dominan (99.6 % del área) y **su área no se puede
reducir de forma segura**: la cascada desplaza todos los píxeles y el espectro
redibuja la curva completa (picos, fill/filter, indicador de frecuencia). La
sonda de sensibilidad de `NATURAL-UI07-N1` ya demostró que recortar el espectro
a media área cambia el PNG (frames viejos): no se vuelve a proponer.

## Hipótesis (antes de medir)

- **H1 (guardas de igualdad, byte-idénticas).** Los updaters que corren por
  timer o desde más de un observer reescriben el mismo valor, y
  `lv_label_set_text` / `lv_obj_set_style_*` invalidan el objeto **aunque el
  valor no cambie** (LVGL 8.3.11 no compara). Comparar contra el valor **vivo**
  del objeto (`lv_label_get_text`, `lv_obj_get_style_*`) antes de escribir
  elimina invalidaciones sin cambiar un solo píxel, y **sin caché global**: el
  valor resuelto del objeto ya refleja el tema/fuente vigente.
- **H2 (medición).** En el banco natural el reloj (texto congelado por `time()`
  fijo en el arnés) y el icono BT (color sin cambio) dejan de invalidar; el
  control `--natural-static` pierde esos ticks. La captura común también baja
  porque tenía un tick de reloj suelto.
- **H3 (subregiones reales).** Sólo se recorta invalidación donde el contenido
  fuera de la región es byte-idéntico (que es el caso de las guardas: no hay
  región nueva, se evita una invalidación que no cambiaba nada). No se propone
  una región mínima para el espectro ni para la cascada.
- **H4 (OFF→ON).** Con el panel apagado `_lv_inv_area()` descarta cada
  invalidación; al re-habilitar, LVGL **no** repinta solo, así que la primera
  invalidación tras despertar tiene que cubrir toda la pantalla. Hoy
  `backlight_switch()` sólo re-habilita la invalidación: los píxeles que
  cambiaron con la pantalla apagada (reloj, banda) quedan viejos. Se agrega la
  invalidación del screen al despertar.
- **H5 (bucle de UI, sin cambios).** No se cambian los 20 FPS ni el read period
  de input ni se pasa a `poll()`: no hay ganancia medible que justifique el
  riesgo de perder eventos.

## Presupuesto cuantitativo (fijado antes de medir)

| Métrica | Baseline (A) | Objetivo (B) | Cómo se mide |
|---|---|---|---|
| `invalidated_px` natural (40 frames) | 11223840 | bajar el reloj/BT (≈ −0.14 %) | `--bench-natural` |
| `--natural-static` invalidated_px | 254840 | apagar reloj+BT (≈ −25 %) | `--bench-natural --natural-static` |
| captura común (settle) flushes/px | 6 / 1127680 | 5 / 1111840 | `metrics` |
| PNG natural / captura | — | **byte-idéntico** | sha256 |
| wakeups/s del lazo | 14 | 14 (sin cambio) | `--bench-wakeups` |
| OFF→ON | 0 invalidaciones al despertar | 1 invalidación de pantalla completa | test nativo + banco |

## Resultados

### Guardas de igualdad (commit `923e4dea`, base `1c599025`)

Banco natural (`tests/check_harness_natural.sh`, `main-natural`, seed 13, 40
frames de 50 ms):

| Métrica | A (baseline) | B (guardas) | Δ |
|---|---|---|---|
| `natural.totals.invalidated_px` | 11223840 | 11208000 | −15840 (−0.14 %) |
| `natural.totals.invalidated_rects` / `flushes` | 67 / 67 | 66 / 66 | −1 |
| `--natural-static` `invalidated_px` | 254840 | 190400 | **−25.3 %** |
| captura común `metrics.flushes` | 6 | 5 | −1 |
| captura común `metrics.flushed_pixels` | 1127680 | 1111840 | −15840 |
| PNG natural (sha256) | `16e1038e…` | `16e1038e…` | **byte-idéntico** |

El control `--natural-static` (drive apagado) es la medida limpia de las guardas:
sus únicos rectángulos eran los timers de 2 s del firmware (reloj `4×15840` +
BT `1080` + band-info `7×27200`). Después de las guardas quedan sólo los
`7×27200` del band-info: el reloj y el BT dejan de invalidar cuando su valor no
cambia. El área dominada por espectro y cascada no se mueve (99.6 % del total),
como corresponde: esas regiones sí cambian.

El PNG byte-idéntico es la prueba de que las guardas **no cambian la
presentación**: sólo suprimen invalidaciones que no cambiaban un píxel.

### Bucle de UI

`--bench-wakeups`: 14 wakeups/s del lazo (11 sin drive), 0 del hilo de tick
(`LV_TICK_CUSTOM`), **igual** en A y B. No se tocó el bucle (H5).

### OFF→ON

`src/backlight.c` invalida el screen entero al volver la pantalla. Cubierto por
`tests/native/test_backlight.c` (la primera invalidación tras despertar es 1, y
apagar no invalida) y por el banco natural `--display-off` (0 px con el panel
oscuro, sin regresión).

### QEMU ARM (binario real, `qemu-arm-static`, `cortex-a7`)

`tools/captura-banco.sh` con el firmware reconstruido del commit `923e4dea`
(`make x6200-gui-dirclean && make x6200-gui-rebuild`):

| Captura | Comando (socket de control) | Resultado |
|---|---|---|
| home | handshake `{"cmd":"get"}` | 20891 B, 210 colores |
| Settings | `{"cmd":"app","text":"Settings"}` | 28183 B |
| Bands | `{"cmd":"app","text":"Bands"}` | 23596 B |

El binario pasa el gate `verifica-firmware-port.py` (13 855 068 B, 3/3 marcas).
`fuente md5 a3c8a64e7943`, `binario md5 9a26b607c67d`. **QEMU user-mode no es un
Cortex-A7 ni mide batería.**

## A/B (arnés `x6200-ab`)

`x6200-ab compare --a 1c599025 --b 923e4dea --profile host-ui` → **PASS**:

| Gate | Requerido | Resultado |
|---|---|---|
| Functional | sí | PASS — 89 escenarios capturados en A y B |
| Visual Regression | sí | PASS — **capturas pixel-idénticas** al baseline |
| Quality Contract | sí | PASS — 913 chequeos sobre el candidato |
| Performance | sí | PASS — proxy de costo dentro del presupuesto |
| Hardware | no (advisory) | BLOCKED — sin X6200 físico |
| Energy | no (advisory) | BLOCKED — sin medición de energía |

Contadores de render de la suite completa (mismo PNG en los 89 escenarios):

| Métrica | A | B | Δ |
|---|---|---|---|
| flushes (suma) | 1324 | 939 | **−385 (−29.1 %)** |
| px flusheados (suma) | 128766674 | 124462396 | **−4304278 (−3.34 %)** |
| escenarios con PNG distinto | — | — | **0 de 89** |

Los escenarios que más bajan son los que corren más tiempo virtual (el reloj
congelado del arnés deja de invalidar): `swrscan-*` (32→18 flushes), `autotune`
(149→90), `settings-*` (−1). En la radio el reloj cambia una vez por segundo, así
que el ahorro real es aproximadamente la mitad de esos ticks; el arnés los
suprime todos porque `time()` está fijo.

La regresión visual nula en 89 escenarios (6 temas, Settings, menús de apps, TX
simulado, ECO, REC, ventanas superpuestas) es la evidencia de que las guardas
sólo suprimen invalidaciones que no cambiaban un píxel.

`x6200-ab audit --revision 923e4dea` → **PASS** (no escribe reporte): 11 issues conocidos, 0
reproducidos, 9 no verificados; inspección estructural REAL sobre 9810 objetos en
89 escenarios, **0 hallazgos estructurales**.

`x6200-ab benchmark --a 1c599025 --b 923e4dea` (`BENCH-OPT4-T1`) →
**INCONCLUSIVE** (exit 3): el host x86 compartido tiene dispersión (CV) > 25 % en
el lado A, que es el umbral del presupuesto para no confiar en la comparación. Los
p95 observados de B son menores o iguales que los de A en casi todos los
escenarios (p. ej. `swrscan-owner-40m-sweep` −44 %), y el RSS/PSS queda dentro del
ruido (≈10.5 MB en ambos lados). **No se declara una mejora de tiempo de CPU**: la
medición de reloj real no es concluyente en este host, y la cadencia de la
Cortex-A7 no se mide acá.

## Descartado (con motivo)

- **Subregión mínima del espectro / de la cascada.** La cascada desplaza todos
  los píxeles visibles (el área invalidada no baja, ya lo documenta #148) y el
  espectro redibuja la curva, picos, fill/filter e indicador de frecuencia. La
  sonda de sensibilidad de `NATURAL-UI07-N1` ya probó que recortar el espectro
  cambia el PNG: no hay subregión segura. Se aplicó el principio de H3: sólo se
  recorta donde el contenido fuera de la región es byte-idéntico, y eso es
  exactamente lo que hacen las guardas (no invalidar cuando nada cambió).
- **Cambiar los 20 FPS / `LV_INDEV_DEF_READ_PERIOD` / pasar el lazo a `poll()`.**
  No hay ganancia medida que lo justifique y el riesgo de perder eventos de
  encoder/teclas es real (H5). El perfil de wakeups quedó igual (14/s).
- **Reimplementar el waterfall incremental.** Ya está en #148; el banco natural
  mide `waterfall_rasterized_px` y `waterfall_full_rebuilds` sin cambios.

## Pendientes

- Medir en la Cortex-A7 real el tiempo de copia al panel y la energía: acá sólo
  hay área/flush exactos (x86) y QEMU user-mode.
- Latencia encoder→cambio visible: el arnés no la mide de punta a punta; queda
  como proxy el `flushes`/área y el benchmark de render (INCONCLUSIVE en este
  host).
- El fixture natural no conduce el **medidor** (el arnés no compila `dsp.cpp`),
  así que la guarda de `src/meter.c` está revisada y con la dependencia de
  `noise_level` incluida, pero no medida por el banco; un fixture que llame
  `meter_update` a la cadencia del DSP la mediría.
- Escenario "20 cambios de Ajustes" y "REC on/off": cubiertos por capturas A/B
  (`settings-*`, fixtures de REC) pero no como secuencia determinista con
  conteo de invalidación por paso.

## Límites honestos

- x86 host: las áreas son exactas (mismo código que la radio), pero el tiempo de
  copia al panel, la cadencia real del FFT bajo carga y todo lo de energía son
  la pasada de hardware (PENDIENTE, sin radio).
- QEMU ARM user-mode corre el binario real, pero **no** es un Cortex-A7 ni mide
  batería.
