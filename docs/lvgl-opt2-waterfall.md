# OPT-2 — Rasterizado incremental del waterfall y anillo de filas (issue #148)

Experimento 2 de 4 de la tanda LVGL 8.3.11. **LVGL y el gitlink quedan
intactos** (`lvgl/` en `74d0a816`, v8.3.11); no se toca kernel, DTS, GPU, DSP,
FFT, RF ni TX, ni el framebuffer de hardware.

Este documento es la **spec del experimento**: la hipótesis y el presupuesto
cuantitativo están escritos **antes** de medir (primer commit de la rama). Los
resultados se agregan abajo, sin reescribir la hipótesis.

## Estado actual (hecho verificable)

`src/waterfall.c`:

- `waterfall_data()` normaliza cada fila contra `grid_min/grid_max` (o los
  defaults en TX) a un índice `uint8_t` y la guarda en `waterfall_cache`, un
  anillo de `height` filas de `WATERFALL_NFFT = 512` muestras (`src/dsp.h`).
  Cada fila guarda además su `center_freq` y `width_hz` históricos.
- `refresh_waterfall()` (encolado por `scheduler_put_noargs`) llama
  `redraw_cb(NULL)` y `lv_obj_invalidate(img)`. `redraw_cb()` recorre **todas**
  las `height` filas y, por cada una, las `WIDTH = 800` columnas: ~`800*height`
  píxeles de mapeo (una división entera por píxel + lookup de paleta + store de
  32 bits) **aunque sólo haya llegado una fila nueva**.
- Con la geometría documentada (`main_screen.c`: `waterfall_set_height(480 - y)`,
  `y = 235` en el fixture natural) son `800*245 = 196 000 px` por refresco. A
  `1000/25 = 40 ms` por fila son ~4.9 Mpx/s de rasterizado sostenido.
