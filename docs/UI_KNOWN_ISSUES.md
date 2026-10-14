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

## UI-010 — Tema Outdoor: franja de frecuencia invisible sobre fondo blanco

- **Severidad**: critical (información crítica ilegible en el campo).
- **Fixture**: `main-otro-tema` (tema Outdoor), región `frecuencia`.
- **Estado**: `verified` — defecto **preexistente** de la ROM, presente en la
  línea base, todavía **abierto**.
- **Síntoma**: la fila de frecuencia (`src/main_screen.c`, región `frecuencia`,
  `y=160..196`) se dibuja **blanca sobre fondo blanco**: los dígitos de la
  frecuencia central y de los extremos no se ven. La región queda **uniforme**
  (content `0.0000`), o sea información crítica para el operador ausente.
  Confirmado mirando la captura del arnés: la banda central sale vacía. No es un
  defecto introducido por el arnés ni por el PR: es la paleta del tema Outdoor.
- **Cómo lo reproduce el arnés**: el contrato objetivo
  `region_min_content:frecuencia` (mínimo 0.02 de píxeles distintos del mediano
  de la región) mide `0.0000` y lo marca. Evidencia:
  `ui-contracts/contracts.json` del run `38497082174` (única violación:
  `region_min_content:frecuencia [main-otro-tema]: content 0.0000 >= 0.0200`).
- **Cómo se clasifica**: la entrada está **conectada al gate** por una
  *exclusión acotada* (`fixture: main-otro-tema`, `region: frecuencia`,
  `rule: region_min_content:frecuencia`, `until: 2027-03-31`, con motivo). Un
  finding que matchea esa exclusión se clasifica **`UNCHANGED DEFECT`**: se
  lista en el resumen y en el reporte, pero **no** convierte el gate en FAIL.
  Una violación que **no** esté en el registro sigue fallando. La exclusión no
  es global ni permanente: vence el 2027-03-31 y entonces el gate vuelve a
  fallar para forzar la re-evaluación.
- **UI-011 (evaluada y plegada acá, no como entrada separada)**: el mismo tema
  muestra además la **cascada de ruido casi blanco sobre blanco** (invisible) y
  las **etiquetas CW/DIG del strip lavadas**. Se evaluó si merecía entrada
  propia: la regla que abastece la región `waterfall` es
  `region_min_content:waterfall`, que sólo pregunta "¿hay contenido?" y en este
  fixture **pasa** (content `0.29`, el ruido casi-blanco cuenta como contenido);
  **ninguna regla** detecta "contenido presente pero ilegible sobre el fondo".
  Como no existe detección y no se inventa una, el síntoma queda **documentado
  como observación** dentro de esta misma entrada (una sola entrada), no como
  defecto "detectado".

---

## UI-011 — VISION: los iconos de la barra desaparecen (glifos ausentes)

- **Severidad**: major.
- **Fixture**: `main-tema-vision` (tema VISION, la misma radio que `home-usb-20m`).
- **Estado**: `verified` — defecto **preexistente** de la ROM, todavía **abierto**.
- **Síntoma**: con VISION el reloj de potencia dibuja un **rectángulo vacío** en
  lugar del símbolo de batería y el icono de WiFi de la barra deja de existir. No
  es un problema de la captura: LVGL lo avisa en su propio log
  (`lv_draw_letter: glyph dsc. not found for U+F240 / U+F1E6 / U+F1EB`) y la
  inspección estructural lo mide (`missing_glyphs: ["U+F240","U+F1E6"]` en el
  reloj, `["U+F1EB"]` en el WiFi).
- **Causa**: `fonts_scale()` (`src/fonts/font_select.c`) escala **copiando la
  estructura entera** de una fuente sobre otra (`*sony[i] = was[i + steps]`), así
  que el tamaño que dibuja hereda también la cobertura de glifos del tamaño con
  el que se reemplaza. Solo `sony_20` y `sony_30` traen los rangos de icono (su
  `cmap` llega hasta `U+F244`); los demás `sony_N` son ASCII (32..126). Con
  `VISION_FONT_STEPS = 3` `sony_20` pasa a ser `sony_26` y `sony_30` pasa a ser
  `sony_36`, los dos ASCII: el reloj (`src/clock.c:58`) y la celda de la barra
  (`src/styles.c:259`, `info_item_style`) se quedan sin sus iconos.
