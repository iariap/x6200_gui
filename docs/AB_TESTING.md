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

> Estos hashes son **previos al fix UI-009** (el reloj de potencia leía un
> puntero sin inicializar en las 7 fixtures): ver más abajo la tabla antes/
> después y por qué cambian. El determinismo (dos corridas idénticas) sigue
> pasando con los valores nuevos.

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
AB-e2e-ui009      clock sin fix vs fix             -> PASS (IMPROVEMENT, UI-009 FIXED)  # ver más abajo
capture CLI       los 8 fixtures                    -> 8 PNG + 8 JSON
```

Puertas que arrancan mínimas y **honestas**: `benchmark` devuelve BLOCKED (no hay
instrumentación de tiempos de frame/FPS; los contadores de render son de tiempo
virtual, no duraciones), y `audit` es solo por región de píxeles hasta que el
arnés emita la inspección estructural.

## Milestone 4 — benchmark de render ✅

Gate: detecta una regresión de rendimiento controlada.

Entregado en `tools/ui_harness/` y `tools/ab_testing/`:

- **Instrumentación real en C** (`src/bench_metrics.c`): con `--bench` el arnés
  mide cada frame con `CLOCK_MONOTONIC` — **separado** del reloj virtual que
  decide los píxeles — y reporta `render_ms`, `flush_ms` (tiempo real dentro del
  flush), `draw_ms` (render − flush, derivado), cantidad de flushes, área
  redibujada (suma de rectángulos) y píxeles distintos redibujados. Al final lee
  RSS/PSS y el pico de memoria de `/proc`. Warm-up y N repeticiones
  configurables por fixture (`bench_warmup`, `bench_repeats`,
  `bench_redraw_passes`) o por CLI (`--warmup`, `--repeats`). El modo normal no
  cambia: el PNG de un escenario es idéntico con y sin `--bench`.
- **Framework Python**: `src/x6200_ab/performance.py` consume esos datos y
  calcula p50/p95/p99, FPS efectivo (desde el frame mediano) y dispersión
  (coeficiente de variación); repite A y B **alternadas** por rondas. Un lado con
  pocas muestras o dispersión excesiva es **INCONCLUSIVE** (nunca PASS por
  ruido). `quality/budgets.yaml` fija el plan de muestreo y los presupuestos
  relativos (p95 de render, flushes, área) **antes** de correr.
- `x6200-ab benchmark` deja de ser BLOCKED: con `--revision` mide; con `--a/--b`
  evalúa regresión relativa y escribe `reports/<id>/`. Se agrega el subcomando
  `contracts` (contratos objetivos sobre el candidato solo) para el check
  separado de CI.

Evidencia (host x86, `uv run pytest -m e2e`, `AB-e2e-benchmark`, regenerada con
el fix UI-009): una revisión de prueba que repinta la pantalla 3 veces por frame
(trabajo de render extra deliberado, `bench_redraw_passes 3`) es detectada por
p95 sobre presupuesto, con 15 frames por lado y sin depender de una sola
medición:

```text
bench-render  FAIL  p95 A 3.774 ms -> B 11.318 ms  (+199.9%)
              flushes 1.0 -> 3.0  (+200%)   área 384000 -> 1152000 px (+200%)
  A: 15 frames, dispersión 2.7%     B: 15 frames, dispersión 3.9%
  razones: p95 render +199.9% (budget 25.0%); flushes +200.0% (budget 20.0%);
           redrawn area +200.0% (budget 20.0%)
