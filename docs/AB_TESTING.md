# Arnés de testing UI: estado

Bitácora de avance de `docs/AB_TESTING_PLAN.md`. Cada milestone se declara
recién cuando sus propias pruebas lo demuestran; sin X6200 físico, lo
verificable en host se completa y lo de hardware queda PENDIENTE.

## Milestone 1 — build limpio, framebuffer virtual, PNG, tiempo virtual ✅

Gate: dos corridas equivalentes producen capturas reproducibles.

Entregado en `tools/ui_harness/` (rama `lu2dsx/ui-ab-harness`):

- Build desde checkout limpio: CMake compila **componentes reales**
  (`theme.c`, `styles.c`, `fonts/*`, `clock.c`, `info.c`, `meter.c`, `band_info.c`)
  contra el submódulo `lvgl/` (stock 8.3.11, SHA
  `74d0a816a440eea53e030c4f1af842a94f7ce3d3`) y fuentes del repo. No usa el
  checkout externo `.x6100_ref`; los headers de build que no viven en el repo
  (`aether_radio/…`, `ft8lib/constants.h`) están vendorizados en
  `tools/ui_harness/third_party/` con su origen.
- Framebuffer virtual 800×480 en RAM + `flush_cb` LVGL → PNG (`libpng`).
- CLI `ui_harness --scenario <file> --png <out.png> --json <out.json>`; el JSON
  lleva escenario, revisión git, cobertura por componente y métricas de render.
- Tiempo virtual (`virtual_time.c`): avanzar N ms = `lv_tick_inc` +
  `lv_timer_handler` en slices fijos, sin sleeps; `get_time()` cableado ahí.
- Determinismo: `tests/check_harness_determinism.sh` corre el escenario dos
  veces y compara sha256 del PNG → PASS.

Evidencia (host, 2026-10-13, revisión `8a20615`):

```text
$ tests/check_harness_determinism.sh
run 1 sha256: d68dc1ab6109de471ed7fefe1bf3dc285f2ee796d8ddb32df12d1f8d9cd99aa6
run 2 sha256: d68dc1ab6109de471ed7fefe1bf3dc285f2ee796d8ddb32df12d1f8d9cd99aa6
PASS: deterministic (identical PNGs)
```

Escenario `home-usb-20m` (barra superior de la pantalla principal: `info_init`
+ `meter_init` + `clock_init`, con sus estilos reales). Cobertura honesta
declarada en el JSON del arnés:

- REAL: `theme.c`, `styles.c`, `fonts/`, `clock.c`, `info.c`, `meter.c`.
- PARTIAL: `band_info.c` (compilado, fuera del layout de este escenario).
- SYNTHETIC: `params`, `cfg`, `radio` (estado del arnés; sin base de datos ni
  dispositivo).
- UNSUPPORTED: `spectrum.c`, `waterfall.c`, `main_screen.c`, `buttons.cpp`
  (camino DSP/FFT/hardware y barra de botones; siguientes incrementos).

## Incremento — pantalla principal con código REAL ✅

Gate: el escenario `home-usb-20m` renderiza la pantalla principal del firmware
con `src/main_screen.c` real, y las dos corridas de cada fixture dan PNG
idénticos.

Entregado en `tools/ui_harness/` (rama `lu2dsx/ui-ab-harness`):

- **`main_screen.c` real**: el arnés llama a `main_screen()` y compila los
  componentes visuales reales (`spectrum.c`, `waterfall.c`, `clock.c`, `info.c`,
  `meter.c`, `band_info.c`, `tx_info.c`, `band_plan.c` + `band_plan_data.c`,
  `pannel.c`, `vol.c`, `msg.c` + `msg_tiny.c`, `adaptive_tuning.c`,
  `cw_tune_ui.c`) y el C++ propio de la GUI (`buttons.cpp`, `mfk.cpp`,
  `controls.cpp`) contra el submódulo `lvgl/` (stock 8.3.11). Se compila además
  el **`src/cfg/subjects.cpp` real** (núcleo Subject/observer), no un mock C.
- **Dependencias de radio/DSP/hardware por mocks explícitos**:
  `mock_radio.c` (estado + registro de llamadas `mock_radio_calls`),
  `mock_cfg.c` (subjects y catálogo de bandas), `mock_params.c`, `mock_dsp.c`,
  `mock_main_screen.c` (diálogos, columna de cluster, cola del scheduler) y
  `mock_ui_deps.c` (bus de eventos, voice, BT/WiFi/backlight).
- **Espectro/agua: PARTIAL** — el componente es real, los datos vienen de un
  generador determinista controlado por el `seed` del escenario
  (`src/signal_gen.c`), no de un FFT/radio. Igual `band_info.c` (categorías de
  banda del catálogo del arnés, no de la DB).
- **Seis fixtures**: `home-usb-20m`, `main-tx-simulado`, `main-bateria-baja`,
  `home-frecuencia-extrema`, `main-otro-tema`, `main-espectro-agua`. Cada uno
  corre con el CLI y produce PNG 800×480 + JSON (con `coverage` y `radio_calls`).
- **Determinismo extendido**: `tests/check_harness_determinism.sh` corre TODOS
  los fixtures dos veces y compara sha256.

Evidencia (host, 2026-10-13):