- **Cómo lo reproduce el arnés**: el contrato `typography_missing_glyph` sobre la
  fixture `main-tema-vision` (la inspección real del árbol, nunca OCR). Está
  **conectado al gate** por una *excepción acotada* en `quality/contracts.yaml`
  (regla + fixture, vence el 2027-03-31): mientras el defecto siga abierto el
  hallazgo se lista en el reporte del audit, pero no convierte la corrida en FAIL.
  Un glifo ausente en **otra** fixture sigue fallando.
- **Fix**: pendiente, UI-04+ (las fuentes de icono tendrían que estar en todos los
  tamaños escalados, o `fonts_scale()` no reemplazar el `cmap`).

---

## UI-012 — VISION: la etiqueta `VFO-A` no entra en su celda de la barra

- **Severidad**: minor.
- **Fixture**: `main-tema-vision`, región `barra_superior`.
- **Estado**: `verified` — defecto **preexistente** de la ROM, todavía **abierto**.
- **Síntoma**: con VISION el texto de la barra crece tres pasos de fuente y la
  celda **no**: `src/info.c` crea los items con posiciones y tamaños fijos
  (`lv_obj_set_pos(item, x * 58 + 15, y * 22 + 5)`, `lv_obj_set_size(item, 56,
  26)`) mientras la fuente sale de `info_item_style` (`src/styles.c:259`,
  `sony_20`). `VFO-A` se desborda de la celda (la inspección reporta
  `text_overflow` en el label de 56x26, modo WRAP).
- **Cómo lo reproduce el arnés**: el contrato `typography_text_overflow` sobre la
  misma fixture, con la **misma excepción acotada** (regla + fixture, vence el
  2027-03-31) que UI-011.
- **Fix**: pendiente, UI-04+ (la geometría de la barra tendría que escalar con la
  fuente, o VISION dibujar esa fila con el tamaño de siempre).

---

## UI-013 — `params.theme_name` no puede guardar un nombre que el menú de temas ofrece

- **Severidad**: major.
- **Fixture**: `main-tema-outdoor-negative`.
- **Estado**: `verified` — defecto **preexistente** de la ROM, todavía **abierto**.
- **Síntoma**: elegir **Outdoor-Negative** en *Ajustes → Display → Theme* (o con el
  long-press de la pantalla principal) **no aplica el tema**: la GUI reinicia con
  IU1WXY. Es silencioso: el único rastro es el aviso de `theme_init()` por stderr
  (`Theme Outdoor-Negativ not found, using IU1WXY`). El desplegable de Ajustes
  además pierde la selección actual, porque compara los nombres completos del menú
  contra el nombre truncado que quedó guardado.
- **Causa**: el menú sale de `theme_list()` (`src/theme.c`), que admite hasta 31
  caracteres, pero la elección viaja al próximo arranque dentro de
  `params.theme_name`, un `params_str_t` cuyo `x[]` son **16 bytes**
  (`src/params/types.h`). `params_str_set()` copia `sizeof(x) - 1` = 15
  (`src/params/params.c`), así que "Outdoor-Negative" (16) queda como
  "Outdoor-Negativ", que no está en `builtin_names[]` ni existe como archivo, y
  `theme_init(params.theme_name.x)` (`src/main.c`) cae al tema por defecto.
- **Cómo lo reproduce el arnés**: el contrato `theme_name_fits_params`. El JSON de
  cada captura trae ahora un bloque `theme` con el nombre que **se renderizó** y
  el que el **store del firmware** contiene (el mock usa el `params_str_t` real),
  así que la fixture `main-tema-outdoor-negative` los separa por medición:
  `requested "Outdoor-Negative"` vs `params_name "Outdoor-Negativ"`,
  `params_max_chars 15`. Está conectado al gate por una excepción acotada (regla +
  fixture, vence el 2027-03-31).
- **Fix**: pendiente, UI-04+ (más ancho en `params_str_t.x` para el nombre del
  tema, o nombres de tema que entren en 15; la copia del nombre a `params_str_t`
  es lo que hay que mirar, no el render).

---

## Anexo: hipótesis sin reproducir

Semillas con evidencia previa en el repo pero **sin detector** todavía: no
bloquean ni pasan, quedan listadas como `unverified` hasta que el arnés las
reproduzca. Cada una dice qué falta.