```

Medición del arnés (lado A de `bench-render`, 15 frames): p50 3.608 ms, p95
3.774 ms, 1 flush/frame, RSS ≈10.1 MB. Todo etiquetado como **proxy x86**: no es
el rendimiento del Cortex-A7 ni la autonomía de batería.

Reporte commiteado: `tools/ab_testing/reports/AB-e2e-benchmark/`.

## Milestone 6 — workflow de CI ✅

Gate: un PR con regresión deliberada falla con evidencia.

- **Imagen reproducible desde cero** (`tools/ui_harness/Dockerfile`): antes era
  `FROM x6200-buildroot` (imagen local de 6.7 GB, no reconstruible en CI). Ahora
  se construye sobre `ubuntu:24.04`: stage `ci` (mínimo: `build-essential`,
  `cmake`, `pkg-config`, `libpng-dev`, `libnm-dev`, `libsqlite3-dev`,
  `libliquid-dev` — los headers del firmware usan `sqlite3.h` y
  `liquid/liquid.h`) y stage `full` (default, con los extras de los tests del
  ROM). El generador de señal no usa FFTW. La imagen `ci` pesa ~576 MB (vs 6.7
  GB) y reproduce **byte a byte** las capturas commiteadas.
- **`.github/workflows/ab-testing.yml`**: jobs separados — UI Functional Tests
  (ruff + format + ty + pytest unit, checks estáticos del repo
  `check_cfg_fills/actions/settings_focus/apps_menu/wcag`, `run_native_tests.sh`),
  UI Quality Contracts, UI Visual Regression (A vs B), UI Performance (benchmark
  host) y UI A/B Summary. Corre en PRs con cambios en `tools/**`, `src/**` o
  `tests/**`. Publica artifacts (screenshots/diffs/reportes) y resumen por
  `$GITHUB_STEP_SUMMARY`. Regla: los fallos funcionales/regresiones objetivas
  bloquean; los cambios estéticos son REVIEW REQUIRED y no bloquean solos; un
  benchmark inconcluso se muestra como tal; Hardware/Energy quedan **PENDING**.

Evidencia:

```text
$ docker build -t x6200-uiharness-ci --target ci tools/ui_harness   # ~32 s, sin imágenes locales
$ UI_HARNESS_IMAGE=x6200-uiharness-ci tests/check_harness_determinism.sh
bench-render: 56d7562491f2c26f20f7c37c044a26566adf92cc4659a6e46e2cf4d804a537e4
home-usb-20m: 14f380ff07444726b7df4765537e219dbe865a85a3643f7f7fb36df0303eeb3f
... (el resto quedan idénticos a los de los milestones 1–3)
PASS: deterministic (identical PNGs for every fixture)

$ uv run pytest -m e2e            # 9 passed
```

> Hashes **previos al fix UI-009**; con el fix la imagen `ci` produce los valores
> nuevos (ver la tabla de la sección UI-009) y el gate sigue en PASS.

## Incremento — inspección estructural y contratos de layout/tipografía ✅

Gate: `x6200-ab audit` deja de estar BLOCKED por falta de instrumentación, opera
con la inspección **real** del árbol LVGL y detecta defectos estructurales
deliberados.

Entregado en `tools/ui_harness/` y `tools/ab_testing/`:

- **`src/ui_inspector.c`**: con `--inspect` el arnés recorre el árbol de objetos
  de la pantalla activa y emite al JSON, por nodo, el bounding box absoluto, el
  recorte real que aplica el padre (`LV_OBJ_FLAG_OVERFLOW_VISIBLE`, como en
  `lv_refr.c`), el box efectivamente visible, la visibilidad (`hidden`/opa), la
  clase de widget, y para los labels el texto con la fuente y las métricas
  **reales** (`lv_txt_get_size`), los colores de texto/fondo, y `missing_glyphs`
  / `fallback_glyphs` resueltos con `lv_font_get_glyph_dsc` (`is_placeholder`,
  `resolved_font`). La instrumentación va por flag: **no toca el camino del
  benchmark ni cambia la captura normal** (el PNG es bit a bit idéntico; los
  sha256 de `check_harness_determinism.sh` no cambian).
- **`inspection.py` / `layout_checks.py` / `typography_checks.py`**, conectados a
  `contracts.py` y a `quality_gates` (Quality Contract Gate). Reglas: elementos
  que pintan algo y quedan fuera del viewport, cajas recortadas por el viewport o
  un padre que clipea, cajas degeneradas (0/negativas) que pintan, dos textos
  superpuestos, controles por debajo del target táctil mínimo (44 px, AGENTS.md);
  textos que desbordan o se truncan, glifos ausentes de **toda** la cadena de
  fuentes y pares texto/fondo por debajo de 4.5:1/3:1 (WCAG 2.1) cuando el fondo
  es opaco. `contracts.yaml` gana las secciones `layout`, `typography` y
  `exceptions` (waivers con fecha, acotados a regla+fixture, que no ocultan el
  hallazgo).
- **`audit`** captura con `--inspect`, reporta la instrumentación real
  (`instrumentation.structural`, `nodes_inspected`) y falla con un finding
  estructural que no esté waiveado. Acepta los alias `--ref` (por `--revision`)
  y `--suite visual|all`.

Evidencia (host, 2026-10-13, `X6200_AB_E2E_REPORTS=tools/ab_testing/reports uv
run pytest -m e2e`: **11 passed**; audit guardado en
[`tools/ab_testing/reports/AB-e2e-structural/`](../tools/ab_testing/reports/AB-e2e-structural/)):

```text
audit base       (home-usb-20m): PASS — 84 objetos inspeccionados, 0 findings
audit regression (botones +30px): FAIL — 10 × layout_clipped + 5 × layout_touch_target
                                  (lv_obj 0,448 160x62 cut to 160x32; labels ídem)
audit structural (defecto       : FAIL — typography_text_overflow  ('VERYLONGMODENAME'
                  deliberado)             en 56x26), typography_missing_glyph (U+F293)
                                          y typography_contrast (1.00:1 < 4.50:1)
```

Determinismo (los hashes son los mismos que los aceptados en M1–M4):

```text
$ tests/check_harness_determinism.sh
home-usb-20m: 14f380ff07444726b7df4765537e219dbe865a85a3643f7f7fb36df0303eeb3f
... (todos idénticos a los de M1–M4)
PASS: deterministic (identical PNGs for every fixture)
```

Las semillas de known issues se re-evaluaron con la instrumentación nueva: las 7
siguen `unverified`, cada una con su motivo concreto (el contraste estructural y
el detector de glifos ahora corren sobre la pantalla real y no encuentran
violaciones). Ver [`docs/UI_KNOWN_ISSUES.md`](UI_KNOWN_ISSUES.md).

## Fix real de firmware: UI-009 (reloj de potencia) — primer caso A/B ✅

Gate: el arnés reproduce un defecto real del firmware, lo clasifica contra su fix
y deja el test que impide reintroducirlo.

`src/clock.c`, `show_time()` rama `CLOCK_POWER`: `bat_sym` se declaraba **sin
inicializar** y el `switch ((uint8_t)(v_bat * 10))` sólo cubría 0–100. Con
`v_bat > 10.2` (12.1 V con fuente externa → 121 tras el cast) ningún `case`
pisaba el puntero y `snprintf` leía la pila: glifos basura (`EHE 12.1v` en las
capturas del toolchain aceptado) y comportamiento indefinido — segfault con otro
toolchain (reproducido con gcc 12 en Debian). El fix inicializa el puntero
(`const char * bat_sym = LV_SYMBOL_BATTERY_FULL;`, una línea).

- **Fixture**: `tools/ui_harness/fixtures/home-bateria-externa.scn` (12.1 V,
  13.8 V externos, cargando). `tests/check_harness_determinism.sh` lo toma solo y
  pasa; el fixture nuevo no requiere tocar el script (recorre `fixtures/*.scn`).
- **Detector**: nueva clase de known issue `leading_glyph_range` en
  `quality/known_issues.yaml` (UI-009) + región `reloj_potencia` en
  `quality/regions.yaml`. Exige que el texto del reloj —leído de la inspección
  estructural, `--inspect`, sin OCR— arranque con un code point del bloque de
  iconos de batería `U+F200..U+F2FF`. `compare` ahora captura también A con
  `--inspect` (los píxeles no cambian) porque el detector necesita el texto en
  ambos lados.
- **Test permanente**: `tests/native/test_clock.c`, compilado con
  `-ftrivial-auto-var-init=pattern` para que reinstalar el puntero pelado crashee
  en vez de pasar por azar (ver [`docs/UI_KNOWN_ISSUES.md`](UI_KNOWN_ISSUES.md)).

Evidencia (host, `uv run pytest -m e2e`, base fijada con
`X6200_AB_E2E_BASE=ec1b0a5`, **12 passed**):

```text
AB-e2e-ui009  clock sin fix (0d96c79f) vs fix (ec1b0a5)  -> PASS (IMPROVEMENT)
  Functional PASS · Visual Regression PASS · Quality Contract PASS · Performance PASS
  Hardware BLOCKED · Energy BLOCKED (advisory: sin unidad física)
  UI-009 FIXED:  A '\x89E… 12.1v' (U+0089, memoria)  ->  B '\uf240 12.1v' (U+F240)
  diff: 1 142 px, todo dentro de `reloj_potencia` (explicado por el fix)
```

PNG A/B y diff commiteados en `tools/ab_testing/reports/AB-e2e-ui009/`
(`screenshots/a-…png` con `EHE`, `screenshots/b-…png` con el ícono de batería,
`diffs/home-bateria-externa-overlay.png` con la caja).

### Hashes de determinismo antes y después del fix

El defecto estaba en **las 7 fixtures previas** — todas con `battery_v` entre 10.6
y 12.4 V, fuera del switch — así que el fix cambia el reloj de todas ellas. Para
un voltaje dentro de rango el render sigue siendo byte a byte idéntico: un fixture
temporal de 8.1 V da
`6e4f8a3d2773daef47866bf150825637b3cf2e697355aa979a8120632004d4eb` antes y
después del fix.

| fixture | antes del fix (aceptado, M1–M6) | después del fix (UI-009) |
|---|---|---|
| `bench-render` | `56d7562491f2c26f20f7c37c044a26566adf92cc4659a6e46e2cf4d804a537e4` | `eae13ebaf270528d1d77e281077c366b13db856158694d92fecf54f2fbd204be` |
| `home-frecuencia-extrema` | `3f6209c3a8edeed38f1e5b0069754e5eacb337c8a3ae85a2ab0671ce4a6d3900` | `2eee0a627d6098b7a44fb7f999ce38ebffa2ef294b4c3fe2b726d151976e76b1` |
| `home-usb-20m` | `14f380ff07444726b7df4765537e219dbe865a85a3643f7f7fb36df0303eeb3f` | `21b9b8b132028b93d33da3162f03329e722697484f988ac92477c1c7d0386c73` |
| `main-bateria-baja` | `5593056bf94d7affe9820114b5bc8e88c6701ed4f79484d778a8ace764ef254e` | `575aa927fdb259dd3d5421d7937ab6c95e31216d3d2ba72432f115e1ac6b0820` |
| `main-espectro-agua` | `b5109240540f052314479e67ea1400d22f838df6c3aa9005cf0294940613cd8f` | `198f255c363a1d38e46c97ed07236b62c18a43c69ca5bbf8e22cfe6285bc8382` |
| `main-otro-tema` | `f59bba03d1780167f75d24124248c750cc77f12f12daa7992113576388a816d2` | `ec6303188d54b567b8f3c252a4de8058ffd50f63539d49c67b317d338ea4b32c` |
| `main-tx-simulado` | `5ba48d4e7e5e2cf9060865591564a56577cc7d8242765be5569b871f7c89cd51` | `b476647d4d4c4a9284b261fccbbed19dd07cf9f3d693b6478a0ebe78880a48d8` |
| `home-bateria-externa` | (fixture nuevo) | `c0cb0ed34c97ac3d1737a580db37067129fef430314e231bccd6feb6452b6064` |

`tests/check_harness_determinism.sh` sigue en **PASS** con los hashes nuevos
(idénticos run-a-run); lo que cambió es qué se dibuja, no el determinismo.

## Pendientes

- **Captura física y energía** (milestone 7): sin un X6200 conectado,
  Hardware/Energy quedan BLOCKED/PENDING, nunca PASS por default.
- **Fixtures de diálogos y apps** (Ajustes, menú de apps, about, cluster): sin
  ellos las semillas UI-002/UI-004/UI-005 quedan `unverified`.
- El reloj de energía dependía de una variable local sin inicializar
  (`src/clock.c`, `show_time`): quedó corregido como **UI-009** con test nativo y
  caso A/B propio (ver la sección **UI-009** arriba). Las capturas aceptadas de
  los milestones 1–6 son **previas** al fix y por eso cambian; el camino de un
  voltaje dentro de rango es byte a byte idéntico antes y después.

