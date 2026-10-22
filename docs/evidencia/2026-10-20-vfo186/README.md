# #186 — Sintonía adaptativa determinista del dial principal + `Accel threshold` + Settings en inglés

Corrida `factory/vfo-accel-186`. **Base**: `60b9f5b2` (`lu2dsx/x6200-rom`, HEAD observado al
redactar el issue). **Rama de la corrida**: `factory/vfo-accel-186` (los commits de código son
`64a3496b`, `7cfade9b` y `7ee5b5cf`; el HEAD exacto lo fija el PR).
Un worktree, un writer, un PR. **Sin push en esta corrida** (lo hace el orquestador).
**La validación física queda fuera**: la prueba en radio real la hace el owner después del merge.

El issue #186 es un contrato cerrado (§0–§7): no hubo fase de diseño ni se sustituyó la curva.
Los multiplicadores, umbrales, histéresis, freno, reverso, cola y rutas son exactamente los
pedidos; los ×0,1 y ×100 quedaron retirados junto con sus tests.

## 1. Qué se implementó

| Área | Archivos | Qué |
|---|---|---|
| Motor puro | `src/adaptive_tuning.[ch]` | API `init/reset/detent` + `weighted_event`, `segments`, `segments_ex`, `thresholds`, `multiplier`, `target_freq`, `threshold_sanitize`. Velocidad real en **detents/s**, multiplicadores fijos, umbrales derivados de `T`, suavizado 0,35/0,65, histéresis 0,80×, arranque, freno y reverso. |
| Cola | `src/rotary_queue.[ch]` (nuevo) | 512 entradas, mutex + condvar POSIX, **backpressure** (espera, nunca descarta), `high_watermark`/`full_waits`, marca de discontinuidad (`diff == 0`). POSIX puro, sin LVGL: testeable en host. |
| Productor/consumidor | `src/rotary.[ch]` | Timestamps reales del `input_event` en µs (`EVIOCSCLOCKID(CLOCK_MONOTONIC)`, fallback + reset), estado por instancia en `rotary_t`, drenaje → tramos → `lv_event_send` con payload en stack. Sin LVGL/malloc desde el hilo lector. |
| Aplicación al VFO | `src/main_screen.c`, `.h` | `delta = weighted_detents × step` en **int64**, sin `align_int()`, validación `radio_check_freq()`, `freq_shift(±1)` exacto, CB con `cb_step_channel()` + `raw_diff` + indicador oculto, resets de contexto y `main_screen_tuning_reset()`. |
| Parámetro | `src/params/params.[ch]` | Solo `freq_accel_threshold` (uint8 3..15, default 7, voz `"Acceleration starts at"`), load/save junto a `freq_accel`, saneo a 7 + `dirty` que ahora arma el temporizador de guardado (`params_unlock`). Sin migración SQL. |
| Settings | `src/dialog_settings.c` | Fila `Accel threshold` (spinbox 3..15 paso 1, sufijo `detents/s`) justo debajo de `Freq acceleration`; aplica en el siguiente movimiento (reset, sin reboot). Normalización al inglés de todo Settings + voz propia. |
| Voz | `src/voice.[ch]` | `voice_say_settings_fmt()` (inglés, sin diccionario), `voice_settings_begin/end()` para que también las locuciones genéricas de `params_*_set` salgan en inglés mientras Settings está abierto. |
| Build | `src/CMakeLists.txt` | `rotary_queue.c` en el target de la GUI (lo detectó el gate ARM/ROM). |
| Arneses | `tests/native/*`, `tools/ui_harness/*` | Tests nuevos + registro; mock de `params` con el default; payload síncrono; `radio_check_freq` del mock valida rango (como `src/radio.c`), no igualdad. |

## 2. Motor: contrato implementado

- **Multiplicadores fijos** (nivel 0..4): None `×1` siempre; Lite `×1/×2/×3/×6/×10`;
  Strong `×1/×3/×6/×12/×24`. Se eliminaron `×0,1` y `×100` y sus tests.