| ID | Tema | Evidencia previa | Qué falta para fundarla |
|---|---|---|---|
| UI-002 | Las apps solo vivían en el long-press de Ajustes | `docs/HALLAZGOS.md` 17, 29 | un fixture que abra el menú de apps (el arnés abre el About real, no el menú) |
| UI-003 | Pares texto/fondo por debajo de 4.5:1 / 3:1 | `docs/HALLAZGOS.md` 24, 26, 27, 28; `tests/check_wcag.py` | un fixture que renderice los pares que incumplen; el arnés ya calcula el contraste de cada par opaco y desde UI-02 lo corre también sobre el diálogo About y sobre los temas (`main-tema-*`) sin encontrar nada |
| UI-004 | Ajustes: widgets inalcanzables con el foco | `docs/HALLAZGOS.md` 18; `tests/check_settings_focus.py` | un fixture que abra Ajustes y emita la grilla de foco (Ajustes arrastra `dialog_field` y decenas de dependencias) |
| UI-005 | Diálogo viejo: texto tapado por los botones | `docs/HALLAZGOS.md` 21 | un fixture de BT/WeFax/SSTV (el About real ya se abre y no lo reproduce: su texto está centrado) |
| UI-006 | Glifo BT U+F293 inexistente en `sony_20` | `docs/HALLAZGOS.md` 19; `src/info.c` | que una pantalla vuelva a dibujar el glifo en un label; el arnés ya detecta glifos ausentes (y la pantalla actual dibuja el icono con `lv_line`) |
| UI-007 | Temas RS sin clave propia, contraste 2.58:1 | `docs/HALLAZGOS.md` 28; `docs/THEMES.md` | un fixture de Ajustes: el par es el riel del switch (`control_border`), que solo aparece ahí; el fixture `main-tema-rs` ya renderiza RS y el chequeo de contraste no ve nada en la pantalla principal. Desde UI-03 el par está cubierto **estáticamente** (`x6200-ab themes` + `quality/theme_pairs.yaml`: resuelve la herencia del `base` como `src/theme.c`) y pasa en los 3 temas JSON, así que sólo falta la evidencia renderizada |
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

## Re-evaluación con los fixtures de UI-02 (2026-10-14)

UI-02 agregó un diálogo real (`dialog-about`, `src/dialog.c` + `src/dialog_about.c`),
la matriz de temas (`main-tema-rs`, `main-tema-outdoor-negative`, `main-tema-classic`,
`main-tema-holo`, `main-tema-material-3`) y un fixture de foco (`main-foco`). Los
contratos corren sobre las 15 fixtures (135 chequeos, 0 fallas fuera de UI-010):

| ID | Resultado | Motivo concreto |
|---|---|---|
| UI-002 | sigue `unverified` | el fixture nuevo abre el **About real**, no el menú de apps; la inspección es de la pantalla activa |
| UI-003 | sigue `unverified` | el chequeo de contraste estructural corre ahora también sobre el About real y sobre los temas nuevos y no encuentra ningún par por debajo de 4.5:1/3:1; las violaciones históricas ya estaban corregidas (hallazgo 24). No hay nada que reproducir |
| UI-004 | sigue `unverified` | no hay fixture que abra Ajustes (arrastra `dialog_field` y decenas de dependencias); `dialog-about` cubre el camino de diálogo, no la grilla de foco de Ajustes |
| UI-005 | sigue `unverified` | el About real no lo reproduce (su texto está centrado, no apoyado en la fila de botones); el caso era de BT/WeFax/SSTV |
| UI-007 | sigue `unverified` | `main-tema-rs` renderiza RS y el contraste estructural no ve nada en la pantalla principal; el par del hallazgo es el riel del switch (`control_border`), que vive en Ajustes |

Una inyección deliberada de `U+F293` en el título del About (`src/dialog_about.c`,
revertida) hace fallar `typography_missing_glyph` en `dialog-about`: el fixture
nuevo está cableado a los detectores, no solo renderizado.

## Auditoría UI-03 (2026-10-14) — el backlog fundado

