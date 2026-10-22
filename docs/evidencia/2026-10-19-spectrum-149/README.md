# #149 — Snapshots del espectro sin carreras (evidencia)

Corrida `factory/lvgl-opt3-149`. **Base**: `a69ea288` (trunk). **Commit de código**: `75a48b89`
(`Spectrum: snapshot por frame y sincronizacion del productor (#149)`). Alcance: `src/spectrum.c`,
`src/scheduler.h/.cpp` (valor de retorno) y el test nativo nuevo.

## 1. Hilos y locks, con líneas (lo que el issue pedía verificar)

El hallazgo estático del issue **se confirma**: escritor y lector corren en hilos distintos.

| Rol | Hilo | Camino | Evidencia |
|---|---|---|---|
| Escritor | `radio_thread` (pthread creado en `src/radio.c:614`) | `radio_tick` → `dsp_samples` (`src/dsp.cpp:168`) → `spectrum_data()` | `src/radio.c:223` llama `dsp_samples()` desde el hilo de radio |
| Lector | hilo principal (GUI/LVGL) | `lv_timer_handler()` → `spectrum_draw_cb` (`src/spectrum.c:85`) | `src/main.c:613` corre el lazo `scheduler_work(); lv_timer_handler();` |
| Refresco | hilo principal | `scheduler_put_noargs(spectrum_refresh)` → `spectrum_refresh` → `lv_obj_invalidate` | `src/main.c:613` drena el scheduler en el hilo de LVGL |
| Otros escritores | hilo de GUI (settings/banda), hilo de CAT/remoto | `spectrum_clear()` (`src/main_screen.c:721,877,978,1042`), observers de `cfg_cur` (no diferidos: corren en el hilo que hace `set()`) | `src/cfg/subjects.cpp:29-35` (`SubjectT::set` notifica en el hilo que llama) |

Locks en la base (`a69ea288`): un solo `pthread_mutex_t data_mux` (`src/spectrum.c:64`, inicializado en
`:302`) tomado **únicamente** por `spectrum_data()` (`:335`). El lector no lo toma nunca:
`spectrum_buf[sp_ind]` (`:138`), `spectrum_buf[i]` (`:141`), `spectrum_peak[i].val` (`:158`),
`spectrum_tx` (`:93`), `grid_min/grid_max` (`:98-99`), `width_hz` (`:196`), `filter_from/to` (`:201-202`),
`dnf_*` (`:212-219`), `cur_mode` (`:268`). Y `spectrum_clear()` (`:398-402`) reescribe los buffers sin
lock, igual que `on_cur_freq_change()` (`:452-462`), `spectrum_update_min/max()` (`:376-391`) y los
observers de grilla (`:422-431`).

**Confirmado con ThreadSanitizer sobre el módulo de la base** (mismo test, mismo stub;
`tsan-base.log`): 7 carreras reportadas, todas sobre los globales `spectrum_buf` / `spectrum_peak` /
`spectrum_tx`, con estos sitios:

```text
Write spectrum_data  src/spectrum.c:336 (spectrum_tx) / :338 (spectrum_buf)   [hilo de radio]
Write spectrum_clear src/spectrum.c:399 (spectrum_buf)                        [hilo de config]
Read  spectrum_draw_cb src/spectrum.c:93 (spectrum_tx) / :141 (spectrum_buf)  [hilo de GUI]
SUMMARY: ThreadSanitizer: data race /sp/redbase/spectrum.c:141 in spectrum_draw_cb
Location is global 'spectrum_buf' of size 3200
```

Con el módulo arreglado el mismo binario bajo TSan termina **sin reportes**
(`native-tests-sanitizers.log`). Cobertura declarada: TSan corrió en **host x86_64** (gcc 13, imagen
`x6200-uiharness`); el binario ARM **no** corre bajo TSan en este arnés, así que estos resultados no se
atribuyen al Cortex-A7 — es el mismo fuente el que se compila para ARM.

## 2. Diseño elegido: snapshot por frame (y por qué no doble buffer)

`spectrum_frame_t` agrupa **todo** el estado que el dibujo lee (800 muestras, 800 picos, `grid_min/max`,
`width_hz`, `filter_from/to`, `dnf_*`, `mode`, `tx`). El productor lo escribe **solo** bajo `data_mux`; el
callback de dibujo lo copia entero a un `snapshot` privado del hilo de GUI y dibuja de ahí:

```c
/* src/spectrum.c:113-120 (fix) */
pthread_mutex_lock(&data_mux);
snapshot = frame;              /* ~16 KB, un memcpy */
pthread_mutex_unlock(&data_mux);
const spectrum_frame_t *f = &snapshot;   /* el dibujo queda FUERA de la sección crítica */
```

- **No se sostiene el mutex durante las 800 `lv_draw_line`**: se toma solo para la copia (el test lo
  prueba de forma determinista: con el dibujo detenido en la línea 700, `spectrum_data` +
  `spectrum_clear` + `spectrum_get_min` de otro hilo **completan**; si el callback sostuviera el lock, la
  pausa vencería y el caso fallaría).