```text
$ tests/check_harness_determinism.sh
home-frecuencia-extrema: 3f6209c3a8edeed38f1e5b0069754e5eacb337c8a3ae85a2ab0671ce4a6d3900
home-usb-20m: 14f380ff07444726b7df4765537e219dbe865a85a3643f7f7fb36df0303eeb3f
main-bateria-baja: 5593056bf94d7affe9820114b5bc8e88c6701ed4f79484d778a8ace764ef254e
main-espectro-agua: b5109240540f052314479e67ea1400d22f838df6c3aa9005cf0294940613cd8f
main-otro-tema: f59bba03d1780167f75d24124248c750cc77f12f12daa7992113576388a816d2
main-tx-simulado: 5ba48d4e7e5e2cf9060865591564a56577cc7d8242765be5569b871f7c89cd51
PASS: deterministic (identical PNGs for every fixture)
```

Cada fixture emite además JSON con la cobertura por componente; por ejemplo el
de `main-tx-simulado` declara `main_screen.c` REAL, `spectrum.c`/`waterfall.c`
PARTIAL, `cfg (catalogue)` SYNTHETIC y `radio_calls.get_state = 2` (el mock se
ejercitó durante el render).

Nota de implementación: el bus de eventos del firmware asigna los `EVENT_*` con
`lv_event_register_id()`; LVGL guarda el filtro del callback en un campo de 8
bits, así que un id hardcodeado >255 se trunca a `LV_EVENT_ALL` y todo callback
recibe todos los eventos. El mock del arnés ahora asigna los ids con
`lv_event_register_id()` (como `events.c`) y `event_init()` corre antes de
construir la pantalla.

## Milestone 2 — inventario de defectos con el arnés ✅ (parcial)

Gate: se documenta al menos un defecto real reproducible; los checks sintéticos
detectan fallos deliberados.

Entregado en `tools/ab_testing/` (framework Python, rama `lu2dsx/ui-ab-harness`):

- `quality/known_issues.yaml` con `UI-001` (la etiqueta del multiplicador de
  tuning visible sobre la barra superior, `ROM_REVIEW.md` §5.3) **reproducido por
  el arnés**: el detector `region_content_above` mide la región `multiplicador`
  sobre el PNG y la marca cuando deja de ser fondo.
- Las 8 semillas del brief entran con su evidencia; las que el arnés todavía no
  reproduce quedan `unverified` y **no bloquean**. Registro:
  [`docs/UI_KNOWN_ISSUES.md`](UI_KNOWN_ISSUES.md).
- Detección de fallos deliberados verificada por los tests: un fixture con la
  barra de botones corrida 30 px produce un diff localizado (48 267 px, región
  `botones`).

Lo pendiente de este milestone es la **inspección estructural** (cajas de
objetos, glifos, overflow): el arnés aún no la emite, así que `audit` la reporta
como instrumentación faltante en vez de inventar resultados.

## Milestone 3 — A/B entre commits, diff, contratos, reportes ✅

Gate: distingue corrección / regresión / cambio esperado.

Entregado en `tools/ab_testing/`:

- **Revisiones aisladas por SHA**: `git worktree` por commit, submódulo `lvgl`
  fijado por gitlink y cacheado; la revisión que no compila queda **BLOCKED**
  con la cola del log. Nunca se reusa silenciosamente el binario de otra
  revisión (el JSON del arnés declara la revisión y se verifica).
- **Diff visual** (Pillow/NumPy): área, cajas (flood fill), regiones semánticas
  (`regions.yaml`), `diff.png` y `overlay.png`, y dimensiones incompatibles como
  defecto.
- **Contratos** independientes de A y **budgets** fijados antes de correr;
  **regiones de revisión** y **cambios esperados** con vencimiento.
- **Evaluador** con las cinco salidas y las seis puertas separadas; Hardware y
  Energy BLOCKED (advisory), nunca PASS por defecto. Códigos de salida 0/1/2/3/4.
- **Reportes** en `reports/<experiment-id>/` con MD, HTML, `results.json`,
  `metadata.json`, `screenshots/`, `diffs/`, `metrics/`, `logs/`. Evidencia
  commiteada en [`tools/ab_testing/reports/`](../tools/ab_testing/reports/README.md).

Evidencia (host, 2026-10-13, `uv run pytest`, incluida la suite `-m e2e` que
compila el arnés real con Docker):

```text
AB-e2e-aa         base vs base                     -> PASS   (diff 0)
AB-e2e-regression base vs barra de botones +30 px  -> FAIL   (48 267 px, región botones)
AB-e2e-fix        etiqueta UI-001 vs base          -> PASS (IMPROVEMENT, UI-001 FIXED)
AB-e2e-reappear   base vs etiqueta UI-001          -> FAIL   (UI-001 REGRESSION)
capture CLI       los 6 fixtures                    -> 6 PNG + 6 JSON
```

Puertas que arrancan mínimas y **honestas**: `benchmark` devuelve BLOCKED (no hay
instrumentación de tiempos de frame/FPS; los contadores de render son de tiempo
virtual, no duraciones), y `audit` es solo por región de píxeles hasta que el
arnés emita la inspección estructural.

## Milestones 4–7 — pendientes

Ver `docs/AB_TESTING_PLAN.md` (tabla de milestones y gates). Benchmark de render
con repeticiones y FPS, workflow de CI, y captura física/energía del equipo.
