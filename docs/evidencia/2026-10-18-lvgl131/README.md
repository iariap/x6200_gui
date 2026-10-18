# Evidencia — #131: el hilo de radio tocaba LVGL con las teclas del panel (`hkey_put`)

**Fecha**: 2026-10-18 · Rama `factory/lvgl-events-131` · base `4e000bca` (trunk `lu2dsx/x6200-rom`)
**HEAD de código**: `7f36f607` — *hkey_put() se difiere al hilo de LVGL (radio_tick lo llamaba desde el hilo de radio)*
**Rebase**: la corrida nació sobre `94c2c6ab`; se rebasó sobre el trunk `4e000bca` (#143, la línea
central del waterfall). El `src/hkey.c` del fix y sus tests son **el mismo árbol** que en `5e425df7`
(`git diff 5e425df7 7f36f607 -- src/hkey.c tests/native/hkey_stubs.c tests/native/hkey_stubs.h
tests/native/test_hkey.c` vacío): cambia el padre, no el fix. Los gates que se re-corrieron sobre el
árbol rebaseado están marcados **[rebase]** en la tabla de la sección 5; los que quedan con la base
vieja están marcados y explicados.
**Binario ARM probado en el banco**: `md5 9042691874a754782601624cd4c4acc3` (14.293.044 B, ELF32
ARM) — del árbol **pre-rebase** `5e425df7`
**Radio**: no accesible durante la corrida. **No hubo prueba en hardware**: lo de abajo es
estático + nativo (host) + banco QEMU (`cortex-a7`) con el binario real del commit. La validación
dinámica del fix es **`requires-radio`** (receta en la sección 6).

## 1. El síntoma medido

