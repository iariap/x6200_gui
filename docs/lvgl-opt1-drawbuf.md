# OPT-1 — Draw buffer con rotación 90° (issue #147)

Experimento 1 de 4 de la tanda LVGL 8.3.11. **LVGL y el gitlink quedan
intactos**; no se toca kernel, DTS, GPU, DSP, RF ni el framebuffer de hardware.

Este documento es la **spec del experimento**: la hipótesis y el presupuesto
cuantitativo están escritos **antes** de medir (ver el primer commit de la
rama). Los resultados se agregan abajo, sin reescribir la hipótesis.

## Estado actual (hecho verificable)

`src/main.c`:

```c
#define DISP_BUF_SIZE (800 * 480 * 4)
static lv_color_t buf[DISP_BUF_SIZE];
lv_disp_draw_buf_init(&disp_buf, buf, NULL, DISP_BUF_SIZE);
```

`lv_conf.h` tiene `LV_COLOR_DEPTH 32`. El cuarto argumento de
`lv_disp_draw_buf_init` es `size_in_px_cnt`: **cantidad de `lv_color_t`, no
bytes**. Entonces el buffer reserva

```text
800 * 480 * 4 = 1 536 000 px  ->  6 144 000 B = 5,859375 MiB
```

que es **4× una imagen 800×480** (una imagen completa son 384 000 px). El `*4`
estaba pensado como "4 bytes por píxel", pero cae sobre la cuenta de píxeles.
El driver registra `hor_res=480`, `ver_res=800`, `sw_rotate=1`,
`rotated=LV_DISP_ROT_90`; LVGL resuelve la pantalla lógica como 800×480 y el
framebuffer físico como 480×800 (el banco lo confirma: "el fb es 480x800x32 BGRA
y hay que rotarlo 270").

## Hipótesis (antes de medir)

- **H1 (correctitud).** Un draw buffer **parcial** de `N` líneas lógicas
  (`800*N` px) con `sw_rotate`/`LV_DISP_ROT_90` produce el **mismo framebuffer
  físico final** que el buffer actual, para `N ∈ {32, 64, 128}` y para un
  framebuffer completo (una pantalla, `800*480` px). La rotación por software de
  LVGL 8.3.11 (`draw_buf_rotate`, `lv_refr.c`) trocea el área lógica por
  `get_max_row()` (tamaño del buffer / ancho) y rota cada trozo con
  `LV_DISP_ROT_MAX_BUF`, así que el tamaño del buffer no debería cambiar el
  resultado, solo cuántos trozos se flushean.
- **H2 (contrato de flush).** Se preserva `lv_disp_flush_ready` (llamada en cada
  flush) y `lv_disp_flush_is_last` (verdadero exactamente una vez por ciclo de
  refresco), la rotación, los dirty rects, la composición de screenrec y la
  secuencia de frames. Ningún rect de flush sale del panel físico.
- **H3 (memoria).** El ahorro es el de la fórmula, reservado estático (BSS):

  | Candidato | px (`800*N`) | bytes | MiB | vs actual |
  |---|---|---|---|---|
  | actual (`800*480*4`) | 1 536 000 | 6 144 000 | 5,859 | — |
  | pantalla completa (`800*480`) | 384 000 | 1 536 000 | 1,465 | −4,394 MiB (−75,0 %) |
  | 128 líneas | 102 400 | 409 600 | 0,391 | −5,469 MiB (−93,3 %) |
  | 64 líneas | 51 200 | 204 800 | 0,195 | −5,664 MiB (−96,7 %) |
  | 32 líneas | 25 600 | 102 400 | 0,098 | −5,762 MiB (−98,3 %) |

- **H4 (presupuesto de rendimiento, host proxy).** Con `N=64` el tiempo de
  render por frame (reloj real, proxy x86, **no** Cortex-A7) sube **≤ 15 %** y
  la cantidad de flushes por ciclo sube **≤ 2×** respecto del actual. Si se
  supera, se prefiere un `N` mayor o el framebuffer completo.
- **H5 (sin regresión visual).** `x6200-ab compare` A (base) vs B (cambio)
  da **pixel-identical** en las 6 fixtures (800×480, temas, fuentes, Settings,
  foco, diálogo, waterfall/espectro, zoom/rotación, banner REC, TX simulado).

## Alcance

1. Instrumentar el baseline A (símbolo estático, RSS/PSS, flushes, área/bytes
   copiados, p95 de redraw) distinguiendo **reservado** vs **residente**.
2. Investigar el buffer parcial con rotación por software en LVGL 8.3.11 y los
   flush callbacks (`screen_rec_flush` → `fbdev_flush`/`drm_flush`).
3. Ensayar 32, 64 y 128 líneas (+ framebuffer completo como control).
4. Mantener `lv_disp_flush_ready`, `lv_disp_flush_is_last`, rotación, dirty
   rects, screenrec y secuencia de frames en ambos backends.
5. Elegir por medición; si lo parcial degrada, documentarlo y dejar una
   configuración segura.

**Fuera de alcance:** profundidad de color, framebuffer de hardware, migración a
LVGL 9.6, DSP/RF/TX, kernel/DTS/GPU.

## Aparato de medición

- **Probe de rotación + buffer parcial** (`tools/ui_harness/probe/disp_buf_probe.c`):
  compila el **LVGL 8.3.11 real** del submódulo con la configuración de display
  del firmware (`hor_res=480`, `ver_res=800`, `sw_rotate=1`, `ROT_90`) y
  reconstruye el framebuffer físico desde los callbacks de flush. Renderiza una
  secuencia de escenas (pantalla completa, invalidaciones parciales, rects
  cortados en límites, más de un flush por frame, áreas no alineadas, cambios de
  pantalla) para cada tamaño de buffer y compara los framebuffers físicos byte a
  byte. No usa radio, red ni GPU.
- **Memoria reservada**: `size` sobre el símbolo estático (BSS).
- **A/B + contratos + benchmark**: `tools/ab_testing` (`x6200-ab compare`,
  `contracts`, `benchmark`), modo natural.
- **QEMU ARM**: banco del skill `pruebas-sin-radio` (binario ARM real). Si el
  entorno no está, se declara **BLOCKED** explícito (no PASS).

## Resultados

### Baseline A (el buffer actual)

`size_in_px_cnt` es una cuenta de píxeles: `(800*480*4)` reserva **4 pantallas**.

- **Reservado (BSS)**: 6 144 000 B = **5,859 MiB** (medido con `nm -S` sobre el
  símbolo estático `buf`; ver tabla abajo).
- **Residente (tocado)**: 1 536 000 B = **1,465 MiB**, o sea **25 %** de lo
  reservado. LVGL nunca escribe más de una pantalla porque `get_max_row()`
  (`lv_refr.c`) devuelve `size / area_w` acotado por la altura del área
  invalidada: con una invalidación de pantalla completa da `min(1536000/800,
  480) = 480` líneas. Las otras 3 pantallas quedan en BSS sin tocar, y el kernel
  no les da página física. **El costo residente real es ~1,5 MiB, no 5,86 MiB**;
  el resto es reserva virtual.
- **Flushes**: una pantalla completa se flushea en 10 llamadas (la rotación por
  software trocea con `LV_DISP_ROT_MAX_BUF`, que en la ROM son 50 líneas);
  la franja del espectro (800×165) en 4; la del waterfall (800×245) en 5.

La medición reservado-vs-residente la hace el propio probe con `mmap` + `mincore`
sobre el buffer (equivalente exacto del array BSS del firmware: páginas anónimas
zero-on-demand). Ver `--px 1536000` en la evidencia.

### Tabla por tamaño (medida)

`px = 800*N` es `size_in_px_cnt`. Reservado y residente salen del probe; flushes
y tiempos son del probe sobre el host (proxy x86, **no** Cortex-A7 ni batería).

| Candidato | px | reservado | residente | full redraw | espectro 800×165 | waterfall 800×245 | p50 host (full) |
|---|---|---|---|---|---|---|---|
| actual (`800*480*4`) | 1 536 000 | 5,859 MiB | 1,465 MiB | 10 flushes | 4 flushes | 5 flushes | ~2,4 ms |
| pantalla completa (480) | 384 000 | 1,465 MiB | 1,465 MiB | 10 | 4 | 5 | ~2,4 ms |
| **128 líneas** | 102 400 | **0,391 MiB** | **0,391 MiB** | 11 | 4 | 6 | ~2,4 ms |
| 64 líneas | 51 200 | 0,195 MiB | 0,195 MiB | 15 | 5 | 8 | ~2,4 ms |
| 32 líneas | 25 600 | 0,098 MiB | 0,098 MiB | 15 | 6 | 8 | ~2,4 ms |

Framebuffer físico: **idéntico byte a byte** en los 5 tamaños, en las 8 fases
(`tests/check_drawbuf_partial.sh`). Contrato de flush: `lv_disp_flush_is_last`
verdadero exactamente una vez por ciclo, ningún rect fuera del panel físico,
cada píxel flusheado una sola vez, `covered == flushed` en todas las fases.

Tiempos p50 (3 corridas de 300 repeticiones, mediana): pantalla completa
2,37–2,46 ms en los 5 tamaños; espectro 0,82–0,85 ms; waterfall 1,23–1,26 ms.
**No hay regresión de tiempo medible**: la diferencia entre tamaños está dentro
del ruido del host. (La corrida inicial de cada proceso es más lenta por
warm-up; se descarta.)

### Ahorro del candidato elegido (128 líneas)

- **Reservado**: 6 144 000 → 409 600 B ⇒ **−5,469 MiB (−93,3 %)**.
- **Residente**: 1 536 000 → 409 600 B ⇒ **−1,074 MiB (−70,0 %)**. El ahorro
  físico es ~1 MiB, no los 5,5 MiB de la reserva: el resto del buffer actual
  nunca se tocaba.
- **Flushes** (costo por llamada, el área total copiada no cambia): espectro
  igual (4), waterfall +1 (6), pantalla completa +1 (11).
- **Tiempo**: sin cambio medible en el host.

### Por qué 128 y no 32/64/full

- **32 y 64** bajan otros ~200–300 KiB de reserva, pero suben los flushes del
  waterfall (5→8) y del espectro (4→5/6): más pasadas de bandas ⇒ más
  `lv_mem_buf_get(LV_DISP_ROT_MAX_BUF)` (160 KiB) por frame en el hot path del
  espectro/waterfall, que corre a ~10-30 fps. El ahorro extra no lo justifica.
- **Pantalla completa (480)** es el control más conservador: mismo conteo de
  flushes que el actual y −75 % de reserva, pero deja 1,07 MiB residentes que
  128 líneas sí liberan.
- **128** es el equilibrio medido: reserva −93,3 %, residente −70 %, el
  espectro con los mismos flushes que hoy y sin regresión de tiempo.

### Cambio aplicado

`src/main.c`: `DISP_BUF_LINES 128` + `DISP_BUF_SIZE (800 * DISP_BUF_LINES)` +
`_Static_assert(DISP_BUF_SIZE >= 800, ...)` (un buffer parcial tiene que
sostener al menos una línea lógica, o `get_max_row()` devuelve 0 y LVGL no
dibuja nada). No se toca `lv_conf.h` de la ROM, la profundidad de color, el
framebuffer de hardware ni el gitlink de LVGL.

**Rollback**: cambiar `DISP_BUF_LINES` (128 → 480 → 1920) es una línea; volver
al valor actual es `#define DISP_BUF_LINES 1920`. Sin migración ni estado.

### QEMU ARM

_(se completa con el resultado del banco; ver más abajo)_

### A/B + contratos

_(se completa; ver más abajo)_

