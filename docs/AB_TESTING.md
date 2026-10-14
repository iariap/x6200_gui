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
`X6200_AB_E2E_BASE=ec1b0a5`, **13 passed**):

```text
AB-e2e-ui009  clock sin fix (f5c36778) vs fix (ec1b0a5)  -> PASS (IMPROVEMENT)
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

## Contratos ↔ registro de defectos conocidos: UI-010 (`UNCHANGED DEFECT`) ✅

Gate: una violación de contrato que el registro ya documenta como defecto de
línea base **abierto y fundado** no falla la corrida —se lista como
`UNCHANGED DEFECT` con su issue—, y **toda otra** violación sigue fallando.

`run_contracts()` corre ahora con el registro (`quality/known_issues.yaml`): un
finding fallido cuyo `(regla, fixture, región)` matchea una *exclusión acotada*
de un issue `verified` con evidencia se marca con `baseline_issue` y pasa a
`UNCHANGED DEFECT`; el gate de calidad lo reporta pero no falla por él. Una
exclusión sin regla —o de otra fixture/región— nunca matchea: no hay exclusiones
globales, y cada una lleva motivo y fecha de vencimiento. El detalle queda en el
reporte (`report.md`/`report.html`), en el JSON de `contracts`
(`unchanged_defects`) y en el resumen del workflow
(`UNCHANGED DEFECT … (UI-010)`).

- **UI-010** (`main-otro-tema`, región `frecuencia`): la franja de frecuencia se
  pintaba blanca sobre blanco (defecto **preexistente** del tema Outdoor); el
  contrato `region_min_content:frecuencia` la detectaba (`content 0.0000`).
  Exclusión acotada `until 2027-03-31` (ver `docs/UI_KNOWN_ISSUES.md`).
  **Corregido** en UI-04/EXP-3 (ver abajo, `src/styles.c`): con el contrato en
  PASS la exclusión se **retiró** en el mismo cambio. El mecanismo sigue vigente
  para un defecto de región abierto (hoy ninguno lo usa) y para el caso
  estructural de UI-013 (`exceptions` de `quality/contracts.yaml`).
- **UI-011** (cascada casi blanca sobre blanco) evaluada y **plegada** en UI-010:
  ninguna regla detecta "contenido presente pero ilegible" (el ruido casi blanco
  cuenta como contenido y `region_min_content:waterfall` pasa), así que queda
  como observación documentada, no como defecto detectado.

Evidencia (host, base fijada con `X6200_AB_E2E_BASE=ec1b0a5`, la evidencia en
`tools/ab_testing/reports/` y `uv run pytest -m e2e`, **13 passed**):

```text
contracts main-otro-tema: PASS — 9 checks, 0 failure(s), 1 unchanged baseline defect(s)
  UNCHANGED DEFECT region_min_content:frecuencia [main-otro-tema] (UI-010): content 0.0000 >= 0.0200 — unchanged defect UI-010: Tema Outdoor: franja de frecuencia invisible sobre fondo blanco
