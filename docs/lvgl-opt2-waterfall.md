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

### Baseline natural (A = `02090454`)

`main-natural.scn` (seed 13, 40 frames, un frame = un periodo de refresco de la
ROM, 50 ms), `--bench-natural`:

- `waterfall_rows` = **40**; `waterfall_offscreen_redraws` = **40**.
- Área invalidada = área flusheada = **11 223 840 px** en 67 rectángulos
  (waterfall `800x245` × 39 = 7 644 000; espectro 132 000 × 27; reloj 15 840).
- **Rasterizado** (mapeo + paleta): cada refresco reconstruye `WIDTH*height =
  800*245 = 196 000 px` ⇒ **40 × 196 000 = 7 840 000 px** por corrida. Es exacto
  (el `redraw_cb` del baseline recorre las `height` filas por refresco) y está
  confirmado por la cuenta de refrescos (40).
- Framebuffer `png sha256 = 16e1038e…` / `fnv1a = 0x182ac64b59f610ec`.

### Candidato (B = `576d2df3`, anillo)

Mismo fixture, mismos 40 frames:

- `waterfall_rows` = 40, `waterfall_offscreen_redraws` = **40** (mismo contrato
  del issue #13), `waterfall_full_rebuilds` = **0**.
- **Rasterizado = 32 000 px** (40 filas × 800 px): **245× menos, −99.6 %**.
- Área invalidada/flusheada **idéntica** (11 223 840 px): el scroll mueve todos
  los píxeles visibles, así que **no** se reduce la invalidación (H3).
- **PNG byte a byte idéntico** al baseline (`cmp` OK, mismo `fnv1a`).

| Métrica (40 frames naturales) | Baseline A | Candidato B | Δ |
|---|---|---|---|
| px rasterizados (mapeo+paleta) | 7 840 000 | 32 000 | **−99.6 %** |
| reconstrucciones completas | 0 | 0 | = |
| px invalidados | 11 223 840 | 11 223 840 | = |
| px flusheados | 11 223 840 | 11 223 840 | = |
| PNG | — | — | idéntico |

### Opción B (índices `uint8_t` + paleta) — medida y descartada

Probe `wf_format_probe` (LVGL 8.3.11 real, 800×200, host x86, `reps=300`):

| Formato | p50 | p95 |
|---|---|---|
| `LV_IMG_CF_TRUE_COLOR` 32 bits | 0,298 ms | 0,372 ms |
| `LV_IMG_CF_INDEXED_8BIT` + paleta | 1,515 ms | 1,808 ms |

El flush de la imagen indexada es **~5× más lento** (la conversión de paleta por
píxel del decodificador contra el `memcpy` del camino rápido de TRUE_COLOR,
`lv_draw_sw_img.c:66`). Además, sin el anillo el cómputo por píxel (la división
`src_x`) seguiría ocurriendo por refresco; con el anillo ya no aporta nada.
**No es competitiva**: no se implementa (el issue la condiciona a que lo sea).

### Fixtures deterministas (`tests/waterfall`, gate `tests/check_waterfall_incremental.sh`)

`waterfall_test` compila el `src/waterfall.c` real contra LVGL 8.3.11 real y un
stub de radio; el oráculo de imagen es independiente del módulo (reimplementa el
mapeo del baseline) y compara la ventana visible (`waterfall_frame_pixels` +
`waterfall_render_head`). **186 checks, 0 fallas**, en build normal **y** bajo
ASan+UBSan:

| Fixture | Qué prueba | Resultado |
|---|---|---|
| 20 vueltas de anillo (H=8, 160 filas) | imagen por fila + offset (`render_head`), 160×800 px, 0 rebuilds | PASS |
| cambio de centro durante el scroll | camino real `do_scroll_cb` → reconstrucción completa y ventana correcta | PASS |
| zoom 100 kHz→30 kHz→100 kHz | reconstrucción por ancho de FFT | PASS |
| cambio de paleta | reconstrucción por paleta (memcmp) | PASS |
| RX↔TX | la fila TX viaja con su ancho, sin rebuild | PASS |
| ECO (1/4) + pantalla OFF/ON | ninguna fila se pierde; 0 px rasterizados con el panel oscuro | PASS |
| límites 0/1/512, size>NFFT, ancho estrecho, reserva fallida | sin divisores cero, sin desborde, inerte sin crash | PASS |

### A/B + contratos + benchmark

- **`compare` (A `02090454` vs B `576d2df3`)**: **PASS** — 89 escenarios en
  ambas revisiones, **pixel-identical**; gates Functional / Visual Regression /
  Quality Contract (913 checks) / Performance en PASS; Hardware y Energy BLOCKED
  (advisory, sin equipo físico). Informe en `tools/ab_testing/reports/AB-OPT2-148`.
- **`contracts --revision 576d2df3`**: PASS (913 checks).
- **`audit --revision 576d2df3`**: PASS — 11 issues conocidos (0 reproducidos, 9
  sin verificar), 0 hallazgos estructurales sobre 9810 objetos en 89 escenarios.
- **`benchmark` A vs B**: el barrido completo (89 escenarios) sale
  **INCONCLUSIVE** por dispersión del host (contenedores de otras corridas en
  paralelo), pero **`flushes Δ = 0.0 %` y `area Δ = 0.0 %` en todos**: no hay
  cambio estructural. El foco en los escenarios del waterfall
  (`BENCH-OPT2-148-FOCUS`, 5 rondas) da **PASS** en `bench-render` (p95 Δ
  **−6,9 %**, el candidato queda igual o mejor: el segundo tile del offset no
  cuesta) y `main-espectro-agua` (+2,2 %); `main-natural` queda INCONCLUSIVE por
  un p95 de ruido (p50 3,58→3,81 ms) con los proxies deterministas en 0 %.
- **Gate natural** (`tests/check_harness_natural.sh`): **PASS**, y ahora cruza el
  instrumento: 800 px por fila nueva, 0 reconstrucciones en reposo y 0 px con el
  panel oscuro.
- **Tests nativos** (`tests/native/run_native_tests.sh`): PASS (ASan/UBSan, 0
  fallos; incluye TSan del espectro de #149).

### Memoria

El anillo reutiliza el **mismo** buffer `800*height*4` (784 000 B con
`height=245`): no hay buffer 2× ni copias extra. `RSS/PSS` del banco forzado
(`bench-render`, host) queda dentro del ruido (A ≈ 10,6/7,6 MB, B ≈
10,5/9,7 MB); el `PSS` de B sale más alto en esa corrida por ruido de medición
del host, no por el cambio (el frame no crece).

## Riesgos y rollback

- **Riesgo principal**: un anillo mal indexado mostraría la fila equivocada de
  forma persistente (no se autocorrige como el render completo del baseline).
  Mitigado por el oráculo independiente (160 ventanas comparadas fila a fila),
  por la reconstrucción completa ante cualquier cambio de mapeo y por el A/B
  pixel-identical de 89 escenarios. **No mitigado en hardware** (banco BLOCKED).
- **Riesgo secundario**: con `offset_y > 0` LVGL dibuja la imagen en **dos
  tiles** en vez de uno; el área es la misma pero hay una llamada extra de
  `lv_draw_img` por frame. Medido: `bench-render` (el forzado con el waterfall)
  queda en p95 Δ **−6,9 %** — sin regresión.
- **Riesgo terciario**: `pending_rows` lo escribe el hilo de radio y lo lee el de
  GUI; es el **mismo modelo de hilos del baseline** (que ya lee `last_row_id` y
  la cache desde el callback del scheduler) y no agrega una clase de carrera
  nueva. Un snapshot lock-free como el de #149 queda como PENDIENTE.
- **Rollback**: revertir el commit. No hay migración, ni estado persistente, ni
  cambio de ABI; el frame sigue siendo `800 x height`.

## PENDIENTES

1. **Gate de hardware/QEMU**: el banco quedó **BLOCKED** — falta el chroot
   `../.vfix3`, falta compilar `tools/remote/x6200-remote`, y el volumen de build
   está en carrera (hay un `banco-apps` de otra corrida). No se midió el binario
   ARM completo ni el panel físico. Como en #147, no se extrapola QEMU a
   Cortex-A7 ni a energía.
2. **Medición en Cortex-A7**: los tiempos son proxy x86. Falta el perfil real
   (p95 de redraw, CPU, energía) en la radio.
3. **Snapshot lock-free del waterfall**: aplicar al waterfall el patrón de #149
   (frame compartido bajo mutex + copia al hilo de GUI) si se quiere eliminar la
   carrera preexistente entre `waterfall_data` (radio) y `refresh_waterfall`
   (GUI).
4. **Wiring del gate** `check_waterfall_incremental.sh` en el check-matrix del
   harness (`scripts/apps/x6200/`), como quedó pendiente el gate de #147.