- **Umbrales derivados de `T`** (`T` = `freq_accel_threshold`, 3..15, default 7):
  `T1 = T`, `T2 = ceil(15T/7)`, `T3 = ceil(30T/7)`, `T4 = ceil(50T/7)`. Con `T=7`: `7/15/30/50`.
- Velocidad `v = min(1000, 1e6/dt_us)`, `dt_us ≥ 1000`; **subida** `0,35v + 0,65s`,
  **bajada** `s = v`; salida de nivel por debajo de `0,80 × Tn`.
- Primer detent tras reset = `×1`; dos intervalos de calentamiento con tope nivel 1;
  freno `dt ≥ 180 ms` → `×1`; reverso → primer detent `×1`; `|value| > 1` partido en detents
  con tiempos interpolados; misma marca temporal → 1000 µs virtuales sin acelerar.
- Todo el estado vive en el consumidor (hilo GUI). El productor solo encola `diff + timestamp_us`.

## 3. Tabla antes/después — 5 trazas fijas

Números reales de los drivers `trazas/before_trace.c` (motor retirado de `60b9f5b2`, emulando su
consumidor: `dt` medido por lectura, timestamps sintéticos `now − dt·(ticks−1−k)` y `align_int()`)
y `trazas/after_trace.c` (motor nuevo, timestamps reales en µs, int64 sin redondeo).