contracts (suite, 8 fixtures): PASS — 72 checks, 0 failure(s), 1 unchanged baseline defect(s)
```

Antes (run de CI `38497082174`, `ui-contracts/contracts.json`): `FAIL — 72
checks, 1 failure(s)` por `region_min_content:frecuencia [main-otro-tema]`.

Una violación distinta de la de línea base sigue **FAIL** (tests unitarios
`test_violation_not_in_the_registry_stays_a_real_failure` y
`test_regression_next_to_a_baseline_defect_still_fails`). El fix de identidad git
de las revisiones efímeras del self-test —el runner de Actions no tiene
`user.name`/`user.email`, así que `_ephemeral` commitea con una identidad de
scaffolding— queda cubierto por `tests/test_git_scaffolding.py`.

> Las revisiones efímeras (`abtest/*`) se recrean en cada corrida de evidencia,
> así que su SHA corto cambia aunque el parche sea el mismo; la evidencia
> commiteada en `tools/ab_testing/reports/` se regeneró con esta corrida.

## UI-02 — cobertura real: diálogo, matriz de temas y foco (issue #7) ✅

Gate: el arnés cubre un **diálogo real del firmware** y la **matriz de temas** de
la ROM, con capturas deterministas y cobertura honesta; los contratos siguen en
verde y un fixture nuevo está cableado a los detectores (probado con una regresión
deliberada).

Entregado en `tools/ui_harness/` y `tools/ab_testing/`:

- **Diálogo real (`dialog-about`)**: el arnés compila el `src/dialog.c` real y el
  `src/dialog_about.c` real y el fixture abre la ventana por el camino del
  firmware (`main_screen_action(ACTION_APP_ABOUT)` → `dialog_construct` →
  `dialog_about`). Los demás diálogos de app siguen siendo mocks inertes (su
  descriptor no hace nada). Sin tocar `src/`: Ajustes/WeFax/SSTV arrastran
  `dialog_field` y decenas de dependencias y quedan fuera del alcance.
- **Matriz de temas**: fixtures `main-tema-rs` (JSON RS con sus imágenes),
  `main-tema-outdoor-negative`, `main-tema-classic`, `main-tema-holo` y
  `main-tema-material-3`. El arnés lee los temas que la ROM trae desde el propio
  repo (`rootfs/usr/share/x6200/themes`, los mismos archivos que instala la
  imagen) y aplica la fuente del tema y el `fonts_scale()` de VISION como el
  `main()` del firmware. **VISION no entra como fixture**: al escalar las fuentes
  aparecen hallazgos reales de tipografía (glifo del reloj ausente, `VFO-A`
  desbordado) que son triage del issue #8, no de este incremento.
- **Foco (`main-foco`)**: el grupo de teclado real, con el `spectrum` que
  `main_screen()` registra (timer de 100 ms) y `lv_group_focus_next()`. El
  spectrum es el único widget del grupo y no tiene estilo de foco, así que el PNG
  es idéntico a `main` (mismo sha256 `a656e096…`): la **cadena** queda
  reproducible, el **resalte visible** no existe sin Settings.
- **Cobertura honesta por pantalla**: `coverage.c` elige la tabla según el
  `screen`; en `dialog-about`, `dialog.c` y `dialog_about.c` son REAL y los demás
  diálogos UNSUPPORTED.
- **Temas largos**: el campo `theme` guarda el nombre completo; `params.theme_name`
  es `char[16]` y `Outdoor-Negative` (16) se truncaba (límite real del firmware,
  anotado abajo). Corregido después en UI-04/AB-UI04-EXP4: `params_str_t.x` es de
  32 bytes y el menú entero entra (UI-013).

Evidencia (host, 2026-10-14, `d082583` + este cambio):

```text
$ tests/check_harness_determinism.sh
bench-render: 5ba21bea…        home-usb-20m: a656e096…      main-otro-tema: 65064bfa…
dialog-about: 213c5cc1…        main-bateria-baja: 975a45e5… main-tema-classic: b251bbe8…
home-bateria-externa: 2c83b8b7…  main-espectro-agua: 41cc0d50… main-tema-holo: d52d097e…
home-frecuencia-extrema: f11829c3…  main-foco: a656e096…     main-tema-material-3: baa8a0ec…
main-tema-outdoor-negative: 31ea5ccc…  main-tema-rs: c2411cb0…  main-tx-simulado: e0281c23…
PASS: deterministic (identical PNGs for every fixture)

$ uv run x6200-ab contracts --revision <head> --json   # (vía local: 15 fixtures)
PASS — 135 checks, 0 failure(s), 1 unchanged baseline defect(s) (UI-010 en main-otro-tema)
```

Los 8 fixtures previos conservan su sha256; `main-foco` coincide con `main`
(esperado, ver arriba).

### Prueba de detección (regresión deliberada en un fixture nuevo)

`U+F293` en el título del About (`src/dialog_about.c`, revertido antes del
commit) hace fallar el detector de tipografía sobre el fixture nuevo:

```text
FAIL dialog-about: typography_missing_glyph -> text '\uf293' uses glyph(s) ['U+F293']
     missing from every font in the chain (LVGL draws a placeholder box)
dialog-about: ok=9 fail=1
```

### Matriz de cobertura por fixture (pantalla / tema / fuente × nivel)

`✓` = cubierto con captura determinista; `PARTIAL`/`SYNTHETIC`/`UNSUPPORTED` como
los declara el JSON del arnés. La tabla exacta por componente está en cada JSON.

| Área | Fixture(s) | REAL | PARTIAL | SYNTHETIC | UNSUPPORTED |
|---|---|---|---|---|---|
| Pantalla `main_screen.c` | todas las `main`/`main-tema-*` | ✓ | | | |
| Diálogo About (`dialog.c` + `dialog_about.c`) | `dialog-about` | ✓ | | | |
| Otros diálogos de app | — | | | | ✗ (mocks inertes) |
| Ajustes / menú de apps | — | | | | ✗ (arrastran `dialog_field` y decenas de deps) |
| Tema IU1WXY | `home-usb-20m`, `main-tx-simulado`, `main-bateria-baja`, `home-frecuencia-extrema`, `home-bateria-externa`, `bench-render` | ✓ | | | |
| Tema Legacy | `main-espectro-agua`, `bench-render` | ✓ | | | |
| Tema Outdoor | `main-otro-tema` | ✓ (defecto UI-010 corregido en UI-04/EXP-3) | | | |
| Tema Outdoor-Negative | `main-tema-outdoor-negative` | ✓ | | | |
| Tema Classic | `main-tema-classic` | ✓ | | | |
| Tema RS (JSON + imágenes) | `main-tema-rs` | ✓ | | | |
| Tema Holo (JSON) | `main-tema-holo` | ✓ | | | |
| Tema Material 3 (JSON) | `main-tema-material-3` | ✓ | | | |
| Tema VISION | — | | | | ✗ (font scale: hallazgos para #8) |
| Fuente por tema (Arial en Classic/RS) | `main-tema-classic`, `main-tema-rs` | ✓ | | | |
| Fuente elegida por el usuario (Roboto/DejaVu/Arial) | — | | | | ✗ (no hay fixture de fuente) |
| Espectro / waterfall | todas | | ✓ (generador determinista, sin FFT) | | |
| Catálogo de bandas (`band_info`) | todas | | ✓ (catálogo del arnés, no la DB) | | |
| Foco / encoder | `main-foco` | | ✓ (cadena de foco real; sin resalte visible) | | |
| `cfg`, `params`, `radio` | todas | | | ✓ (mock explícito, sin DB/dispositivo) | |
| Hardware / energía | — | | | | ✗ **BLOCKED** (sin X6200 físico) |

### Límite real anotado: nombre de tema largo

`params.theme_name` es `params_str_t` con `char x[16]`; `"Outdoor-Negative"` tiene
16 caracteres y `params_str_set()` lo trunca a 15 → `theme_init()` no lo reconoce
y cae a `IU1WXY`. La ruta de Ajustes (`dialog_settings.c` `theme_update_cb`)
guarda por ese mismo campo, así que elegir **Outdoor-Negative** en pantalla
terminaría en el tema por defecto. Es un hallazgo real **fuera del alcance** de
UI-02 (cobertura): el arnés usa el nombre completo para poder renderizar el tema y
lo deja documentado para el triage del issue #8. **Corregido en UI-04/AB-UI04-EXP4
(UI-013)**: `params_str_t.x` pasó a 32 bytes (el ancho que `theme_list()` admite
por nombre), sin migración de `params.db`; la excepción del contrato
`theme_name_fits_params` se retiró en el mismo cambio.

## Fix real de firmware: UI-011 (iconos de VISION) — EXP-1 de UI-04 (issue #9) ✅

Gate: el arnés reproduce el defecto fundado de la auditoría UI-03, el fix lo cierra y
el A/B queda **PASS con el cambio confinado** a la barra superior y **15 fixtures
byte-idénticas**.

`src/fonts/font_select.c`, `fonts_scale()`: copiaba la struct entera de una fuente
sobre otra (`*sony[i] = was[i + steps]`), así que el tamaño que dibuja heredaba la
cobertura de glifos del tamaño con el que se reemplazaba. Solo `sony_20`/`sony_30`
traen los rangos de icono (`cmap` hasta `U+F244`); con `VISION_FONT_STEPS = 3`
`sony_30` (reloj) pasa a `sony_36` y `sony_20` (barra) a `sony_26`, ASCII, y el
reloj y el icono de WiFi quedaban en rectángulo vacío (UI-011). El fix conserva en
cada tamaño escalado **su fuente de origen como fallback**
(`sony[i]->fallback = &was[i]`): el tamaño grande dibuja el texto y el tamaño
original aporta los iconos; con una fuente alternativa, `was[i]` resuelve a su vez
por su propia copia Sony. Un archivo, sin iconos ni paquetes de fuentes nuevos.

- **Fixture**: `main-tema-vision` (agregada en UI-03). La hipótesis previa al
  cambio: el diff se confina a `reloj_potencia`/`barra_superior`.
- **Región corregida**: `quality/regions.yaml` — la banda superior llega a **y=62**
  bajo VISION (el reloj son dos líneas del tamaño escalado, ~61 px; las celdas de
  `info.c` hasta y=53), no a 46; con 46 la segunda línea del reloj y la celda del
  WiFi caían dentro de `espectro` y el cambio se reportaba fuera de su región.
  Igual que UI-009 agregó `reloj_potencia`, acá se corrige su altura.
- **Cómo lo reproduce el arnés**: contrato `typography_missing_glyph` sobre
  `main-tema-vision` (inspección estructural, sin OCR). En A (`b029907`) **FAIL** con
  dos hallazgos (reloj `U+F240`/`U+F1E6`, WiFi `U+F1EB`); en B, **0 findings**. La
  *excepción acotada* que lo waiveaba se **retiró en el mismo PR**.
- **Sin detector de píxeles**: un glifo ausente deja su code point en el texto del
  label, así que UI-011 no puede explicarse por `known_issues.yaml` como UI-001/009;
  el cambio visual se declara como `expected_change` acotado y fechado
  (`barra_superior`, hasta 2027-03-31) en `quality/contracts.yaml`.

Evidencia (host, 2026-10-14, `b029907` → `c471ecb`):

```text
audit b029907 (A): FAIL — 2 × typography_missing_glyph [main-tema-vision]
  text '\uf240 12.1v\n\uf1e6 13.8v' missing ['U+F240','U+F1E6']
  text '\uf1eb'                      missing ['U+F1EB']
audit c471ecb (B): PASS — 0 structural findings (1350 objetos, 16 fixtures)

contracts c471ecb: PASS — 161 checks, 0 failure(s), 1 unchanged baseline defect(s)
  UNCHANGED DEFECT region_min_content:frecuencia [main-otro-tema] (UI-010)

compare AB-UI04-EXP1: PASS (exit 0)
  Functional PASS · Visual Regression PASS · Quality Contract PASS · Performance PASS
  Hardware BLOCKED · Energy BLOCKED (advisory: sin unidad física)
  main-tema-vision: 1943 px cambiados (0.51%), TODO dentro de `barra_superior`
                     (reloj U+F240/U+F1E6 + WiFi U+F1EB); 15 fixtures en 0 px
```

Los 161 contratos (antes 163) bajan exactamente los dos `typography_missing_glyph`
que la excepción waiveaba y que el fix eliminó. Screenshots A/B y `diff`/`overlay`
en [`tools/ab_testing/reports/AB-UI04-EXP1/`](../tools/ab_testing/reports/AB-UI04-EXP1/)
(`a-`/`b-main-tema-vision.png`, `diffs/main-tema-vision-*.png`).

Determinismo (mismos sha para cada fixture entre corridas; `main-tema-vision` es la
única que cambia de contenido respecto de UI-03):

```text
tests/check_harness_determinism.sh   -> PASS (16 fixtures)
main-tema-vision   UI-03 (A): d59e7cf8…   EXP-1 (B): abe5d57f…
```

`tests/native/run_native_tests.sh` PASS (538 + 7) y `uv run pytest -m "not e2e"` PASS
(118). El fix no toca `src/clock.c` ni `src/info.c` (UI-012 es EXP-2).

```text
benchmark BENCH-UI04-EXP1: PASS (exit 0) — 16 escenarios, main-tema-vision p95 -9.6%
  (una corrida previa con builds concurrentes dio INCONCLUSIVE por dispersión del
  host >25%; en máquina ociosa el veredicto es PASS)
```

## Fix real de firmware: UI-012 (celda del VFO en VISION) — EXP-2 de UI-04 (issue #9) ✅

Segundo experimento del mismo issue, con la misma disciplina: repro primero con la
excepción retirada, fix mínimo y reversible, A/B con el cambio confinado y la
excepción **retirada en el mismo cambio**.

`src/info.c`: las seis celdas de la barra siguen siendo las **56×26 fijas** del
bitmap (`lv_obj_set_size(item, 56, 26)`), pero VISION escala todas las fuentes tres
pasos (`fonts_scale(3)`), así que `info_item_style` (`src/styles.c:259`, `sony_20`)
rinde el tamaño efectivo de `sony_26`. `VFO-A` pasa de **48 a 66 px** de ancho: no
entra, el label (WRAP) la parte en dos líneas y la celda recorta la segunda — en
pantalla queda `VFO-` y el `A` no se ve (**UI-012**). De las seis celdas es la única
que no entra: las otras cinco (`USB` 44, `AUTO` 56, `P/A` 43, `ATU1` 55, el icono de
WiFi 19) sí, así que **sólo el VFO** vuelve al tamaño de los otros temas
(`font_down(&sony_20, VISION_FONT_STEPS)` bajo `theme_shape.vision`, medido 51×17 en
la caja de 56×26), el mismo idioma que `NF()` de `src/cluster_column.c` para sus
columnas de ancho fijo y la alternativa que `UI_KNOWN_ISSUES.md` ya anotaba ("VISION
dibujar esa fila con el tamaño de siempre"). No se toca `fonts_scale()` (el fix de
UI-011), ni `src/clock.c`, ni las otras cinco celdas; los otros temas quedan
byte-idénticos porque el cambio está detrás de `theme_shape.vision`.

- **Repro antes del fix** (`8512d86` con la excepción de UI-012 retirada sólo en un
  directorio de calidad local): audit **FAIL** con un único finding —
  `typography_text_overflow [main-tema-vision] text 'VFO-A' does not fit its 56x26
  box (WRAP)` (medición de la inspección: `text_natural w=66` contra
  `text_box w=56`, `text_wrapped_h=40` en una caja de 26).
- **Región real del cambio**: `barra_superior` (la celda del VFO es la primera de la
  banda superior; no toca `espectro` ni `reloj_potencia`, a diferencia de EXP-1). Se
  declara como segundo `expected_change` acotado y fechado en
  `quality/contracts.yaml` (la entrada de UI-011 se conserva: el gate lee las
  entradas de una región como un conjunto).
- **Cómo lo reproduce el arnés**: contrato `typography_text_overflow` sobre
  `main-tema-vision` (inspección estructural, sin OCR). A: FAIL; B: **0 structural
  findings**. La excepción acotada que lo waiveaba se **retiró en el mismo cambio**.

Evidencia (host, 2026-10-14, `8512d86` → `6a17b83`):

```text
audit 8512d86, excepcion retirada (A): FAIL — 1 × typography_text_overflow [main-tema-vision]
  text 'VFO-A' does not fit its 56x26 box (WRAP)   (natural w=66, wrapped_h=40)
audit 6a17b83 (B): PASS — 0 structural findings (1350 objetos, 16 fixtures)

contracts 6a17b83: PASS — 160 checks, 0 failure(s), 1 unchanged baseline defect(s)
  UNCHANGED DEFECT region_min_content:frecuencia [main-otro-tema] (UI-010)
  (161 en A con la excepcion — el finding del overflow waiveado; el fix lo elimina)

compare AB-UI04-EXP2: PASS (exit 0)
  Functional PASS · Visual Regression PASS · Quality Contract PASS · Performance PASS
  Hardware BLOCKED · Energy BLOCKED (advisory: sin unidad física)
  main-tema-vision: 373 px cambiados (0.097%), TODO dentro de `barra_superior`,
                     un solo box 17,13-68,26 (la celda del VFO); 15 fixtures en 0 px
```

Respecto del set de contratos, el único finding distinto entre A y B es el del
overflow (`0/38/0`, waiveado en A, ausente en B): los otros 160 quedan iguales.
Screenshots A/B y `diff`/`overlay` en
[`tools/ab_testing/reports/AB-UI04-EXP2/`](../tools/ab_testing/reports/AB-UI04-EXP2/)
(`a-`/`b-main-tema-vision.png`, `diffs/main-tema-vision-*.png`).

Determinismo (mismos sha para cada fixture entre corridas; la cadena de
`main-tema-vision` acompaña a cada fix de VISION):

```text
tests/check_harness_determinism.sh   -> PASS (16 fixtures)
main-tema-vision   UI-03 (A): d59e7cf8…   EXP-1: abe5d57f…   EXP-2 (B): e5b176ed…
```

`tests/native/run_native_tests.sh` PASS (538 + 7) y `uv run pytest -m "not e2e"` PASS
(118); `ruff check`/`ruff format --check`/`ty check` sin hallazgos.

```text
benchmark BENCH-UI04-EXP2: PASS (exit 0) — 16 escenarios dentro de presupuesto
  main-tema-vision p95 3.415 -> 3.740 ms (+9.5%) con el MISMO trabajo de redibujo
  (flushes 1.0 -> 1.0, area 384000 -> 384000 px, cv 6.1/5.1%): dispersión del host,
  no costo del cambio
```

## Fix real de firmware: UI-010 (frecuencia invisible en Outdoor) — EXP-3 de UI-04 (issue #9) ✅

Tercer experimento del issue, con la misma disciplina: repro primero con el waiver
retirado, causa medida en el árbol real (no leída), fix mínimo y reversible, A/B con
el cambio confinado y el waiver **retirado en el mismo cambio**. Es el defecto de
mayor severidad del backlog (critical: la frecuencia de trabajo no se veía en el tema
de campo).

Repro en A (`4d24990`, exclusión de UI-010 retirada sólo en un directorio de calidad
local — el waiver no se toca antes de tener el fix):

```text
contracts --revision 4d24990 --scenario main-otro-tema (sin el waiver): FAIL (exit 1)
  1 failure: region_min_content:frecuencia [main-otro-tema]: content 0.0000 >= 0.0200
Inspección estructural de la MISMA captura: los tres labels de la fila
  '#FFFFFF 9.933' / '#FFFFFF 10.125.000' / '#FFFFFF 10.317' con text_color #FFFFFF
  sobre una región de fondo #FFFFFF (1:1).
```

**Causa real (medida, no la hipótesis del registro)**: el color de esos tres labels
no sale del estilo sino del **markup de recolor** del propio label:
`on_fg_freq_change()` y `update_freq_boundaries()` (`src/main_screen.c`) escriben
`"#%03X %i.%03i.%03i"` con `subject_get_int(freq_lock) ? 0xBBBBBB : 0xFFFFFF`, y
LVGL **pisa** el `text_color` del estilo cuando el parámetro del recolor es válido
(`lvgl/src/draw/lv_draw_label.c`: sólo lo acepta si mide **6 dígitos**). El tema
Outdoor es el tema claro (`src/theme.h`, `THEME_COLORS`: `bg`/`strip` `0xffffff`,
`text` `0x000000`), así que el blanco cableado quedaba blanco sobre blanco.
`freq_style`/`freq_main_style` (`src/styles.c:142,150`) cableaban **el mismo** blanco
como default del estilo: muerto para estos labels, pero la misma trampa. Un intento
previo de fix sólo en `styles.c` se descartó **con la medición en la mano**: la
captura salía byte-idéntica (el markup manda).

- **Fix** (`src/main_screen.c` + `src/styles.c`): el markup pide los roles del tema
  (`theme_hex(TC_text)` con el dial suelto, `theme_hex(TC_text_soft)` —el rol
  "inactive knob / lock" de `theme.h`— con el dial trabado) con `#%06X`, y los dos
  estilos `TC(text)`. Mismo idioma que el propio archivo ya usa para ATT/PRE/ATU
  (`"#%06X ...", theme_hex(TC_text)`) y que `freq_strip.c` para los dígitos del dial.
- **Región real del cambio**: `frecuencia` (y=160..196), declarada como
  `expected_change` acotado y fechado en `quality/contracts.yaml`.
- **Blast radius medido**: `TC(text)` vale `0xFFFFFF` en IU1WXY/Legacy/Classic/VISION
  y `negative(0x000000)` en Outdoor-Negative; `TC_text_soft` vale el `0xBBBBBB` de
  antes en IU1WXY. **14 de 16 fixtures byte-idénticas**; sólo se mueve, además de
  `main-otro-tema`, `main-tema-material-3` (su `text` es `#e2e2e9`, 29/255 en los
  mismos tres labels). Ninguna fixture traba el dial, así que el camino
  `TC_text_soft` no cambia un píxel medido.

Evidencia (host, 2026-10-14, `4d24990` → `fb2e5a8`):

```text
contracts HEAD: PASS — 160 checks, 0 failure(s), 0 unchanged baseline defect(s)
  region_min_content:frecuencia [main-otro-tema]: content 0.0979 >= 0.0200
  (sin waiver: la exclusión scoped de UI-010 se retiró en este PR)
audit HEAD: PASS — 0 structural findings (1350 objetos, 16 fixtures)

compare AB-UI04-EXP3: PASS (exit 0)
  Functional PASS · Visual Regression PASS · Quality Contract PASS · Performance PASS
  Hardware BLOCKED · Energy BLOCKED (advisory: sin unidad física)
  main-otro-tema:        2819 px (0.734%), 21 boxes, TODO dentro de `frecuencia`
  main-tema-material-3:  3027 px (0.788%), 22 boxes, TODO dentro de `frecuencia`
  14 fixtures en 0 px; flushes y área redibujada idénticos en las 16
  improvement: false — UI-010 no tiene detector de píxeles propio (su detector es la
  regla de contrato): A/B la clasifica UNVERIFIED, nunca FIXED. La mejora la prueba
  el contrato (FAIL en A con el waiver retirado → PASS en B sin waiver).

determinismo (16 fixtures): PASS
  main-otro-tema         A 65064bfa… -> B 8592ccff…
  main-tema-material-3   A baa8a0ec… -> B 7d3a1016…   (las otras 14 sin cambio)
```

Screenshots A/B, `diff`/`overlay` de los dos fixtures que se mueven en
[`tools/ab_testing/reports/AB-UI04-EXP3/`](../tools/ab_testing/reports/AB-UI04-EXP3/).

Test permanente del fix (idioma de UI-009): una revisión efímera
(`abtest/style`) vuelve a cablear el blanco en el markup y el contrato **vuelve a
fallar sin baseline** (`tests/test_e2e.py`), mientras la comparación contra el fix
prueba que el único cambio está en `frecuencia`. Los 15 tests `-m e2e` pasan.

```text
benchmark BENCH-UI04-EXP3: PASS (exit 0) — 16 escenarios dentro de presupuesto
  flushes 1.0 -> 1.0 y área 384000 -> 384000 px en las 16: el cambio es de color,
  no de trabajo de redibujo
  los p95 se mueven entre -30% y +13% según el escenario con cv de hasta 17.6% en el
  lado A (que se mide primero en cada ronda): dispersión del host, no costo
```

`BENCH-UI04-EXP3-repeat` (segunda corrida mía, mismo A/B) **falló** en
`bench-render` (+44% p95) con el **mismo trabajo** (flushes 1.0→1.0, 384000 px): con
esta carga de host (load average ~3) el veredicto de p95 a este tamaño de muestra es
ruido, y lo que decide el costo real (flushes/área) no cambió. El reporte repetido
queda commiteado al lado del aceptado para que el número esté a la vista.

`tests/native/run_native_tests.sh` PASS (538 + 7), `uv run pytest -m "not e2e"` PASS
(118), e2e completo PASS (15), los cinco chequeos estáticos del repo PASS; `ruff
check`/`ruff format --check` limpios; el build de A y B da los mismos **67
warnings**. `ty check src tests` en ty 0.0.87 **no encuentra archivos** (avisa "No
python files found under the given path(s)": con un archivo de prueba con un error de
tipos deliberado el comando pasa igual); corriendo `ty` sobre la lista explícita de
archivos del framework sí analiza y no reporta nada (ver Pendientes).

## Design tokens (UI-05 incremento 1, issue #10): la geometría de la barra de info ✅

Primer incremento del issue #10 (*design tokens, reusable widgets, Flex/Grid*),
elegido por ser el nido de números mágicos donde ya vivieron dos defectos
(UI-011/UI-012 estaban en la geometría fija de `src/info.c`). El issue pide
centralizar de a poco ("small reviewable PRs, not single massive rewrite"), así
que este PR es **un refactor puro y nada más**: cero cambio visual.

- **`src/ui_tokens.h`** (nuevo): los nombres de dominio de la geometría de la
  barra. El header no tiene lógica ni estado del tema —solo los números, más la
  fuente de la celda, que el fix UI-012 necesita nombrar—: consumirlos desde el
  JSON de tema es un incremento aparte, con su propio PR.
- **`src/info.c`**: los literales de geometría pasan a los tokens. No se toca la
  lógica de fuente VFO / `theme_shape.vision` del fix EXP-2, ni `styles.c`,
  `main_screen.c`, `clock.c`, `fonts_scale` ni `lvgl/`.
- **Sin Flex/Grid todavía**: la conversión de layout es el incremento 2, para
  que este diff sea verificable pixel-por-pixel.

Los tokens y los literales que reemplazan. El borrador del issue nombra
`56×26/72/2`, pero el **72 no existe en la fuente**: el paso de columna real es
**58 = 56 (celda) + 2 (gap)** y el de fila **22** (los renglones además se
solapan 4 px, porque la celda mide 26):

| token | valor | reemplaza en `info.c` |
|---|---|---|
| `UI_INFO_COLS` / `UI_INFO_ROWS` | `3` / `2` | `x < 3`, `y < 2` y el tamaño del array de items |
| `UI_INFO_CELL_W` / `UI_INFO_CELL_H` | `56` / `26` | `lv_obj_set_size(item, 56, 26)` y el alto `26` de la celda del WiFi |
| `UI_INFO_CELL_GAP` | `2` | el gap implícito de la rejilla |
| `UI_INFO_COL_PITCH` | `56 + 2` = `58` | `x * 58 + 15` |
| `UI_INFO_ROW_PITCH` | `22` | `y * 22 + 5` |
| `UI_INFO_ORIGIN_X` / `UI_INFO_ORIGIN_Y` | `15` / `5` | el origen de la rejilla |
| `UI_INFO_CELL_RADIUS` | `3` | `lv_obj_set_style_radius(item, 3, 0)` |
| `UI_INFO_WIFI_COL` / `UI_INFO_WIFI_ROW` | `2` / `1` | la celda 6 (WiFi/BT) en el índice de la rejilla |
| `UI_INFO_WIFI_W` | `56 / 2` = `28` | `lv_obj_set_size(items[INFO_WIFI], 28, 26)` |
| `UI_INFO_BT_LINE_W` | `2` | `lv_obj_set_style_line_width(bt_item, 2, 0)` |
| `UI_INFO_BT_POINTS` | `7` | `lv_line_set_points(bt_item, bt_pts, 7)` |
| `UI_INFO_BT_DY` | `8` | `1 * 22 + 8` (la runa baja 8 px en la celda) |
| `UI_INFO_FONT` | `&sony_20` | la fuente por defecto de la celda (dos usos: el default y la base de `font_down` bajo VISION) |

Quedan **a propósito** fuera de los tokens (y anotados en el header):
las 7 coordenadas de la runa de BT (son una forma, no layout), el timer de
refresco de 2 s del icono de BT (es una política de tiempo, no geometría) y los
textos/colores, que salen de los roles del tema.

### Evidencia (host, 2026-10-14, `135f297` → `ccaa95f`)

El criterio 1 de la spec es la garantía: **si un solo píxel cambia, el refactor
está mal**. No cambia ninguno de los 16 fixtures.

```text
$ tests/check_harness_determinism.sh          # hashes INALTERADOS vs 135f297
bench-render: 5ba21bea6dcd30cffeea0bf171f578ba3fb51c811e44aa22c875c5fc74378beb
home-usb-20m: a656e0961a41c890bd6663a46dc3d36d2606df4f0b56eae126a88ac32bb974f9
main-tema-vision: e5b176ed6a03a5b75100a403afb53e1094d6271bff61ff81e61348c2946a049d
... (las 16 con el mismo sha256 que el tip previo)
PASS: deterministic (identical PNGs for every fixture)

$ cd tools/ab_testing && uv run x6200-ab compare --a 135f297 --b HEAD --experiment-id AB-UI05-T1
experiment AB-UI05-T1: PASS (exit 0)
  Functional PASS · Visual Regression PASS (captures are pixel-identical to the baseline)
  Quality Contract PASS (160 checks) · Performance PASS
  Hardware BLOCKED · Energy BLOCKED (advisory: sin unidad física)
  16/16 fixtures: changed_pixels 0

$ uv run x6200-ab contracts --revision HEAD
contracts ccaa95fdd9e7: PASS — 160 checks, 0 failure(s), 0 unchanged baseline defect(s)

$ uv run x6200-ab audit --revision HEAD --suite visual
audited ccaa95fdd9e7: 13 known issues — 0 reproduced, 11 unverified; structural
inspection REAL over 1350 objects on 16 scenario(s): 0 structural finding(s)

$ uv run x6200-ab benchmark --a 135f297 --b HEAD --experiment-id BENCH-UI05-T1
benchmark 135f2970eb86 (A) vs ccaa95fdd9e7 (B): PASS (exit 0)
  16 escenarios: flushes 1.0 -> 1.0 y área 384000 -> 384000 px en los 16 (0.0%);
  p95 entre -11.7% y +12.8% (dispersión del host, no costo: el trabajo de redibujo
  es idéntico)

$ uv run pytest -m "not e2e" -q            → 120 passed, 17 deselected
$ tests/native/run_native_tests.sh         → 545 comprobaciones, 0 fallos + 7, 0 fallos
repo static checks (cfg_fills/actions/settings_focus/apps_menu/wcag) → PASS
ruff check / ruff format --check → limpios   (ty: "No python files found", pendiente conocido)
build de A y B: los mismos 67 warnings cada lado (ninguno en info.c)
```

### Dos verificaciones extra (que el "cero cambio" no sea un falso verde)

1. **Los tokens manejan el render (no un build viejo).** Cambiando a mano
   `UI_INFO_CELL_GAP` de 2 a 3, los hashes de la barra se mueven
   (`home-usb-20m: a656e096… → 23930e15…`); al revertir vuelven exactos. Prueba
   que el arnés recompila `info.c` desde el fuente montado y que la geometría
   sale de los tokens.
2. **La aritmética es el literal, verificado en compilación.** Un TU temporal
   (no commiteado) con `_Static_assert` sobre cada token y sobre las expresiones
   exactas que reemplazan (`UI_INFO_WIFI_COL * UI_INFO_COL_PITCH +
   UI_INFO_ORIGIN_X + UI_INFO_WIFI_W == 2 * 58 + 15 + 28`, etc.) compila sin
   observaciones: los tokens valen el literal histórico.

### Blast radius revisado

`tools/ab_testing/tests/test_e2e.py::_inject_structural_defects` parchea
`src/info.c`, pero solo textos y colores de labels (no la geometría), así que los
tests `-m e2e` siguen aplicando igual. `quality/known_issues.yaml` (UI-012) y
`tests/check_wcag.py` citan los literales viejos **como historia** del defecto /
como etiqueta de la tabla; ninguna regla parsea el fuente, así que no hay
detector que se rompa. La instrumentación estructural sigue contando los mismos
1350 objetos.

## Flex real en la barra de info (UI-05 incremento 2, issue #10) ✅

Segundo incremento del issue #10, sobre el tip del incremento 1 (`41532e7`): la
rejilla 3×2 de `src/info.c` deja de posicionar cada celda con `x*58+15, y*22+5` y
pasa a **dos contenedores `LV_FLEX_ROW`** (uno por fila) alimentados por los
tokens. El flex gobierna el eje X (gap = `UI_INFO_CELL_GAP`, celdas de
`UI_INFO_CELL_W×H`) y el Y de cada fila queda en el origen exacto de su token
(`UI_INFO_ROW_PITCH`), así que el solapamiento de 4 px entre filas (26 > 22) se
conserva **sin márgenes negativos**. La celda WiFi/BT (`UI_INFO_WIFI_*`) y la
runa de BT siguen su camino actual (la runa es una forma, no layout).

- **`src/info.c`**: un `lv_obj` por fila con `LV_LAYOUT_FLEX`/`LV_FLEX_FLOW_ROW`,
  `pad_all 0`, `pad_column = UI_INFO_CELL_GAP`, fondo transparente (la barra es
  un bitmap: el contenedor no debe pintar), radio/borde 0, sin scroll, y tamaño
  exacto `3*56 + 2*2 = 172 × 26`; las seis celdas pasan a hijas del contenedor de
  su fila. Flex coloca, no redimensiona: ninguna celda lleva `flex_grow`, así que
  conserva su `56×26` fijo (por eso el fix UI-012 de la fuente VFO sigue igual).
- No se toca `lvgl/`, `styles.c`, `main_screen.c`, `clock.c`, los tokens de
  `src/ui_tokens.h` ni el JSON de tema.

Es un refactor puro: mismas posiciones y mismos píxeles que el incremento 1.

### Costo estructural (+2 objetos por fixture) y paths que se mueven

Los dos contenedores son **dos objetos nuevos en el árbol LVGL**: la
instrumentación pasa de **1350 a 1382 objetos** en las 16 fixtures (+32 = 2×16).
Es el riesgo declarado de la spec: como los contenedores de fila intercalan un
nivel, los **paths `0/N/i`** de los labels se desplazan. Las **cajas no cambian**
(la instrumentación da coordenadas absolutas):

| celda | path antes | path después | caja (idéntica) |
|---|---|---|---|
| VFO-A | `0/38/0` | `0/38/0/0` | 15,5 56×26 |
| USB | `0/38/1` | `0/38/0/1` | 73,5 56×26 |
| AUTO | `0/38/2` | `0/38/0/2` | 131,5 56×26 |
| P/A | `0/38/3` | `0/38/1/0` | 15,27 56×26 |
| ATU1 | `0/38/4` | `0/38/1/1` | 73,27 56×26 |
| WiFi | `0/38/5` | `0/38/1/2` | 131,27 28×26 |
| runa BT | `0/38/6` (`lv_line`) | `0/38/2` (`lv_line`) | 159,30 16×22 |
| contenedor fila 0 | — | `0/38/0` (`lv_obj`) | 15,5 172×26 |
| contenedor fila 1 | — | `0/38/1` (`lv_obj`) | 15,27 172×26 |

### Blast radius: ningún consumidor del framework dependía de los índices

Se revisó el blast radius **antes** de codear y se verificó corriendo los tests
que lo cubren: **no hubo que adaptar ningún test del framework** (no se debilitó
ninguno).

- `quality/contracts.yaml` → `exceptions: []`: el único lugar del framework que
  compara el path de un nodo (`RuleException.path`) no tiene entradas.
- `known_issues.py` → los detectores son por **píxel** (`region_content_above`) o
  por **texto** (`leading_glyph_range`, un regex sobre el texto del label; el
  path se descarta, `_path`). `layout_checks.py` y `typography_checks.py`
  recorren el árbol por geometría/clase/texto; el path solo aparece en el
  `detail` del finding, nunca como criterio.
- `test_e2e.py::_inject_structural_defects` parchea `src/info.c` por **strings**
  (`lv_label_set_text(items[INFO_MODE], ...)`, el glifo del WiFi y su
  `text_color`), que quedan intactos; `pytest -m e2e -k audit` pasa (3 tests,
  incluido `test_audit_detects_text_glyph_and_contrast_defects`) sobre el nuevo
  tip.
- Los reportes commiteados (`reports/AB-ui03-audit`, `AB-UI04-EXP1`, …) y este
  mismo documento citan `0/38/*` como **historia** de corridas pasadas; no son
  consumidores.

### Evidencia (host, 2026-10-14, `41532e7` → `84c9115`)

```text
$ tests/check_harness_determinism.sh          # 16 fixtures x2, hashes idénticos
... (las 16 con el mismo sha256 que el tip 41532e7)
PASS: deterministic (identical PNGs for every fixture)

$ cd tools/ab_testing && uv run x6200-ab compare --a 41532e7 --b HEAD --experiment-id AB-UI05-T2
experiment AB-UI05-T2: PASS (exit 0)
  Functional PASS · Visual Regression PASS (captures are pixel-identical to the baseline)
  Quality Contract PASS (160 checks) · Performance PASS
  Hardware BLOCKED · Energy BLOCKED (advisory: sin unidad física)
  16/16 fixtures: changed_pixels 0   (CASO A puro)

$ uv run x6200-ab contracts --revision HEAD
contracts 84c9115ccb20: PASS — 160 checks, 0 failure(s), 0 unchanged baseline defect(s)

$ uv run x6200-ab audit --revision HEAD --suite visual
audited 84c9115ccb20: 13 known issues — 0 reproduced, 11 unverified; structural
inspection REAL over 1382 objects on 16 scenario(s): 0 structural finding(s)

$ uv run x6200-ab benchmark --a 41532e7 --b HEAD --experiment-id BENCH-UI05-T2
benchmark 41532e786bf0 (A) vs 84c9115ccb20 (B): PASS (exit 0)
  16 escenarios: flushes 1.0 -> 1.0 y área 384000 -> 384000 px en los 16 (0.0%);
  p95 entre -20.9% y +10.2% (dispersión del host, no costo: el trabajo de
  redibujo es idéntico)

$ uv run pytest -m "not e2e" -q            → 120 passed, 17 deselected
$ X6200_AB_E2E_BASE=HEAD uv run pytest -m e2e -k audit -q → 3 passed
$ tests/native/run_native_tests.sh         → 545 comprobaciones, 0 fallos + 7, 0 fallos
repo static checks (cfg_fills/actions/settings_focus/apps_menu/app_handlers/wcag) → PASS
ruff check / ruff format --check → limpios   (ninguno en src/info.c)
build de A y B: sin warnings nuevos en info.c
```

### Verificación extra (que el layout salga del flex)

Cambiando temporalmente el gap del contenedor (`lv_obj_set_style_pad_column(row,
10, 0)`, en vez de `UI_INFO_CELL_GAP`), la celda USB se mueve (`73 → 81`, AUTO
`131 → 147`, ATU1 `73 → 81`); al revertir al token, los 16 sha256 vuelven
exactos. Prueba que la posición X de las celdas sale del flex (no de un
`lv_obj_set_pos` residual) y que el arnés recompila `src/info.c`.

## Tokens de la fila de frecuencia (UI-05 incremento 3, issue #10) ✅

Tercer incremento del issue #10, sobre el tip del incremento 2 (`19aa618`): la
**fila de frecuencia** de `src/main_screen.c` (los tres labels `freq[]`) sale de
sus literales a la familia `UI_FREQ_*` de `src/ui_tokens.h`. Es un refactor puro
como el incremento 1: cero cambio visual, verificado por el gate A/B.

### Qué se tokenizó

La fila son tres labels que **teselan** el ancho fijo del panel (800 px, el
800×480 que fija el issue): el extremo izquierdo (150 px), la frecuencia central
(500 px) y el extremo derecho (150 px). 150 + 500 + 150 = 800, por eso el centro
arranca en 150 y el derecho en 650. Las anchuras no son una decisión de layout
libre: son las de `freq_style` / `freq_main_style` (`src/styles.c`, fuera de este
incremento), y las x salen de esas anchuras; por eso los tokens nombran la
relación en vez de repetir el número.

| token | valor | reemplaza en `main_screen.c` |
|---|---|---|
| `UI_FREQ_SCREEN_W` | `800` | el `800` de la aritmética de las x |
| `UI_FREQ_SIDE_W` | `150` | el `150` de `800 - 150` (ancho de los extremos, `freq_style`) |
| `UI_FREQ_MAIN_W` | `500` | el `500` de `800/2 - 500/2` (ancho del centro, `freq_main_style`) |
| `UI_FREQ_ROW_H` | `36` | `static uint16_t freq_height = 36` |
| `UI_FREQ_LEFT_X` | `0` | `lv_obj_set_pos(f, 0, y)` |
| `UI_FREQ_CENTER_X` | `800/2 - 500/2` = `150` | `lv_obj_set_pos(f, 800/2 - 500/2, y)` |
| `UI_FREQ_RIGHT_X` | `800 - 150` = `650` | `lv_obj_set_pos(f, 800 - 150, y)` |

`UI_FREQ_CENTER_X` y `UI_FREQ_RIGHT_X` conservan **la expresión exacta** de la
fuente (no una versión "más linda": el centro es el centro del panel menos media
anchura del label central). Un `_Static_assert` en el header fija la relación de
la tesela —`2*UI_FREQ_SIDE_W + UI_FREQ_MAIN_W == UI_FREQ_SCREEN_W`—; cambiando a
mano `UI_FREQ_SCREEN_W` a 900, el build falla con
`static assertion failed: "fila de frecuencia: extremos + centro deben teselar el panel"`.

### Qué quedó NO-tokenizable (y por qué)

- **El Y de la fila**: es el cursor del layout apilado de `main_screen()`
  (`y += spectrum_height` con `spectrum_height = 480/3`), no un literal; el
  espectro además es área que el issue prohíbe tokenizar. La fila no tiene un `y`
  propio, así que no hay métrica estable que nombrar.
- **`src/freq_strip.c`** (la "franja de frecuencia"): el issue y la spec la
  nombran, pero **no es la geometría de la fila**. Los "números chicos de los
  extremos del dial" son `freq[0]` / `freq[2]` de `main_screen.c` (arriba). El
  `freq_strip.c` es otro componente: un strip `LV_FLEX_ROW` / `LV_FLEX_COLUMN`
  cuyos offsets (14, −14, los gaps, el dot de 10×10) **dependen de la variante
  VISION**, y su `freq_strip_init()` **no lo monta ningún consumidor del árbol**
  (no aparece fuera de su propia definición y su declaración en el header):
  ninguna de las 16 fixtures lo renderiza, así que un refactor ahí no lo podría
  verificar el gate A/B — sería exactamente el "falso verde" que la verificación
  de sensibilidad existe para evitar. Queda para cuando tenga captura.
- **`freq_height` sigue siendo una variable mutable**: el token da el valor
  inicial (`UI_FREQ_ROW_H`), no cambia el tipo ni el uso.
- No hay un **pitch** de columna: los tres anchos son distintos (150/500/150), a
  diferencia de la rejilla de la barra (paso 58). La spec lo pedía como si
  existiera; no existe en la fuente.

### Evidencia (host, 2026-10-14, `19aa618` → `644e55d`)

```text
$ tests/check_harness_determinism.sh          # 16 fixtures x2, hashes = los del tip 19aa618
bench-render: 5ba21bea6dcd30cffeea0bf171f578ba3fb51c811e44aa22c875c5fc74378beb
home-usb-20m: a656e0961a41c890bd6663a46dc3d36d2606df4f0b56eae126a88ac32bb974f9
main-tema-vision: e5b176ed6a03a5b75100a403afb53e1094d6271bff61ff81e61348c2946a049d
... (las 16 con el mismo sha256 que el tip previo)
PASS: deterministic (identical PNGs for every fixture)

$ cd tools/ab_testing && uv run x6200-ab compare --a 19aa618 --b HEAD --experiment-id AB-UI05-T3
experiment AB-UI05-T3: PASS (exit 0)
  Functional PASS · Visual Regression PASS (captures are pixel-identical to the baseline)
  Quality Contract PASS (160 checks) · Performance PASS
  Hardware BLOCKED · Energy BLOCKED (advisory: sin unidad física)
  16/16 fixtures: changed_pixels 0

$ uv run x6200-ab contracts --revision HEAD
contracts 644e55dfb1b6: PASS — 160 checks, 0 failure(s), 0 unchanged baseline defect(s)

$ uv run x6200-ab audit --revision HEAD --suite visual
audited 644e55dfb1b6: 13 known issues — 0 reproduced, 11 unverified; structural
inspection REAL over 1382 objects on 16 scenario(s): 0 structural finding(s)

$ uv run x6200-ab benchmark --a 19aa618 --b HEAD --experiment-id BENCH-UI05-T3
benchmark 19aa6182d209 (A) vs 644e55dfb1b6 (B): PASS (exit 0)
  16 escenarios: flushes 1.0 -> 1.0 y área 384000 -> 384000 px en los 16 (0.0%);
  p95 entre -34.2% y +22.7% (dispersión del host, no costo: el trabajo de
  redibujo es idéntico)

$ uv run pytest -m "not e2e" -q            → 120 passed, 17 deselected
$ uv run pytest -m e2e -k audit -q         → 3 passed
$ tests/native/run_native_tests.sh         → 545 comprobaciones, 0 fallos + 7, 0 fallos
repo static checks (cfg_fills/actions/settings_focus/apps_menu/app_handlers/wcag) → PASS
```

El `benchmark` host es ruidoso con esta carga (load ~3-5): una repetición del
mismo A/B (`BENCH-UI05-T3-repeat`, commiteada al lado) **falló** en dos escenarios
**distintos** con el mismo trabajo (flushes/área Δ 0.0%), y una corrida más salió
INCONCLUSIVE por dispersión de A > 25%. Lo que decide el costo real —flushes y
área redibujada— no cambió en ninguna corrida; el veredicto de p95 a este tamaño
de muestra es ruido del host, como ya quedó anotado en `BENCH-UI04-EXP3-repeat`.

### Verificación anti-falso-verde (que los tokens manejan el render)

Cambiando a mano `UI_FREQ_LEFT_X` de 0 a 10, la fila se mueve: el sha de
`home-usb-20m` pasa de `a656e096…` a `29ea36e0…` (el de las otras 15 también
cambia, porque el extremo izquierdo está en todas las fixtures del main screen).
Al revertir al token, los 16 sha256 vuelven exactos. Prueba que la geometría de la
fila sale del token y que el arnés recompila `src/main_screen.c` desde el fuente
montado. (Y el `_Static_assert` de la tesela, arriba, es el guard de la relación.)

### Blast radius revisado

`tools/ab_testing/tests/test_e2e.py` parchea `src/main_screen.c` por strings: el
label del multiplicador (UI-001), la asignación de color del recolor
(`theme_hex(subject_get_int(freq_lock) ? TC_text_soft : TC_text)`, ×2) y los dos
formatos `"#%06X …"` del fix UI-010 (EXP-3). Los tres siguen intactos
(`grep -cF`) y `pytest -m e2e -k audit` (3 tests) pasa sobre el nuevo tip: no se
tocó ninguna de esas líneas ni la lógica de `freq_lock`. Las inyecciones
estructurales (`_inject_structural_defects`) son todas de `src/info.c` y no se
tocaron. El audit estructural sigue contando **1382 objetos**: este incremento no
agrega nodos al árbol (solo cambia constantes de posición).

## Flex real en la fila de frecuencia (UI-05 incremento 4, issue #10) ✅

Cuarto incremento del issue #10, sobre el tip del incremento 3 (`fad0b42`): la
fila de frecuencia de `src/main_screen.c` (los tres labels `freq[]` con la x por
token) pasa a **un contenedor `LV_FLEX_ROW`** del ancho del panel
(`UI_FREQ_SCREEN_W` = 800) cuyos hijos conservan sus anchuras por los estilos
actuales (`freq_style` 150 / `freq_main_style` 500). Mismo idioma que la barra
del incremento 2.

- **`src/main_screen.c`**: un `lv_obj` con `LV_LAYOUT_FLEX`/`LV_FLEX_FLOW_ROW`,
  `pad_all 0` + `pad_column 0`, fondo transparente (radius/borde 0, sin scroll) y
  tamaño exacto `800 × UI_FREQ_ROW_H (36)`; los tres labels pasan a hijos del
  contenedor. Ninguno lleva `flex_grow` (flex **coloca, no redimensiona**), así
  que la tesela `150 + 500 + 150 = 800` se mantiene sola —el `_Static_assert` del
  header sigue vigente—. Los tokens de x del incremento 3 no se borran:
  `UI_FREQ_CENTER_X`/`UI_FREQ_RIGHT_X` quedan definidos en `src/ui_tokens.h` (ya
  no hay un `lv_obj_set_pos` por label) y el contenedor se ancla en
  `UI_FREQ_LEFT_X`.
- **El Y no lo toca el flex**: el contenedor se posiciona con el MISMO cursor `y`
  del layout apilado (`lv_obj_set_pos(freq_row, UI_FREQ_LEFT_X, y)`, justo antes
  de `y += freq_height`); con pad/borde 0 los labels quedan en el mismo cursor que
  tenían. El espectro (`y += spectrum_height`) y el flujo vertical quedan
  intactos. Es solo el eje X.
- No se toca `lvgl/`, `styles.c`, `info.c`, `clock.c`, los **valores** de
  `src/ui_tokens.h` ni el **markup del EXP-3** (`"#%06X ..."` + `theme_hex(...)`
  en `freq[1]`/`on_fg_freq_change`/`update_freq_boundaries`): se preservan
  literales.

Es un refactor puro: las tres cajas cambian de padre pero **no** de posición ni
de tamaño (los tres labels siguen en `0/160`, `150/160`, `650/160` con
150/500/150 px).

### Costo estructural (+1 objeto por fixture) y paths que se mueven

El contenedor es **un objeto nuevo** en el árbol LVGL. El conteo agregado de la
inspección sube **+1 por fixture** (86 → 87 en `home-usb-20m`; **1382 → 1398** en
las 16 capturas, `audit`). Como el contenedor intercala un nivel, la fila de
frecuencia **colapsa tres hijos en uno** (3 labels + 1 contenedor = +1 nodo neto,
−2 hermanos del screen) y los paths se corren:

- los tres labels pasan de ser hijos directos del screen a hijos del contenedor
  (`0/1`, `0/2`, `0/3` → `0/1/0`, `0/1/1`, `0/1/2`);
- el contenedor ocupa el `0/1`;
- **todo** hijo del screen desde el viejo índice 4 en adelante baja **2** (3
  labels → 1 contenedor): p. ej. el label del reloj de potencia `0/37` → `0/35`,
  las seis celdas de la barra `0/38/…` → `0/36/…` (mismos boxes), la etiqueta del
  multiplicador `0/41` → `0/39`.

| nodo | path antes (`fad0b42`) | path después (`c52ca22`) | caja (idéntica) |
|---|---|---|---|
| extremo izquierdo | `0/1` | `0/1/0` | 0,160 150×36 |
| frecuencia central | `0/2` | `0/1/1` | 150,160 500×36 |
| extremo derecho | `0/3` | `0/1/2` | 650,160 150×36 |
| contenedor fila | — | `0/1` (`lv_obj`) | 0,160 800×36 |

Las cajas **no cambian** (la inspección da coordenadas absolutas; el render es
byte a byte idéntico, ver la evidencia).

### Blast radius: ningún consumidor del framework dependía de los índices

Se revisó **antes** de codear y se verificó corriendo los tests que lo cubren: no
hubo que adaptar ningún test del framework.

- `quality/contracts.yaml` → `exceptions: []`: el único lugar que compara el path
  de un nodo no tiene entradas.
- `known_issues.py` → detectores por **píxel** (`region_content_above`) o por
  **texto** (`leading_glyph_range`, un regex sobre el texto); `layout_checks.py`
  y `typography_checks.py` recorren el árbol por geometría/clase/texto. El path
  solo aparece en el `detail` del finding, nunca como criterio.
- `tools/ab_testing/tests/test_e2e.py` parchea `src/main_screen.c` por **strings**:
  el label del multiplicador (UI-001), la asignación del recolor
  (`theme_hex(subject_get_int(freq_lock) ? TC_text_soft : TC_text)`, ×2) y los dos
  formatos `"#%06X …"` del fix UI-010 (EXP-3). Los tres siguen intactos
  (`grep -cF`) y `pytest -m e2e` (17) pasa sobre el nuevo tip; las inyecciones
  estructurales (`_inject_structural_defects`) son todas de `src/info.c`.
- Los reportes commiteados y este documento citan paths viejos como **historia**
  de corridas pasadas; no son consumidores.

Los contratos `region_min_content:*` (incluido `frecuencia`) y el detector
`region_content_above` de UI-001 miden **rectángulos de píxel**: con el render
idéntico quedan iguales (`contracts` PASS, 160 checks).

### Evidencia (host, 2026-10-14, `fad0b42` → `c52ca22`)

```text
$ tests/check_harness_determinism.sh          # 16 fixtures x2, hashes = los del tip fad0b42
bench-render: 5ba21bea6dcd30cffeea0bf171f578ba3fb51c811e44aa22c875c5fc74378beb
home-usb-20m: a656e0961a41c890bd6663a46dc3d36d2606df4f0b56eae126a88ac32bb974f9
main-tema-vision: e5b176ed6a03a5b75100a403afb53e1094d6271bff61ff81e61348c2946a049d
... (las 16 con el mismo sha256 que el tip previo)
PASS: deterministic (identical PNGs for every fixture)

$ cd tools/ab_testing && uv run x6200-ab compare --a fad0b42 --b HEAD --experiment-id AB-UI05-T4
experiment AB-UI05-T4: PASS (exit 0)
  Functional PASS · Visual Regression PASS (captures are pixel-identical to the baseline)
  Quality Contract PASS (160 checks) · Performance PASS
  Hardware BLOCKED · Energy BLOCKED (advisory: sin unidad física)
  16/16 fixtures: changed_pixels 0   (CASO A puro)

$ uv run x6200-ab contracts --revision HEAD
contracts c52ca22aa4c3: PASS — 160 checks, 0 failure(s), 0 unchanged baseline defect(s)

$ uv run x6200-ab audit --revision HEAD --suite visual
audited c52ca22aa4c3: 13 known issues — 0 reproduced, 11 unverified; structural
inspection REAL over 1398 objects on 16 scenario(s): 0 structural finding(s)

$ uv run x6200-ab benchmark --a fad0b42 --b HEAD --experiment-id BENCH-UI05-T4
benchmark fad0b42aa803 (A) vs c52ca22aa4c3 (B): PASS (exit 0)
  16 escenarios: flushes 1.0 -> 1.0 y área 384000 -> 384000 px en los 16 (0.0%);
  p95 entre -3.4% y +17.6% (dispersión del host, no costo: el trabajo de
  redibujo es idéntico)

$ uv run pytest -m "not e2e" -q            → 120 passed, 17 deselected
$ uv run pytest -m e2e -q                  → 17 passed
$ tests/native/run_native_tests.sh         → 545 comprobaciones, 0 fallos + 7, 0 fallos
repo static checks (cfg_fills/actions/settings_focus/apps_menu/wcag) → PASS
build de A y B: mismos 67 warnings (ninguno en la fila de frecuencia)
```

**CASO A**, como en los incrementos 1–3: el flex es real pero la tesela
150/500/150 sobre 800 px es exacta, así que no hay sub-píxeles de redondeo y el
render queda byte a byte idéntico. No hubo `expected_change`, ni capturas A/B/diff
especiales, ni sección REVIEW REQUIRED (no se movió un solo píxel).

### Verificación anti-falso-verde (que el layout salga del flex)

Cambiando temporalmente el gap del contenedor (`lv_obj_set_style_pad_column(
freq_row, 10, 0)`, en vez de `0`), la fila se corre (el central arranca en 160, el
derecho en 670) y el sha de `home-usb-20m` pasa de `a656e096…` a `9f19e379…`; al
revertir al token, el sha vuelve **exacto** (`a656e096…`, `git diff` limpio).
Prueba que la posición X de los tres labels sale del flex (no de un
`lv_obj_set_pos` residual) y que el arnés recompila `src/main_screen.c` desde el
fuente montado. El `_Static_assert` de la tesela del header sigue siendo el guard
de la relación `2*UI_FREQ_SIDE_W + UI_FREQ_MAIN_W == UI_FREQ_SCREEN_W`.

## Fix real de firmware: UI-014 (la celda del modo en VISION) — EXP-5 de UI-04 (issue #28) ✅

Hermano exacto de UI-012 en la misma barra: con **VISION** el texto crece tres
pasos de fuente (`fonts_scale()`) y la celda del **modo** (`items[INFO_MODE]`,
`src/info.c`) siguió con las 56x26 fijas de `src/ui_tokens.h`, sin el tratamiento
que EXP-2 le dio al VFO. `CW-R` mide **exactamente 56 px**, el ancho de la celda
**0 px de margen**: se pega a la celda de AGC (que muestra `AUTO`) y en pantalla se
lee `CW-RAUTO` (issue #28, evidencia de radio `e4c329c`). `USB-D`, el modo más ancho
de los que la radio lista, mide **66 px** y el `WRAP` lo parte en dos líneas que la
celda recorta.

**La pieza que faltaba para fundarlo: el campo `mode` del escenario.** Existía desde
el primer commit del arnés (`scenario.c` → `mock_radio.mode`;
`mock_radio_apply()` lo publica en `cfg_cur.mode`, que es de donde `src/info.c` saca
la etiqueta), pero sólo entendía formas tipo enum (`cwr`, `usb_dig`). Ahora acepta
además los nombres canónicos que la radio lista y acepta (`mode_names[]`,
`src/remote_server.cpp`): `LSB`, `LSB-D`, `USB`, `USB-D`, `CW`, `CW-R`, `AM`, `NFM`
(más `SAM`/`WFM`, que están en el enum). Los dos spellings rinden **byte-idéntico**
(verificado por sha256).

**Dos fixtures nuevos** (el set pasa de **16 a 18**), mismos radio state que
`home-usb-20m` salvo el modo:

- `main-tema-vision-cwr` (VISION + `CW-R`): el estado exacto del issue. La etiqueta
  mide 56 px, el ancho de la celda.
- `main-tema-vision-usbd` (VISION + `USB-D`): 66 px, el fixture que el contrato
  `typography_text_overflow` puede medir.

### Sensibilidad medida (por qué el fixture que dispara es `usbd` y no `cwr`)

La etiqueta del modo bajo VISION, medida con `lv_txt_get_size` (inspección
estructural, sin OCR) sobre una celda de 56 px:

| modo | etiqueta | ancho (px) | margen | contrato (`WRAP`) |
|---|---|---|---|---|
| `CW` | CW | 34 | 22 | OK |
| `LSB` / `USB` | LSB / USB | 42 / 44 | 14 / 12 | OK |
| `NFM` / `SAM` | NFM / SAM | 49 | 7 | OK |
| `WFM` | WFM | 54 | 2 | OK |
| `CW-R` | CW-R | **56** | **0** | OK (`56 > 56` es falso: no hace `WRAP`) |
| `LSB-D` | LSB-D | **64** | −8 | **FAIL** |
| `USB-D` | USB-D | **66** | −10 | **FAIL** |

`CW-R` mide **el ancho exacto** de la celda, así que no hace `WRAP` y el contrato
(exige que el texto crezca más allá de la caja) no puede dispararse sobre él —el
pegado queda fuera de su alcance por 1 px—. Por eso el fixture que funda UI-014 es
`main-tema-vision-usbd` y `main-tema-vision-cwr` queda como la evidencia del pegado
reportado, que el arnés reproduce **pixel a pixel** contra la captura de la radio:
la tinta del modo va de 74 a 127 y la de `AUTO` de 131 a 185 en las **dos**.

### Defecto y fix

- **A** (`9c031c8`, sólo los fixtures): el audit falla —`text 'USB-D' does not fit
  its 56x26 box (WRAP)` sobre `main-tema-vision-usbd`— y el fixture `cwr` muestra el
  pegado (`CW-R` a 0 px de margen).
- **Fix** (`src/info.c`): bajo `theme_shape.vision` la celda del modo usa
  `font_down(UI_INFO_FONT, VISION_FONT_STEPS)` = `sony_14`, el mismo idioma que
  `NF()` de `src/cluster_column.c` y que la celda del VFO de UI-012. Medido: `CW-R`
  pasa de **56 a 42 px** y `USB-D` de **66 a 52 px**.
- **B** (`7b644a7`): audit **PASS** (0 findings, 1572 objetos, 18 fixtures) y
  contracts **PASS** (180 checks, 0 excepciones).

### Blast radius (medido, no afirmado)

`compare` de A a B: **15 de 18 fixtures byte-idénticas**; los 3 diffs quedan
**confinados a `barra_superior`** y son la celda del modo de los tres fixtures
VISION (`main-tema-vision` 406 px, `main-tema-vision-cwr` 446 px,
`main-tema-vision-usbd` 448 px). `main-tema-vision` **cambia** aunque su modo sea
`USB`: el fix es a nivel de celda (la fuente de las seis celdas es la misma), así
que `USB` (44 → 35 px) también se redibuja; el cambio está declarado en
`expected_changes` (región `barra_superior`). `home-frecuencia-extrema`,
`main-tx-simulado`, `main-espectro-agua` y las demás no mueven un píxel.

### Evidencia (host, 2026-10-14, `9c031c8` → `7b644a7`)

```text
$ tests/check_harness_determinism.sh          # 18 fixtures x2
main-tema-vision: 626412d8b6f47f1ce76f0ec700dd16b1d2f4e975a9c81ffc698cdffd25de609a
main-tema-vision-cwr: 25fd0585073f3c1218df046055bc7778c29f688b68f7550364e15ff12c1720b9
main-tema-vision-usbd: aab452ee2ca0f143efbeaf6dc9159448dd706e9dedf565420b2748bbefbf96d6
... (las otras 15 con el sha256 del tip previo, salvo main-tema-vision por el fix)
PASS: deterministic (identical PNGs for every fixture)

$ uv run x6200-ab audit --revision 9c031c8 --suite visual       # A, sólo los fixtures
audited 9c031c85d4ea: 14 known issues — 0 reproduced, 12 unverified; structural
inspection REAL over 1572 objects on 18 scenario(s): 1 structural finding(s)
  FAIL typography_text_overflow [main-tema-vision-usbd]: text 'USB-D' does not fit
  its 56x26 box (WRAP)

$ uv run x6200-ab audit --revision HEAD --suite visual          # B, el fix
audited 7b644a771458: 14 known issues — 0 reproduced, 12 unverified; structural
inspection REAL over 1572 objects on 18 scenario(s): 0 structural finding(s)

$ uv run x6200-ab contracts --revision HEAD
contracts 7b644a771458: PASS — 180 checks, 0 failure(s), 0 unchanged baseline defect(s)

$ uv run x6200-ab compare --a 9c031c8 --b HEAD --experiment-id AB-UI04-EXP5
experiment AB-UI04-EXP5: PASS (exit 0) — 15/18 byte-idénticas; los 3 diffs de
VISION dentro de barra_superior

$ uv run x6200-ab benchmark --a 9c031c8 --b HEAD --experiment-id BENCH-UI04-EXP5-repeat
benchmark 9c031c8 (A) vs 7b644a7 (B): PASS (exit 0) — 18 escenarios, p95 entre
-10.7% y +9.1% (dentro del presupuesto del 25%)

$ uv run pytest -m "not e2e" -q            → 120 passed, 17 deselected
$ uv run pytest -m e2e -q                  → 17 passed
$ tests/native/run_native_tests.sh         → 545 comprobaciones, 0 fallos + 7, 0 fallos
```

`BENCH-UI04-EXP5` (la primera corrida) **falla** en `main-tema-rs` (+38.6% p95) y su
repetición `BENCH-UI04-EXP5-repeat` pasa con `main-tema-rs` a −2.7 %: `main-tema-rs`
es **byte-idéntico** entre A y B (el fix no lo toca), así que el FAIL es ruido del
host (load average ~4), el mismo caso que `BENCH-UI04-EXP3`/`-repeat`. Las dos
corridas quedan commiteadas.

## Inventario de contraste estático por tema (UI-06 incremento 1, issue #12) ✅

Gate: `x6200-ab themes` reporta **todos** los temas que sirve la ROM, no sólo los
tres archivos que resolvía antes, con una fila por tema (fuente/schema → pares →
resultado) y los que no puede resolver declarados **UNSUPPORTED** con motivo.
Nada del render cambia (no se toca `src/`, fixtures ni harness).

### `x6200-ab themes` (offline, sin Docker)

Antes: `3 shipped file(s) … 27 pair(s) checked` (sólo Holo, Material 3 y RS).
Ahora: `7 resolvable theme(s) (4 built-in + 3 file(s)), 9 pair(s) declared,
63 pair(s) checked, 0 failure(s), 2 unsupported (declared)`.

| Tema | Fuente (schema) | En el menú | Pares | Resultado |
|---|---|---|---|---|
| IU1WXY | `builtin` — `src/theme.h` `THEME_COLORS(X)` | sí | 9/9 | PASS |
| Legacy | `builtin` — `src/theme.h` | **no** (`theme_list()` lo saltea) | 9/9 | PASS |
| Outdoor | `builtin` — `src/theme.h` | sí | 9/9 | PASS |
| Classic | `builtin` — `src/theme.h` | sí | 9/9 | PASS |
| Outdoor-Negative | derivado — `use_builtin()` (`negative()` sobre Outdoor) | sí | 0/9 | **UNCOVERED** |
| VISION | derivado — `use_builtin()` (`vision_colors[]` sobre IU1WXY) | sí | 0/9 | **UNCOVERED** |
| Holo | `file` — `Holo.json` (base IU1WXY) | sí | 9/9 | PASS |
| Material 3 | `file` — `Material 3.json` (base IU1WXY) | sí | 9/9 | PASS |
| RS | `file` — `RS/theme.json` (base Classic) + PNG | sí | 9/9 | PASS *(parcial)* |

- **`builtin`**: las cuatro paletas de datos de `src/theme.h` — las mismas que un
  archivo usa como `base`. Se miden acá también, así que ningún tema servido queda
  fuera del mapa. Los ratios coinciden con `tests/check_wcag.py` (mismo par, misma
  paleta).
- **UNCOVERED — Outdoor-Negative y VISION**: su paleta se arma en **runtime** con
  `use_builtin()` (`src/theme.c`), no es data. Re-derivarla en el framework sería
  re-implementar ese C en Python (una segunda fuente de verdad que podría divergir
  en silencio), así que se declaran `unsupported` en `quality/theme_pairs.yaml` con
  el motivo, en vez de forzar el parser. Las cubren `tests/check_wcag.py` (que sí
  deriva las nueve paletas) y los fixtures `main-tema-outdoor-negative` y
  `main-tema-vision`. Quedan como el próximo incremento de #12 si se quiere la
  derivación dentro del framework.
- **RS (parcial)**: su bloque `"images"` pinta `bar/btn/btn_active/btn_page/strip/
  dialog` con PNGs binarios. El par de los roles `dialog` se mide igual (el rol es
  el valor que `src/theme.c` deja cuando la imagen falta) pero el píxel real sale
  del PNG: el resultado se marca `binary` y el tema queda como cobertura
  **partial**. Las superficies `panel`/`list` no usan imagen y se miden normales.
  El fixture `main-tema-rs` valida el render real.

### Hallazgos estáticos (ejecutado)

**Ninguno.** Los 63 pares de los 7 temas resolubles cumplen WCAG (normal ≥ 4.5:1,
no-texto/texto-grande ≥ 3.0:1); los 2 temas derivados quedan UNCOVERED, no en
falta. El sweep completo del propio ROM (`tests/check_wcag.py`, las nueve paletas
y ~135 pares por tema) también cierra en **0 FALLAS**: lo único por debajo del
umbral son los pares que la tabla **excluye a propósito** (WCAG 1.4.3,
deshabilitado/incidental: `btn_off_text`, `text_mute`, `text_faint`, `icon_off`,
`switch_off`, `off`, `track`) y que `check_wcag.py` imprime sin fallar. Por eso
este incremento **no agrega entradas** a `known_issues.yaml`: el registro sólo
acepta defectos que el arnés **reproduce** en pantalla, y acá no hay ninguno
(ni estático). Ver `docs/UI_KNOWN_ISSUES.md` → *estático, sin fixture*.

### Evidencia (host, 2026-10-14, `318b9e7` + este cambio)

```text
$ cd tools/ab_testing && uv run x6200-ab themes
themes PASS: 7 resolvable theme(s) (4 built-in + 3 file(s)), 9 pair(s) declared, 63 pair(s) checked, 0 failure(s), 2 unsupported (declared)
  IU1WXY (src/theme.h; built-in palette): all 128 roles from the C table
  Legacy (src/theme.h; built-in palette): all 128 roles from the C table [not offered in the menu]
  Outdoor (src/theme.h; built-in palette): all 128 roles from the C table
  Classic (src/theme.h; built-in palette): all 128 roles from the C table
  Holo (rootfs/usr/share/x6200/themes/Holo.json; base IU1WXY): 38/128 roles from the base
  Material 3 (rootfs/usr/share/x6200/themes/Material 3.json; base IU1WXY): 33/128 roles from the base
  RS (rootfs/usr/share/x6200/themes/RS/theme.json; base Classic): 53/128 roles from the base [binary surfaces: bar, btn, btn_active, btn_page, dialog, strip]
  UNSUPPORTED Outdoor-Negative (builtin): paleta generada en runtime por use_builtin() (src/theme.c) …
  UNSUPPORTED VISION (builtin): paleta generada en runtime por use_builtin() (src/theme.c) …

$ uv run ruff check src tests && uv run ty check       → All checks passed
$ uv run pytest -m "not e2e" -q                        → 132 passed, 17 deselected
```

El render no se toca: los fixtures y `src/` quedan byte-idénticos, así que los 18
hashes de determinismo y los contratos de `HEAD` no cambian (ver *Pendientes* para
el detalle del host).

## Perfil del renderer (UI-07 incremento 1, issue #13) ✅ — sin cambio de render

Primer incremento de #13 (*optimizar el costo de presentación de espectro/cascada*):
**perfil primero**, con el bench existente y la lectura del código, sin tocar `src/`.
El incremento **no implementa** la invalidación parcial: el perfil no confirma el
candidato que la spec suponía, por dos razones medidas (abajo). No se mueve un píxel
ni un contador: `compare` 18/18 idénticas, `benchmark` Δ0, determinismo con los hashes
del tip base.

### 1. El full-frame redraw de 384000 px **no** viene del firmware: lo fabrica el bench

La spec partía de `redrawn_area_mean = 384000` (= 800×480) y `flushes_mean = 1.0` en
todos los escenarios y preguntaba si eso venía de que espectro/cascada invalidan el
objeto entero cada tick. **No**: viene del lazo del bench.

`tools/ui_harness/src/harness_main.c:307-325` — con `--bench`, **cada** pase medido
invalida la **pantalla completa** y repinta:

```c
for (int p = 0; p < scenario.bench_redraw_passes; p++) {
    lv_obj_invalidate(lv_scr_act());   /* harness_main.c:321 */
    lv_refr_now(NULL);
}
```

y el display del arnés es un framebuffer de pantalla completa con `flush_cb` 1:1
(`tools/ui_harness/src/display.c:37` `malloc(800*480)`, `display.c:15-36`). Así que el
rectángulo del flush es la pantalla entera **por construcción**, sea cual sea el
contenido del fixture. Las tres pruebas de que es el lazo y no los widgets:

1. **Todos** los escenarios miden lo mismo, incluido `dialog-about` (un diálogo, sin
   espectro ni cascada): `redrawn_area_mean = 384000`, `unique_redrawn_mean = 384000`,
   `flushes_mean = 1.0` (tabla 1).
2. Con `bench_redraw_passes 3` (`bench-render`) el área es **exactamente** 3×384000 =
   1152000 px y los flushes 3.0 (`AB-e2e-benchmark`, milestone 4): el contador escala
   con el número de veces que el lazo invalida la pantalla, no con lo que dibujan los
   componentes.
3. Traza real de los flushes (protocolo al final de la sección 2): con `--bench` los 8
   pases (3 de warm-up + 5 medidos) son `800x480` cada uno, sin importar el escenario.

> Consecuencia directa: **ningún** cambio de invalidación en el firmware puede mover
> `redrawn_area_mean`/`flushes_mean`. La puerta de `benchmark` que pedía la spec para
> este experimento (`área ↓`) es inalcanzable por diseño del arnés, no por el código
> de la GUI.

### Tabla 1 — bench por escenario (`BENCH-UI07-P1`, 20 frames c/u, host x86)

`uv run x6200-ab benchmark --revision HEAD --experiment-id BENCH-UI07-P1` → PASS.
Los `render p50/p95` son el proxy de costo del **repaint completo forzado** por el
lazo; `flushes`/`área` son constantes del arnés (ver arriba).

| escenario | frames | flushes_mean | redrawn_area_mean | unique_redrawn_mean | render p50 (ms) | render p95 (ms) | cv % |
|---|---|---|---|---|---|---|---|
| `bench-render` | 20 | 1.0 | 384000 | 384000 | 3.465 | 5.270 | 14.9 |
| `dialog-about` | 20 | 1.0 | 384000 | 384000 | 6.122 | 6.620 | 8.8 |
| `home-bateria-externa` | 20 | 1.0 | 384000 | 384000 | 3.202 | 3.798 | 10.7 |
| `home-frecuencia-extrema` | 20 | 1.0 | 384000 | 384000 | 3.166 | 3.220 | 2.9 |
| `home-usb-20m` | 20 | 1.0 | 384000 | 384000 | 3.219 | 3.375 | 10.9 |
| `main-bateria-baja` | 20 | 1.0 | 384000 | 384000 | 3.179 | 3.817 | 7.1 |
| `main-espectro-agua` | 20 | 1.0 | 384000 | 384000 | 3.397 | 3.774 | 12.6 |
| `main-foco` | 20 | 1.0 | 384000 | 384000 | 3.268 | 3.537 | 11.2 |
| `main-otro-tema` | 20 | 1.0 | 384000 | 384000 | 3.125 | 3.254 | 1.9 |
| `main-tema-classic` | 20 | 1.0 | 384000 | 384000 | 3.226 | 4.274 | 16.3 |
| `main-tema-holo` | 20 | 1.0 | 384000 | 384000 | 3.288 | 3.751 | 10.4 |
| `main-tema-material-3` | 20 | 1.0 | 384000 | 384000 | 3.341 | 3.638 | 6.8 |
| `main-tema-outdoor-negative` | 20 | 1.0 | 384000 | 384000 | 3.232 | 3.443 | 12.2 |
| `main-tema-rs` | 20 | 1.0 | 384000 | 384000 | 3.228 | 3.402 | 9.7 |
| `main-tema-vision` | 20 | 1.0 | 384000 | 384000 | 3.303 | 3.657 | 6.5 |
| `main-tema-vision-cwr` | 20 | 1.0 | 384000 | 384000 | 3.314 | 3.345 | 1.1 |
| `main-tema-vision-usbd` | 20 | 1.0 | 384000 | 384000 | 3.295 | 3.764 | 8.2 |
| `main-tx-simulado` | 20 | 1.0 | 384000 | 384000 | 3.287 | 3.568 | 10.6 |

El costo real de un repaint completo es ~3.2-3.5 ms (p50) en x86 y lo que más pesa es
`dialog-about` (6.1 ms: el diálogo real sobre la pantalla principal). Ese es el número
que el banco traduciría a presupuesto de frame en el Cortex-A7; **no** es la autonomía
ni el costo por trama del radio.

### 2. Dónde invalida el firmware (y qué mide el camino **natural**)

El harness tiene una segunda vista, más fiel, de la invalidación: la captura normal
(sin `--bench`), que termina con **un** repaint completo forzado
(`harness_main.c:327`) pero durante el avance de reloj virtual deja correr los timers
y el `scheduler`, que es donde el firmware invalida por sí mismo. Ahí los flushes
**no** son la pantalla: son objetos.

Traza real de los flushes de `home-usb-20m` (protocolo al final de esta sección;
`DIAGINV` son las áreas invalidadas que LVGL acumuló en el ciclo y `DIAGOBJ` el nodo
del árbol cuyo box + 5 px coincide con el rect que se flushea):

```text
DIAGFLUSH #0 t=1010 area=0,0 800x480
DIAGINV inv_p=1: #0[0,0 800x480]
DIAGOBJ d=0 other coords=0,0 800x480          # la pantalla: repaint forzado por el arnés
DIAGFLUSH #1 t=1040 area=0,0 800x165
DIAGINV inv_p=2: #0[0,0 800x165] #1[0,235 800x245]
DIAGOBJ d=1 other coords=0,0 800x160          # spectrum_init(): el objeto del espectro
DIAGFLUSH #2 t=1040 area=0,235 800x245
DIAGOBJ d=1 other coords=0,240 800x240        # waterfall_init(): el objeto
DIAGOBJ d=2 other coords=0,240 800x240        # y su lv_img (el que invalida refresh_waterfall)
DIAGFLUSH #3 t=1500 area=580,0 220x72         # reloj (timer de 500 ms; caja previa del label)
DIAGFLUSH #4 t=2000 area=580,0 220x72         # reloj
DIAGFLUSH #5 t=2200 area=0,0 800x480          # repaint completo forzado por el arnés
```

`main-foco` es igual más un flush #3 a t=1100 con la banda del espectro
(`0,0 800x165`, `DIAGOBJ coords=0,0 800x160`): es el `lv_group_focus_next()` del
settle, que invalida el objeto por el cambio de estado de foco.

Suma = 2×384000 + 132000 + 196000 + 2×15840 = **1127680 px / 6 flushes**, que es
exactamente el `metrics.flushed_pixels`/`metrics.flushes` que la captura venía
reportando en las 18 fixtures (commit `9c031c8`, `reports/AB-UI04-EXP5/metrics/a.json`
y verificado de nuevo en HEAD). `main-foco` tiene **7** flushes y
1259680 = 1127680 + 132000 px: la banda del espectro del `lv_group_focus_next()` del
settle.

| escenario (captura, HEAD) | flushes | flushed_pixels |
|---|---|---|
| `main-foco` | 7 | 1259680 |
| `main-tema-holo` / `-material-3` / `-vision` / `-vision-cwr` / `-vision-usbd` | 6 | 1128266 |
| los otros 12 | 6 | 1127680 |

De esas dos vistas sale el reparto del trabajo por actualización (el rect real con el
que el firmware invalida y cuánto pesa contra el frame completo):

| región | rectángulo medido | px | qué la invalida |
|---|---|---|---|
| espectro | `0,0 800x165` | 132000 | `spectrum_refresh()` → `lv_obj_invalidate(obj)` (`src/spectrum.c:274-276`) |
| cascada | `0,235 800x245` | 196000 | `refresh_waterfall()` → `redraw_cb()` + `lv_obj_invalidate(img)` (`src/waterfall.c:288-295`) |
| reloj/estado | `580,0 220x72` | 15840 | `show_time()` cada **500 ms** (`src/clock.c:155`) → `lv_label_set_text` (`src/clock.c:140`) |
| resto (frecuencia, spots, band plan, botones, panel) | — | 0 | solo en el paint inicial |

Los rectángulos son **el box del objeto más 5 px de cada lado**, recortado al
viewport. La cadena es: `lv_obj_invalidate` infla la caja por `ext_draw_size` (0 en
estos objetos), `lv_obj_area_is_visible` recorta contra el padre y llama a
`lv_obj_get_transformed_area`, que termina en `lv_area_increase(area, 5, 5)`
(`lvgl/src/core/lv_obj_pos.c`), y `_lv_inv_area` recorta al viewport
(`lvgl/src/core/lv_refr.c`). El `DIAGOBJ` de arriba lo verifica en los dos sentidos:
el objeto del espectro (0,0 800x160) da 800x165 por el borde superior recortado, y la
imagen de la cascada (0,240 800x240) da 800x245. Si en un ciclo hay dos áreas y una
queda **contenida** en la otra, `_lv_inv_area` descarta la segunda (`lv_refr.c:246-250`):
es lo que pasa con el reloj, cuyo label cambia de caja al re-renderizarse —la caja
previa (585,5 210x62) +5 da el `580,0 220x72` medido y la nueva (591,0 209x61) queda
adentro, por eso el ciclo cierra con `inv_p=1`. Los 5 px son de LVGL, **no** del
firmware (son hasta 8000 px por objeto de ancho completo), y son el único renglón de
este perfil que se podría atacar desde un `rounder_cb` del driver, no desde `src/`.

Protocolo de la traza (diagnóstico **temporal**, revertido antes de commitear: el árbol
queda sin cambios en `tools/ui_harness/`). En `tools/ui_harness/src/display.c`, dentro
de `flush_cb`, un `fprintf(stderr, ...)` por flush con el rectángulo y el reloj virtual,
más las áreas invalidadas acumuladas en `lv_disp_get_default()->inv_areas` y el nodo
del árbol cuyo box + 5 px coincide con el rect (recorrido recursivo); se corre el
harness local (`docker run … x6200-uiharness`, `make` + `./ui_harness`) capturando
`stderr`. El PNG y los contadores no cambian (la instrumentación no toca el camino de
dibujo; los 18 sha256 no se movieron durante el diagnóstico):

```diff
+#include <stdio.h>
+#include "virtual_time.h"
 static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *px)
 {
     uint32_t pixels = 0;

+    static unsigned flush_seq;
+    lv_disp_t *d = lv_disp_get_default();
+    fprintf(stderr, "DIAGFLUSH #%u t=%llu area=%d,%d %dx%d\n", flush_seq++,
+            (unsigned long long) virtual_time_now_ms(), area->x1, area->y1,
+            area->x2 - area->x1 + 1, area->y2 - area->y1 + 1);
+    if (d) {
+        fprintf(stderr, "DIAGINV inv_p=%u:", d->inv_p);
+        for (uint16_t i = 0; i < d->inv_p; i++) {
+            fprintf(stderr, " #%u[%d,%d %dx%d]%s", i,
+                    d->inv_areas[i].x1, d->inv_areas[i].y1,
+                    d->inv_areas[i].x2 - d->inv_areas[i].x1 + 1,
+                    d->inv_areas[i].y2 - d->inv_areas[i].y1 + 1,
+                    d->inv_area_joined[i] ? "(joined)" : "");
+        }
+        fprintf(stderr, "\n");
+    }
+    diag_walk(lv_scr_act(), area, 0);   /* coords + 5 px recortados al viewport */
     bench_flush_begin();
```

### 3. Hipótesis ordenadas por área ahorrable (y qué necesitaría cada una)

| # | candidato | área por actualización | ¿ahorrable con el PNG idéntico? | qué necesitaría para ser válido |
|---|---|---|---|---|
| H2 | **cascada**: invalidar la imagen entera por cada fila nueva | 196000 px (51% del frame) | **No** | la cascada se "scrollea" con un remapeo modular: `dst_y = ((height - src_y + last_row_id) % height)` (`src/waterfall.c:258`) y `redraw_cb` reescribe los 800×240 px de `frame`; **cada** fila destino cambia con cada fila nueva. Invalidar solo la fila nueva deja la imagen vieja (PNG distinto). El ahorro de *trabajo de C* que sí existe (memmove + 1 fila) es un cambio de **algoritmo**, no de invalidación, y el bench no lo mide (`redraw_cb` corre al llegar datos, `src/waterfall.c:137`, no en el repaint forzado). |
| H1 | **espectro**: invalidar el objeto entero por cada trama de FFT | 132000 px (34%) | **No** | `spectrum_data()` reemplaza los **800** bins (`SPECTRUM_NFFT` = `SPECTRUM_SIZE` = 800, `src/dsp.h:32`/`src/spectrum.c:31`, llamada en `src/dsp.cpp:189`) y `spectrum_draw_cb` dibuja las 800 columnas desde ese buffer (`src/spectrum.c:121-174`): todos los píxeles del objeto pueden cambiar. La invalidación actual ya es el mínimo correcto. |
| H4 | banda del reloj/estado | 15840 px cada 500 ms | No | es `lv_label_set_text` sobre el label del reloj (se invalida su caja previa, 210x62): no hay menos que invalidar. 1.4% del costo natural. |
| H3 | los 5 px por lado que LVGL agrega a cada objeto | 8000 px por objeto de ancho completo | No, y no es del firmware | sería un `rounder_cb` del driver (arnés/display real), no un cambio de `src/`. |
| H5 | resto de la pantalla (filas 165..234, 56000 px) | 0 | — | los objetos estáticos (frecuencia, spots, band plan, botones, panel) **no** se invalidan por frame: no hay costo que recortar. |

Orden por área: H2 (196000) > H1 (132000) > H4 (15840) > H3 > H5. **Ninguno ahorra
área con el PNG idéntico**, así que este incremento no toca `src/` (spec §3).

### 4. Prueba diferencial (anti-falso-verde, y por qué la Parte B no se implementa)

Para que la conclusión no sea una lectura de código, se forzó una invalidación parcial
en el espectro **y se revirtió** (mismo protocolo temporal que usan las verificaciones
anti-falso-verde de los incrementos anteriores):

```diff
 static void spectrum_refresh(void *data) {
-    lv_obj_invalidate(obj);
+    lv_area_t area;
+    lv_obj_get_coords(obj, &area);
+    area.y1 = area.y1 + 80;          /* sólo la mitad inferior: 800x80 */
+    lv_obj_invalidate_area(obj, &area);
 }
```

Resultado (3 fixtures, HEAD, harness local):

```text
                            flushes  flushed_pixels   PNG sha256
home-usb-20m     antes            6         1127680   a656e0961a41c890...
                 parcial          6         1067680   a656e0961a41c890...  (idéntico)
bench-render     antes            6         1127680   5ba21bea6dcd30cf...
                 parcial          6         1067680   5ba21bea6dcd30cf...  (idéntico)
main-espectro-agua antes          6         1127680   41cc0d5051a06250...
                 parcial          6         1067680   41cc0d5051a06250...  (idéntico)

--bench, con la invalidación parcial:  flushes=1  redrawn_area=384000  unique=384000  (sin cambio)
```

Los −60000 px son exactamente la banda que se dejó de invalidar (132000 → 72000:
`0,75 800x90` con los 5 px de LVGL). Tres hechos, los tres incómodos para la hipótesis
de la spec:

1. **El bench es ciego a la invalidación**: `redrawn_area`/`flushes` no se mueven ni un
   dígito. La puerta que la spec proponía no puede aprobar ni rechazar el cambio.
2. **El camino natural sí es sensible** (1127680 → 1067680, determinista): existe un
   contador del framework que mide el trabajo de invalidación de verdad.
3. **El gate de PNG no puede validar una invalidación parcial**: la mitad del área del
   espectro sin invalidar da un PNG **byte-idéntico**. El motivo es del arnés: los datos
   se alimentan **una sola vez**, al construir la pantalla
   (`tools/ui_harness/src/screens.c:54` → `signal_gen_feed`), así que el
   `lv_obj_invalidate` del espectro a t=1040 es redundante (su contenido ya se pintó
   igual en el paint inicial). Con datos por frame (como en el radio: `spectrum_fps_ms`
   / `waterfall_fps_ms`, `src/dsp.cpp:186-197`) esa mitad habría quedado vieja.

Lo que esto dice del incremento: implementar la invalidación parcial en
espectro/cascada sería (a) **incorrecto en el radio** (los 800 bins y las 240 filas
cambian enteros por actualización, así que cualquier sub-región deja píxeles viejos) y
(b) **indemostrable en host**: el gate de PNG no distingue una invalidación parcial
correcta de una rota con los fixtures actuales, y el gate de `benchmark` no ve ninguna
de las dos. Sin camino de verificación no hay experimento: se termina el incremento con
el perfil (spec §3), sin tocar `src/`.

### 5. Qué proveería el banco/ARM y qué le falta al arnés

- **ARM / banco (hardware-pass, BLOCKED)**: el Cortex-A7 con el driver real mide lo que
  el host no: la **cadencia** real (`spectrum_fps_ms`/`waterfall_fps_ms`, una trama de
  espectro y una fila de cascada por intervalo, `src/dsp.cpp:186-197`) y el costo del
  flush real del framebuffer (el driver copia el rectángulo a la pantalla; el arnés
  copia a RAM). Ahí `1127680 px` deja de ser un número de `memcpy` y pasa a ser trabajo
  de bus/DRAM. Nada de esto se puede atribuir desde el host.
- **Próximo incremento del arnés (si #13 quiere medir invalidación en host)**: (1) un
  modo de bench que **no** fuerce `lv_obj_invalidate(lv_scr_act())` sino que corra
  timers/scheduler sobre el reloj virtual y mida los flushes que el firmware produce;
  (2) fixtures con **datos por frame** (hoy `signal_gen_feed` corre una vez,
  `screens.c:54`), sin los cuales ningún cambio de invalidación en espectro/cascada es
  verificable en píxeles (medido: la mitad del área da PNG idéntico). Sin (1) y (2),
  el trabajo de presentación de #13 no es host-demostrable y queda para el banco.

### Evidencia (host, 2026-10-14, `d9c63b2` = tip base; este incremento no cambia `src/`)

```text
$ tests/check_harness_determinism.sh          # 18 fixtures x2, hashes = los del tip base
bench-render: 5ba21bea6dcd30cffeea0bf171f578ba3fb51c811e44aa22c875c5fc74378beb
home-usb-20m: a656e0961a41c890bd6663a46dc3d36d2606df4f0b56eae126a88ac32bb974f9
main-tema-vision-usbd: aab452ee2ca0f143efbeaf6dc9159448dd706e9dedf565420b2748bbefbf96d6
main-foco: a656e0961a41c890bd6663a46dc3d36d2606df4f0b56eae126a88ac32bb974f9
PASS: deterministic (identical PNGs for every fixture)

$ cd tools/ab_testing && uv run x6200-ab benchmark --revision HEAD --experiment-id BENCH-UI07-P1
benchmark d9c63b2d92b5: PASS (exit 0) — tabla 1

$ uv run x6200-ab compare --a d9c63b2 --b HEAD --experiment-id AB-UI07-T1
experiment AB-UI07-T1: PASS (exit 0) — 18/18 fixtures pixel-identical (changed_pixels 0)
  (A y B son el mismo commit: este incremento sólo agrega este documento y los reportes)

$ uv run x6200-ab benchmark --a d9c63b2 --b HEAD --experiment-id BENCH-UI07-T1
benchmark d9c63b2d92b5 (A) vs d9c63b2d92b5 (B): FAIL (exit 1) por p95 en 2 escenarios
  flushes 1.0 -> 1.0 y área 384000 -> 384000 px en los 18 (Δ 0.0%): el trabajo es idéntico
  bench-render +38.7% y main-espectro-agua +47.8% de p95 — ruido del host (load ~1.9)
$ uv run x6200-ab benchmark --a d9c63b2 --b HEAD --experiment-id BENCH-UI07-T1-repeat
benchmark ...: FAIL (exit 1) — ahora falla OTRO escenario (main-tema-classic +39.8%) y los
  dos anteriores pasan (bench-render -5.1%, main-espectro-agua +6.1%); flushes/área Δ 0.0%
  otra vez. Los dos reportes quedan commiteados lado a lado.

$ uv run x6200-ab contracts --revision HEAD
contracts d9c63b2d92b5: PASS — 180 checks, 0 failure(s), 0 unchanged baseline defect(s)

$ uv run x6200-ab audit --revision HEAD --suite visual
audited d9c63b2d92b5: 14 known issues — 0 reproduced, 12 unverified; structural
inspection REAL over 1572 objects on 18 scenario(s): 0 structural finding(s)

$ uv run pytest -m "not e2e" -q   → 132 passed, 17 deselected
$ uv run pytest -m e2e -q         → 17 passed
$ tests/native/run_native_tests.sh → 545 comprobaciones, 0 fallos + 7, 0 fallos
repo static checks (cfg_fills/actions/settings_focus/apps_menu/app_handlers/wcag) → PASS
ruff check / ruff format --check → limpios; ty (con la lista de archivos) → limpio
```

El `FAIL` de las dos corridas de `benchmark` es el caso ya documentado
(`BENCH-UI04-EXP3-repeat`, `BENCH-UI05-T3-repeat`): con load ~2-4 el veredicto de p95 a
4 rondas × 5 frames es ruido, y acá el A/B es **el mismo commit**, así que no hay
regresión posible. Lo que decide el costo real (flushes y área redibujada) es idéntico
y estable en las 18. Los reportes quedan en
[`tools/ab_testing/reports/AB-UI07-T1/`](../tools/ab_testing/reports/AB-UI07-T1/),
`BENCH-UI07-P1/`, `BENCH-UI07-T1/` y `BENCH-UI07-T1-repeat/`.

## Modo bench natural: datos por frame e invalidación real (UI-07 incremento 2, issue #13) ✅

El incremento 1 cerró con lo que faltaba **en el arnés** (no en `src/`): (1) un bench que
no forzara `lv_obj_invalidate(lv_scr_act())` + `lv_refr_now()` sino que corrieran los
timers/scheduler sobre el reloj virtual, y (2) fixtures con **datos por frame** (hoy
`signal_gen_feed()` corre una sola vez, `screens.c:54`). Este incremento entrega los dos
como **modo extra y adicional**: `--bench-natural`. El modo forzado (`--bench`), la
captura, `compare`, `contracts` y `audit` conservan formato y números; los 18 fixtures
existentes no se tocan.

### Cómo se usa

```sh
# El loop natural de LVGL, N frames de tiempo virtual, datos por frame del
# generador determinista (el fixture declara `natural_frames`, el CLI puede pisarlo):
./ui_harness --scenario ../fixtures/main-natural.scn \
    --png out/natural.png --json out/natural.json --bench-natural
./ui_harness … --bench-natural 10          # override del conteo (el fixture dice 40)
./ui_harness … --natural-frames 10         # lo mismo, forma larga
./ui_harness … --bench-natural --natural-static   # control de delimitación (sin drive)
```

Un **frame natural** es un período de refresh de LVGL (`LV_DISP_DEF_REFR_PERIOD`, 30 ms de
reloj virtual), `slice_ms = 10`. La señal sale del generador determinista que ya existía
(`src/signal_gen.c`), ahora con dos entradas nuevas —`signal_gen_spectrum_frame(seed,
frame)` y `signal_gen_waterfall_row(seed, row)`— que alimentan `spectrum_data()` y
`waterfall_data()` con el mismo seed del fixture y filas que avanzan por frame: sin
`rand()`, sin reloj, sin estado compartido. La cadencia espeja la del lazo de DSP del
radio (`src/dsp.cpp`: `spectrum_fps_ms = 1000/15`, `waterfall_fps_ms = 1000/25`, condición
`now - last > fps_ms`); el reloj invalida cada 500 ms por su propio timer
(`src/clock.c`). El PNG y los `metrics` no cambian de formato: la sección nueva es
`natural`.

Salida real de un frame típico (de `reports/NATURAL-UI07-N1/natural.json`):

```json
{ "frame": 2, "virtual_end_ms": 2290, "refresh_cycles": 1,
  "invalidated_px": 132000, "invalidated_rects": 1, "joined_px": 0,
  "flushed_px": 132000, "flushes": 1, "rects_complete": true,
  "rects": [[0, 0, 800, 165]] }
```

`invalidated_px` es lo que LVGL acumuló en su buffer de invalidación para el ciclo
(`inv_areas`/`inv_p`, leído dentro del flush antes de que LVGL lo limpie: la misma traza
que el incremento 1 usó como diagnóstico temporal), `joined_px` es lo que LVGL mergeó
(para no contarlo dos veces) y `flushed_px`/`flushes` es lo que recibió el driver. Con el
draw buffer full-screen del arnés, 1:1.

### Checkpoints medidos y correlación con el perfil del incremento 1

`main-natural` (seed 13, 40 frames = 1200 ms, `325ec32`, run x86):

| región | rectángulo | px | veces / 40 frames | qué la invalida |
|---|---|---|---|---|
| espectro | `0,0 800x165` | **132000** | 16 | `spectrum_refresh()` (src/spectrum.c) |
| cascada | `0,235 800x245` | **196000** | 23 | `refresh_waterfall()` (src/waterfall.c) |
| reloj | `580,0 220x72` | **15840** | 1 | `show_time()` cada 500 ms (src/clock.c) |
| band info | `0,241 800x34` | 27200 | 4 | fade del timer de 2 s (src/band_info.c) |
| icono BT | `152,23 30x36` | 1080 | 1 | `bt_state_timer_cb`, timer de 2 s (src/info.c) |

Los tres rectángulos del perfil del incr. 1 coinciden **exactos** (132000 px por trama de
FFT, 196000 px por fila, 15840 px por tick de 500 ms). Los otros dos son de los timers de
2 s del firmware, que solo disparan porque esta corrida llega a t > 3 s (la captura común
avanza 1.2 s y no los ve; el trace del incr. 1 tampoco). **Ninguna otra región se
invalida**: cada rectángulo de cada frame es uno de esos cinco, y eso lo asserta el gate.
De los 40 frames, 37 tienen trabajo y 3 no (nada que refrescar): totales 6745720 px en 45
rectángulos/45 flushes. Los conteos de drive (17 tramas de espectro, 24 filas de cascada)
son los que produce una reimplementación independiente de la condición de `dsp_samples()`
con los dos `fps` del radio sobre slices de 10 ms, y `metrics.flushes = 6 + 45`,
`metrics.flushed_pixels = 1127680 + 6745720`: la captura settle conserva sus números y los
frames naturales **suman** los propios.

### Self-tests (el producto)

`tests/check_harness_natural.sh` (corre el arnés en Docker, cuatro modos):

| self-test | resultado |
|---|---|
| idempotencia: 2 corridas del natural con el mismo seed | PNG sha `8ec63121…` las dos veces (**idéntico**) y **todo** el JSON byte a byte igual (`cmp`) |
| correlación con el perfil | espectro 132000 ×16, cascada 196000 ×23, reloj 15840 ×1 — exactos |
| delimitación vs el drive (`--natural-static`) | 304760 px = **4.5%** de los 6745720: sin drive solo quedan reloj/BT/band-info → las áreas del perfil no pueden venir de otra cosa que del drive determinista |
| los datos por frame llegan a pantalla | `framebuffer_fnv1a` del natural (`0x1cf41b099f85ece4`) ≠ del control (`0x6576db34a91c95f8`) |
| modo forzado intacto | `--bench`: 3 frames × 1 flush × 384000 px, sin sección `natural`, mismo PNG que la captura común (`3986bc9a…`) |
| override por CLI | `--bench-natural 10` corrió 10 frames (el fixture declara 40) |

La evidencia cruda (4 JSON + PNG + hashes) queda en
[`tools/ab_testing/reports/NATURAL-UI07-N1/`](../tools/ab_testing/reports/NATURAL-UI07-N1/),
y `tools/ab_testing/tests/test_natural.py` corre el mismo chequeo en `pytest -m "not e2e"`
sobre esa evidencia (no necesita Docker).

### Cómo lo siguen #13/#14

* **#13 (optimizar el costo de presentación)**: quedó el instrumento que faltaba. Ahora
  una invalidación parcial en espectro/cascada es **host-demostrable**: se cambia `src/`
  (por ejemplo invalidar por área en `spectrum_refresh()`), se corre `--bench-natural` en
  A y B y se comparan las tres cosas que el incr. 1 no podía medir: `invalidated_px`
  (trabajo de invalidación), `rects` (qué región exacta se redibuja) y el PNG (que los
  datos por frame sí cambian los píxeles: la mitad del área ya **no** da un PNG idéntico,
  el falso-verde del incr. 1 §4 desaparece).
* **#14 y el resto del pipeline**: el modo es **extra**. `compare` sigue tomando los
  fixtures de la revisión A (los 18), `contracts`/`audit` suman `main-natural` como un
  fixture más (una captura común: 6 flushes/1127680 px, igual que `home-usb-20m`) y
  `benchmark` no cambia. El formato del JSON solo **agrega** `natural`; ninguna clave
  existente cambia de nombre ni de significado.
* **Un fixture nuevo por experimento**: el drive es del modo, no del fixture, así que
  alcanza con el fixture dedicado (o con `--natural-frames` sobre `home-usb-20m`).
* **ARM/banco**: la cadencia real bajo carga, el costo del flush real y la energía siguen
  siendo del pase de hardware (BLOCKED, sin radio). El arnés ahora reporta el trabajo
  correcto; no puede afirmar el tiempo real del Cortex-A7.
* Límites honestos del modo: los cinco rectángulos son de **este** fixture (tema por
  defecto, sin peaks, seed 13); otro tema mueve las cajas del reloj/band-info (las fuentes
  cambian la métrica). Y el aviso `_lv_inv_area: detected modifying dirty areas in render`
  sigue apareciendo 1 vez por corrida en todos los modos (también en la captura común, con
  PNGs byte-idénticos a la base): es del firmware (callback de `DRAW_POST_END` de
  band_info/middle_line), preexistente.


## Pendientes

- **UI-05 (Flex/Grid) sigue abierto**: el **incremento 1** fue la capa de tokens
  + `src/info.c`; el **incremento 2** es la conversión de la barra de info a
  `LV_FLEX_ROW`; el **incremento 3** tokeniza la fila de frecuencia
  (`src/main_screen.c`); el **incremento 4** convierte esa fila a un contenedor
  `LV_FLEX_ROW` (solo eje X, el Y sigue en el cursor apilado). Las
  otras regiones (botones, `band_plan`, filas de Ajustes) son incrementos
  siguientes, y spectrum/waterfall/band_plan quedan afuera (áreas de dibujo fijo:
  el issue prohíbe convertirlas a ciegas). Los tokens son constantes por ahora;
  consumirlos desde el JSON de tema es otro PR.

- **UI-06 (temas) incremento 1 hecho, quedan los siguientes**: el chequeo estático
  ya cubre los 7 temas resolubles (4 built-ins + 3 JSON), con **Outdoor-Negative** y
  **VISION** declarados UNCOVERED (paleta de runtime, `use_builtin()`). Los
  incrementos siguientes de #12 deciden si se deriva esa paleta dentro del framework
  o se deja sólo en `tests/check_wcag.py`, y suman focus states / waterfall palettes.
  Sin hallazgos estáticos nuevos (ver la sección de UI-06 arriba).

- **UI-07 (render) incrementos 1-2 hechos**: el **incremento 1** fue el perfil
  (sección *Perfil del renderer*) y **descartó** el candidato de invalidación parcial que
  suponía la spec (el full-frame de 384000 px lo fabrica el lazo del bench y los objetos
  de espectro/cascada cambian enteros por actualización). El **incremento 2** (sección
  *Modo bench natural*) entregó lo que el perfil pidió como paso siguiente, y todo del
  **arnés**: `--bench-natural` mide la invalidación natural con **datos por frame**
  deterministas (funciones nuevas de `src/signal_gen.c` sobre la cadencia de `src/dsp.cpp`)
  y un fixture dedicado (`main-natural`), con el modo forzado y los 18 fixtures intactos.
  Los incrementos siguientes de #13 ya tienen el instrumento: una optimización de
  presentación de espectro/cascada se puede ahora medir en áreas (`invalidated_px`,
  `rects`) **y** en píxeles (los datos por frame cambian el PNG, así que una invalidación
  parcial rota sí cambia el render). Hardware/Energy siguen **BLOCKED** (sin banco/radio):
  la cadencia real y el costo del flush real son del pase de hardware.

- **Captura física y energía** (milestone 7): sin un X6200 conectado,
  Hardware/Energy quedan BLOCKED/PENDING, nunca PASS por default.
- **Fixtures de diálogos de app** (Ajustes, menú de apps, BT/WeFax/SSTV): el
  About real ya está cubierto (`dialog-about`); Ajustes y los diálogos que
  arrastran `dialog_field`/audio/tabla siguen fuera, y con ellos las semillas
  UI-002/UI-004/UI-005 quedan `unverified`.
- **Tema VISION y fuentes de usuario**: VISION ya tiene fixture
  (`main-tema-vision`, UI-03) y sus tres defectos de tipografía quedaron **fixed**:
  los glifos (**UI-011**) en UI-04/EXP-1, el overflow de `VFO-A` (**UI-012**) en
  UI-04/EXP-2 y la celda del modo (**UI-014**) en UI-04/EXP-5 (ver arriba), cada uno
  con su excepción retirada en el mismo cambio. Ojo con UI-014: el fixture que
  **dispara** el contrato es `main-tema-vision-usbd` (`USB-D`, 66 px); el `CW-R` del
  issue mide **exactamente** el ancho de la celda (56 px), así que se pega pero no
  hace `WRAP` y el contrato no puede medirlo (queda como evidencia pixel a pixel).
  Sigue sin
  haber fixture de fuente elegida por el usuario (Roboto/DejaVu/Arial):
  `font_down()` devuelve el tamaño del slot, que con `fonts_apply()` es el de la
  fuente alternativa elegida, así que el fix de UI-012 cubre también ese camino,
  pero ninguna captura lo ejerce todavía.
- El reloj de energía dependía de una variable local sin inicializar
  (`src/clock.c`, `show_time`): quedó corregido como **UI-009** con test nativo y
  caso A/B propio (ver la sección **UI-009** arriba). Las capturas aceptadas de
  los milestones 1–6 son **previas** al fix y por eso cambian; el camino de un
  voltaje dentro de rango es byte a byte idéntico antes y después.
- **Gate QEMU ARM (#15)**: sigue **BLOCKED** y no se atribuye un PASS ficticio. El
  flow del repo (`tests/e2e_chroot.sh`) necesita la raíz extraída de la imagen
  (`../images/rootfs_*.tar`, hermana del worktree) y las bibliotecas de `../trace`:
  ninguna de las dos está en el worktree, y no hay job de CI que lo corra. Es el
  mismo estado que dejó anotado EXP-2.
- **Benchmark de render en un host cargado**: `BENCH-UI04-EXP3` (PASS) y su
  repetición `BENCH-UI04-EXP3-repeat` (**FAIL** en `bench-render`, +44% p95) miden
  **el mismo trabajo** (flushes 1.0 y 384000 px redibujados en los 16 escenarios) y
  difieren sólo en p95: con load average ~3 el veredicto de p95 a 4 rondas × 5
  frames es ruido. Lo que decide el costo real (flushes/área) es estable; conviene
  subir el muestreo o el umbral antes de confiar en un FAIL de p95 solo.
- **`ty` con un directorio no chequea nada**: `uv run ty check src tests`
  (ty 0.0.87, el comando del job `functional`) avisa `No python files found under
  the given path(s)` y sale 0 —con un archivo de prueba con un error de tipos
  deliberado el comando pasa igual—, así que ese paso del CI no está midiendo nada.
  Nombrando los archivos (`ty check <lista de .py>`) sí analiza y pasa. El step del
  workflow queda para una corrida de CI/harness (tocar `.github/**` no es de este
  experimento).
- **El par `text` sobre `bg`/`strip` de la pantalla principal no lo mide nadie**:
  `quality/theme_pairs.yaml` (estático) cubre los pares de Ajustes y de los diálogos
  sobre los temas shipeados y las cuatro paletas incorporadas (UI-06 incr. 1), y
  `typography_contrast` sólo puede medir un par
  cuando el label pinta un fondo opaco (los de la fila de frecuencia tienen
  `bg_opa 0`). Con `text` como rol el par queda bien en todos los temas medidos
  (`text`/`bg`: IU1WXY 13.24, Legacy 9.37, Outdoor 21.00, Classic 21.00, Holo 21.00,
  Material 3 14.41, RS 15.72). Ojo: para UI-010 un par estático **no** habría
  alcanzado —la paleta de Outdoor ya tenía `text` negro sobre `bg` blanco—: el
  defecto era el código escribiendo un color en vez del rol, y lo vio el contrato de
  región (`content 0.0000`). El agujero que queda es otro: un par claro-sobre-claro
  que **no** deje la región uniforme. Candidato para `/factory-correct`.

