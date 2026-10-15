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
  *(Corregido después: Ajustes **no** arrastra `dialog_field` y quedó cubierto en
  2026-10-14 — ver la sección **Fixture del diálogo de Ajustes**.)*
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
| Diálogo de Ajustes (`dialog.c` + `dialog_settings.c`) | `settings-home`, `settings-wfbat` | ✓ | | | |
| Menú de apps | — | | | | ✗ (no hay fixture del menú) |
| Otros diálogos de app (BT/WeFax/SSTV, `dialog_field.c`) | — | | | | ✗ (mocks inertes; `dialog_field` **no** es dependencia de Ajustes) |
| Servicios que Ajustes llama (audio/DSP/wfview/RTC/transverters/backlight/BT) | `settings-home`, `settings-wfbat` | | | ✓ (mocks explícitos, sin dispositivo) | |
| Tema IU1WXY | `home-usb-20m`, `main-tx-simulado`, `main-bateria-baja`, `home-frecuencia-extrema`, `home-bateria-externa`, `bench-render` | ✓ | | | |
| Tema Legacy | `main-espectro-agua`, `bench-render` | ✓ | | | |
| Tema Outdoor | `main-otro-tema` | ✓ (defecto UI-010 corregido en UI-04/EXP-3) | | | |
| Tema Outdoor-Negative | `main-tema-outdoor-negative` | ✓ | | | |
| Tema Classic | `main-tema-classic` | ✓ | | | |
| Tema RS (JSON + imágenes) | `main-tema-rs` | ✓ | | | |
| Tema Holo (JSON) | `main-tema-holo` | ✓ | | | |
| Tema Material 3 (JSON) | `main-tema-material-3` | ✓ | | | |
| Tema Aurora (JSON, THEMES v2) | `main-tema-aurora`, `main-tema-aurora-agua`, `main-tema-aurora-tx`, `main-tema-aurora-eco`, `settings-tema-aurora`, `bt-dialog-tema-aurora` | ✓ | | | |
| Tema Trail (JSON, THEMES v2) | `main-tema-trail`, `main-tema-trail-agua`, `main-tema-trail-tx`, `main-tema-trail-eco`, `settings-tema-trail`, `bt-dialog-tema-trail` | ✓ | | | |
| Tema VISION | — | | | | ✗ (font scale: hallazgos para #8) |
| Fuente por tema (Arial en Classic/RS) | `main-tema-classic`, `main-tema-rs` | ✓ | | | |
| Fuente elegida por el usuario (Roboto/DejaVu/Arial) | — | | | | ✗ (no hay fixture de fuente) |
| Espectro / waterfall | todas | | ✓ (generador determinista, sin FFT) | | |
| Catálogo de bandas (`band_info`) | todas | | ✓ (catálogo del arnés, no la DB) | | |
| Foco / encoder | `main-foco`, `settings-wfbat` | | ✓ (cadena de foco real: en `main-foco` sin resalte visible, en `settings-wfbat` el foco del tab Espectro) | | |
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

`main-natural` (seed 13, 40 frames = 1200 ms, `376ed09`, run x86):

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

### Verificación anti-falso-verde: la traza de píxeles ya no es ciega

El perfil del incr. 1 cerró con un agujero **medido**: con una invalidación parcial
temporal del espectro, el PNG quedaba byte-idéntico (1127680 → 1067680 px de flush,
mismo sha256), así que el gate de píxeles no podía distinguir una invalidación parcial
correcta de una rota — y por eso el incremento 1 no tocó `src/`. Con los datos por frame
ese agujero se cierra. Mismo experimento (patch **temporal y revertido**; evidencia cruda
en `reports/NATURAL-UI07-N1/sensitivity/`):

| corrida | PNG sha256 | captura común `flushed_pixels` | natural `invalidated_px` | rectángulo del espectro |
|---|---|---|---|---|
| base | `8ec63121…` | 1127680 | 6745720 | `0,0 800x165` (132000) |
| patch (mitad inferior) | `492910b4…` | 1067680 | 5801560 | `0,75 800x90` (72000) |

- el natural **mide** el cambio: las áreas por frame y el rectángulo del espectro se
  mueven con la invalidación (132000 → 72000 px por trama, total 6745720 → 5801560);
- el natural **ve la rotura**: el PNG cambia, porque la mitad que no se invalida queda con
  el contenido del frame anterior. Un candidato de invalidación parcial ya no puede pasar
  el gate de píxeles por accidente;
- la captura común mantiene el PNG byte-idéntico (`3986bc9a…` en las dos filas): la
  ceguera era del **camino común** (los datos se alimentan una vez), no del arnés.

### Evidencia (host, 2026-10-14, `325ec32` → `376ed09`)

```text
$ tests/check_harness_determinism.sh          # 19 fixtures (los 18 + main-natural) x2
bench-render: 5ba21bea6dcd30cffeea0bf171f578ba3fb51c811e44aa22c875c5fc74378beb
home-usb-20m: a656e0961a41c890bd6663a46dc3d36d2606df4f0b56eae126a88ac32bb974f9
main-natural: 3986bc9a6047744aa2cd8db1c9879d73e71e16e45d7737b0d7035c06e22ec821
... (los 18 previos con el MISMO sha256 del tip base)
PASS: deterministic (identical PNGs for every fixture)

$ tests/check_harness_natural.sh
  profile: spectrum 132000 px x 16 frames, waterfall 196000 px x 23 rows, clock 15840 px x 1 tick(s)
  natural totals: invalidated 6745720 px in 45 rectangles / 45 flushes over 37/40 frames
  delimitation: drive off -> 304760 px (4.5% of the 6745720 px with the drive), only clock/BT/band-info rectangles
  forced mode: 3 frames x 1 flush x 384000 px (natural: 6745720 px in 40 frames)
  CLI override: --bench-natural 10 ran 10 frames (the fixture says 40)
PASS: natural bench deterministic, profiled and delimited (main-natural)
  png sha256: 8ec631215c28e27fb901f186273fe6afa0fd6b9470550cf810cb1988f84273fd

$ cd tools/ab_testing && uv run x6200-ab compare --a 325ec32 --b HEAD --experiment-id AB-UI07-T2
experiment AB-UI07-T2: PASS (exit 0)
  Functional PASS · Visual Regression PASS (captures are pixel-identical to the baseline)
  Quality Contract PASS (180 checks) · Performance PASS · Hardware BLOCKED · Energy BLOCKED
  18/18 fixtures: changed_pixels 0

$ uv run x6200-ab contracts --revision HEAD
contracts 376ed09bc56a: PASS — 190 checks, 0 failure(s), 0 unchanged baseline defect(s)

$ uv run x6200-ab audit --revision HEAD --suite visual
audited 376ed09bc56a: 14 known issues — 0 reproduced, 12 unverified; structural
inspection REAL over 1659 objects on 19 scenario(s): 0 structural finding(s)

$ uv run pytest -m "not e2e" -q   → 136 passed, 17 deselected
$ uv run pytest -m e2e -q         → 17 passed
$ tests/native/run_native_tests.sh → 545 comprobaciones, 0 fallos + 7, 0 fallos
ruff check / ruff format --check → limpios; ty (con la lista de archivos) → limpio
```

Dos diferencias de conteo, todas por el fixture nuevo: `contracts` pasa de 180 a **190**
checks y el `audit` de 1572 objetos/18 escenarios a **1659/19**, porque `main-natural`
entra en esas suites (son las de la revisión HEAD). El `compare` sigue con los **18**:
toma los fixtures de la revisión A, así que la cadena A/B no gana un caso. El reporte
queda en `tools/ab_testing/reports/AB-UI07-T2/`.

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


## Wiring de la politica de bateria al waterfall (UI-08 incremento 2, issue #14) ✅

La capa de politica del incremento 1 quedo sin llamadores a proposito: era pura y probada,
pero nada la consumia. Este incremento la conecta a su primer consumidor real — el **modo
Ahorro del waterfall** (`params.waterfall_battery` -> `waterfall_set_battery_mode()`, que ya
existia como toggle del usuario) — sin tocar backlight (incr. 3) ni la UI de Ajustes (incr. 4)
ni el DSP/RF.

### Donde se conecta

| camino | punto de conexion | conversion |
|---|---|---|
| ROM | `src/radio.c`, `radio_tick()`, en el mismo bloque que ya publica el pack en `clock_update_power()` (por paquete, cada ~11 lecturas) | los campos `vbat`/`vext` vienen en **decimas de voltio** (`clock.c` dibuja `v_bat*10` y `radio.c` convierte el mismo campo con `*0.1` a voltios), asi que **mV = campo x 100**: `121` -> 12.1 V -> 12100 mV |
| arnes | `tools/ui_harness/src/screens.c`, `build_main()`, junto al mismo `clock_update_power(...)` de la fixture | la fixture da **voltios** (12.1), asi que **mV = V x 1000** (redondeado, `volts_to_mv()`) |

Los dos llaman a la **misma** funcion, `visual_policy_waterfall_apply()` (`src/visual_policy.c`):
ECO y CRITICAL fuerzan el ahorro; QUALITY y BALANCED devuelven el modo al toggle del usuario.
El `params.waterfall_battery` **nunca se escribe** (el modulo no incluye `params.h`: el toggle
entra como parametro, leido del lado de LVGL). En el ROM la aplicacion se **difiere por el
scheduler** al hilo de LVGL (el mismo patron que `tx_info_update`, `src/tx_info.c`), porque
`radio_tick()` corre en el hilo de la radio.

### Declarado ANTES de correr (anti-cherry-pick)

1. **`compare` A/B (A = tip base sin wiring, suite de A = 19 fixtures): 0 diffs.** El wiring
   cambia el **modo runtime** de la cascada (`refresh_period = 4`), no su contenido: la
   captura comun alimenta las 200 filas de una sola vez (`signal_gen_feed()`) y 200 es
   multiplo de 4, asi que el ultimo redibujo de la cascada ocurre igual con la columna
   entera cacheada -> mismo PNG, mismos `metrics.flushes`/`flushed_pixels`. La fixture nueva
   (`main-natural-eco`) **no entra** en el `compare`: la suite sale de la revision A, que no
   la tiene (mismo limite ya anotado en UI-07 incr. 2).
2. **Anti-falso-verde de ese cero** (que el cero no sea "el wiring no hace nada"):
   a) el test nativo asserta la tabla perfil -> modo contra el stub que registra
   `waterfall_set_battery_mode`; b) `--bench-natural` mide la cadencia, que es donde el
   ahorro existe; c) el pixel-diff entre `main-natural` (QUALITY) y `main-natural-eco` (ECO)
   tiene que quedar **confinado al reloj de potencia** (que es lo unico que la fuente externa
   cambia en los pixeles): la cascada tiene que ser pixel-identica entre los dos perfiles.
3. **Cadencia natural por perfil** (mismo seed 13, 40 frames, 24 filas de cascada):
   - **QUALITY** (`main-natural`, 12.1 V + 13.8 V de fuente externa): cascada **196000 px por
     fila**, 23 rectangulos de cascada, totales **6745720 px** (el baseline del corpus UI-07).
   - **ECO** (`main-natural-eco`, 11.2 V sin fuente externa): la cascada redibuja **1 de cada
     4 frames** (filas 4, 8, 12, 16, 20, 24) -> ~**6** rectangulos de cascada en vez de 23, la
     cadencia por fila cae a **196000/4 = 49000 px**, y los totales bajan a **~3.22-3.41 M px**
     (16x132000 de espectro + 5-6x196000 de cascada + reloj/band-info/BT intactos). El
     espectro, el reloj, el band-info y el icono BT tienen que dar **exactamente** los del
     perfil QUALITY (sus timers no dependen del pack).
4. **Determinismo**: 20 fixtures x 2 corridas, PNG sha256 identico en cada fixture; los 19
   sha256 previos no se mueven por este cambio (el wiring no altera ningun otro pixel).

Si la cascada no cae a ~1/4 (por ejemplo porque `refresh_period` no llegue a la captura, o
porque el join de rectangulos de LVGL absorba el cambio), se documenta la investigacion con
los datos: **no se fuerza el numero**.

### Medido (host, 2026-10-14, `5301918` -> `84f9f08`)

Evidencia cruda en
[`tools/ab_testing/reports/NATURAL-UI08-N1/`](../tools/ab_testing/reports/NATURAL-UI08-N1/)
(los cuatro JSON del banco natural + las dos capturas comunes + `sha256.txt`).

**1. `compare` A/B: 0 diffs, tal como se declaro.**

```text
$ uv run x6200-ab compare --a 5301918 --b HEAD --experiment-id AB-UI08-T2
experiment AB-UI08-T2: PASS (exit 0)
  Functional PASS · Visual Regression PASS (captures are pixel-identical to the baseline)
  Quality Contract PASS (190 contract checks) · Performance PASS · Hardware BLOCKED · Energy BLOCKED
  19/19 escenarios: changed_pixels 0
```

Los 19 fixtures de la revision A — `main-bateria-baja` incluido, que es el que la politica
manda a CRITICAL — salen **byte-identicos**. La razon es la declarada: la captura comun
alimenta las 200 filas de la cascada de una sola vez (`signal_gen_feed()`, 200 = 4 x 50) y
`redraw_cb()` solo lee la cache, asi que el ultimo redibujo ocurre igual con
`refresh_period` en 1 o en 4. Lo prueba el par de capturas comunes de la evidencia:
`metrics.flushes` 6 y `metrics.flushed_pixels` 1127680 en las dos, con el mismo par de
valores que el corpus.

**2. Anti-falso-verde del cero** (que "0 diffs" no sea "el wiring no hace nada"):

a) el test nativo asserta la tabla perfil -> modo contra el stub que registra
`waterfall_set_battery_mode` (**27 comprobaciones** nuevas — la de la tabla corre por los 4
perfiles —; `682, 0 fallos` en total);

b) la cadencia medida en `--bench-natural` (abajo) se mueve **exactamente** como la
declaracion predijo;

c) el pixel-diff entre las dos capturas comunes (`main-natural` QUALITY vs
`main-natural-eco` ECO) da **1136 px cambiados, bounding box `(649,10,93,43)`** — dentro del
reloj de potencia, que es lo unico que la fuente externa cambia en los pixeles — y **0 px
dentro del rectangulo de la cascada `0,235 800x245`**: en la captura comun la cascada es
pixel-identica entre los dos perfiles.

**3. Cadencia natural por perfil (mismo seed 13, 40 frames, 24 filas).**