- `refresh_period` (ECO = 4, diálogo = 2, UI-08 incr. 2) y el corte con el panel
  apagado (`backlight_is_on()`, issue #13) ya existen y **se conservan**.
- El instrumento `waterfall_offscreen_redraw_count()` (issue #13) cuenta
  *reconstrucciones completas* de la paleta; con el panel apagado la cascada las
  saltea y la cuenta queda en 0.

Baseline aceptado: **#147** (draw buffer de 128 líneas, `5ebb8abe`/`410c066b`) y
**#149** (snapshots del espectro + coalescing del scheduler, `95eebd3b`), ambos
ya en el trunk (`02090454`). No se reimplementa nada de eso.

## Hipótesis (antes de medir)

- **H1 (correctitud / equivalencia visual).** Mantener un **anillo de filas ya
  rasterizadas** y resolver el scroll por **indexación** (offset de imagen de
  LVGL) produce un framebuffer **idéntico byte a byte** al baseline para todo
  estado visible: la misma imagen final, los mismos cambios de centro/zoom,
  RX↔TX, las seis paletas y el modo ECO. El mapeo por fila no cambia: sólo
  cambia *cuándo* se recalcula cada fila.
- **H2 (trabajo de software).** Al entrar una fila nueva, con los parámetros de
  mapeo sin cambios (`wf_center_freq`, `width_hz` global, paleta), el
  rasterizado baja de `800*height` a **`800` px** por refresco (una sola fila).
  Para `height = 245` es `196 000 → 800 px`, **−99.6 %** (245×). Se agrega un
  instrumento (`waterfall_rasterized_px`) porque la invalidación/flush **no**
  cambia.
- **H3 (sin reducir copias, honesto).** El scroll mueve *todos* los píxeles
  visibles, así que el área invalidada y flusheada por fila **sigue siendo
  `800*height`** (`196 000 px`). **No** se vende una reducción de invalidación:
  el ahorro es de CPU/rasterizado, no de flush. La indexación por offset **no**
  agrega ninguna copia de memoria respecto del baseline (el baseline reescribe
  las `height` filas; la propuesta escribe 1).
- **H4 (reconstrucción completa sólo cuando corresponde).** Reconstruir las
  `height` filas **sólo** cuando cambia algo que lo exige: centro (`wf_center_freq`,
  scroll suave o salto), zoom (`width_hz`/FFT), tamaño (resize), paleta/tema, o
  el historial es incompatible (llegaron ≥ `height` filas sin refrescar). Los
  ids ya están normalizados al escribir, así que **min/max y TX no** exigen
  reconstruir (se documenta). Durante el scroll suave (transitorio) el costo
  vuelve al del baseline; en reposo cae a 1 fila.
- **H5 (memoria).** El anillo reutiliza el **mismo** buffer de imagen
  `800*height*4` (`784 000 B` con `height = 245`): **sin aumento** de memoria
  respecto del baseline. No se usa un buffer 2× (`offset` con tiling de LVGL ya
  envuelve el anillo sin copias). RSS/PSS dentro del ruido.
- **H6 (opción B).** Una imagen de índices `uint8_t` + LUT convertida al dibujar
  (`LV_IMG_CF_INDEXED_8BIT`) **no** es competitiva: el cómputo por píxel
  (división) sigue ocurriendo por refresco salvo que se combine con el anillo, y
  la conversión de paleta se mueve al flush de LVGL (por píxel) en vez del
  `memcpy` del `TRUE_COLOR` de 32 bits. Se mide/argumenta y se descarta salvo
  sorpresa.

## Presupuesto cuantitativo (fijado antes de medir)

| Métrica | Baseline (A) | Objetivo (B, anillo) | Cómo se mide |
|---|---|---|---|
| px rasterizados por fila nueva (reposo) | `196 000` | `800` (−99.6 %) | instrumento nuevo en `waterfall.c` + banco natural |
| px rasterizados por fila en scroll suave | `196 000` | `196 000` (sin regresión) | fixture de cambio de centro durante el scroll |
| área invalidada / flusheada por fila | `196 000` | `196 000` (**sin cambio, declarado**) | `natural_evidence.py` (`invalidated_px`, `flushed_px`) |
| copias de memoria extra por fila | `0` (reescribe) | `0` (indexa) | argumento + inspección del código |
| memoria del frame | `800*245*4 = 784 000 B` | `784 000 B` (sin cambio) | `RSS/PSS` del banco + `nm`/`size` |
| p95 `render_ms` (natural, host) | línea base | **≤ −5 %** o dentro del ruido | `x6200-ab benchmark` |
| equivalencia visual | — | **pixel-identical** (0 px) en las 87 fixtures | `x6200-ab compare` |
| contratos | — | PASS (sin regresiones nuevas) | `x6200-ab contracts` |

Umbral de decisión: si el rasterizado por fila no baja a ~1 fila y/o la
equivalencia visual falla, **no se fuerza** el cambio: se entrega el informe
comparativo (el DoD lo permite). Si el anillo no aporta y el costo se mantiene,
se declara INCONCLUSIVE y se deja el baseline.

## Alcance

1. Instrumentar `waterfall.c` (px rasterizados, filas renderizadas por camino,
   reconstrucciones completas) sin cambiar la imagen.
2. Anillo de filas rasterizadas + resolución del scroll por indexación
   (`lv_img_set_offset_y`, que en LVGL 8.3.11 envuelve la fuente cuando la
   imagen es mayor o igual que el objeto).
3. Reconstrucción completa sólo ante cambios de parámetros de mapeo, tamaño,
   paleta o historial incompatible; conservar `refresh_period` y el corte por
   backlight apagado, mostrando **todas** las filas pendientes al despertar.
4. Robustez de límites: `height = 0`, `size > WATERFALL_NFFT`, `width_hz`/`WIDTH`
   y `src_w` divisores, `malloc/calloc` fallidos, resize seguro.
5. Comparar A/B (anillo vs baseline) y medir/argumentar la opción B.

**Fuera de alcance:** profundidad de color, RGB565, framebuffer de hardware,
migración a LVGL 9.6, DSP/FFT/RF/TX, kernel/DTS/GPU, el servidor remoto (#157).

## Aparato de medición

- **Banco natural** (`tools/ui_harness --bench-natural`, `main-natural.scn`):
  perfil de invalidación real por frame, separado del bench forzado.
- **Instrumento nuevo** (`waterfall_rasterized_px`, expuesto en el JSON del
  banco natural): píxeles efectivamente rasterizados por el mapeo, para separar
  el costo de datos del de rasterizado del framebuffer interno y del de
  LVGL/flush/rotación.
- **Fixtures deterministas**: banco del arnés (`tools/ui_harness`) con la
  cascada real y LVGL real (20 vueltas de anillo con asserts de imagen y offset,
  scroll con cambio de centro, zoom, seis paletas, RX↔TX, ECO, display OFF/ON,
  límites), más los tests nativos con ASan/UBSan.
- **A/B + contratos + benchmark**: `tools/ab_testing` (`compare`, `contracts`,
  `audit`, `benchmark`).
- **QEMU ARM**: banco del skill `pruebas-sin-radio` (binario ARM real) o BLOCKED
  explícito. QEMU/host **no** son Cortex-A7 ni batería: se declara.

## Resultados

(pendiente: se agregan tras medir, sin reescribir la hipótesis)

## Riesgos y rollback

(pendiente)

## PENDIENTES

(pendiente)