| Traza | Perfil / paso | **Antes** (base `60b9f5b2`) | **Después** (#186) |
|---|---|---|---|
| **Lento**: 1 detent, 400 ms | Lite, 10 Hz | `×0,1` → **+1 Hz** → 14.200.001 | `×1` → **+10 Hz** → 14.200.010 |
| **Rápido**: 10 detents @14 ms | Strong, 100 Hz | `×0,1` los 4 primeros y luego `×100` (tope) → 14.260.000 | tabla fija con calentamiento, `weighted = +175` → 14.217.500 |
| **Freno**: 8 @14 ms + 1 @180 ms | Strong, 100 Hz | el detent de freno sigue `×100` → 14.250.000 | freno `×1` → 14.212.800 |
| **Reversa**: 8 @14 ms + 1 inverso @14 ms | Strong, 100 Hz | el inverso sigue `×100` → 14.230.000 | inverso `×1` → 14.212.600 |
| **GUI con lag**: 5 @14 ms + stall + 4 @14 ms | Strong, 100 Hz | lote `×100` → 14.290.000, **distinto** del caso 1-por-frame (14.260.000): el resultado depende del lote/carga | `weighted = +100` → 14.210.000; el test de batches prueba **bit a bit** que 1-a-1, 4-por-read y todo-junto dan lo mismo |

La traza "GUI con lag" del "antes" es una **emulación fiel del consumidor retirado** (el stall entra
en la lectura siguiente del productor y el GUI drena el lote junto); el "después" es el motor real
del issue. La dependencia de lote/carga está documentada en §0 del contrato y ahora está cubierta
por el test `test_batches_are_bit_identical`.

## 4. Settings

- Fila nueva `Accel threshold`: spinbox **3..15**, paso 1, sufijo `detents/s`, valor `07` (formato
  de 2 dígitos, igual que Día/Mes), justo debajo de `Freq acceleration`, en `GRP_OPERACION`.
  Esquema único de grid del #175 (`COL_CONTROLES/SPAN_2A` + `COL_2B/SPAN_2B`), en el grupo de foco
  (`dialog_item`), sin scroll horizontal ni superposición. `check_settings_focus` y
  `check_settings_grid` pasan.
- Cambiar el spinbox guarda con `params_uint8_set()` (anuncio por la voz de Settings) y llama
  `main_screen_tuning_reset()`: el primer detent posterior es `×1`, sin reboot.
- **Idioma**: todo `dialog_settings.c` normalizado a inglés, incluidas locuciones. Verificación
  estática `tests/native/test_settings_language.c` (recorre cada string literal de
  `dialog_settings.c`, falla con acentos o palabras españolas) + auditoría dinámica del árbol LVGL
  en los 6 temas. Las únicas cadenas en español que quedan en el árbol de la pantalla de Settings
  pertenecen al **editor DFL inline** (#179/#182: `Esquema`, `Restablecer`), quedan **detrás** del
  diálogo (no son Settings) y están fuera del alcance de este issue.

### Capturas (seis temas) — `capturas/`

`settings-op*.png/json` (arnés host, con `--inspect`): en los seis temas la fila es visible,
`text_overflow=false`, `missing_glyphs=[]`, el spinbox está en el grupo de foco y muestra `07`.

### Banco QEMU (binario ARM real) — `banco/`

`01-home.png`, `02-settings-operation.png`: el binario ARM del port, **reconstruido limpio**
(`dirclean + rebuild`) desde este worktree, muestra `Accel threshold / 07 / detents/s` en
Settings → Operation. Gate #86 `verifica-firmware-port.py`: **3/3 marcas**. `crash.txt` no existe.

## 5. Tests §6 (salidas reales)

`tests/native/run_native_tests.sh` → **44391 comprobaciones, 0 fallos** (log `logs/native-tests.txt`).
Cubre, entre otros:

- **Un detent ±1** exacto en `step = 10/100/500/1000/5000` × None/Lite/Strong; nunca 0 ni 1 Hz.
- **Curva** 5/10/20/40/70 detents/s con `T = 3/7/15`; `T=7` reproduce exactamente `7/15/30/50`.
- **Histéresis 0,80×**, **reverso ×1**, **freno** (180 ms exactos, 250–400 ms siempre ×1), pausa.
- **Batches bit a bit**: 1-a-1, 4-por-read y todo-junto dan la misma lista de multiplicadores y la
  misma frecuencia final.
- **Cola**: FIFO, wrap, backpressure con 512+ y productor/consumidor concurrentes, sin pérdida,
  duplicado ni deadlock; `high_watermark`/`full_waits`.
- **No cruces**: miles de giros VOL/MFK interno no tocan VFO ni comparten estado.
- **Grilla**: 14.200.003 + `×1` con step 10 → **14.200.013**; límites int32 y `radio_check_freq`.
- **CB**: 1→2, 23→24 (baja la frecuencia aunque sube el número), 40 saturado, inverso y enganche
  fuera de canal, también con Strong y alta velocidad: 1 detent = 1 canal.
- **Persistencia real (SQLite)**: `test_params_threshold.c` + `params_stubs.c` corren el
  `params_init()`/`params_load()`/`params_save()` reales sobre una base SQLite en archivo:
  sin clave → 7; 3/7/15 se recuperan idénticos; 0/2/16/255 → 7 + `dirty`; cambio 3→15 persistido y
  recuperado tras "reinicio". 10 casos, todos OK.
- **Settings en inglés**: verificación estática + fila/orden/wiring.
- **UI**: indicador `xN` entero, oculto en CB/lock, se oculta por timeout sin mover frecuencia.

`tests/run_all_tests.sh --quick` → **15 OK, 0 fallos** (`logs/run_all_tests-quick.txt`).

## 6. Gates

| Gate | Resultado | Evidencia |
|---|---|---|
| Nativos | **PASS** 44391/0 | `logs/native-tests.txt` |
| `run_all_tests.sh --quick` | **PASS** 15 OK / 0 | `logs/run_all_tests-quick.txt` |
| ASan/UBSan | **SKIP** | toolchain del host sin libasan (`== sanitizers: NO disponibles en este toolchain`); nunca se declara PASS |
| ARM/ROM (`compila-gui.sh`) | **PASS** | binario `md5 452735f5d81f`, sello fuente `6f0ac61e433e`, commit `7ee5b5cf` (`logs/rom-build.txt`, `logs/rom-build-rebuild-check.txt`) |
| Determinismo arnés | **PASS** | `logs/harness-determinism.txt` |
| A/B `AB-VFO186` | **PASS** | `ab-AB-VFO186/report.md`: Functional/Visual/Quality/Performance PASS; 144 escenarios; 1493 contratos; Hardware/Energy BLOCKED (advisory, sin radio) |
| Themes (WCAG) | **PASS** | `logs/ab-themes.txt` (10 paletas, 90 pares, 0 fallos; no-text 19 objetos, 0 fallos) |
| `ruff` + `format` (ab_testing) | **PASS** | `logs/ruff-ab.txt` |
| `pytest` (ab_testing) | **PASS** 240 | `logs/pytest-ab.txt` |
| Banco QEMU (#86) | **PASS** | `banco/*.png`, `logs/banco-qemu.txt` |

El A/B detectó y corrigió un falso bloqueo del arnés: el mock de `radio_check_freq()` devolvía
igualdad con la frecuencia actual en vez de validar el rango, y el escenario `dfl-editor-rotary0`
fallaba. Se replicó la semántica de `src/radio.c` (500 kHz..55 MHz).

Nota: el A/B se corrió sobre `7cfade9b` (candidato del reporte). Sus fuentes de UI son idénticas a
`7ee5b5cf`; la única diferencia es la lista de fuentes de `src/CMakeLists.txt`, que el arnés host no
compila. El binario ARM/ROM y el del banco QEMU sí son de `7ee5b5cf` (mismo sello `6f0ac61e433e`).

## 7. Limitaciones / SKIP (nunca PASS)

- **ASan/UBSan**: no disponibles en el toolchain del host → **SKIP** con razón exacta.
- **Codecs WSPR/SSTV/WEFAX**: **SKIP** en `run_all_tests.sh --quick` porque la imagen
  `x6200-uiharness` es la mínima y no trae `fftw3.h`/`sndfile.h`.
- **JS8 / webserver**: omitidos por `--quick` (el runner los saltea).
- **e2e_chroot.sh**: **SKIP** con razón exacta — el script monta el volumen fijo
  `x6200-build-output` (`tests/e2e_chroot.sh` línea 122) y este run construyó en un volumen privado
  (`x6200-build-output-dfl173`); correrlo probaría otro binario. El camino real de la GUI en ARM
  queda cubierto por el **banco QEMU** con el binario de esta corrida.
- **Validación física**: fuera del issue; la hace el owner.
- **Settings vs DFL inline**: `Esquema`/`Restablecer` del editor DFL (#182) no son Settings y
  quedan fuera del alcance de la normalización pedida.
- La persistencia se prueba con SQLite real en el host, pero con los servicios externos de
  `params.c` simulados (voz, `cfg_init`, log de arranque): el camino de base y saneo es el real.

## 8. Archivos modificados

`src/adaptive_tuning.[ch]`, `src/rotary.[ch]`, `src/rotary_queue.[ch]` (nuevos),
`src/main_screen.[ch]`, `src/params/params.[ch]`, `src/dialog_settings.c`, `src/voice.[ch]`,
`src/CMakeLists.txt`, `tests/native/{run_native_tests.sh,test_main.c,test_adaptive_tuning.c,
test_rotary_queue.c,test_settings_language.c,test_params_threshold.c,params_stubs.c,
stubs/liquid/liquid.h}`, `tools/ui_harness/{mocks/mock_params.c,mocks/mock_ui_deps.c,
mocks/mock_radio.c,src/screens.c}`, `docs/evidencia/2026-10-20-vfo186/**`,
`tools/ab_testing/reports/AB-VFO186/**`.

## 9. Commits

- `64a3496b` — motor, cola, VFO/CB, parámetro, fila Settings, inglés, tests.
- `7cfade9b` — arnés: `radio_check_freq` del mock valida rango.
- `7ee5b5cf` — build: `rotary_queue.c` en `src/CMakeLists.txt`.
- `8f17a552` — evidencia + reporte A/B `AB-VFO186` + capturas + banco.
- (HEAD) — verificación estática del reset por umbral/perfil.