| medición | QUALITY (`quality.json`) | ECO (`eco.json`) | diferencia |
|---|---|---|---|
| redibujos de cascada | 24 (uno por fila) | **6** (1 de cada 4) | 6 x 196000 = 1176000 = **49000 px/fila** |
| rectángulos de cascada reportados | 23 | **5** (+1 fusionado de 196000) | -78% |
| px **invalidados** de cascada (`0,235 800x245`) | 23 x 196000 = 4508000 | 5 x 196000 = **980000** | -78% (la declaración pedía ~-75%) |
| `natural.totals.invalidated_px` | **6745720** | **3474360** | -48.5% |
| `natural.totals.invalidated_rects` | 45 | 33 | -12 |
| `natural.totals.refresh_cycles` | 37 | 27 | -10 |
| `natural.totals.frames_with_flush` | 37/40 | 27/40 | -10 |
| `metrics.flushed_pixels` | 7873400 | 4602040 | -41.5% |

Los redibujos de la cascada en ECO son 6 de 24 filas (1 cada 4 filas: las filas 4, 8, 12,
16, 20 y 24); 5 de ellos quedan como rectangulo propio — frames **6, 13, 20, 26 y 33** — y el
sexto se fusiona con otro rectangulo (`joined_px = 196000`, exactamente una cascada). El
espaciado observado (~6.7 frames) sale de que una fila llega cada 40 ms y un frame dura 30 ms.
El perfil QUALITY da **exactamente** el baseline del corpus UI-07 incr. 2
(196000 x 23, 6745720 px, 45 rectangulos) y el gate del modo natural sigue PASS con el
mismo sha256 (`8ec63121…`): el wiring no toco el caso que no ahorra.