- **Doble buffer descartado** (se evaluó, era la opción B del issue): el lector puede quedarse con el
  frame durante un redibujo de ECO (1 de cada N refrescos), así que un swap de punteros necesitaría un
  tercer buffer o un handshake de ownership con descarte de frames; a cambio ahorraría una copia de
  ~16 KB por frame dibujado (0,7-4,9 µs medidos en el host para el tramo equivalente del productor). Más
  protocolo, no menos: se eligió la copia.
- **Snapshot en el draw, no en el refresco**: en ECO los datos siguen llegando a 25 Hz y el dibujo ocurre
  1/N; copiar en el refresco haría N copias de más por frame dibujado.
- `spectrum_clear()`, los observers de config (grilla, filtro real, DNF, modo, ancho de FFT),
  `spectrum_get_min()`, `spectrum_update_min/max()` y `on_cur_freq_change()` (desplazamiento de picos al
  mover el dial) entran al mismo lock; `spectrum_tx` dejó de ser un flag suelto y viaja en el frame.
- `visor_height` (eventos TX/RX) queda fuera del frame a propósito: lo escribe y lo lee el mismo hilo de
  LVGL (`event_send` encola, `event_obj_check` drena ahí).
- **LVGL solo desde el hilo de GUI**: el productor sigue usando `scheduler_put_noargs(spectrum_refresh)`;
  el test lo pinnea (`spectrum_stub_foreign_lvgl == 0` con el productor en otro hilo).
- **Bounds**: `spectrum_data()` recorta `size > SPECTRUM_SIZE` (antes escribía fuera del buffer; ver
  `tdd-red-asan.log`: `runtime error: index 800 out of bounds for type 'float [800]'`, y en el build del
  test la escritura pisaba el objeto `.bss` contiguo y el proceso moría con SIGSEGV).
- **Backlog**: a lo sumo un `spectrum_refresh` encolado (latest-frame). Si el scheduler rechaza el trabajo
  (cola llena), el productor **devuelve el ticket** y el frame siguiente lo reintenta: por eso
  `scheduler_put()`/`scheduler_put_noargs()` ahora devuelven si el trabajo quedó encolado (los callers
  actuales siguen ignorando el valor; se ajustaron el mock del harness y el stub del test de hkey).

## 3. Pruebas (salidas reales)

`tests/native/test_spectrum.c` + `stubs_spectrum/`: el **módulo real** contra stubs que registran cada
primitiva de dibujo (color + geometría) y el hilo que la emitió, con una **pausa determinista** en la
línea 700 de la traza para intercalar al productor sin depender del timing.

| Caso | Qué pinnea | Base (RED) | Fix (GREEN) |
|---|---|---|---|
| `producer_defers_lvgl…` | el productor no toca LVGL; el refresco queda encolado | ok | ok |
| `draw_is_a_single_generation` | valores + picos + metadatos de UNA generación | `A=351 B=449` en la misma traza, rect del filtro de B | 800 líneas A, 800 picos A, rect de A |
| `clear_during_draw…` | `spectrum_clear()` concurrente | `A=701 limpiado=99` | traza entera de A |
| `draw_after_clear…` | frame limpiado + datos nuevos a mitad de dibujo | `limpiado=701 B=99` | traza entera limpiada |
| `producer_progresses_while_paused` | el lock no cubre el dibujo | ok | ok (y `< 500 ms`) |
| `refresh_backlog_is_bounded…` | coalescing + ticket devuelto si la cola está llena | `50 refrescos encolados (pico 50)` | 1 encolado, y rearma tras el rechazo |
| `oversized_frame_is_clamped` | bounds | UBSan: `index 800 out of bounds` | 800 muestras recortadas |
| `concurrent_stress…` | 120 dibujos con productor + config en paralelo | `120 de 120 dibujos con generaciones mezcladas` | 0 mezclados |

- `native-tests.log`: suite nativa completa del host — **65 comprobaciones, 0 fallos** (sin sanitizers en
  este host: no están las runtimes de gcc 16.2.1 Fedora; el script lo informa y sigue).
- `native-tests-sanitizers.log`: la misma suite dentro de `x6200-uiharness` (ubuntu, gcc 13) — ASan+UBSan
  **65/0**, TSan **65/0 sin reportes**, y el fixture de carrera deliberada reportando (ver
  `tsan-fixture.log`).
- `tdd-red.log` / `tdd-red-asan.log` / `tsan-base.log`: el RED contra el `spectrum.c` de la base.
- `-fanalyzer` sobre el módulo staged: sin observaciones.

Perfil del host (x86_64, `perfil host:` en `native-tests.log`): dibujo del stub 22,7 µs/dibujo; **lock
visto por un lector concurrente (`spectrum_get_min`) 14,8 µs máximo**; `spectrum_data` + refresco
0,9 µs/frame. En el radio el dibujo son 800 `lv_draw_line` (los 132 000 px por frame del banco natural),
así que el lock (la copia) queda dos órdenes por debajo.

## 4. A/B y determinismo

