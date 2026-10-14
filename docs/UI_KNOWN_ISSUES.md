# Defectos conocidos de la UI

Registro de defectos de la interfaz. **Regla**: un defecto entra acá solo si el
**arnés lo reproduce** (`tools/ab_testing/`); lo que no se puede reproducir queda
en el [anexo de hipótesis](#anexo-hipótesis-sin-reproducir) con lo que haría
falta para fundarlo. La fuente de verdad operativa es
[`tools/ab_testing/quality/known_issues.yaml`](../tools/ab_testing/quality/known_issues.yaml):
este documento la explica, no la duplica.

Un defecto corregido pasa a test permanente: en cuanto el arnés lo reproduce, la
comparación A/B lo clasifica (`FIXED` / `UNCHANGED DEFECT` / `REGRESSION` /
`NEW DEFECT`) y una reaparición vuelve a fallar la corrida.

## UI-001 — Etiqueta del multiplicador de tuning visible sobre la barra superior

- **Severidad**: major.
- **Fixture**: `home-usb-20m`, región `multiplicador`.
- **Estado**: `fixed` (si vuelve a aparecer en B, es `REGRESSION`).
- **Síntoma**: la etiqueta `x%.1f` del tuning adaptativo se creaba **visible** y
  tapaba la barra de estado (clima/voltaje). Ver `docs/ROM_REVIEW.md` §5.3.
- **Fix en el código**: la etiqueta se crea con `LV_OBJ_FLAG_HIDDEN` y el timer
  de ocultado la vuelve a esconder (`src/main_screen.c`).
- **Cómo lo reproduce el arnés**: el detector `region_content_above` mide la
  fracción de píxeles que se apartan del color mediano de la región
  `multiplicador`. Oculta, la región es fondo puro (≈0); si la etiqueta se
  dibuja, sube muy por encima del umbral (0.05). Ver la corrida
  `tools/ab_testing/reports/AB-e2e-fix/` (`FIXED`) y `AB-e2e-reappear/`
  (`REGRESSION`).

---

## UI-009 — Reloj de potencia: símbolo de batería con puntero sin inicializar

- **Severidad**: major.
- **Fixture**: `home-bateria-externa` (12.1 V con fuente externa conectada).
- **Estado**: `fixed` (si vuelve a aparecer en B, es `REGRESSION`).
- **Síntoma**: en `src/clock.c`, `show_time()` rama `CLOCK_POWER`, `bat_sym` se
  declaraba **sin inicializar** y el `switch ((uint8_t)(v_bat * 10))` sólo cubría
  0–100. Con `v_bat > 10.2` (12.1 V → 121 tras el cast) ningún `case` pisaba el
  puntero y `snprintf("%s", bat_sym)` imprimía memoria: **glifos basura** en la
  esquina superior derecha (las capturas del toolchain aceptado muestran
  `EHE 12.1v`) y comportamiento indefinido — **segfault** con otro toolchain
  (reproducido con gcc 12 en Debian).
- **Fix en el código**: `const char * bat_sym = LV_SYMBOL_BATTERY_FULL;`
  (`src/clock.c`). El mapeo ya clampea el extremo bajo (0–56 → `BATTERY_EMPTY`) y
  78–100 → `BATTERY_FULL`, así que una lectura por encima del tope es batería
  llena; con `BATTERY_EMPTY` el reloj mostraría una batería vacía a 12.1 V, que
  sería un defecto de información nuevo.
- **Test permanente**: `tests/native/test_clock.c` (nativo, sin hardware) corre el
  camino `CLOCK_POWER` con voltajes dentro y fuera de rango y exige que el texto
  del reloj arranque con un `LV_SYMBOL_BATTERY_*`. El binario se compila con
  `-ftrivial-auto-var-init=pattern`, así que reinstalar la declaración pelada
  vuelve a leer un puntero envenenado y el test crashea en vez de pasar por azar.
  `tests/native/run_native_tests.sh` lo corre junto al resto de los tests nativos.
- **Cómo lo reproduce el arnés**: el detector `leading_glyph_range` lee el texto
  del reloj de la inspección estructural (`ui_harness --inspect`, sin OCR) y exige
  que su primer code point esté en el bloque de iconos de batería
  `U+F200..U+F2FF` (`region: reloj_potencia`). Sobre la revisión sin el fix el
  primer code point es memoria (`U+0089`), y el detector lo marca; sobre el fix
  arranca con `U+F240` (`BATTERY_FULL`) y pasa.
- **Evidencia**: `tools/ab_testing/reports/AB-e2e-ui009/` — comparación A/B del
  fixture `home-bateria-externa` con la revisión sin fix en A (glifos `EHE`) y el
  fix en B (símbolo válido); veredicto **PASS** marcado *IMPROVEMENT*, con
  `UI-009` clasificado **FIXED**.

Nota: el mismo defecto estaba presente en **las 7 fixtures previas** (todas tienen
`battery_v > 10.0` V), así que el fix cambia el reloj en todas ellas; los hashes
de determinismo aceptados son los previos al fix y están fechados en
`docs/AB_TESTING.md`. Para un voltaje dentro de rango el render es byte a byte
idéntico antes y después del fix (verificado con un fixture temporal de 8.1 V).

---

## Anexo: hipótesis sin reproducir

Semillas con evidencia previa en el repo pero **sin detector** todavía: no
bloquean ni pasan, quedan listadas como `unverified` hasta que el arnés las
reproduzca. Cada una dice qué falta.

| ID | Tema | Evidencia previa | Qué falta para fundarla |
|---|---|---|---|
| UI-002 | Las apps solo vivían en el long-press de Ajustes | `docs/HALLAZGOS.md` 17, 29 | un fixture que abra el menú de apps |
| UI-003 | Pares texto/fondo por debajo de 4.5:1 / 3:1 | `docs/HALLAZGOS.md` 24, 26, 27, 28; `tests/check_wcag.py` | un fixture que renderice los pares que incumplen (los 9 temas); el arnés ya calcula el contraste de cada par opaco |
| UI-004 | Ajustes: widgets inalcanzables con el foco | `docs/HALLAZGOS.md` 18; `tests/check_settings_focus.py` | un fixture que abra Ajustes y emita la grilla de foco |
| UI-005 | Diálogo viejo: texto tapado por los botones | `docs/HALLAZGOS.md` 21 | un fixture que abra el diálogo |
| UI-006 | Glifo BT U+F293 inexistente en `sony_20` | `docs/HALLAZGOS.md` 19; `src/info.c` | que una pantalla vuelva a dibujar el glifo en un label; el arnés ya detecta glifos ausentes (y la pantalla actual dibuja el icono con `lv_line`) |
| UI-007 | Temas RS sin clave propia, contraste 2.58:1 | `docs/HALLAZGOS.md` 28; `docs/THEMES.md` | un fixture que renderice los temas RS; el chequeo de contraste estructural ya existe |
| UI-008 | CMA: 128 MB reservados para GPU no usada | `docs/RESOURCE_USAGE.md` | presupuesto de recursos del firmware, no defecto de la UI; el arnés no mide memoria del sistema |

## Re-evaluación con instrumentación estructural (2026-10-13)

El arnés emite ahora la inspección del árbol LVGL real (`--inspect`: cajas
absolutas con el recorte de los padres, visibilidad, clase de widget, texto con
las métricas reales de fuente y glifos ausentes; ver
`tools/ui_harness/README.md`). Con eso corren dos familias de contratos nuevos
—**layout** y **tipografía**— y cada semilla se re-evaluó contra la pantalla
principal real. Evidencia: `tools/ab_testing/reports/AB-e2e-structural/`.

| ID | Resultado | Motivo concreto |
|---|---|---|
| UI-002 | sigue `unverified` | no hay fixture que abra el menú de apps; la inspección es de la pantalla activa |
| UI-003 | sigue `unverified` | el chequeo de contraste ahora recorre todos los pares texto/fondo **opacos** de la pantalla real y ninguno baja de 4.5:1/3:1; las violaciones de los hallazgos 24/26/27/28 ya estaban corregidas y ningún fixture renderiza los pares que fallaban |
| UI-004 | sigue `unverified` | no hay fixture que abra Ajustes |
| UI-005 | sigue `unverified` | no hay fixture que abra el diálogo |
| UI-006 | sigue `unverified` | el detector de glifos ausentes existe (se prueba con un defecto deliberado: `U+F293` en un label da `typography_missing_glyph`), pero la pantalla principal ya no dibuja ese glifo: el icono BT se dibuja con `lv_line` (`src/info.c`), así que no hay nada que reproducir en la fixture actual |
| UI-007 | sigue `unverified` | igual que UI-003: el chequeo de contraste estructural corre sobre la fixture real y no encuentra pares por debajo del umbral; los temas RS no están entre las fixtures |
| UI-008 | sigue `unverified` | es un presupuesto de recursos del firmware; el arnés de UI no mide memoria del sistema |

Los detectores de layout/tipografía no se declaran fundados por sí solos: se
verifica que **detectan un defecto deliberado** (texto recortado, elemento fuera
de pantalla, glifo faltante, color ilegible) en las autopruebas
`tools/ab_testing/tests/test_e2e.py`; la pantalla real, hoy, no los exhibe.