**4. La divergencia con lo declarado (y su causa, medida):** el total de ECO quedo en
**3474360 px**, apenas **por encima** del rango declarado (3.22-3.41 M), y el conteo de las
regiones vecinas cambio (espectro 16 -> 17, band-info 4 -> 8, reloj 1 -> 2) aunque sus
timers disparan igual (17 frames de FFT, 24 filas, 2 ticks de reloj). Causa: LVGL **fusiona
los rectangulos de cada ciclo de refresh**, y con la cascada redibujando 1 de cada 4 ciclos
quedan 10 frames sin cascada (37 -> 27 ciclos con flush) donde las regiones vecinas dejan de
fusionarse con ella. Los **eventos** del firmware son los mismos; el **reparto en ciclos** no.
Es el unico punto donde la declaracion peco de optimista ("exactamente los del perfil
QUALITY"), y queda corregido con el dato: la cascada es lo unico que cambia de trabajo, pero
los rectangulos que el banco reporta alrededor tambien se reagrupan.

**5. Delimitacion: el efecto sigue al pack, no al fixture.** Dos corridas extra del mismo
modo sobre fixtures que ya existian y con **otro seed**:

| fixture | pack | perfil | rectangulos de cascada | totales |
|---|---|---|---|---|
| `main-natural-eco` | 11.2 V sin fuente | ECO | 5 (+1 fusionado) | 3474360 |
| `main-bateria-baja` (`baja.json`) | 10.6 V sin fuente | CRITICAL | 5 (+1 fusionado) | 3474360 |
| `home-bateria-externa` (`ext.json`) | 12.1 V + fuente | QUALITY | 23 | 6745720 |
| `main-natural` | 12.1 V + fuente | QUALITY | 23 | 6745720 |

Dos fixtures distintos con el pack bajo dan el mismo perfil reducido y dos fixtures
distintos con fuente externa dan el perfil completo: la cadencia la decide el estado del
pack, no el fixture.

**6. Gates restantes.**

```text
$ tests/check_harness_determinism.sh          # 20 fixtures x 2 corridas
main-natural-eco: 6297ef99d8b3babfd844749206afb87aaf59e7c52b5830446a46170357b01292
bench-render: 5ba21bea… (igual al tip base) · home-usb-20m: a656e096… (idem)
main-natural: 3986bc9a… (idem)
PASS: deterministic (identical PNGs for every fixture)   # los 19 sha256 previos no se movieron

$ tests/check_harness_natural.sh               # SKIP_BUILD=1
  profile: spectrum 132000 px x 16 frames, waterfall 196000 px x 23 rows, clock 15840 px x 1 tick(s)
  natural totals: invalidated 6745720 px in 45 rectangles / 45 flushes over 37/40 frames
  png sha256: 8ec631215c28e27fb901f186273fe6afa0fd6b9470550cf810cb1988f84273fd
PASS: natural bench deterministic, profiled and delimited (main-natural)

$ uv run x6200-ab contracts --revision HEAD
contracts 84f9f0840501: PASS — 200 checks, 0 failure(s), 0 unchanged baseline defect(s)

$ uv run x6200-ab audit --revision HEAD --suite visual
audited 84f9f0840501: 14 known issues — 0 reproduced, 12 unverified; structural inspection
REAL over 1746 objects on 20 scenario(s): 0 structural finding(s)

$ uv run pytest -m "not e2e" -q   -> 136 passed, 17 deselected
$ uv run pytest -m e2e -q         -> 17 passed
$ tests/native/run_native_tests.sh -> 682 comprobaciones, 0 fallos + 7, 0 fallos
ruff check / ruff format --check -> limpios; ty (con la lista de archivos) -> limpio
```

`contracts` pasa de 190 a **200** checks y el `audit` de 1659/19 a **1746 objetos en 20
escenarios**: lo mismo que en el incremento anterior, el fixture nuevo entra en esas suites
(son de HEAD). El `compare` sigue con los **19** de la revision A, asi que la cadena A/B no
gana el caso nuevo (mismo limite anotado en UI-07 incr. 2).

### Interaccion con el toggle del usuario (decision declarada)

`params.waterfall_battery` es del usuario y el perfil **no lo escribe** (el modulo de
politica ni siquiera incluye `params.h`: el toggle entra como parametro y se lee del lado de
LVGL). El cambio de modo del waterfall se aplica **en la transicion del perfil**
(`visual_policy_result_t.changed`), que es lo que pidio la spec: al entrar a ECO/CRITICAL se
fuerza el ahorro y al volver a QUALITY/BALANCED se restituye el valor del param. Consecuencia
honesta de la politica "solo en la transicion": si el usuario apaga el switch de Ajustes
**mientras** el pack esta en ECO, el waterfall vuelve a `refresh_period = 1` y no se re-fuerza
hasta la proxima transicion del perfil (y el switch de Ajustes, como manda la spec, sigue
reflejando unicamente el param, no el perfil). Re-aplicar en cada paquete queda fuera del
alcance de este incremento: no cambia la evidencia y agrega una escritura por paquete.

## Fixture del diálogo de Ajustes (issue #11, desbloqueo de #14 incr. 5) ✅

Gate: el arnés renderiza el **diálogo real de Ajustes** por el camino del firmware, con
capturas deterministas (22 fixtures × 2) y las **20 previas byte-idénticas**; `contracts` y
`audit` corren también sobre las dos fixtures nuevas, con un waiver acotado y fechado por cada
defecto real que la fixture mide.

### Lo que faltaba de verdad (medido, no la nota vieja)

`src/dialog_settings.c` **no** arrastra `dialog_field` (la nota de UI-02 —"arrastran
`dialog_field` y decenas de dependencias"— era **incorrecta**): se compila y linkea entero con
lo que el arnés ya tenía (radio, backlight, waterfall, cluster, voice, teclado, scheduler,
`dialog.c` real). Enumerado por el linker, lo que faltaba es corto:

| pieza | por qué | cómo se resolvió |
|---|---|---|
| `speex/speex_preprocess.h` | lo incluye `src/audio_enhancement.h`, que `dialog_settings.c` incluye por el preset de audio | `libspeexdsp-dev` en el stage `ci` del Dockerfile (el arnés **no linkea** speex: solo el header), junto a `libsqlite3-dev`/`libliquid-dev`, que ya estaban por el mismo motivo |
| `audio_set_play_vol` | `src/audio.c` maneja el mixer ALSA | mock: devuelve el valor pedido, **exactamente** lo que hace el firmware cuando el mixer no está (`src/audio.c:387`) |
| `audio_enh_preset_load` | preset de DSP (speex+liquid) | mock no-op: en el host no corre DSP |
| `wfview_server_get_enabled`/`set_enabled` | lee `/mnt/wfview.conf` y abre UDP (pulse/cat) | mock: sin archivo ni socket, off |
| `cfg.cluster_column`, `cfg_transverters[]` | el firmware los inicializa desde SQLite | subjects del mock con los defaults de `src/cfg/transverter.c:36-50` (sin ellos el diálogo desreferenciaba NULL) |
| `time()` + `localtime()` | los campos Day/Month/Year y Hour/Min/Sec salen del RTC | `--wrap=time` (CMakeLists) + `__wrap_time` con época fija + `TZ=UTC` en `harness_main`: el reloj de pared es otro servicio que el arnés posee, igual que `get_time()` |

El build suma **2 warnings**, los dos de `src/dialog_settings.c` (locales `obj` sin usar,
líneas 691 y 1270): son del firmware (el ROM compila el mismo código), no del arnés.

### Las dos fixtures

- **`settings-home`**: el diálogo abierto por `main_screen_action(ACTION_APP_SETTINGS)` sobre la
  pantalla principal real (grupo Reloj: fecha/hora, Clock view, timeouts). El grupo de teclado
  está creado antes de construir, así que `dialog_item()` registra cada widget como en la radio.
- **`settings-wfbat`**: Ajustes con el **modo Ahorro del waterfall** encendido (clave de
  escenario nueva `waterfall_battery`, que escribe `params.waterfall_battery.x`) y la vista
  saltada al grupo **Espectro por el mismo camino del usuario**: `buttons_press(4)` carga la
  página 2 con el botón "Mas >" y `buttons_press(0)` llama al tab "Espectro" →
  `settings_goto()` (scroll + foco). El switch "Waterfall battery mode" se dibuja **marcado** y
  el policy visual lleva la cascada real a su cadencia de ahorro (detrás del diálogo).

Cobertura honesta (`coverage.c`, tabla nueva): `dialog.c` y `dialog_settings.c` **REAL**;
`audio.c`, `audio_enhancement.c`, `wfview_server.c`, `cfg/transverter.c`, el RTC,
`backlight.c` y `bt_state.c` **SYNTHETIC** (mocks explícitos); `dialog_field.c`
**UNSUPPORTED y documentado como no-dependencia** de Ajustes.

### Las reglas de layout que la fixture obligó a arreglar (no a silenciar)

Ajustes es un **diálogo con scroll sobre la pantalla principal** y las reglas de layout estaban
escritas para una pantalla única sin scroll: sobre el diálogo daban 93–96 `layout_offscreen`,
4/1 `layout_overlap` y 1/1 `layout_clipped` **falsos positivos**. Se arreglaron de raíz (con
test unitario por caso), no con un waiver:

- **Contenido de un área de scroll** (`layout_checks._scroll_reach`): las filas bajo el pliegue
  no son "nada en pantalla", son alcanzables. El inspector emite ahora **`scroll_dir`** y la
  regla salta el nodo que sale de un área de scroll *en el eje que esa área scrollea*. El
  viewport **no** es un área de scroll (`scroll_dir: all`, el default de LVGL en cualquier
  objeto) y **ninguna sub-widget tampoco**: un widget empujado fuera de la pantalla por un
  defecto sigue fallando (la regresión deliberada de la barra de botones lo sigue cazando, ver
  `test_e2e.py`), y un label recortado por su propio spinbox también (el caso UI-016).
- **Oclusión** (`layout_checks._overlap_hidden`): una etiqueta tapada por una **superficie opaca
  pintada después** (el fondo del diálogo) no es "dos textos superpuestos": el usuario ve una.
  El par se salta solo si la superficie cubre la intersección de las dos cajas.
- **Target táctil**: un control scrolleado se mide con su **tamaño real**, no con la franja
  visible (el dropdown a medio scrollear no es "un control de 14 px").

Los 20 fixtures previos siguen dando **0 findings** (la prueba de que el cambio es inerte ahí)
y cada comportamiento nuevo tiene su test en `tests/test_layout_checks.py` (incluido el caso
que **sí** debe seguir fallando en cada clase).

### Dos defectos reales de Ajustes (medidos y registrados, no arreglados acá)

La spec prohíbe tocar Ajustes en esta corrida (los fixes son los issues UI-002…008), así que
los dos hallazgos **reales** que la fixture mide quedan registrados con **excepción acotada
(regla + fixture, nunca global) y fecha** en `quality/contracts.yaml`, visibles en el reporte:

- **UI-015** (`layout_touch_target`): los ocho switches del grupo Espectro son 111×30 y el
  slider del brillo del backlight 159×13, por debajo del **target de campo de 44 px**
  (AGENTS.md).
- **UI-016** (`layout_clipped`): el campo **Año** dibuja "2026" con 84 px de texto y el área de
  contenido del spinbox le deja 80 → el último dígito pierde sus 4 px de la derecha.
  `typography_text_overflow` **no** lo ve (mide contra la caja sin recortar); lo ve
  `layout_clipped`.

### Evidencia (host, 2026-10-14, `f360f92` → `63ecc87`)

```text
$ tests/check_harness_determinism.sh        # 22 fixtures x 2 corridas
settings-home: 369cb648116732a75a88b88bfcf7062e20dc0e8be5fffbfbe8292a93fdd6e067
settings-wfbat: c456422d50c408d5cd416b17a1a2c720bd26080cb255473f9642e49fa50b8aae
... (las 20 previas con el MISMO sha256 del tip f360f92)
PASS: deterministic (identical PNGs for every fixture)

$ cd tools/ab_testing && uv run x6200-ab compare --a f360f92 --b HEAD --experiment-id AB-AJUSTES-T1
experiment AB-AJUSTES-T1: PASS (exit 0)
  Functional PASS · Visual Regression PASS (captures are pixel-identical to the baseline)
  Quality Contract PASS (200 checks) · Performance PASS · Hardware BLOCKED · Energy BLOCKED
  20/20 fixtures: changed_pixels 0        # la suite sale de la revision A (20 fixtures)

$ uv run x6200-ab contracts --revision HEAD
contracts 63ecc8716859: PASS — 230 checks, 0 failure(s), 0 unchanged baseline defect(s)
  (220 de las 22 fixtures + 10 findings estructurales de Ajustes, los 10 waived por UI-015/UI-016)

$ uv run x6200-ab audit --revision HEAD --suite visual
audited 63ecc8716859: 16 known issues — 0 reproduced, 14 unverified; structural
inspection REAL over 2306 objects on 22 scenario(s): 0 structural finding(s)

$ uv run x6200-ab benchmark --revision HEAD --experiment-id BENCH-AJUSTES-M1
benchmark 63ecc8716859: PASS (exit 0) — 22 escenarios
  settings-home  p50 6.623 p95 6.792 ms    settings-wfbat  p50 6.581 p95 6.881 ms
  (los mas pesados del corpus, junto a dialog-about 6.119; la pantalla principal ~3.2-3.4 ms)

$ uv run pytest -m "not e2e" -q           → 146 passed, 17 deselected
$ tests/native/run_native_tests.sh        → 759 + 7 + 13 comprobaciones, 0 fallos
repo static checks (cfg_fills/actions/settings_focus/apps_menu/app_handlers/wcag) → PASS
ruff check / ruff format --check → limpios   (ty: "No python files found", pendiente conocido)
build: 70 warnings (los mismos de siempre + 2 de src/dialog_settings.c, upstream)
```

`BENCH-AJUSTES-T1` (A vs B, 20 escenarios) **falla** en 5 escenarios con el **mismo trabajo**
(flushes 1.0 y 384000 px en los 20) y su repetición `BENCH-AJUSTES-T1-repeat` pasa con p95 entre
−13% y +21%: es el ruido del host ya documentado (`BENCH-UI04-EXP3-repeat`,
`BENCH-UI05-T3-repeat`, `BENCH-UI07-T1-repeat`; load average ~3.7). Las dos corridas quedan
commiteadas lado a lado.

El tip del cambio es `892fc7c`, que agrega una comprobación de que la fila que `settings-wfbat`
afirma mostrar está a la vista (si el renglón de tabs del pie se reordena, el arnés corta en vez
de capturar otro grupo): **no mueve un solo píxel**, así que el determinismo, `contracts`
(230 chequeos) y `audit` (2306 objetos / 22 escenarios) dan los mismos números en el tip. Los
reportes commiteados citan `63ecc87`, la revisión del código contra la que corrió el `compare`.

El `compare` sigue tomando los fixtures de la **revisión A** (el límite ya anotado en UI-07
incr. 2 y UI-08 incr. 2): A no tiene las fixtures nuevas, así que la cadena A/B no gana el caso
nuevo. La afirmación del render de Ajustes la dan el determinismo, `contracts`/`audit` sobre
HEAD y las capturas PNG (`settings-home`/`settings-wfbat`), no el diff A/B.

### Cómo lo siguen #14 incr. 5 y los issues

- **#14 incr. 5 (UI de Ajustes / persistencia de la política)**: el arnés ya renderiza Ajustes,
  así que el toggle de la política (y las filas que toque ese incremento) se pueden capturar y
  medir en píxeles. La fixture `settings-wfbat` ya muestra el estado "Ahora" del switch del
  waterfall, y `waterfall_battery` es una clave de escenario de primera clase.
- **#10/#12 (tokens/Flex, restante de "filas de Ajustes")**: la grilla de Ajustes es ahora
  visible y medible (`--inspect` da sus 280 objetos y los paths); convertir sus filas a
  Flex/Grid se puede verificar pixel a pixel contra estas dos fixtures.
- **#11 (semillas)**: ver la re-evaluación en `docs/UI_KNOWN_ISSUES.md` — el fixture de Ajustes
  es el habilitante que faltaba para **UI-004** y **UI-007**; ambos quedan a un detector (grilla
  de foco / variante de tema para el riel del switch) de fundarse.

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
  About real (`dialog-about`) y el **diálogo de Ajustes** (`settings-home`,
  `settings-wfbat`, ver arriba) ya están cubiertos. Quedan fuera el menú de apps
  y los diálogos BT/WeFax/SSTV (esos sí arrastran `dialog_field`/tabla); con
  ellos las semillas UI-002/UI-005 quedan `unverified` y UI-004/UI-007 quedan a
  un detector de distancia (ver `docs/UI_KNOWN_ISSUES.md`).
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


## La app Bands desde el menú de apps (reporte del owner) ✅

Gate: el defecto se reproduce **en el banco** con el binario ARM real (ruta del owner:
APP → páginas APP → Bands: la pantalla no cambia), y el arnés de host queda con **dos fixtures
nuevas** (24 × 2 en determinismo) que fallan sin el fix y pasan con él.

### El defecto, medido en el banco

El botón "Bands" de las páginas APP (`src/buttons.cpp`, `make_app_btn`) llama a
`main_screen_start_app()`, que **no tenía `case ACTION_APP_BANDS`**: caía en el `default` y no
hacía nada. La acción se despachaba **solo** en `main_screen_action()`, donde además construía
`dialog_bands` **sin cerrar** la ventana abierta (por el long press de Ajustes la ventana
anterior quedaba huérfana: `current_dialog` pisado y su objeto sin destructor). La ruta y los
controles están en `docs/evidencia/2026-10-14-bands-settings/README.md` (PNGs + md5 del binario
de cada lado): con el fix, la misma secuencia abre la ventana real de Bands ("BANDS 21.2500 MHz
/ RBN connected", 12 bandas, spots de cluster/RBN, barra `(BANDS 1:1) Test beacon Use band`).

Mismo defecto, misma causa: el botón **About** de `APP 5:6` (`ACTION_APP_ABOUT` tampoco está en
`main_screen_start_app`) también queda muerto. **No se arregló en esta corrida** (fuera del
alcance de la spec) y **no** está registrado todavía en HALLAZGOS/known_issues: queda como
pendiente con evidencia en la carpeta de la corrida.

### Las dos fixtures y su límite honesto

- **`app-bands`**: la ruta del owner (el menú de apps de `buttons_group_app`, botón "Bands" de
  `APP 3:6`).
- **`settings-bands`**: Ajustes y después la acción de Bands (`main_screen_action(ACTION_APP_BANDS)`).

`dialog_bands` es un **mock inerte** en el arnés (`src/dialog_bands.c` no se compila; su subárbol
—`dialog_field.c`, `field/bands.c`, `cluster/spots.c`— tampoco), así que las fixtures **miden la
ruta, no los píxeles de la ventana**: `scenario_settle` corta la corrida si no se cumple el estado
del firmware que el fix garantiza (el botón tiene que llegar a *algún* diálogo; con Ajustes
abierto, Ajustes tiene que quedar cerrado). La cobertura lo declara por fixture y honesto
(`dialog_bands.c` UNSUPPORTED). Que la ventana abre de verdad se midió en el banco, que es donde
el binario real la dibuja.

Sin cambios en `quality/contracts.yaml`: el corpus no cambia un pixel (las 22 fixtures comparables
salen byte-idénticas), y las dos nuevas pasan los contratos sin waiver (la barra de botones
descargada por el mock deja **content 0.677** en la región `botones`, muy por encima del 0.02).

### Evidencia (host, 2026-10-14, `89851e6` → `1ef010a`)

```text
$ tests/check_harness_determinism.sh        # 24 fixtures x 2 corridas
PASS: deterministic (identical PNGs for every fixture)

$ cd tools/ab_testing && uv run x6200-ab compare --a 89851e6 --b 487af1e --experiment-id AB-BANDS-T1
experiment AB-BANDS-T1: PASS (exit 0)
  Functional PASS · Visual Regression PASS (pixel-identical) · Quality Contract PASS (230 checks)
  Performance PASS · Hardware BLOCKED · Energy BLOCKED
  22/22 fixtures: changed_pixels 0        # la suite sale de la revision A (22 fixtures)

$ uv run x6200-ab contracts --revision 487af1e
contracts 487af1e5b99b: PASS — 250 checks, 0 failure(s), 0 unchanged baseline defect(s)

$ uv run x6200-ab audit --revision 487af1e
audited 487af1e5b99b: 16 known issues — 0 reproduced, 14 unverified; structural
inspection REAL over 2480 objects on 24 scenario(s): 0 structural finding(s)

$ uv run x6200-ab benchmark --revision 487af1e --repeats 5 --experiment-id BENCH-BANDS-T1
benchmark 487af1e5b99b: PASS (exit 0) — 24 escenarios
  app-bands p50 3.169 p95 3.382 ms    settings-bands p50 3.141 p95 3.246 ms
  (los dos, junto a la pantalla principal ~3.2-3.5 ms; Ajustes ~6.6-7.1)

$ uv run pytest -m "not e2e" -q           → 146 passed, 17 deselected (dos corridas)
$ uv run ruff check . / ruff format --check . → limpios
$ uv run ty check                         → All checks passed!
tests/native/run_native_tests.sh          → ok
repo static checks (cfg_fills/actions/settings_focus/apps_menu/app_handlers/wcag) → PASS
banco QEMU (preflight + params.db + captura de humo) → ok, md5 del binario 061f8b88dea9
docs/evidencia/2026-10-14-bands-settings/arnes-antes-despues.txt → las dos fixtures: exit 1 en
  89851e6, exit 0 con el fix (mismo comando)
```

Las fixtures nuevas **no entran en el `compare`**: la suite sale de la revisión A y ahí no
existen (mismo comportamiento que `settings-home`/`settings-wfbat` en la corrida anterior). Su
cobertura es determinismo (24×2), `contracts`, `audit` y `benchmark`, más el antes/después
explícito de `arnes-antes-despues.txt`.


Orden del owner: **todos los temas tienen que tener buen contraste, medido**. Hasta
acá el framework medía 7 temas × 9 pares (texto) y dejaba Outdoor-Negative y VISION
en **UNCOVERED**; ahora mide los 9 y suma el contraste **no-texto (WCAG 1.4.11)** de
los objetos gráficos informativos sobre los píxeles del render.

### Texto — 9 temas × 9 pares = 81 mediciones

`theme_files.py` deriva `use_builtin()` (src/theme.c) en Python: Outdoor-Negative es
`negative()` sobre Outdoor con los roles que el C fija a mano, y VISION es IU1WXY más
`vision_colors[]`. La derivación se lee del mismo C (no se re-implementan números) y
se pinea **byte a byte** contra `tests/check_wcag.py` (las nueve paletas, rol por rol)
y **ratio por ratio** en los pares declarados. Con eso no queda tema sin resolver y
`theme_pairs.yaml` ya no tiene sección `unsupported`.

| Tema | fuente | pares | peor ratio | resultado |
|------|--------|------:|-----------:|-----------|
| IU1WXY | `src/theme.h` | 9 | 3.11 | PASS |
| Legacy | `src/theme.h` (no en el menú) | 9 | 3.08 | PASS |
| Outdoor | `src/theme.h` | 9 | 3.11 | PASS |
| Classic | `src/theme.h` | 9 | 3.12 | PASS |
| Outdoor-Negative | derivado `use_builtin()` | 9 | 4.06 | PASS |
| VISION | derivado `use_builtin()` | 9 | 7.02 | PASS |
| Holo | `rootfs/.../Holo.json` | 9 | 3.12 | PASS |
| Material 3 | `rootfs/.../Material 3.json` | 9 | 5.14 | PASS |
| RS | `rootfs/.../RS/theme.json` | 9 | 3.08 | PASS |

El peor par de cada tema es `control_border` sobre `dialog`/`panel` (el riel del
switch/slider de Ajustes, UI-007): pasa el 3.0:1 de texto grande/no-texto con poco
margen, por eso figura primero en la tabla.

### No-texto (1.4.11) — objetos informativos, medidos por píxeles

`nontext_checks.py` mide, sobre los renders commiteados en `tools/ab_testing/golden/`
(capturas deterministas de la revisión), cada objeto con rectángulos **fijos
declarados** en `quality/nontext.yaml` (el mismo idioma de los contratos `region_*`).
El color del objeto es el percentil alto de su región y el de la superficie el
percentil bajo del fondo; como el objeto puede ser más claro o más oscuro que su
fondo (el waterfall de Outdoor es tinta oscura sobre blanco, el de Classic es tinta
clara sobre negro), se mide en ambas polaridades y se reporta la que separa al objeto
de su superficie. Umbral 3.0:1. Self-tests sintéticos fijan el algoritmo (pasa/falla,
ambas polaridades, región que "yerra" al objeto, un píxel anti-aliased no decide).

| Tema | objeto no-texto | ratio | fg / bg |
|------|-----------------|------:|---------|
| IU1WXY | S-meter (arco `meter_over`) | 9.86 | #ffffff / #364552 |
| IU1WXY | waterfall (columna) | 5.73 | #35b129 / #000d73 |
| IU1WXY | foco (celda `TC(focus)`) | 8.76 | #ffffff / #394e56 |
| Legacy | S-meter | 9.40 | #ffffff / #2e4866 |
| Legacy | waterfall (`main-espectro-agua`) | 4.71 | #149748 / #00065b |
| Outdoor | S-meter | 3.74 | #000000 / #5e6972 |
| Outdoor | waterfall | 13.58 | #181818 / #e1e1e1 |
| Classic | S-meter | 3.30 | #ff4040 / #2f3b47 |
| Classic | waterfall | 16.61 | #e7f918 / #000043 |
| Classic | franja CW / DIG del strip | 4.07 / 5.87 | #426bbd / #3a9955 sobre #000000 |
| Holo | S-meter | 3.35 | #ff4444 / #2f3b47 |
| Holo | waterfall | 5.73 | #35b129 / #000d73 |
| Material 3 | S-meter | 6.37 | #ffb4ab / #313f4b |
| Material 3 | waterfall | 5.73 | #35b129 / #000d73 |
| Outdoor-Negative | S-meter | 11.43 | #ffffff / #2f3b47 |
| Outdoor-Negative | waterfall | 11.14 | #d3d3d3 / #1e1e1e |
| VISION | S-meter | 7.13 | #ffd040 / #34414e |
| VISION | waterfall | 5.73 | #44b023 / #000d73 |
| RS | S-meter | 3.22 | #ff5252 / #36424e |
| RS | waterfall | 17.09 | #e7f952 / #000038 |

> **#44 (2026-10-15)**: los números de la tabla son los de la revisión de #42. En el equipo el
> fondo real del arco es `(62,78,92)` y ni `#ff4040` ni `#ff5252` llegaban a 3:1; el arco pasó a
> `#f08080` en la tabla compilada de `src/theme.h` (columna Classic, la que resuelve RS) y en
> `themes/RS/theme.json`. Con eso el objeto `s_meter_arc` mide ahora **3.96:1** (RS) y
> **4.41:1** (Classic, la columna compartida); sigue 21 objetos y 0 fallas.

21 objetos, 0 fallas. El **waterfall de Outdoor en claro (gris sobre gris)** de la
observación de UI-010 **no se reproduce** con estos números: la columna de señal del
waterfall de Outdoor es tinta oscura (#181818) sobre la cara clara (#e1e1e1) = 13.58:1,
y el tema Legacy (el que usa `main-espectro-agua`) da 4.71:1. La paleta del waterfall de
Outdoor/Classic/Legacy **sí es del tema** (`theme_shape.waterfall`), pero pasa el umbral;
para IU1WXY/Holo/Material 3/RS/VISION el color del bin sale de `wf_palette_gauss[256]`
(DSP), no del tema, y también pasa (5.7:1).

### Exentos (WCAG 1.4.3, disabled/incidental)

Se declaran como datos en `theme_pairs.yaml` (`exempt:`) con su razón, y
`tests/test_theme_files.py` los pinea contra el conjunto `EXENTOS` de
`tests/check_wcag.py` y verifica que cada uno sea un rol que el sweep del ROM mide
(no data muerta): `btn_off_text` (botón deshabilitado), `text_mute`, `text_faint`
(texto atenuado/en cola), `icon_off`, `switch_off`, `off`, `track` (estados apagados /
pistas inactivas). Se miden e informan sin fallar; **no están en la tabla de pares**.

### Fix aplicado en el mismo cambio (alcance tema)

- **RS / S-meter**: el arco `TC(meter_over)` = `#ff4040` sobre la cara del medidor de
  RS (`bar.png`) medía **2.96:1**, por debajo del 3:1 de 1.4.11. `RS/theme.json` lo
  sube a `#ff5252` → **3.22:1** medido. El cambio se confina a `s_metro` y se declara
  en `contracts.yaml` (`expected_changes`); el resto de los 9 renders queda
  byte-idéntico. Los otros temas (Classic 3.30, Holo 3.35) ya pasaban.

### Gates (host, 2026-10-14, HEAD del cambio)

```
tests/check_harness_determinism.sh                     → PASS (24 fixtures x2, hashes idénticos)
uv run x6200-ab themes                                 → PASS, 9 temas x 9 pares = 81 + 21 objetos no-texto, 0 fallas
uv run x6200-ab compare --a 0684861 --b HEAD           → PASS (24 escenarios; 1 diff declarado en s_metro/RS)
uv run x6200-ab contracts --revision HEAD              → PASS (250 checks, 0 fallas)
uv run x6200-ab audit --revision HEAD --suite visual   → PASS (2480 objetos, 0 findings estructurales)
uv run x6200-ab benchmark --revision HEAD              → PASS
uv run pytest -m "not e2e" -q                          → 164 passed, 17 deselected
uv run pytest -m e2e -q                                → 17 passed
tests/native/run_native_tests.sh                       → ok
repo static checks (cfg_fills/actions/settings_focus/apps_menu/app_handlers/wcag) → PASS
     tests/check_wcag.py → 990 OK / 0 fallas (9 paletas)

## La app About desde el menú de apps (follow-up del reporte de Bands) ✅

Gate: el botón **About** de `APP 5:6` abre la ventana real de About en el banco (binario ARM
real, ruta del owner APP → páginas APP → About) y el arnés de host queda con una fixture nueva
(`app-about`, 25 × 2 en determinismo) que falla sin el fix y pasa con él.

### El defecto

Mismo camino y misma causa que Bands (sección anterior, donde quedó **declarado y sin
arreglar**): el botón About del menú de apps (`src/buttons.cpp`, `make_app_btn` →
`button_app_page_cb`) llama a `main_screen_start_app()`, que **no tenía `case
ACTION_APP_ABOUT`**: caía en el `default`. La acción se despachaba **solo** en
`main_screen_action()`, que el botón no usa. El fix es el `case` en `main_screen_start_app()`
(3 líneas, idénticas al de Bands): `dialog_construct(dialog_about, obj)`.

Evidencia en `docs/evidencia/2026-10-14-about-fix/README.md` (PNGs del banco + md5 del binario
de cada lado: `061f8b88dea9` antes, `7f2c94ecb2c7` después). Con el fix, el botón abre la
ventana real ("About", "Version v0.1.2-lu2dsx", "73 de LU2DSX", "Made with love by LU2DSX"); sin
él, el botón no cambia nada.

### La fixture y su límite honesto

- **`app-about`**: la ruta del menú (el menú de apps de `buttons_group_app`, botón "About" de
  `APP 5:6`).

`dialog_about` es **real** en el arnés (`src/dialog_about.c` se compila, la misma ventana que
abre `dialog-about`), así que la fixture **renderiza la ventana real** y `scenario_settle` corta
la corrida si el botón no la abre (`dialog_about->run`): sin el `case`, exit 1. Las etiquetas de
la ventana no se pueden mirar en `scenario_settle` porque LVGL calcula el layout recién en el
refresh posterior, así que la aserción es el **estado del firmware**; que la ventana dibuja se ve
en el PNG, que sale **byte a byte idéntico** a `dialog-about` (mismo sha256 en el determinismo).

### El chequeo estático (`tests/check_apps_menu.py`)

El chequeo leía los `case ACTION_APP_*` de **todo** `main_screen.c`, así que el botón About pasaba
estando muerto (existía en `main_screen_action()`). Ahora el cuerpo de cada función se extrae
balanceando llaves y cada botón se valida contra el dispatcher que le corresponde (`make_app_btn`
→ `main_screen_start_app`, `make_action_btn` → `main_screen_action`): con el fix da **21/21 + 2/2**
(y las 23 apps siguen en las páginas APP); restando el `case`, da **20/21** y **FALLA**
(`page_app_5: el boton btn_about (ACTION_APP_ABOUT) no tiene case en main_screen_start_app()`).

### `tests/run_all_tests.sh`: los chequeos estáticos cuentan de verdad

Los estáticos usaban `fallos=$((fallos+1))` con una variable que **no existía**: la falla se
anotaba en otro contador y la suite terminaba **verde**. Ahora van por el mismo `run()` que el
resto (suman a `fail`, el que imprime el resumen y decide el exit). Contracara medida con
`check_wcag.py` roto a propósito: el script original daba **`6 OK, 0 fallos`, exit 0**; el
corregido, **`10 OK, 1 fallos`, exit 1**.

### Evidencia (host, 2026-10-14, `0684861` → `9224734`)

```text
$ tests/check_harness_determinism.sh        # 25 fixtures x 2 corridas
PASS: deterministic (identical PNGs for every fixture)
  app-about: 213c5cc1…  == dialog-about: 213c5cc1…   # misma ventana, mismo sha256

$ cd tools/ab_testing && uv run x6200-ab compare --a 0684861 --b HEAD --experiment-id AB-ABOUT-T1
experiment AB-ABOUT-T1: PASS (exit 0)
  Functional PASS · Visual Regression PASS (pixel-identical) · Quality Contract PASS (250 checks)
  Performance PASS · Hardware BLOCKED · Energy BLOCKED
  24/24 fixtures: changed_pixels 0        # la suite sale de la revision A (24 fixtures)

$ uv run x6200-ab contracts --revision HEAD
contracts 9224734864e8: PASS — 260 checks, 0 failure(s), 0 unchanged baseline defect(s)

$ uv run x6200-ab audit --revision HEAD --suite visual
audited 9224734864e8: 16 known issues — 0 reproduced, 14 unverified; structural
inspection REAL over 2573 objects on 25 scenario(s): 0 structural finding(s)

$ uv run x6200-ab benchmark --a 0684861 --b HEAD
benchmark: PASS (exit 0) — 24 escenarios, p95 dentro de budget (delta ~0; el fix no cambia
el render-cost)

$ uv run pytest -m "not e2e" -q           → 146 passed, 17 deselected
$ uv run ruff check . / ruff format --check . → limpios
$ uv run ty check src tests               → All checks passed!
tests/native/run_native_tests.sh          → 13 comprobaciones, 0 fallos
repo static checks (cfg_fills/actions/settings_focus/apps_menu/app_handlers/wcag) → PASS
banco QEMU: preflight ok; botón About de APP 5:6 → abre la ventana real (md5 7f2c94ecb2c7);
  sin el fix (md5 061f8b88dea9) el botón no hace nada
docs/evidencia/2026-10-14-about-fix/arnes-antes-despues.txt → app-about: exit 1 en 0684861,
  exit 0 con el fix (mismo comando)
```

Sin cambios en `quality/contracts.yaml`: el corpus no cambia un pixel (las 24 fixtures
comparables salen byte-idénticas) y la fixture nueva pasa los contratos sin waiver.

La fixture nueva (`app-about`) **no entra en el `compare`** por la misma razón que las de Bands:
la suite sale de la revisión A. Su cobertura es determinismo (25×2), `contracts`, `audit` y
`benchmark`, más el antes/después explícito del banco y de `arnes-antes-despues.txt`.

PENDIENTE del banco: `tools/captura-banco.sh` reinicia la GUI en cada llamada porque el handshake
`{"cmd":"get"}` falla contra una GUI ya arrancada (contenedor vivo, socket que no contesta); por eso
las capturas de varios pasos se hicieron con un driver de una sola corrida. No se tocó el script
(el defecto es del banco, no de este cambio).

## Perfil visual de bateria en Ajustes: UI + persistencia + los 2 clamps (UI-08 incremento 5, issue #14) ✅

Cierra el ultimo tramo de UI-08: la politica de bateria (incr. 1-4) queda **manejable desde la GUI**
y **persistente**. Ajustes gana la fila **"Visual profile"** (AUTO + los cuatro perfiles), el perfil
que el usuario fija **sobrevive al reboot**, y los dos clamps que la spec dejo pendientes —el preview
del slider de brillo y el OFF/ON del switch— pasan por la **unica puerta con cap**. Nada de
RF/TX/CAT/DSP ni del submodulo `lvgl/` pineado.

### La fila: el seteado (desplegable) y el VIGENTE (etiqueta)

- **Desplegable** (`src/dialog_settings.c`, `make_visual_profile`): `Auto`, `Quality`, `Balanced`,
  `Eco`, `Critical`. Es el valor **seteado** y lo persiste en `params.visual_profile` con
  `params_uint8_set()` (el mismo lock/dirty que las filas vecinas).
- **Etiqueta** de la fila: `Visual profile (<perfil>)`, y el `<perfil>` es el **VIGENTE**, no el
  seteado. Sale de `visual_policy_current()` (`src/visual_policy.c`): el ultimo perfil que paso por
  `visual_policy_brightness_apply()`. Se registra en el **apply** y no en la evaluacion a proposito:
  en la radio la evaluacion corre en el hilo de la radio (`radio_tick`) y el apply en el de LVGL (lo
  encola el scheduler), que es el mismo del dialogo, asi que la lectura de la UI no cruza hilos. En
  AUTO lo decide la bateria; con un override esta congelado. Es una foto del estado **al abrir
  Ajustes** (el dialogo es modal y el perfil no cambia mientras esta abierto).

La fila vive en el grupo **Apariencia** (con Theme y Font: es el preset de look-and-feel). No va en
Espectro (donde esta el toggle "Waterfall battery mode") porque ese grupo ya ocupa 5 filas y una
sexta se **recortaria** contra el viewport de 330 px de la grilla; y no va en Pantalla porque su
fixture volveria a medir la geometria pre-existente de **UI-015** (el slider y los switches por
debajo del target de 44 px), que esta **N/A** en este incremento. Asi los contratos cierran **sin
agregar un waiver**.

### Persistencia: `params.visual_profile` (y por que NO es `waterfall_battery`)

| pieza | donde | que hace |
|---|---|---|
| valor reservado | `src/visual_policy.h`, `VP_PARAM_AUTO` = `0xFF` | fuera del enum: agregar un perfil no convierte un "automatico" guardado en un perfil fijo |
| traduccion (pura) | `visual_policy_override_from_param(u8)` | `0..VP_PROFILE_COUNT-1` -> `VP_MANUAL(perfil)`; **cualquier otro valor** (AUTO o un byte desconocido) -> `VP_AUTO` (fallback declarado) |
| store | `src/params/params.h` (`params_uint8_t visual_profile`), `.c` | carga por nombre y guarda por nombre (`params_load_uint8`/`params_save_uint8`); por defecto `VP_PARAM_AUTO` |
| ROM | `src/radio.c` | `apply_visual_override_at_boot()` (con el byte cargado por `params_init`) y **por muestra** en `radio_tick()`, los dos convertidos con `visual_policy_override_from_param()` |
| arnes | `tools/ui_harness/src/screens.c` | `build_main()` usa **la misma conversion** (mock_params deja AUTO), asi el banco no puede divergir del ROM |

La politica **no incluye `params.h`** (solo recibe el byte) y **nunca escribe el parametro**: el
unico que lo guarda es la fila de Ajustes. `waterfall_battery` sigue siendo **otra cosa** —el toggle
de usuario del modo Ahorro que vive en disco—; el perfil es la **capa por encima** del toggle
(ECO/CRITICAL lo fuerzan, QUALITY/BALANCED lo devuelven al usuario), no el toggle. Los defaults de
ingenieria (umbrales mV, caps de brillo) no se tocan: son de la pasada fisica.

### Los 2 clamps plegados

1. **Preview del slider de brillo** (`backlight_brightness_update_cb`, `src/dialog_settings.c`): el
   `LV_EVENT_VALUE_CHANGED` del arrastre llamaba `backlight_set_brightness(params.brightness_normal)`
   **directo**. Ahora llama `visual_policy_brightness_rewrite(params.brightness_normal)`: el setting
   se guarda entero en `params`, pero el nivel que va al panel es `min(setting, cap del perfil
   vigente)`.
2. **`backlight_switch()`** (`src/backlight.c`): el camino de vuelta a encender llamaba
   `set_brightness(params.brightness_normal)` directo (saltaba el cap). Ahora pasa por
   `visual_policy_brightness_rewrite()`; `on` se pone en `true` **antes** porque
   `backlight_set_brightness()` solo escribe con el display encendido (el gate de la puerta unica).
   El apagado no pasa por el cap: sube el PWM al maximo con la luz apagada (no emite).

Ambos clamps son la **misma query** que ya usan el tick por input y el timer de idle (incr. 4): el
cap lo deja vigente el `apply` y **sobrevive** tanto al preview como al OFF/ON.

### Fixture y regiones (sin waivers nuevos)

- **`settings-perfil`** (nueva): pantalla principal real + Ajustes por el camino del firmware, con el
  grupo **Apariencia** a la vista (`buttons_press(4)` -> "Mas >", `buttons_press(2)` -> "Apariencia",
  el mismo `settings_goto()` del usuario). La bateria es **11.2 V sin fuente externa**, asi que el
  automatico decide **ECO**: el desplegable dice `Auto` (el seteado) y la etiqueta `Visual profile
  (Eco)` (el VIGENTE). `scenario_settle` **codifica** ese texto exacto (`label_on_screen`): si el
  perfil de la bateria o la fila se mueven, el arnes corta en vez de capturar otra cosa. Golden nuevo
  en `tools/ab_testing/golden/settings-perfil.png`.
- **`ajustes_perfil`** (`quality/regions.yaml`): la caja de la fila (x=20..783, y=199..255), medida
  con `--inspect` sobre el render.
- **`ajustes_scrollbar`** + un **`expected_change`** acotado: la fila nueva alarga el contenido de la
  grilla **una fila (54+5 px)**, asi que el pulgar de la barra de scroll —que solo se dibuja en la
  fixture scrolleada `settings-wfbat`— queda mas corto. Es la **unica** region que se mueve en
  `settings-wfbat`. El `expected_change` lleva autor y fecha de vencimiento, y **no** se agrego
  ningun waiver (`exceptions` queda con las dos entradas de UI-015 de siempre).

`quality/contracts.yaml` gana un solo `expected_changes` (el del pulgar); ningun cambio de regla ni
`exceptions` nuevo.

### Evidencia (host, 2026-10-14, A = `688e393` -> B = `8b26972`)

```text
$ tests/check_harness_determinism.sh            # 24 fixtures x 2 corridas
settings-home:   d83f57b39ff7706dffc7ad041269781c7e2a1520f7b27e35f05232b5e63b5395
settings-perfil: 72d3197c3063b5a829e2161b27907c34f887a6ae1d4eee957264d7a585825f83
settings-wfbat:  2b9e1a08bc40b6eeece3df7414088575652790c9d13feee88c9aac29966fff24
... (las 21 fixtures previas con el MISMO sha256 del tip 688e393)
PASS: deterministic (identical PNGs for every fixture)

$ cd tools/ab_testing && uv run x6200-ab compare --a 688e393 --b HEAD --experiment-id AB-UI08-I5
experiment AB-UI08-I5: PASS (exit 0)
  Functional PASS · Visual Regression PASS (no unexplained visual regressions)
  Quality Contract PASS (239 checks) · Performance PASS · Hardware BLOCKED · Energy BLOCKED
  23/23 fixtures (las de A): solo settings-wfbat cambia, 50 px, todos en `ajustes_scrollbar`

$ uv run x6200-ab contracts --revision HEAD
contracts 8b26972043aa: PASS — 249 checks, 0 failure(s), 0 unchanged baseline defect(s)

$ uv run x6200-ab audit --revision HEAD --suite visual
audited 8b26972043aa: 16 known issues — 0 reproduced, 14 unverified; structural
inspection REAL over 2679 objects on 24 scenario(s): 0 structural finding(s)

$ uv run pytest -m "not e2e" -q           → 170 passed, 17 deselected
$ uv run pytest -m e2e -q                 → 17 passed, 170 deselected
$ uv run ruff check . / ruff format --check . → limpios
tests/native/run_native_tests.sh          → 790 + 7 + 19 comprobaciones, 0 fallos
repo static checks (cfg_fills/actions/settings_focus/apps_menu/app_handlers/wcag) → PASS
```

Evidencia cruda del A/B en
[`tools/ab_testing/reports/AB-UI08-I5/`](../tools/ab_testing/reports/AB-UI08-I5/) (reporte, `results.json`,
diffs por fixture, screenshots). Impacto en pixeles: **50 px cambiados en total** (un solo fixture, la
barra de scroll), **0 regresiones sin explicar**. Como en los incrementos anteriores, la fixture nueva
**no entra en el `compare`** (la suite sale de A): su cobertura la dan el determinismo (24x2),
`contracts`/`audit` sobre HEAD y su PNG commiteado.

`settings-home` no se mueve (sha256 identico al del tip): la fila nueva **no toca** ningun pixel de
las fixtures previas salvo el pulgar de la barra de scroll de `settings-wfbat`, que esta declarado.

### Anti-falso-verde

- **Persistencia**: el test nativo cubre `visual_policy_override_from_param` entera (los cuatro
  indices, `VP_PARAM_AUTO`, `VP_PROFILE_COUNT` y bytes desconocidos), los nombres de la UI y
  `visual_policy_current()`, mas el camino de punta a punta (byte -> override -> apply -> cap). El
  byte se guarda/carga por nombre en `params.c` (mismo mecanismo que los vecinos).
- **Clamps**: el test nativo de `src/backlight.c` (`tests/native/test_backlight.c`) asserta que el
  OFF/ON encendido pasa por `visual_policy_brightness_rewrite()` con el nivel de `params`; volver a
  un `backlight_set_brightness()` directo deja el recorder vacio y el test rojo. El clamp del preview
  usa la **misma** funcion (cubierta en `test_visual_policy.c`).
- **La fila**: la fixture **codifica** el texto del perfil vigente (`Visual profile (Eco)`) y el
  golden lo fija pixel a pixel.

### Stretch: el wiring del spectrum — PENDIENTE (con analisis)

El tercer consumidor que el header de `visual_policy.h` declara pendiente **no se hizo**. El
waterfall tiene un hook listo (`waterfall_set_battery_mode()` -> `refresh_period`), pero el
**espectro no tiene equivalente**: su dibujo es por evento (`LV_EVENT_DRAW_MAIN_END`,
`src/spectrum.c:75`) y no hay ningun contador de frames ni setter de cadencia. Conectarlo requiere
**diseno nuevo** (un contador + una funcion tipo `spectrum_set_refresh_period()` y decidir que se
saltea: el espectro, o solo el relleno), fuera del alcance de "solo si hay un hook listo". Queda como
candidato del proximo incremento de UI-08.

### PENDIENTES honestos

- **Calibracion fisica**: los umbrales de mV y los caps de brillo siguen siendo **defaults de
  ingenieria**; el canal de bugs de la radio (#15) los ajusta en la pasada fisica.
- **Puerta del owner (layout de bateria)**: el reparto visual de la bateria en el reloj de potencia
  es decision del owner; este incremento no lo toca.
- **Etiqueta "vigente"**: es una **foto al abrir Ajustes** (no se re-evalua con el dialogo abierto).
  El perfil no cambia mientras el dialogo esta abierto salvo por este control, asi que una
  re-lectura live no agregaria informacion hoy; queda anotado por si en el futuro el dialogo se abre
  de forma no modal sobre la pantalla.

## Audio: el slider de Rec gain y el volumen que el control remoto no veia (issues #26 y #30) ✅

Dos issues de audio en el mismo cambio: uno de UI (el slider de entrada movia la salida) y uno de
estado (el control remoto contestaba `"vol": 0` con el volumen en la base). El segundo lo decidio el
banco: ahi no hay stack de audio, pero el camino `params.db -> estado -> control remoto` si corre,
y ahi estaba el defecto.

### #26: el slider de `Rec gain` movia la salida

`rec_gain_update_cb()` (`src/dialog_settings.c`) salio copiado del callback de al lado y llamaba a
`audio_set_play_vol()`: el slider de la ganancia **de entrada** movia la de **salida**, y su propia
ganancia no se aplicaba nunca (lo que reporta el issue). El arreglo es la palabra que el issue
propone: `audio_set_rec_vol()`.

**El chequeo estatico** (`tests/check_settings_focus.py`, uno de los seis de la matriz) ahora
comprueba, ademas del grupo de foco, que **cada slider de Ajustes mueve el parametro que muestra**:
el cuerpo de su callback tiene que nombrar el parametro que el `slider_with_text()` le pasa, y **dos
sliders distintos no pueden aplicar la misma funcion**. Revirtiendo a mano esa sola palabra, el
chequeo queda rojo:

```text
$ python3 tests/check_settings_focus.py
  widgets de foco creados: 29 | dialog_item(): 30
  sliders con parametro propio: 4
  FALLA linea 1399: el slider de params.rec_gain_db_f (rec_gain_update_cb) y el de
  params.play_gain_db_f (play_gain_update_cb, linea 1389) aplican los dos
  audio_set_play_vol() -> uno de los dos mueve lo del otro
```

**Un defecto adyacente, de la misma fila** (lo encontro la fixture nueva): la etiqueta de valor de
los dos sliders de ganancia salia con basura. Medido con el inspector del arnes sobre el render de
`settings-audio`, los dos labels de la fila "Play,Rec gain" median 11 px y decian `f`. Eran dos
causas de codigo sumadas: `slider_with_text()` le pasaba un `int32_t` a un formato `"%.1f"` (tipo
equivocado en los argumentos variables), y el `lv_conf.h` **del arnes** no habilitaba el `%f` de
LVGL (`LV_SPRINTF_USE_FLOAT`, cuyo default es 0), que la ROM si habilita en el suyo (`lv_conf.h` de
la raiz): el arnes decia `f` donde la radio dice el valor, una divergencia del banco. Corregidas las
dos, la etiqueta de Play gain dice `3.0` y la de Rec gain el valor al que la fixture la arrastra
(`-7.0`).

### La fixture `settings-audio` y su fidelidad

`tools/ui_harness/fixtures/settings-audio.scn` abre el Ajustes real por el camino del firmware
(`ACTION_APP_SETTINGS` -> `dialog_construct(dialog_settings)`) y despues aprieta el tab **Audio**
(pagina 1, indice 3) con `buttons_press(3)`, que es lo que hace el usuario: `settings_goto()`
scrollea la grilla hasta el grupo y le mueve el foco. Geometria de lo que mide, del propio render
(`--inspect`): la etiqueta de la fila en la columna 0 y las dos celdas de slider de 141x56 en
`x=499` y `x=643`, la fila entera en `y=140..196` (los sliders de 46x13 con su etiqueta al lado).

Fidelidad, punto por punto:

- Los dos sliders **se ven iguales**: lo unico que los distingue es el parametro detras, y la
  fixture lo fija con `play_gain_db 3` / `rec_gain_db -4` (dos claves nuevas del `.scn`; la radio
  las carga de `params.db` con `params_load_float()`). Asi el slider de cada uno se identifica por
  su valor en el render.
- `scenario_settle` **arrastra el slider de Rec** (setea el valor y manda `LV_EVENT_VALUE_CHANGED`,
  que es el evento que manda LVGL en un arrastre) y comprueba el observable que hay: **cual de los
  dos setters corrio**. Los setters de audio del arnes (`mocks/mock_ui_deps.c`) son un **recorder**,
  el mismo idioma del stub de la politica en el test de backlight: con los dos callbacks cableados a
  la misma funcion el registro queda en `play` y la corrida **para** (no puede salir verde midiendo
  otra cosa). Ademas se comprueba que el estado quedo en `rec_gain_db_f = -7` y que
  `play_gain_db_f` no se movio.
- La fixture **codifica** lo que significa antes de usarlo: que el tab de indice 3 sea el de
  "Audio" y que la fila "Play,Rec gain" este en pantalla despues del toque; si los tabs o el grupo
  se reordenan, el arnes corta en vez de capturar otro grupo.
- El setter del slider de Rec y el parametro que guarda son **el mismo camino** que en la radio
  (`audio_set_rec_vol()` -> `params.rec_gain_db_f`), asi que el arrastre de la fixture mide la
  conexion que el issue reporta rota.

**Region y waivers**: `ajustes_audio_ganancia` (`[20, 138, 764, 60]`, `quality/regions.yaml`) aisla
la fila para que un cambio suyo no se atribuya a la banda del espectro que el dialogo tapa. Esa
region **se solapa** con `frecuencia` (y=160..196) porque la fila del grupo Audio queda sobre esa
banda cuando el grupo se scrollea al tope; el solapamiento esta escrito en el `regions.yaml`, y el
test e2e que fijaba "el unico cambio de UI-010 es `frecuencia`" ahora comprueba ademas que **todas**
las cajas cambiadas caigan adentro de la fila de frecuencia (mas fuerte que antes, no mas debil). La
fixture agrega su propia excepcion acotada y fechada de **UI-015** (`layout_touch_target`) por la
geometria heredada de esas filas: los cuatro sliders de 13 px y el switch de "Speaker mode" (111x30),
los mismos controles que ya excepcionan `settings-home` y `settings-wfbat`.

**Sin `expected_changes` y sin golden, con el porque**: la fixture **no existe en A** y la suite del
`compare` sale de A, asi que no entra en el diff A/B (igual que `settings-perfil` en UI-08 incr. 5):
su cobertura la dan el determinismo (25x2), `contracts`/`audit` sobre HEAD y su render. El golden
(`tools/ab_testing/golden/`) es la entrada de la tabla de contraste no-texto
(`quality/nontext.yaml`) y esta fixture no declara ningun objeto no-texto: un PNG ahi que nadie mide
seria peso muerto, asi que no se agrega (archivar el render es una decision aparte).

### #30: la repro en el banco y el fix del espejo del control remoto

**La repro, medida en el banco** (binario ARM de la imagen, `tools/captura-banco.sh`, commit
`5f07ac6`, md5 `b662ca41cde6`): con `vol = 10` en `params.db`, el `GET` del control remoto
contestaba `"vol": 0` (`nb` y `nr` tambien en 0 con 1 y 1 en la base) y el log tenia **28 avisos de
sujeto NULL**, que son 7 por cada `GET` de esa corrida: `vol`, `sql`, `nb`, `nb_level`, `nb_width`,
`nr`, `nr_level` - los siete Param del espejo que leen items de `cfg` en `state_line()`
(`src/remote_server.cpp:610-614`).

**La causa, en el codigo**: `cfg_sm` (`src/cfg/settings_manager.cpp`) es un global C++ y su
constructor corre antes de que `cfg_init()` cree los items de `cfg` y sus subjects; los Param de
esos items quedaban ligados a un `Subject *` NULL **para siempre** (`init_live()` rehacia el mapeo
de los subjects del dial, `cfg_cur.*`, pero no el de los persistidos). Leer un Param asi devuelve el
neutro (0) y escribir no hace nada, en silencio: es el `"vol": 0` del issue. `pwr` no estaba roto
porque su `tief()` lee `cfg.pwr.val` en cada llamada, no al construir.

**El fix**: `init_persisted()` vuelve a ligar los Param persistidos y `cfg_sm_init_live()` (el
ultimo paso de `cfg_init()`) lo llama junto con `init_live()`; `check_live()` pasa a recorrer los
**20** parametros (persistidos y de estado vivo) y **devuelve** cuantos quedaron sin subject.

**Anti-falso-verde**: el arnes compila ahora `src/cfg/settings_manager.cpp` (contra los headers
reales de `cfg`/`params`) y `mocks/mock_settings_manager.cpp` **repite el ultimo paso del arranque**
y lo comprueba: `cfg_sm_init_live()` sobre los items que `mock_cfg_init()` ya creo, `check_live() == 0`
y que el espejo y el estado digan lo mismo (`vol`, `sql`, `nb`, `nr`, `band`). Con el mapeo sin
rehacer, **toda** corrida del arnes corta ahi. El camino real lo convalida el banco, con el fix
(commit `54341c8`, md5 `e8572f2d8355`): el `GET` contesta el valor de la base (`vol: 8`), el comando
`vol` del control remoto mueve ese estado (`vol 30` -> `"vol": 30`) y el log queda con **0** avisos
de sujeto NULL. El detalle medido esta en `docs/HALLAZGOS.md` §40.

**Lo que NO se puede medir en el banco**: el silencio del sink (el `parecord` del monitor con
RMS -240 dBFS del issue). El banco corre la GUI con framebuffer falso y sin Pulse ni codec: no hay
monitor que grabar. Ese tramo (`x6200_control_rxvol_set` -> codec) es del **canal fisico (#15)**. No
se reintento la migracion de `mixer_setup()` a la API de ALSA: se probo y se revirtio con datos
(`docs/HALLAZGOS.md` §37, y el `fb640dc` que la revierte).

### Evidencia (host, 2026-10-14, A = `5f07ac6` -> B = `54341c8`)

```text
$ tests/check_harness_determinism.sh
building ui_harness (revision 54341c8e26d9...)
running every fixture 2 times...
app-bands: 5781b1f17a972051925f34c23a2de4518e0710b573cfda5dd1b595752a8044c3
...
settings-audio: 015e2798d26bce9f18b6fd19431a65b2846db16f7bdeab469530f27e54d99885
PASS: deterministic (identical PNGs for every fixture)      [25 fixtures x2]
$ cd tools/ab_testing
$ uv run x6200-ab compare --a 5f07ac6 --b HEAD --experiment-id AB-AUDIO2630
experiment AB-AUDIO2630: PASS (exit 0)
  Functional         PASS            (required) — 24 scenarios captured on both revisions
  Visual Regression  PASS            (required) — captures are pixel-identical to the baseline
  Quality Contract   PASS            (required) — 249 contract checks passed on the candidate
  Performance        PASS            (required) — render-cost proxy within budget
  Hardware           BLOCKED         (advisory) — no physical X6200 attached; hardware behaviour is not measured
  Energy             BLOCKED         (advisory) — no power measurement; energy evidence must be physical
$ uv run x6200-ab contracts --revision HEAD
contracts 54341c8e26d9: PASS — 264 checks, 0 failure(s), 0 unchanged baseline defect(s)
$ uv run x6200-ab audit --revision HEAD --suite visual
audited 54341c8e26d9: 16 known issues — 0 reproduced, 14 unverified; structural inspection REAL over 2963 objects on 25 scenario(s): 0 structural finding(s)
$ uv run pytest -m "not e2e" -q
181 passed, 17 deselected in 2.03s
$ uv run pytest -m e2e -q
17 passed, 181 deselected in 123.05s (0:02:03)
$ tests/native/run_native_tests.sh
791 comprobaciones, 0 fallos     (suite principal: adaptive_tuning, audio_enhancement, ft8_udp,
                                update_check, band_plan, avi_writer, wfview, cluster, field, theme,
                                teclado, bt_devices, dxcc, flags, controls, voice, gps, visual_policy)
7 comprobaciones, 0 fallos       (clock, UI-009)
19 comprobaciones, 0 fallos      (backlight, UI-08 incr. 4)
$ 6 chequeos estaticos (check_cfg_fills, check_actions, check_settings_focus, check_apps_menu,
  check_app_handlers, check_wcag)
check_cfg_fills:    OK: los 69 campos se inicializan, en orden
check_actions:      38 acciones ofrecidas, todas con manejador
check_settings_focus: 29 widgets en el grupo de foco | 4 sliders con parametro propio
check_apps_menu:    las 22 apps del menu llegan a su dispatcher
check_app_handlers: 268 botones en 61 paginas, 0 sin handler
check_wcag:         9 temas, 990 pares OK, 0 FALLAS
```

Y el banco, antes y despues (los dos con su md5 de binario y el `params.db` del chroot):

```text
# A = 5f07ac6, binario md5 b662ca41cde6 (la GUI viva del banco)
$ sqlite3 .vfix3/mnt/params.db "select name,val from params where name in ('vol','sql','nb','nr');"
nb|1
nr|1
sql|0
vol|10
$ docker run --rm --entrypoint "" -v .vfix3:/t x6200-buildroot \
      sh -c "chroot /t /tmp/x6200-remote '{\"cmd\":\"get\"}'"
{"t":"state","vfo":"A","freq":21250000,...,"sql":0,"vol":0,...,"nb":0,"nr":0,"nb_level":0,
 "nb_width":0,"nr_level":0,"pwr":50,...,"did":""}
$ grep -c "sujeto NULL" .vfix3/out-banco.txt
28
# B = 54341c8, binario md5 e8572f2d8355 (el mismo banco, mismo chroot)
$ sqlite3 .vfix3/mnt/params.db "select name,val from params where name in ('vol','sql','nb','nr');"
nb|1
nr|1
sql|0
vol|8
$ docker run --rm --entrypoint "" -v .vfix3:/t x6200-buildroot \
      sh -c "chroot /t /tmp/x6200-remote '{\"cmd\":\"get\"}'"
{"t":"state","vfo":"A","freq":21250000,...,"sql":0,"vol":8,...,"nb":1,"nr":1,"nb_level":0,
 "nb_width":0,"nr_level":0,"pwr":50,...,"did":""}
$ docker run --rm --entrypoint "" -v .vfix3:/t x6200-buildroot \
      sh -c "chroot /t /tmp/x6200-remote '{\"cmd\":\"vol\",\"val\":30}'"
{"t":"ok"}
# y el GET que sigue: ..."vol":30...
$ grep -c "sujeto NULL" .vfix3/out-banco.txt
0
```

Sobre el `mixer` y el sink, lo unico que se puede afirmar hoy: el estado que ve el control remoto es
el de la base y el comando `vol` lo mueve (medido arriba). Que eso suene es del canal fisico.

### PENDIENTES honestos

- **El sink real (issue #30, canal #15)**: el banco no tiene stack de audio, asi que el
  `RMS -240 dBFS` del monitor no se reproduce ahi. El defecto del *estado* quedo arreglado y medido;
  el tramo del aplicador al codec queda para la pasada fisica. **Sin fundar, no se afirma nada mas**.
- **Sin test nativo para `settings_manager`**: la pieza se cubre compilando el modulo real en el
  arnes, contra los headers reales de `cfg`/`params` (y con el arranque repetido). Un test en
  `tests/native/` necesitaria stubs de `cfg.h`/`params.h` -menos fidelidad que la que ya hay-, asi
  que no se agrego; si el nativo quiere cubrirlo, el stub de `params.h` del backlight es el patron.
- **La etiqueta de los sliders de ganancia**: el valor que el slider guarda es un float y el slider
  trabaja en enteros, asi que un `+3.7 dB` guardado se muestra como `3.0`. La truncacion es
  preexistente y no se toco (fuera del alcance del issue); queda anotado.
- **Un test nativo flakey observado**: en una corrida del suite nativo en paralelo con el build de
  Docker, `tests/native/test_wfview.c:309` (`CHECK(n > 0x15)`, el socket del servidor CI-V) fallo
  una vez; tres corridas seguidas despues dieron `791 comprobaciones, 0 fallos` y el commit base
  tambien pasa. Ningun archivo de los que toca este cambio se compila en ese suite. Queda anotado
  como candidato a flake por carga, sin tocarlo.

## Cierre de las semillas del issue #11 (UI-002/004/005/007/008) — 2026-10-15

Corrida `factory/seeds-11-fixtures` (base `c4ff0c5`, 25 fixtures) sobre las cinco semillas. Las
cuatro que el arnes podia medir quedan con fixture y **ninguna reproduce defecto**; la quinta es un
presupuesto de recursos y queda declarada `out-of-scope`. Los detalles por semilla y la decision de
UI-008 estan en `docs/UI_KNOWN_ISSUES.md` → *Cierre de las semillas del issue #11*; el registro
(`quality/known_issues.yaml`) pasa UI-002/004/005/007 a `fixed` y UI-008 a `out-of-scope`.

Lo que agrega esta corrida, en piezas:

- **UI-002** (`apps-menu-1..6`): seis fixtures abren el menu de apps por el camino del firmware
  (`buttons_load_page_group(buttons_group_app)`, pagina por pagina con `buttons_press`) y cubren las
  **22 celdas** post-About. `scenario_settle` lee la tabla real `buttons_group_app` y corta la corrida
  si el rotulo `(APP n:6)` o una app de la pagina no quedan en la barra. La suite de fixtures pasa de
  **25 a 33**.
- **UI-004** (`layout_focus_unreachable`): el inspector emite `in_keyboard_group` por nodo (y
  `keyboard_group` en el bloque) y la regla marca todo control de clase de foco que este dibujado y
  fuera del grupo. Cero hallazgos sobre los fixtures que abren Ajustes, `bt-dialog`, `main-foco` y
  `apps-menu-*`. Autoprueba e2e: la revision efimera `abtest/focus` quita el
  `lv_group_add_obj()` de `dialog_item()` y el audit falla.
- **UI-005** (`bt-dialog`): el dialogo real de Bluetooth (`ACTION_APP_BT` → `src/dialog_bt.c`) con
  `btaudio-ctl` mockeado (`__wrap_system` + una lista sembrada). Medido: la linea de estado
  "no headphones, RX off" termina en y2=396, **arriba** de la fila de botones (y=418) → no reproduce
  el texto tapado; `scenario_settle` codifica ese invariante. El contrato
  `typography_text_truncated` encontro y se cerro un recorte real de 3 px de la **nota** del dialogo
  (363 px de texto en una caja de 360; `NOTE_W`, `src/dialog_bt.c`).
- **UI-007** (`settings-rs-theme`): la ventana de Ajustes (`settings-wfbat`) bajo el tema **RS**,
  scrolleda al grupo Espectro: los ocho switches salen con `bg_color #6E6E6E` (el `control_border` de
  RS) — la evidencia renderizada que faltaba; el par ya lo cubria el chequeo estatico.

### La dispersion del benchmark: un frame preemptado no es "el host es ruidoso"

En la suite `-m e2e` completa el gate del benchmark (CLOCK_MONOTONIC sobre un host compartido)
devolvio **INCONCLUSIVE** de forma intermitente y el test fallaba; el mismo test pasaba aislado. El
numero que lo decide es `dispersion_cv_pct` del lado medido, comparado contra
`max_dispersion_cv_pct: 25.0`:

| Corrida | frames | `stdev/mean` del lado | veredicto |
|---|---|---|---|
| suite plena, 1a (con el suite nativo en paralelo) | 15 | **29.1%** (B, `bench_regression`) | INCONCLUSIVE → el assert de FAIL falla |
| host quieto, `--bench --repeats 40` | 40 | 3.1% | PASS (dispersion robusta 1.0%) |
| con compilacion nativa en paralelo (2 frames de 1.24x) | 15 | 7.8% | la robusta mide 1.4% |

El `stdev/mean` lo domina **un** frame que el host preempta, y ese frame no puede mover un p95 (1
frame de 20 queda por debajo del percentil 95, asi que no cambia ninguna comparacion de p95): la
pregunta del gate es "¿una fraccion no trivial de la muestra es outlier?", que es lo que si mueve un
p95. `dispersion_cv_pct` pasa a ser el coeficiente de variacion robusto por mediana
(`robust_dispersion_cv_pct`: `1.4826 * MAD / mediana`), **sin tocar el umbral** (25%) ni los demas
presupuestos; una muestra ruidosa de verdad sigue leyendo muy por encima y declarando INCONCLUSIVE
(sample alternado 1/10 del test unitario: 121%; `tests/test_performance.py`,
`test_noisy_side_is_inconclusive_not_pass`), y un frame aislado ya no alcanza
(`test_a_few_host_spikes_are_not_dispersion`).

### Evidencia (host, 2026-10-15; gates de A/B sobre `1c3db39`)

```text
$ tests/check_harness_determinism.sh
running every fixture 2 times...
app-bands: 5781b1f17a972051925f34c23a2de4518e0710b573cfda5dd1b595752a8044c3
apps-menu-1: bf6ff1457fe52d736502b9280c19842e7c2381430c8464f8fb257b636d26d90e
...
settings-rs-theme: e8bfb3a3077a481a06f3f518a4a2298faef2863026cbcf6f94a36f880aefbb0b
PASS: deterministic (identical PNGs for every fixture)      [33 fixtures x2]
$ cd tools/ab_testing
$ uv run x6200-ab compare --a c4ff0c5 --b HEAD --experiment-id AB-SEEDS11
experiment AB-SEEDS11: PASS (exit 0)
  Functional         PASS            (required) — 25 scenarios captured on both revisions
  Visual Regression  PASS            (required) — captures are pixel-identical to the baseline
  Quality Contract   PASS            (required) — 264 contract checks passed on the candidate
  Performance        PASS            (required) — render-cost proxy within budget
  Hardware           BLOCKED         (advisory) — no physical X6200 attached; hardware behaviour is not measured
  Energy             BLOCKED         (advisory) — no power measurement; energy evidence must be physical
$ uv run x6200-ab contracts --revision HEAD
contracts 1c3db3940828: PASS — 352 checks, 0 failure(s), 0 unchanged baseline defect(s)
$ uv run x6200-ab audit --revision HEAD --suite visual
audited 1c3db3940828: 16 known issues — 0 reproduced, 14 unverified; structural inspection REAL over
3861 objects on 33 scenario(s): 0 structural finding(s)
$ uv run x6200-ab themes
themes PASS: 9 served theme(s) (6 built-in + 3 file(s)), 9 pair(s) declared, 81 pair(s) checked,
0 failure(s); non-text (WCAG 1.4.11, pixels): 21 object(s) measured, 0 failure(s)
$ uv run pytest -m "not e2e" -q
182 passed, 18 deselected
$ uv run pytest -m e2e -q          # tres corridas completas seguidas tras el fix
18 passed, 182 deselected in 121.40s (0:02:01)
18 passed, 182 deselected in 122.03s (0:02:02)
18 passed, 182 deselected in 122.30s (0:02:02)
$ tests/native/run_native_tests.sh
791 comprobaciones, 0 fallos + 7, 0 fallos (clock, UI-009) + 19, 0 fallos (backlight)
repo static checks (cfg_fills/actions/settings_focus/apps_menu/app_handlers/manual_apps/wcag) → PASS
```

El `compare` es **PASS sin un solo pixel cambiado** en las 25 fixtures que A y B comparten: los
fixtures nuevos no tocan ninguna pantalla existente. La unica fuente que se movio es la del nuevo
`bt-dialog` (el ancho de la nota, `NOTE_W`), que A no tiene y por eso no entra en el diff A/B. El
corpus pasa de **25 a 33 fixtures** y el contrato suma 352 chequeos. La evidencia de la corrida es
`tools/ab_testing/reports/AB-SEEDS11/` (commiteada al lado).

## UI-07 remanente: wakeups del tick y pantalla apagada (issue #13) — corrida `factory/spectrum-eco-13`

Cierre del remanente declarado de UI-07 (`docs/spectrum-eco-spec.md`): medir el
wake-up del tick, verificar la ruta de pantalla apagada y decidir el espectro en
ECO. **La ROM no cambia** (ni un archivo de `src/`, `lvgl/` pineado `74d0a816`):
las dos primeras piezas terminan en **medición + guardas del banco** y la tercera
en **PENDIENTE fundado**.

### 1. Wakeups: el hilo del tick es el 93.5% de los wakeups de la GUI

La ROM tiene dos fuentes de wakeups: el lazo de `src/main.c` (duerme hasta el
próximo timer que `lv_timer_handler()` diga; los posts del DSP **no** lo
despiertan — `src/scheduler.cpp` es una cola común que se drena en la próxima
vuelta) y el hilo del tick (`while (1) { usleep(5 * 1000); lv_tick_inc(5); }`).

Modo nuevo y adicional del arnés, `--bench-wakeups [MS]` (ver
`tools/ui_harness/README.md`): reproduce el lazo sobre la **misma base de timers**
que la ROM arma (los componentes reales son los que crean y arman los timers) y
lo reporta al lado de la cadencia del tick, que el gate lee de `src/main.c`. El
lazo corre con el periodo de refresco **de la ROM** (`LV_DISP_DEF_REFR_PERIOD 50`,
`lv_conf.h`), no con el default de LVGL del `lv_conf.h` del host (30 ms): el gate
lee los dos números (y la cadencia del DSP de `src/dsp.cpp`) de las fuentes.

| medición (fixture `main-natural`, seed 13, ventana 1000 ms) | valor |
|---|---|
| wakeups del lazo, con el camino de datos | **14** |
| wakeups del lazo, sin el camino de datos (control) | **11** |
| sueños del lazo (min–max) | **10–200 ms** |
| px flusheados en el lazo, con / sin datos | **2492000 / 414080** |
| wakeups del hilo del tick (5 ms) | **200** |
| **share del tick sobre las dos fuentes** | **93.5%** |
| vueltas sin sueño (`lv_timer_handler()` en 0) o sin timer armado | **0** |

Con `--bench-wakeups 3000`: lazo 31, tick 600 (el conteo escala con la ventana).
Evidencia cruda: `tools/ab_testing/reports/WAKEUPS-ECO13/`.

**Recomendación (no aplicada: el spec pide medir y recomendar).** Con el tick como
93.5% de los wakeups, la vía es sacar el hilo, no optimizar el lazo: (1)
`LV_TICK_CUSTOM 1` con una fuente monotónica (`clock_gettime(CLOCK_MONOTONIC)`, que
la ROM ya usa en `get_time()`) elimina el hilo entero (200 → 0 wakeups/s) y toca
`lv_conf.h` + `src/main.c`, **no** `lvgl/`; (2) sin tocar `lv_conf.h`,
`usleep(20*1000)` + `lv_tick_inc(20)` en `src/main.c` baja a 50 wakeups/s a costa
de la resolución del tick. Cualquiera de las dos es una decisión sobre la cadencia
global de la ROM y va con pasada por hardware: el banco no mide el costo de un
wakeup (x86; la energía del Cortex-A7 sigue BLOCKED).

### 2. Pantalla apagada: la ruta de la ROM ya se salta todo el render (medido)

`--display-off` recorre la ruta real (la tecla POWER: `src/keypad.c` →
`main_screen_keypad_cb` → `backlight_switch()` → `lv_disp_enable_invalidation(false)`,
con `src/backlight.c` compilado de verdad en el arnés) antes de la corrida natural:

| 40 frames, drive de datos activo | pantalla encendida | pantalla apagada |
|---|---|---|
| `natural.totals.invalidated_px` | 6745720 | **0** |
| `natural.totals.flushes` / `rects` / `cycles` | 45 / 45 / 37 | **0 / 0 / 0** |
| `metrics.flushes` / `flushed_pixels` (captura) | 6 / 1127680 | 5 / **743680** |
| PNG de la corrida | `8ec63121…` | `3986bc9a…` (= la captura común) |

El espectro, la cascada y el reloj siguen disparando y siguen recibiendo datos (17
frames de FFT y 24 filas en la ventana): lo único que desaparece es el trabajo de
render. De la captura desaparece exactamente el repaint de pantalla completa que
el arnés hace antes de medir (384000 = 1127680 − 743680) y el PNG queda byte a
byte igual al de la pantalla encendida: **la misma imagen, menos trabajo**. No
había nada que arreglar en la ROM (el corte ya estaba); lo que faltaba era la
guarda: el modo queda en `tests/check_harness_natural.sh` +
`tests/natural_evidence.py` y la propiedad del módulo real (una sola llamada a
`lv_disp_enable_invalidation` por switch, `false` al apagar y `true` al volver) en
`tests/native/test_backlight.c`.

### 3. PENDIENTES honestos

1. **Espectro en ECO (diseño, no medido).** El cableado del espectro al perfil
   sigue pendiente y **no se fuerza**: el dibujo del espectro es por evento
   (`LV_EVENT_DRAW_MAIN_END`), no hay contador de frames ni setter de cadencia, y
   la decisión de producto (¿se saltea cadencia, relleno o banda?) no es de esta
   corrida. Diseño mínimo para el próximo run (5 líneas): (a) el perfil ECO fija
   un período de refresco del espectro (contador en `spectrum_refresh()` como
   `refresh_period` del waterfall, `src/spectrum.c`), (b) el primer refresco tras
   encender/ECO siempre dibuja (borde de subida), para no dejar el panel vacío
   cuando los datos llegan despacio, y (c) la medición es `--bench-natural` con el
   fixture `main-natural-eco` (el camino ya existe: el perfil llega al waterfall).
   Potencial **medido** (no implementado): el espectro es 132000 px × 16 frames =
   **2112000 px de los 6745720** del bench natural (**31.3%**); una cadencia de
   1/4 dejaría ~528000 px de espectro, ~23% del total, sujeto al reagrupamiento de
   rectángulos de LVGL que ya se midió en el waterfall (que bajó 48.5% con una
   cadencia de 1/4, no el 75% nominal). Requiere además decidir el borde de bajada
   (que el último frame no quede sin dibujar) — es la parte que hace que el cambio
   no sea chico y por eso queda para un run propio.
2. **El redibujo offscreen de la cascada sigue corriendo con la pantalla apagada
   (hallazgo, sin medición en el banco).** `refresh_waterfall()`
   (`src/waterfall.c`) hace `redraw_cb(NULL)` —reconstruye los 800×245 px de la
   paleta— **antes** de invalidar; con el display apagado la invalidación es
   nop pero el dibujo offscreen ya ocurrió: ~196000 px de trabajo de software por
   fila a 25 filas/s si el DSP sigue alimentando. El `--display-off` de hoy **no lo
   ve** (el banco mide lo que el driver flushea, y ahí no llega nada): haría falta
   un contador del redibujo offscreen (por ejemplo `redraw_cb` contado en el
   arnés, o un instrumento explícito del componente) para fundar el fix
   (`refresh_waterfall()` salteando el redibujo mientras el display está apagado,
   y un redibujo al volver). El espectro, en cambio, no dibuja nada fuera de
   LVGL: su `spectrum_refresh()` sólo invalida.

### Gates (salida real, HEAD `9b6fe17`)

```text
$ tests/check_harness_determinism.sh          # 33 fixtures x 2 corridas
...
PASS: deterministic (identical PNGs for every fixture)
# los 33 sha256 son IDENTICOS a los del tip base ec89fc3: compilar src/backlight.c
# de verdad en el arnes no movio un pixel de ninguna fixture.

$ tests/check_harness_natural.sh
  profile: spectrum 132000 px x 16 frames, waterfall 196000 px x 23 rows, clock 15840 px x 1 tick(s)
  natural totals: invalidated 6745720 px in 45 rectangles / 45 flushes over 37/40 frames
  delimitation: drive off -> 304760 px (4.5% of the 6745720 px with the drive), only clock/BT/band-info rectangles
  forced mode: 3 frames x 1 flush x 384000 px (natural: 6745720 px in 40 frames)
  CLI override: --bench-natural 10 ran 10 frames (the fixture says 40)
  display off: 0 px in 40 frames (against 6745720 px lit) and a capture 384000 px lighter
PASS: natural bench deterministic, profiled and delimited (main-natural)

$ tests/check_harness_wakeups.sh               # gate nuevo
  loop: 14 wakeups with the data drive (11 without it) in 1000 ms; sleeps 10-200 ms
  tick thread: 200 wakeups in 1000 ms (5 ms, src/main.c) = 93.5% of the two sources
  window override: --bench-wakeups 3000 ran 3000 ms with 31 loop wakeups (the default window: 1000 ms)
  plain capture: no `wakeups` block (the mode is additive)
PASS: wakeup profile measured, sourced and delimited (main-natural)

$ uv run x6200-ab compare --a ec89fc3 --b HEAD --experiment-id AB-ECO13
experiment AB-ECO13: PASS (exit 0)
  Functional PASS (33 scenarios captured on both revisions) · Visual Regression PASS
  (captures are pixel-identical to the baseline) · Quality Contract PASS (352 contract checks)
  Performance PASS · Hardware BLOCKED · Energy BLOCKED

$ uv run x6200-ab contracts --revision HEAD
contracts 9b6fe17abc54: PASS — 352 checks, 0 failure(s), 0 unchanged baseline defect(s)

$ uv run x6200-ab audit --revision HEAD --suite visual
audited 9b6fe17abc54: 16 known issues — 0 reproduced, 14 unverified; structural inspection REAL
over 3861 objects on 33 scenario(s): 0 structural finding(s)

$ uv run pytest -m "not e2e" -q
187 passed, 18 deselected
$ uv run pytest -m e2e -q
18 passed, 187 deselected in 126.06s (0:02:06)
$ tests/native/run_native_tests.sh
791 comprobaciones, 0 fallos + 7, 0 fallos (clock, UI-009) + 23, 0 fallos (backlight:
la guarda nueva de lv_disp_enable_invalidation suma 4 sobre las 19 de UI-08 incr. 4)
$ 6 chequeos estaticos (check_cfg_fills, check_actions, check_settings_focus, check_apps_menu,
  check_app_handlers, check_wcag) → PASS
$ cd tools/ab_testing && uv run ruff check . && uv run ruff format --check .   → PASS
```

Nota de herramienta: en este entorno `uv run ty check src tests` (la forma que corre el
CI) imprime `WARN No python files found under the given path(s)` con `ty 0.0.87` y sale 0;
se reprodujo igual en el checkout base (`work/x6200_gui`), así que es preexistente.
Pasándole archivos (`ty check $(find src tests -name '*.py')`) sí chequea y sale limpio.

Reproducibilidad **x3**: tres corridas seguidas de `--bench-natural`,
`--bench-natural --display-off` y `--bench-wakeups` dan los mismos números
(natural 6745720 px, apagada 0 px / captura 743680, wakeups lazo 14 / tick 200) y
los mismos PNG (`8ec63121…`, `3986bc9a…`, `c1c66701…`); los JSON sólo difieren en
`png_path`/`png_bytes` (el nombre del archivo de salida).

Evidencia commiteada de la corrida: `tools/ab_testing/reports/AB-ECO13/`
(compare A/B), `tools/ab_testing/reports/WAKEUPS-ECO13/` (perfil de wakeups) y
`tools/ab_testing/reports/NATURAL-UI07-N1/off.json` (pantalla apagada).

## eco-final: tick sin hilo, cadencia ECO del espectro y corte del redibujo offscreen (#13) ✅

Aplica las tres decisiones del owner sobre los tres pendientes que dejó la corrida
anterior: (2) `LV_TICK_CUSTOM` con `CLOCK_MONOTONIC` — el hilo del tick se
elimina; (3) el wiring del espectro al perfil (cadencia ECO); (4) el corte del
redibujo offscreen de la cascada con la pantalla apagada. No toca `lvgl/`
(pinned `74d0a816`), audio, fuentes libres ni DSP/RF/TX.

### Decision 2 — el hilo del tick desaparece (`lv_conf.h` del proyecto + `src/main.c`)

`lv_conf.h` (el del **proyecto**, no el de `lvgl/`) pasa a `LV_TICK_CUSTOM 1` con
`LV_TICK_CUSTOM_INCLUDE <time.h>` y una expresión de milisegundos monotónicos
(`clock_gettime(CLOCK_MONOTONIC)`), la misma fuente que `get_time()`
(`src/util.cpp`). El hilo `while (1) { usleep(5 * 1000); lv_tick_inc(5); }` sale
de `src/main.c`. La resolución no cambia: 1 ms monotónicos, los mismos que el
hilo declaraba con `lv_tick_inc(5)` cada 5 ms.

| medición (fixture `main-natural`, seed 13, ventana 1000 ms) | antes (ec89fc3) | ahora |
|---|---|---|
| wakeups del hilo del tick | 200 (5 ms, 93.5% de las dos fuentes) | **0** (no existe) |
| wakeups del lazo de la ROM, con / sin datos | 14 / 11 | **14 / 11** (el lazo no cambia) |
| `tick_custom` en el perfil | — | `true` |

El modo `--bench-wakeups` **refleja el modo**: el CMake del arnés deriva
`HARNESS_ROM_TICK_CUSTOM` del `lv_conf.h` de la ROM y el perfil declara
`tick_custom: true`, `tick_thread_ms: 0`, `tick_thread_wakeups: 0`. El gate
`tests/check_harness_wakeups.sh` (vía `tests/wakeup_evidence.py`) sigue leyendo de
fuentes: con la macro exige el `tick_custom` y los 0 wakeups de tick **y** la
coherencia de la fuente (`time.h` + `clock_gettime` + `CLOCK_MONOTONIC` en
`lv_conf.h`); sin la macro sigue validando el hilo (cadencia leída de
`src/main.c`), así que revertir la decisión no deja el gate ciego. El arnés no
usa su propio tick (avanza LVGL con `lv_tick_inc` desde el reloj virtual), por eso
su `lv_conf.h` queda en `LV_TICK_CUSTOM 0`.

### Decision 3 — cadencia ECO del espectro (`src/spectrum.c` + el wiring de `visual_policy`)

`spectrum_set_refresh_period(frames)` + un contador en `spectrum_refresh()`: con
el perfil en ECO/CRITICAL el espectro dibuja 1 de cada `VP_SPECTRUM_ECO_PERIOD`
(= 4) refrescos; con QUALITY/BALANCED dibuja siempre (período 1). Es el mismo
mecanismo del modo Ahorro del waterfall, y el borde de bajada queda cubierto
porque al volver el período a 1 el próximo refresco dibuja (ningún frame queda en
deuda). El wiring nuevo, `visual_policy_spectrum_apply()`, va en los **mismos dos
puntos** que el del waterfall (`src/radio.c` en `apply_visual_profile`/boot y
`tools/ui_harness/src/screens.c`), así que la decisión del banco y la de la radio
no pueden divergir; el test nativo fija la tabla perfil → período contra el stub
que registra `spectrum_set_refresh_period`.

Medido en `--bench-natural` sobre `main-natural-eco` (seed 13, 40 frames; 11.2 V
sin fuente = ECO), con los píxeles del banco:

| medición | QUALITY (`main-natural`) | ECO (`main-natural-eco`) |
|---|---|---|
| redibujos del espectro (132000 px c/u) | 16 | **4** (1 de cada 4) |
| redibujos de la cascada (196000 px c/u) | 23 | 5 |
| `natural.totals.invalidated_px` | 6745720 | **1758360** |
| `natural.totals.invalidated_rects` | 45 | 20 |
| `metrics.flushed_pixels` | 7873400 | 2754040 |

La pieza del espectro baja los redibujos del espectro de 16 a 4 (−75%) y el total
ECO de **3474360 px** (el ECO de la corrida anterior, sólo cascada) a **1758360 px**
(−49.4%; −73.9% contra QUALITY). El reparto de rectángulos vecinos se reagrupa
como ya se había medido con la cascada (LVGL fusiona por ciclo de refresh), pero
el trabajo del **espectro** es lo que mueve: sus rectángulos caen de 16 a 4 y el
reloj/cascada mantienen sus eventos.

### Decision 4 — el redibujo offscreen de la cascada con la pantalla apagada (`src/waterfall.c`)

`refresh_waterfall()` hacía `redraw_cb(NULL)` —la paleta de 800 × height px,
~196000 px— **antes** de invalidar; con el display apagado la invalidación es nop
pero el trabajo offscreen ya había ocurrido. Ahora la cascada lo **saltea mientras
`backlight_is_on()` es false** (el mismo estado que `backlight_switch()` pone junto
a `lv_disp_enable_invalidation(false)`, `src/backlight.c`), y al volver la pantalla
la próxima fila redibuja con la caché al día. El instrumento es
`waterfall_offscreen_redraw_count()` (cuenta los `redraw_cb` que efectivamente
corrieron) y lo reporta el banco natural como `waterfall_offscreen_redraws`.

| 40 frames con el drive andando | pantalla encendida | pantalla apagada |
|---|---|---|
| `natural.waterfall_offscreen_redraws` | **24** (una por fila) | **0** |
| `natural.totals.invalidated_px` / `flushes` | 6745720 / 45 | 0 / 0 |
| `metrics.flushes` / `flushed_pixels` (captura) | 6 / 1127680 | 5 / 743680 |

Las filas siguen llegando igual (`waterfall_rows = 24`): lo que desaparece es el
cómputo de la paleta. La captura sigue siendo la misma imagen, 384000 px más
liviana. En el test nativo, `tests/native/test_backlight.c` fija que el estado que
consume la cascada (`backlight_is_on()`) sigue al switch (false al apagar, true al
encender): si el apagado dejara el estado en true, la cascada seguiría trabajando y
el test queda rojo.

### Gates (salida real, HEAD `ce6410b`)

```text
$ tests/check_harness_determinism.sh           # 33 fixtures x 2 corridas
main-natural-eco: 6297ef99d8b3babfd844749206afb87aaf59e7c52b5830446a46170357b01292
main-natural: 3986bc9a6047744aa2cd8db1c9879d73e71e16e45d7737b0d7035c06e22ec821
PASS: deterministic (identical PNGs for every fixture)
# los 33 sha256 son identicos a los del tip base baf6164: ni el tick sin hilo, ni
# la cadencia ECO del espectro, ni el corte offscreen mueven un pixel de la
# captura comun.

$ tests/check_harness_natural.sh                # SKIP_BUILD=1
  profile: spectrum 132000 px x 16 frames, waterfall 196000 px x 23 rows, clock 15840 px x 1 tick(s)
  natural totals: invalidated 6745720 px in 45 rectangles / 45 flushes over 37/40 frames
  delimitation: drive off -> 304760 px (4.5% of the 6745720 px with the drive), only clock/BT/band-info rectangles
  forced mode: 3 frames x 1 flush x 384000 px (natural: 6745720 px in 40 frames)
  CLI override: --bench-natural 10 ran 10 frames (the fixture says 40)
  display off: 0 px in 40 frames (against 6745720 px lit) and a capture 384000 px lighter
PASS: natural bench deterministic, profiled and delimited (main-natural)
  png sha256: 8ec631215c28e27fb901f186273fe6afa0fd6b9470550cf810cb1988f84273fd

$ tests/check_harness_wakeups.sh                # SKIP_BUILD=1
  loop: 14 wakeups with the data drive (11 without it) in 1000 ms; sleeps 10-200 ms
  tick thread: none (LV_TICK_CUSTOM, lv_conf.h: CLOCK_MONOTONIC) = 0 wakeups in 1000 ms
  window override: --bench-wakeups 3000 ran 3000 ms with 31 loop wakeups (the default window: 1000 ms)
  plain capture: no `wakeups` block (the mode is additive) and its 1127680 px settle plus the two passes add up to the run's flushed pixels
PASS: wakeup profile measured, sourced and delimited (main-natural)
  png sha256: c1c66701b47c8926adbb37221406705404e82be85c574ce8108c71d3dd6428ae

$ uv run x6200-ab compare --a baf6164 --b HEAD --experiment-id AB-ECOFINAL
experiment AB-ECOFINAL: PASS (exit 0)
  Functional PASS · Visual Regression PASS (captures are pixel-identical to the baseline)
  Quality Contract PASS (352 contract checks) · Performance PASS · Hardware BLOCKED · Energy BLOCKED
  33/33 escenarios: changed_pixels 0

$ uv run x6200-ab contracts --revision HEAD
contracts ce6410bc1c3e: PASS — 352 checks, 0 failure(s), 0 unchanged baseline defect(s)

$ uv run x6200-ab audit --revision HEAD --suite visual
audited ce6410bc1c3e: 16 known issues — 0 reproduced, 14 unverified; structural inspection REAL
over 3861 objects on 33 scenario(s): 0 structural finding(s)

$ uv run pytest -m "not e2e" -q    -> 187 passed, 18 deselected
$ uv run pytest -m e2e -q          -> 18 passed, 187 deselected in 135.86s
$ tests/native/run_native_tests.sh -> 810 comprobaciones, 0 fallos + 7, 0 fallos (clock) + 25, 0 fallos
  (backlight: +2 sobre las 23 de UI-08 incr. 4: el estado que consume la cascada sigue al switch)
$ 6 chequeos estaticos (check_cfg_fills, check_actions, check_settings_focus,
  check_apps_menu, check_app_handlers, check_wcag) -> PASS
$ cd tools/ab_testing && uv run ruff check . && uv run ruff format --check . && uv run ty check $(find src tests -name '*.py')
  -> All checks passed! / 114 files already formatted / All checks passed!
```

Reproducibilidad **x3**: tres corridas de `--bench-natural` (QUALITY), de
`--bench-natural --display-off` y de `--bench-natural` sobre `main-natural-eco`
dan los mismos números (6745720 px / offscreen 24; 0 px / offscreen 0; 1758360 px /
offscreen 6) y los mismos PNG; los JSON sólo difieren en `png_path`/`png_bytes`.

Evidencia commiteada de la corrida: `tools/ab_testing/reports/AB-ECOFINAL/`
(compare A/B), `tools/ab_testing/reports/NATURAL-ECOFINAL/` (banco natural,
pantalla apagada y el ECO) y `tools/ab_testing/reports/WAKEUPS-ECOFINAL/` (perfil
de wakeups del tick sin hilo). Los directorios `*-ECO13` / `NATURAL-UI07-N1`
quedan como evidencia histórica de la corrida anterior.

### PENDIENTES honestos

1. **El costo del wakeup y la energía del Cortex-A7 siguen BLOCKED**: el banco
   mide cuentas, no tiempos en un wakeup (host x86). Con el hilo del tick
   eliminado, la pasada física del canal de bugs (#15) confirmaría latencia y
   consumo, como con cualquier cambio de cadencia global.
2. **La cadencia ECO del espectro es una decisión de producto no calibrada**: el
   período (4, como el waterfall) es un default de ingeniería; el canal físico
   ajusta si el ECO deja el panel demasiado "quieto".
3. **`update_check`/voz dependen del tick**: con `LV_TICK_CUSTOM` el tick ya no lo
   alimenta un hilo, sino el reloj monotónico; los timers de LVGL y las esperas de
   `get_time()` corren sobre la misma fuente (no hay cambio esperado), pero la
   pasada física de latencia de audio/voz queda como verificación del canal.
4. **El corte offscreen no instrumenta escrituras a sysfs**: `backlight_is_on()`
   es el estado en memoria del módulo; la confirmación de que el GPIO no escribe
   con el panel oscuro es del canal físico.


## 2026-10-15 — El frame natural es un frame de la ROM (decisión 8, corrida `fonts-libre`)

**Qué cambió.** Un "frame natural" es *un período de refresco de LVGL*, y el banco
nuevo lo medía con el default de LVGL del arnés (**30 ms**) porque el `lv_conf.h`
del arnés no define `LV_DISP_DEF_REFR_PERIOD`. La ROM configura **50 ms**
(`lv_conf.h`), así que la ventana que el banco reportaba no era una ventana de la
ROM. Ahora el CMake del arnés lee el número del `lv_conf.h` de la ROM y lo pasa
como `HARNESS_ROM_REFR_PERIOD`; `natural_bench_run()` lo pone en el `refr_timer`
del display antes de medir y lo reporta en `frame_ms`. Es la **misma pieza** que
el perfil de wakeups ya usaba (`wakeup_bench.c`, #56): ahora las dos leen el
período de una sola fuente y ninguna lo hardcodea, así que no pueden declarar
períodos distintos.

**Cómo se codifica la restricción** (no en prosa): `tests/natural_evidence.py`
re-lee `lv_conf.h` y falla si `natural.frame_ms` no es el número de la ROM; la
cadencia del drive se sigue reproduciendo desde `src/dsp.cpp`
(1000/15, 1000/25). El gate sigue siendo `tests/check_harness_natural.sh`
(determinismo byte a byte + perfil + delimitación + pantalla apagada + `--bench`
intacto) y `tests/check_harness_wakeups.sh` para el lazo.

**Re-baseline medido** (harness local, fixture `main-natural`, seed 13, 40 frames;
evidencia cruda re-generada en `tools/ab_testing/reports/NATURAL-ECOFINAL/`):

| métrica | antes (30 ms) | después (50 ms) |
|---|---|---|
| `frame_ms` / ventana de 40 frames | 30 ms / 1200 ms | 50 ms / 2000 ms |
| `spectrum_frames` / `waterfall_rows` | 17 / 24 | 28 / 40 |
| `waterfall_offscreen_redraws` | 24 | 40 |
| `natural.totals.invalidated_px` | 6745720 | 11223840 |
| `natural.totals.invalidated_rects` | 45 | 67 |
| `joined_px` | 784000 | 1372000 |
| frames con flush | 37/40 | 39/40 |
| delimitación (`--natural-static`) | 304760 px (4.5%) | 254840 px (2.3%) |
| `--bench-natural --display-off` | 0 px | 0 px |
| `metrics.flushed_pixels` | 1127680 + 6745720 | 1127680 + 11223840 |
| `main-natural-eco` invalidated px | 1758360 (offscreen 6) | 2861240 (offscreen 10) |

Ninguna **región** perfilada se movió: el espectro sigue invalidando 132000 px por
frame de FFT, la cascada 196000 px por fila, el reloj 15840 px por tick de 500 ms,
la band info 27200 px y el ícono de BT 1080 px (son geometría, no cadencia). Lo que
cambió es cuántas entran en la ventana y, con ello, el histograma de rectángulos:
con el período de la ROM la cascada dispara exactamente una vez por frame y el
espectro una de cada dos, así que casi todos los frames llevan una región de datos
y los dos rectángulos de los timers de 2 s (band info, BT) llegan dentro de ellas
(el espectro cubre `0,0 800x165`; la cascada, `0,235 800x245`) y LVGL los une: se
cuentan como `joined_px`. Los dos siguen midiéndose —y son los que reporta el
control de delimitación `--natural-static`—: 7 band info y 1 BT en la ventana.

**Blast radius.** `--bench` (forzado), el formato de `metrics` y el de la captura
común quedan intactos: los PNG de `forced`/`static`/`off` son byte a byte los
mismos (ver `sha256.txt`); sólo cambian los PNG de las corridas natural (más
frames de trabajo dibujados). El submódulo LVGL no se toca.