- `ab-compare.log` / `ab-compare.md` — `x6200-ab compare --a a69ea288 --b 75a48b89`: **PASS**, 87
  escenarios pixel-idénticos, 893 chequeos de contrato, 0 defectos nuevos. Reporte completo en
  `tools/ab_testing/reports/AB-OPT3-149/`.
- `ab-benchmark.log` + `ab-benchmark-control.log` — `x6200-ab benchmark`: **p50 mediana +0,1 %** (máx
  +8,4 %, 0/87 >25 %), draw medio mediana +2,2 %, y los **proxies deterministas (flushes, área
  redibujada) idénticos**. El p95 del host (mediana +6,9 %) **no es evidencia**: el control A/A (dos
  corridas de la MISMA revisión) da p95 de −42,8 % a +50,7 % con 31/87 escenarios por encima del 25 %,
  mientras el p50 no se mueve. Reportes en `tools/ab_testing/reports/BENCH-OPT3-149{,-B1,-B2}/`.
- `determinism.log`: PNG idénticos en dos corridas de los 87 fixtures.
- `natural.log`: banco natural con el perfil **sin cambios** (espectro 132 000 px × 27 frames, total
  11 223 840 px, mismo sha256 de captura): el trabajo de render invalidado es exactamente el mismo.
- `audit` (11 known issues: 0 reproducidos, 9 sin verificar; inspección estructural real sobre 9636
  objetos en 87 escenarios: 0 hallazgos) y `contracts` (893 chequeos, 0 fallos) — salidas en
  `ab-audit-contracts.log`.

## 5. QEMU / banco (ARM real, `qemu-arm-static`, cortex-a7)

- `banco-smoke.log`: rebuild ARM desde el commit, `make x6200-gui-dirclean` + `x6200-gui-rebuild`,
  `fuente: md5 13b4c4321a5b`, `binario: md5 b5ac3b029818`, gate del port 3/3 marcas, captura de 331
  colores (`banco-home.png`).
- `banco-apps.log` + `banco-apps-barrido.json`: barrido de las 23 apps → **23/23 vivas, 0 falladas, 0
  crashes**. (El primer barrido marcó 19 apps como "muertas": en 21 de los 42 intentos el contenedor del
  banco no existía —"No such container: banco-apps"— y el script, por diseño, guarda el peor intento. El
  barrido limpio es la evidencia.)
- `banco-secuencia.log`: 106 órdenes de banda/filtro/modo/frecuencia/AGC/pre + los 5 botones de la barra;
  todas confirmadas por el estado (`SIN CONFIRMAR` solo en los `btn` 5-11, que el firmware rechaza con
  `Invalid value` porque la barra tiene 5 botones).
- `banco-crash.txt`: el banco registró **SIGABRT** en libc durante la corrida. **No es de este cambio**:
  es el defecto ya documentado en `docs/evidencia/2026-10-14-caidas-aleatorias/README.md` (fortify de
  glibc: `*** buffer overflow detected ***: terminated`, camino de audio `cw_put_audio_samples`), con la
  **misma firma** (PC/LR con el mismo delta de 12 B, `r2=6`). La prueba cruzada es del mismo día: el
  chroot del run `factory/cb-27mhz-146` (`.vfix-cb146/crash.txt`, 01:00, otro binario) tiene exactamente
  el mismo `PC: 0x40b36dd4 LR: 0x40b36dc8 r6=40a42000 r7=0000010c SP: 0x40800ac8`. Tampoco lo cubre este
  issue (el #149 no toca audio ni DSP).
- Límites del banco: QEMU no es el Cortex-A7 (ni RF ni batería) y el espectro del banco no tiene datos de
  radio (no hay FFT), así que las capturas de espectro con señal/picos son del arnés de UI, no de QEMU.

## 6. PENDIENTES honestos

1. **TSan es de host x86_64**, no del ARM: el módulo es el mismo fuente, pero la corrida ARM no lleva
   sanitizer (el arnés no lo permite). Queda como evidencia del código de sincronización, no del binario
   de la radio.
2. **El callback de dibujo puede correr más de una vez por refresco** (LVGL dibuja por áreas
   invalidadas): cada pasada es consistente por sí misma, pero dos pasadas de un mismo refresco pueden
   usar snapshots distintos. Es el mismo comportamiento que antes (peor: antes podía mezclar dentro de
   una pasada). No se instrumentó el caso multi-pasada.
3. **Costo de la copia**: +16 KB de memcpy por frame dibujado (1/N en ECO). Medido en el host (0,7-4,9 µs
   el tramo equivalente del productor; 14,8 µs máx el lock visto por un lector concurrente) y sin efecto
   en los proxies deterministas del banco natural. En el Cortex-A7 no se midió.
4. **`params` sigue fuera del snapshot** (por ejemplo `params.spectrum_peak`): lo lee el productor y el
   dibujo, y lo escribe Ajustes en el hilo de LVGL. Es preexistente, ajeno al frame del espectro y no
   entra en el alcance del issue.
5. **Validación en radio**: pendiente (requires-radio). El banco no tiene FFT ni audio real.