La corrida UI-03 (issue #8) corrió el audit completo sobre el tip aceptado
(`667b5e8`, con UI-02 mergeado), inspeccionó capturas y árbol LVGL, y triageó cada
semilla. Se agregó un fixture (`main-tema-vision`), un contrato nuevo
(`theme_name_fits_params`) y un chequeo estático nuevo de contraste sobre los temas
JSON de `rootfs` (`uv run x6200-ab themes`); quedaron tres entradas fundadas
(UI-011, UI-012, UI-013). Los contratos corren sobre las **16 fixtures** (163
chequeos) y los tres defectos nuevos están conectados al gate con excepciones
acotadas y con vencimiento, igual que UI-010.

| ID | Título | Estado | Severidad | Detector | Fixture / evidencia |
|---|---|---|---|---|---|
| UI-001 | Multiplicador de tuning visible sobre la barra | `fixed` | major | `region_content_above` | `home-usb-20m`; `AB-e2e-fix` / `AB-e2e-reappear` |
| UI-002 | Apps solo en el long-press de Ajustes | `unverified` | major | — (falta fixture del menú de apps) | `HALLAZGOS.md` 17, 29 |
| UI-003 | Pares texto/fondo por debajo de 4.5:1 / 3:1 | `unverified` | major | `typography_contrast` + `theme_pairs:*` (ninguno dispara) | `check_wcag.py`; 16 fixtures y los 3 temas JSON sin violaciones |
| UI-004 | Ajustes: widgets inalcanzables con el foco | `unverified` | major | — (falta fixture de Ajustes) | `HALLAZGOS.md` 18; `check_settings_focus.py` |
| UI-005 | Diálogo viejo: texto tapado por los botones | `unverified` | minor | — (falta fixture BT/WeFax/SSTV) | `HALLAZGOS.md` 21 |
| UI-006 | Glifo BT `U+F293` inexistente en `sony_20` | `unverified` | minor | `typography_missing_glyph` (no dispara: el icono se dibuja con `lv_line`) | `HALLAZGOS.md` 19; `src/info.c` |
| UI-007 | Temas RS sin `control_border` propio (2.58:1) | `unverified` | major | `theme_pairs:control_border_on_dialog` (estático, no dispara: el JSON ya define la clave) | `HALLAZGOS.md` 28; `main-tema-rs`; `quality/theme_pairs.yaml` |
| UI-008 | CMA: 128 MB reservados para GPU no usada | `unverified` | info | — (no es un defecto de UI) | `RESOURCE_USAGE.md` |
| UI-009 | Reloj de potencia: símbolo de batería sin inicializar | `fixed` | major | `leading_glyph_range` | `home-bateria-externa`; `AB-e2e-ui009` |
| UI-010 | Tema Outdoor: franja de frecuencia invisible | `verified` (abierto) | critical | `region_min_content:frecuencia` | `main-otro-tema`; exclusión scoped hasta 2027-03-31 |
| UI-011 | VISION: los iconos de la barra desaparecen | `verified` (abierto) | major | `typography_missing_glyph` | `main-tema-vision`; excepción scoped hasta 2027-03-31 |
| UI-012 | VISION: `VFO-A` no entra en su celda | `verified` (abierto) | minor | `typography_text_overflow` | `main-tema-vision`; excepción scoped hasta 2027-03-31 |
| UI-013 | `params.theme_name` no guarda un nombre que el menú ofrece | `verified` (abierto) | major | `theme_name_fits_params` | `main-tema-outdoor-negative`; excepción scoped hasta 2027-03-31 |

**Orden propuesto para UI-04+** (severidad y alcance; lo aprueba el owner):

1. **UI-010** (critical, ya registrado): información crítica invisible en el campo.
2. **UI-011** (major, VISION): es el tema pensado para vista débil y pierde los
   iconos de batería/WiFi en la barra; el arreglo es acotado (`fonts_scale`/fuentes).
3. **UI-013** (major): una opción de Ajustes que se elige y no se aplica, con el
   desplegable mostrando además otra selección; el arreglo toca `params`/menú.
4. **UI-012** (minor, VISION): consecuencia de la geometría fija de la barra.
5. **UI-004/UI-007** (habilitante): un fixture de Ajustes cierra la última clase de
   detección que falta para las semillas `unverified` (foco y contraste del riel).

Los dos hallazgos que UI-02 dejó *surfaced*, y cómo quedaron:

- **El truncamiento de `params.theme_name`** se fundó como **UI-013** midiendo, no
  leyendo: el JSON del arnés trae el nombre pedido y el que el store del firmware
  contiene, y el contrato los compara. No hizo falta tocar `src/`: el
  `params_str_t` que el mock comparte con el firmware ya truncaba el nombre, solo
  no se estaba midiendo.
- **La tipografía de VISION** se fundó como **UI-011** y **UI-012** con el fixture
  `main-tema-vision`, que aplica `fonts_scale()` exactamente como `main()`
  (`harness_main.c`). El audit del tip queda **PASS** con los tres defectos
  registrados y visibles en el reporte: la excepción acotada los mantiene fuera
  del veredicto, no fuera de la evidencia, y vence el 2027-03-31 para forzar la
  re-evaluación.