Tercer `SIGSEGV` con la misma firma (issue #131, reabierto). El `CRASHLOG` de la radio:

```
sig=11 fallo=0x15 pc=0xb6186ae8 lr=0xb618d4f0
stack: b618db48 00000000 b6196128 ...
```

Con `liblvgl` `cd30ca6c58ad` y base de carga `0xb6170000` (resuelta en el #137), `pc` cae en
`_lv_event_mark_deleted+0x30` y `lr` en `lv_obj_destructor+0x10`; la pila resuelve
`obj_del_core -> _lv_obj_destruct -> lv_obj_destructor -> _lv_event_mark_deleted`. En
`lvgl/src/core/lv_event.c`, `event_head` es un estático (línea 37) que **sólo** se asigna dentro
de `lv_event_send` (líneas 72 y 78) y que `_lv_event_mark_deleted` recorre sin lock (líneas
151-158): la lista de eventos de LVGL quedó apuntando a un frame de pila muerto.
`fallo=0x15` es la lectura de `r3+4` con `r3` ya corrupto. Contexto del log:
`on_high_filter_change: Radio set filter_high=3000`; los dos crashes previos tienen `pc`/`lr`
idénticos con otro contexto (`on_vfo_mode_change`).

## 2. Auditoría de observers no diferidos (la hipótesis del ticket)

El ticket proponía que un observer **no diferido** del camino filtro/banda/modo tocara LVGL. Se
auditaron todos los `subject_add_observer*` y `->subscribe` sin `ObserverDelayed` de ese camino:
**ninguno llama a LVGL**. Todos copian escalares, escriben SQLite o hablan con la SDK de la radio.

| Registro (código) | Observer | Hilo | Toca LVGL? | Qué toca |
|---|---|---|---|---|
| `src/cfg/band.c:109-125` | `on_ab_*`, `on_band_id_change`, `on_vfo_change`, `on_cur_mode_change` | el que llama a `set()` | **no** | guarda/carga params en SQLite y copia valores a los sujetos `cfg_cur.*` |
| `src/cfg/mode.c:65-91` | `on_cur_mode_change`, `on_cur_filter_*`, `update_cur_low/high_filter`, `update_real_filters` | el que llama a `set()` | **no** | recalcula low/high/bw y filtros reales; SQLite al cargar |
| `src/cfg/cfg.c:210,409-423` | `on_item_change`, `update_sql_level`, `update_fft_width` | el que llama a `set()` | **no** | item en params/SQLite y escalares |
| `src/cfg/atu.c:66-68` | `update_atu_network` | el que llama a `set()` | **no** | cache en RAM, SQLite y sujetos `atu_network.*` |
| `src/radio.c:505-525` | `on_vfo_*`, `on_low/high_filter_change` | el que llama a `set()` | **no** | `x6200_control_*` (SDK) y `LV_LOG_USER` |
| `src/spectrum.c:317-328`, `src/waterfall.c:92-109`, `src/band_info.c:158`, `src/dsp.cpp:152-153`, `src/cw.cpp:85-90`, `src/cat.cpp:1020` | observers de ancho de FFT, filtro real, modo, grilla y frecuencia | el que llama a `set()` | **no** | escalares, buffers de PSD/waterfall y la cola del CAT |

El mecanismo (`src/cfg/subjects.cpp:169-173`) notifica en el hilo que llama a `set()`, sin lock
sobre la lista de observers; por eso la auditoría se hizo observer por observer y no por el
mecanismo. En esta app esos `set()` del camino banda/filtro/modo llegan del hilo de radio
(`radio_tick`/SDK), del hilo de LVGL (diálogos, botones, rueda) o del hilo del CAT: la columna que
decide es **toca LVGL?**, y en las 6 filas la respuesta es no. **La hipótesis literal del ticket
no se sostiene**; se reporta como tal.

## 3. El defecto que sí se fundó

En el mismo camino de teclas del panel: `hkey_put()` (base `4e000bca`, `src/hkey.c:44` — el mismo
cuerpo que en `94c2c6ab`, el trunk no tocó `src/hkey.c`) lo llama
`radio_tick()` **desde el hilo de radio** (`src/radio.c:290`, `hkey_put(pack->hkey)`) y su cuerpo
tocaba LVGL ahí mismo:

- `lv_group_get_editing()` para las teclas numéricas;
- `event_send_key()` (`src/events.c:340-352`), que lee `lv_group_get_focused()`;
- `backlight_tick()` (`src/backlight.c:60-80`): `lv_timer_create`, `lv_timer_reset`,
  `lv_timer_set_period`, `lv_timer_set_repeat_count`;
- el timer del long-press (`lv_timer_del/create/set_repeat_count`) y la mutación de
  `event.state`/`timer`, compartidos con el hilo de LVGL.

`hkey_put()` tiene un solo llamador en `src/` (`grep -rn "hkey_put" src` → `radio.c:290`). El
resto de `radio_tick` ya estaba diferido: `msg_schedule_text_fmt`, `notify_tx`/`notify_rx`
(`event_send`, cola con mutex), `tx_info_update`, `dialog_swrscan_update` y
`apply_visual_profile` (`scheduler_put*`); `clock_update_power` y `visual_policy_eval` no tocan
LVGL; `dsp_samples` es DSP.

## 4. El fix

Commit `7f36f607` (rebase de `5e425df7` sobre el trunk), sólo `src/hkey.c` + tests:

- `hkey_put()` sólo publica la tecla con `scheduler_put(hkey_entry, &key, sizeof key)`
  (`scheduler_put` copia el argumento; el cuerpo corre en el hilo de LVGL vía `scheduler_work()`).
- El cuerpo pasa a `hkey_process()` y `hkey_entry()` lo envuelve.
- `atomic_bool key_armed`: lo prende el productor al publicar una tecla y lo apaga el consumidor
  al procesar el release. Así los `0` que manda el flow cuando no hay tecla apretada no encolan
  trabajo por tick, sin cambiar la máquina de estados.
- `hkey_event()` chequea el `malloc` (lo marcaba `-fanalyzer` en el archivo tocado).
- No cambia nada visible (A/B pixel-idéntico).

## 5. Verificación (gates)

| Gate | Resultado | Evidencia |
|---|---|---|
| TDD RED (base `src/hkey.c`) | **18 fallos / 92 comprobaciones**, exit 1 | `tdd-red.log` |
| TDD GREEN (fix) | **0 fallos / 92 comprobaciones**, exit 0 | `tdd-green.log` |
| Suite nativa completa + `-fanalyzer` **[rebase]** | exit 0 (bloque HKEY nuevo incluido; los tests del trunk — autotune, temas — también) | `native-tests.log` |
| Estáticos de la app (8) + `bash -n` **[rebase]** | 8/8 ok, `bash -n` ok | `check-matrix.log` |
| Determinismo del arnés de UI **[rebase]** | PASS (PNG idénticos por fixture; header `revision 7f36f607`) | `determinism.log` |
| Contraste de temas **[rebase]** | OK (10 paletas, 90 pares, 0 fallos; +2 objetos `waterfall_center_line` del #143) | `themes.log` |
| A/B `AB-LVGL131` (`4e000bca` vs `7f36f607`) **[rebase]** | **PASS**: 87 escenarios **pixel-idénticos**, 893 chequeos de contrato; Hardware/Energy BLOCKED (advisory) | `ab-lvgl131.log` |
| Cross-compile ARM (toolchain buildroot) | build completo, `100% Built target x6200_gui` (`make[1]: Leaving directory '/build'`); únicos warnings: 3 preexistentes en `src/widgets/lv_waterfall.c` (no tocado). **Árbol pre-rebase** | `arm-cross-build.log` |
| Identidad del binario probado en el banco | ELF32 ARM, 14.293.044 B, `md5 9042691874a754782601624cd4c4acc3`. **Árbol pre-rebase `5e425df7`** (no se repitió sobre el rebase) | `banco-smoke.log` |
| Banco QEMU (ARM emulado) — smoke | OK, `banco-home.png` 26.229 B / 359 colores. **Árbol pre-rebase** | `banco-smoke.log`, `banco-home.png` |
| Banco QEMU — secuencia banda/filtro/modo | **95 órdenes, 0 errores**, GUI viva, framebuffer 8.388.608 B, **sin `crash.txt`**. **Árbol pre-rebase** | `banco-secuencia.log` |
| Matriz de la app (`check-matrix.sh 4e000bca factory/lvgl-events-131`) **[rebase]** | exit 0; banco **BLOCKED** sólo por rutas por defecto del runner (se corrió aparte) | `check-matrix.log` |

Los gates marcados **[rebase]** se re-corrieron sobre el árbol rebasado. Los de ARM y banco son del
árbol pre-rebase `5e425df7`: el `src/hkey.c` del fix es idéntico en `7f36f607`, pero la base cambió
(el trunk toca `src/theme.c`, `waterfall.c`, `cw.cpp`), así que su binario y su `md5` son de aquel
árbol; repetirlos pide el host del owner (chroot + buildroot + volumen). El A/B sí se re-generó
(host, Docker) con los ids nuevos y es el gate de no-regresión visual del rebase.

Notas de entorno (para reproducir): el árbol fuente de buildroot y el `CHROOT`/`BUILDROOT` por
defecto no existen en el worktree, así que `make x6200-gui-rebuild` / `compila-gui.sh` fallan tal
cual (`arm-build.log`). El cross-compile de arriba se hizo con el toolchain del volumen
(`/home/pablo/desa/x6200/AetherX6200Buildroot`) en un build aislado, y el banco con
`CHROOT=/home/pablo/desa/x6200/.vfix3`, `BUILDROOT=...` y `RECONSTRUIR=no`. El binario del
chroot y el del volumen quedaron en su md5 original (`ccaa60d9431590bf06d1349369a9e006`).

## 6. `requires-radio` — la validación que el banco no puede hacer

El banco no tiene *flow*, así que **no ejecuta `radio_tick()` ni `hkey_put()`**: el fix se validó
en su mecanismo (test nativo con hilos registrados), no en el camino real. Con el binario del
commit `7f36f607` (el rebasado) desplegado:

1. desplegar con `tools/prueba-gui-en-radio.sh` (pasa antes por `verifica-firmware-port.py`) y
   confirmar el `md5` que corre. Ojo: el `9042691874a7` que figura arriba es el binario del árbol
   **pre-rebase** `5e425df7`; el del árbol rebasado sale del mismo build y hay que leerlo del
   `banco-smoke.log`/`arm-cross-build.log` de la corrida que lo construya;
2. con la GUI en home y la radio en RX, repetir **200 veces** este ciclo sin tocar el control
   remoto: **FIL** (abre/cierra el filtro) → **rueda TUNE** para mover el filtro bajo y alto →
   **MODE** (LSB → USB → CW → AM → LSB) → **BAND** / app Bands; entre paso y paso, **F1** y
   **F2** (las dos van por `hkey_put`);
3. cada **20 ciclos** verificar `CRASHLOG` **0** en el log vivo y que `pidof x6200_gui` no cambió
   (si cambió, la GUI se relanzó);
4. cerrar con `tools/verifica-radio.sh` (sin señales de crash) y leer `/mnt/lastbreath`: tras el
   apagado ordenado la línea tiene que mostrar `clean=1`; un `LASTBREATH` al arrancar significa
   corte sucio.

## 7. Residual (Anexo A de `docs/HALLAZGOS.md`)

Se fundó el **acceso a LVGL fuera de hilo** y se eliminó. **No** se fundó el paso exacto por el
que ese acceso termina corrompiendo `event_head`: la corrupción necesita concurrencia real y el
banco no corre `radio_tick`. Tampoco se fundó que el trigger sea *sólo* el cambio de filtro (los
crashes comparten `pc`/`lr` con contextos distintos). Queda como hipótesis en el anexo, no como
hallazgo.

## Archivos de esta evidencia

| Archivo | Qué es |
|---|---|
| `tdd-red.log` | suite nativa con el `src/hkey.c` de la base: 18/92 fallos |
| `tdd-green.log` | suite nativa con el fix: 0/92 fallos |
| `native-tests.log` | suite nativa completa del worktree + `-fanalyzer` |
| `check-matrix.log` | matriz de la app (8 estáticos, `bash -n`, banco BLOCKED) |
| `determinism.log` | gate de determinismo del arnés de UI |
| `themes.log` | contraste WCAG de los temas |
| `ab-lvgl131.log` | resumen del A/B `AB-LVGL131` (reporte completo en `tools/ab_testing/reports/AB-LVGL131/`) |
| `arm-cross-build.log` | cross-compile ARM completo (toolchain buildroot) |
| `arm-build.log` | intento del `compila-gui.sh` de la app, bloqueado por falta de buildroot |
| `banco-smoke.log`, `banco-home.png` | smoke del banco QEMU con el binario del commit |
| `banco-secuencia.log` | 95 órdenes de banda/filtro/modo, 0 errores, sin `crash.txt` |
