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

> **Cierre del epic THEMES v2 (issue #74, 2026-10-15).** El registro retiró
> UI-007, UI-010, UI-011, UI-012, UI-013 y UI-014: cada una estaba anclada a un
> tema legado (RS, Outdoor, VISION) que el #74 retiró, y su fixture salió con él.
> Los **fixes quedan en el código** (`src/main_screen.c` pide el rol `text` del
> tema, `src/info.c` devuelve las celdas del VFO/modo al tamaño de los otros temas,
> `params_str_t.x` es de 32 bytes) y sus guardas permanentes también: el chequeo
> estático de pares (`x6200-ab themes`), los contratos (`region_min_content`,
> `theme_name_fits_params`) y `tests/native/test_theme.c`. Lo que se fue con los
> temas es la **evidencia renderizada** sobre fixtures que ya no existen. Los
> defectos que no dependían de un tema legado (UI-001, UI-009, UI-015/016/017,
> UI-002..006, UI-008) siguen acá.

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
- **Estado**: `fixed` (UI-04 / AB-UI04-EXP3). La exclusión acotada de
  `quality/known_issues.yaml` se **retiró en el mismo cambio**: el contrato ya no
  falla, así que no hay nada que eximir (ver "Qué prueba el fix y qué no").
- **Síntoma**: la fila de frecuencia (`src/main_screen.c`, región `frecuencia`,
  `y=160..196`) se dibujaba **blanca sobre fondo blanco**: los dígitos de la
  frecuencia central y de los extremos no se veían. La región quedaba
  **uniforme** (content `0.0000`), o sea información crítica para el operador
  ausente. Confirmado mirando la captura del arnés: la banda central salía vacía.
  No es un defecto introducido por el arnés ni por el PR: es la paleta del tema
  Outdoor.
- **Causa**: el color de los tres labels viaja en el **markup de recolor** del
  label, no en el estilo: `on_fg_freq_change()` y `update_freq_boundaries()`
  (`src/main_screen.c`) escriben `"#%03X %i.%03i.%03i"` con
  `subject_get_int(freq_lock) ? 0xBBBBBB : 0xFFFFFF`, y ese markup **pisa** el
  `text_color` del estilo (LVGL sólo pinta el color del label cuando el parámetro
  del recolor mide **6 dígitos**, `lvgl/src/draw/lv_draw_label.c`). El tema Outdoor
  es el tema **claro**: `bg` y `strip` son `0xffffff` y `text` es `0x000000`
  (`src/theme.h`, `THEME_COLORS`, columna Outdoor; `src/theme.c`, `use_builtin`).
  Blanco fijo sobre fondo blanco = 1:1 y la fila desaparece. La inspección
  estructural lo dejó medido: los tres labels (`9.933`, `10.125.000`, `10.317`)
  salían con `text_color #FFFFFF` sobre un `bg #FFFFFF`. `freq_style` y
  `freq_main_style` (`src/styles.c:142,150`) cableaban **el mismo** blanco como
  default del estilo: muerto para estos labels, pero era la misma trampa.
- **Fix en el código** (`src/main_screen.c` + `src/styles.c`): el markup pide los
  **roles del tema** (`theme_hex(TC_text)` con el dial suelto, `theme_hex(TC_text_soft)`
  —el rol "inactive knob / lock" de `theme.h`— con el dial trabado) y los dos
  estilos piden **`TC(text)`**; en Outdoor los dígitos salen **negros** sobre el
  fondo blanco. Es el mismo rol que la franja de frecuencia ya usa para los dígitos
  del dial (`src/freq_strip.c`, `freq_label` con `theme_hex(TC_text)`) y el mismo
  formato (`"#%06X ...", theme_hex(TC_text)`) que este archivo ya usa para
  ATT/PRE/ATU. El `%03X` de antes pasa a `%06X` porque es lo que LVGL exige para
  aceptar el parámetro: con 4 dígitos el recolor se descarta **en silencio** y el
  texto cae al color del estilo. No se tocan la geometría ni las paletas de los
  temas.
- **Blast radius medido**: `TC(text)` (y `TC_text_soft`) son blanco/0xBBBBBB en
  IU1WXY (el default de la radio: **idénticos**, incluso el estado trabado, que ya
  valía `0xBBBBBB`), Legacy, Classic, VISION (sus colores propios) y
  Outdoor-Negative (`negative(...)`), así que 14 de las 16 fixtures quedan
  **byte-idénticas**. Además de `main-otro-tema` se mueve `main-tema-material-3`,
  cuyo `text` es `#e2e2e9` (un paso de 29/255 en los mismos tres labels de la
  misma región); el cambio se declara en `expected_changes` (región `frecuencia`),
  como los fixes de EXP-1/EXP-2. Ninguna fixture traba el dial, así que el camino
  `TC_text_soft` no mueve un píxel medido.
- **Cómo lo reproduce el arnés**: el contrato objetivo
  `region_min_content:frecuencia` (mínimo 0.02 de píxeles distintos del mediano
  de la región). Sobre A (`4d24990`, con la exclusión retirada) falla:
  `region_min_content:frecuencia [main-otro-tema]: content 0.0000 >= 0.0200`; sobre
  B la misma región mide **content 0.0979** y la inspección reporta los tres labels
  con `text_color #000000`. Un test permanente lo fija: la revisión efímera
  `abtest/style` vuelve a cablear el blanco en el markup y el contrato vuelve a
  fallar, **sin baseline** (`tests/test_e2e.py`).
- **Qué prueba el fix y qué no**: esta entrada no tiene `detector:` propio (igual
  que UI-011/UI-012: su detector es la regla de contrato, que no dice dónde se
  mueven los píxeles), así que la comparación A/B **no** puede clasificarla
  `FIXED` —la reporta `UNVERIFIED`— y la mejora queda probada por el **contrato**:
  `region_min_content:frecuencia` FAIL en A con el waiver retirado → PASS en B sin
  waiver, más el diff confinado a la región `frecuencia`. La protección contra una
  reaparición es la misma: sin waiver, el contrato vuelve a fallar.
- **Evidencia**: `tools/ab_testing/reports/AB-UI04-EXP3/` (screenshots A/B,
  `diff`/`overlay`, `results.json`); repro de A: contracts `FAIL
  region_min_content:frecuencia [main-otro-tema]`. Hash de determinismo del fixture:
  `main-otro-tema` `65064bfa…` (A) → **`8592ccff…`** (B); el otro que se mueve es
  `main-tema-material-3` `baa8a0ec…` → `7d3a1016…`, y las otras 14 fixtures conservan
  su sha256 (ver `docs/AB_TESTING.md`, sección EXP-3).
- **UI-011 (evaluada y plegada acá, no como entrada separada)**: el mismo tema
  muestra además la **cascada de ruido casi blanco sobre blanco** y las
  **etiquetas CW/DIG del strip del band plan**. Sigue como **observación**, no
  como defecto detectado: la regla que abastece la región `waterfall` es
  `region_min_content:waterfall`, que sólo pregunta "¿hay contenido?" y en este
  fixture **pasa** (content `0.29`, el ruido casi-blanco cuenta como contenido);
  **ninguna regla** detecta "contenido presente pero ilegible sobre el fondo" y no
  se inventa una. Las etiquetas del band plan no son un defecto de color: su
  `text_color` lo elige `src/band_plan.c` por contraste WCAG sobre el color del
  segmento (negro o blanco, el que más contraste tenga), así que no dependen del
  tema y este cambio no las toca.

---

## UI-011 — VISION: los iconos de la barra desaparecen (glifos ausentes)

- **Severidad**: major.
- **Fixture**: `main-tema-vision` (tema VISION, la misma radio que `home-usb-20m`).
- **Estado**: `fixed` (UI-04 / AB-UI04-EXP1; si vuelve a aparecer en B, es
  `REGRESSION`). La excepción acotada de `quality/contracts.yaml` se **retiró en el
  mismo PR**: el hallazgo ya no existe.
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
- **Fix en el código** (`src/fonts/font_select.c`, `fonts_scale()`): cada tamaño
  escalado conserva **su fuente de origen como `fallback`**
  (`sony[i]->fallback = &was[i]`). El tamaño grande sigue dibujando el texto y el
  tamaño original aporta los rangos de icono que solo `sony_20`/`sony_30` traen;
  con un tema de fuente alternativa, `was[i]` resuelve a su vez por su propia
  copia Sony, así que el icono sale igual. Un glifo presente en el tamaño escalado
  no cambia (el `fallback` solo se consulta cuando la fuente primaria no lo
  tiene): ningún icono nuevo, ningún paquete de fuentes, ningún cambio fuera de
  ese archivo.
- **Cómo lo reproduce el arnés**: el contrato `typography_missing_glyph` sobre la
  fixture `main-tema-vision` (la inspección real del árbol, nunca OCR). Sobre A
  (`b029907`) falla con dos hallazgos —reloj `U+F240`/`U+F1E6` y WiFi `U+F1EB`—;
  sobre B pasa (0 findings). El cambio visual queda **confinado a la barra
  superior** y declarado como `expected_change` acotado en `quality/contracts.yaml`.
  Un glifo ausente en otra fixture sigue fallando.
- **Evidencia**: `tools/ab_testing/reports/AB-UI04-EXP1/` (screenshots A/B y diff,
  `results.json`); audit de A: `FAIL typography_missing_glyph ×2`.

---

## UI-012 — VISION: la etiqueta `VFO-A` no entra en su celda de la barra

- **Severidad**: minor.
- **Fixture**: `main-tema-vision`, región `barra_superior`.
- **Estado**: `fixed` (UI-04 / AB-UI04-EXP2; si vuelve a aparecer en B, es
  `REGRESSION`). La excepción acotada de `quality/contracts.yaml` se **retiró en
  el mismo cambio**: el hallazgo ya no existe.
- **Síntoma**: con VISION el texto de la barra crece tres pasos de fuente y la
  celda **no**: `src/info.c` crea los items con posiciones y tamaños fijos
  (`lv_obj_set_pos(item, x * 58 + 15, y * 22 + 5)`, `lv_obj_set_size(item, 56,
  26)`) mientras la fuente sale de `info_item_style` (`src/styles.c:259`,
  `sony_20`, que VISION escala a `sony_26`). `VFO-A` pasa de **48 a 66 px** de
  ancho: no entra en la celda, el WRAP la parte en dos líneas y la celda recorta
  la segunda — en pantalla queda `VFO-` y el `A` no se ve. La inspección lo mide
  (`text_overflow` en el label de 56x26, modo WRAP; `text_natural w=66` contra
  `text_wrapped_h=40` en una caja de 26).
- **Por qué sólo esa celda**: de las seis celdas de la barra, `VFO-A` es el texto
  más largo; las otras cinco (`USB`, `AUTO`, `P/A`, `ATU1` y el icono de WiFi)
  miden 44–56 px y **sí** entran con la fuente escalada, así que no hay motivo
  para bajarlas.
- **Fix en el código** (`src/info.c`, `info_init`): bajo `theme_shape.vision` la
  celda del VFO vuelve al tamaño de los otros temas
  (`font_down(&sony_20, VISION_FONT_STEPS)`), que rinde el `sony_20` de siempre
  (medido: `51x17` dentro de la caja de 56x26) porque `fonts_scale()` reemplaza el
  contenido del slot y `font_down()` devuelve el slot que conserva el tamaño
  original. Es el mismo idioma que `NF()` en `src/cluster_column.c` para sus
  columnas de ancho fijo, y la alternativa que este documento ya anotaba ("VISION
  dibujar esa fila con el tamaño de siempre"). Los otros temas quedan
  **byte-idénticos**: el cambio está detrás de `theme_shape.vision`. No se toca
  `fonts_scale()` (el fix de UI-011), ni `src/clock.c`, ni las otras cinco celdas.
- **Cómo lo reproduce el arnés**: el contrato `typography_text_overflow` sobre la
  fixture `main-tema-vision` (la inspección real del árbol, nunca OCR). Sobre A
  (`8512d86`, con la excepción retirada) falla exactamente con
  `text 'VFO-A' does not fit its 56x26 box (WRAP)`; sobre B pasa (0 findings). El
  cambio visual queda confinado a la **celda del VFO** dentro de `barra_superior`
  y declarado como `expected_change` acotado en `quality/contracts.yaml`. Un
  overflow en otra celda o en otro tema sigue fallando.
- **Evidencia**: `tools/ab_testing/reports/AB-UI04-EXP2/` (screenshots A/B y
  `diff`/`overlay`, `results.json`); audit de A: `FAIL
  typography_text_overflow [main-tema-vision]`.

---

## UI-013 — `params.theme_name` no puede guardar un nombre que el menú de temas ofrece

- **Severidad**: major.
- **Fixture**: `main-tema-outdoor-negative`.
- **Estado**: `fixed` (UI-04 / AB-UI04-EXP4; si vuelve a aparecer en B, es
  `REGRESSION`). La excepción acotada de `quality/contracts.yaml` se **retiró en
  el mismo cambio**: era la última del registro, que hoy no tiene ningún waiver.
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
- **Fix en el código** (`src/params/types.h`): `params_str_t.x` pasa de **16 a 32
  bytes**, el ancho que `theme_list()` admite por nombre (31 caracteres + el NUL).
  El ancho es **sólo el buffer en RAM**: `params.db` guarda `params(name, val)` como
  clave/valor **sin ancho** (el texto vive en `val`, afinidad dinámica de SQLite),
  así que no hay migración ni cambio de esquema, y `params_load_str()` sigue
  copiando hasta `sizeof(x) - 1`: un `params.db` de usuario se lee igual y su
  configuración no se trunca. Se descartó **normalizar el menú** (renombrar o
  acortar "Outdoor-Negative") porque los temas JSON del usuario también admiten
  hasta 31 caracteres y el store seguiría truncando la clase entera; y se descartó
  la **sola detección del fallback** porque el contrato exige
  `params_name == requested`, que un store que trunca nunca cumple.
- **Cómo lo reproduce el arnés**: el contrato `theme_name_fits_params`. El JSON de
  cada captura trae un bloque `theme` con el nombre que **se renderizó** y el que el
  **store del firmware** contiene (el mock usa el `params_str_t` real). Sobre A
  (`cc55931`, con la excepción retirada) la fixture `main-tema-outdoor-negative`
  separa `requested "Outdoor-Negative"` de `params_name "Outdoor-Negativ"`
  (`params_max_chars 15`) y falla; sobre B `params_name` es el nombre entero
  (`params_max_chars 31`) y pasa (0 findings). El arnés **renderiza** el tema que
  pide la fixture (le pasa el nombre entero a `theme_init`) y **mide** el store
  aparte: por eso A y B son **byte-idénticos** — el defecto y el fix son del store,
  no del render — y lo que cambia es la medición y el contrato. El guard permanente
  es doble: el contrato sin excepción (un store que vuelva a truncar deja la fixture
  en FAIL) y el test nativo `test_names_fit_the_store`
  (`tests/native/test_theme.c`), que hace el round-trip de "Outdoor-Negative"
  contra el `params_str_t` real y exige que **todo** nombre de `theme_list()` entre
  en el store.
- **Evidencia**: `tools/ab_testing/reports/AB-UI04-EXP4/` (screenshots A/B y
  `diff`/`overlay`, `results.json`); contracts de A sin la excepción: `FAIL
  theme_name_fits_params [main-tema-outdoor-negative]`; contracts de B: PASS.

---

## UI-014 — VISION: la celda del modo no entra en su celda de 56 px (`CW-R` se pega a `AUTO`; `USB-D` se parte)

- **Severidad**: minor.
- **Fixture**: `main-tema-vision-usbd` (`USB-D`, el fixture que dispara el contrato);
  `main-tema-vision-cwr` (`CW-R`) es la evidencia del pegado reportado.
- **Estado**: `fixed` (UI-04 / AB-UI04-EXP5). El contrato no lleva ninguna
  **excepción** en `quality/contracts.yaml`: sin waiver, una reaparición vuelve a
  fallar la corrida.
- **Síntoma** (issue #28, evidencia de radio `e4c329c`): con **VISION** y el modo en
  **`CW-R`** la celda del modo se pega a la de `AUTO` y en pantalla se lee
  **`CW-RAUTO`**; con `CW` (3 caracteres) no pasa. El hermano exacto de UI-012, en
  la misma barra (`src/info.c`) y con el mismo defecto de fondo: la geometría fija
  de 56x26 contra las fuentes que `fonts_scale()` agranda tres pasos.
- **Causa**: `src/info.c` crea las seis celdas de la barra con el tamaño fijo de
  `src/ui_tokens.h` (`UI_INFO_CELL_W` = 56) mientras la fuente sale de
  `info_item_style` (`src/styles.c`, `sony_20`, que VISION escala a `sony_26`). UI-012
  le devolvió a la celda del **VFO** la fuente de los otros temas y la celda del
  **modo** (`items[INFO_MODE]`) quedó sin ese tratamiento. Medido con las métricas
  propias de LVGL (`lv_txt_get_size`, inspección estructural, sin OCR): `CW-R` mide
  **exactamente 56 px**, el ancho de la celda — **0 px de margen**, que es el pegado
  a la celda de AGC (que muestra `AUTO`); `USB-D`, el modo más ancho de los que la
  radio lista (`mode_names[]`, `src/remote_server.cpp`), mide **66 px** y el `WRAP` lo
  parte en dos líneas que la celda recorta. `CW`, `LSB`, `AM`, `NFM`, `SAM` y `WFM`
  (hasta 54 px) ya entraban.
- **Fix en el código** (`src/info.c`): bajo `theme_shape.vision` la celda del modo
  usa la fuente de los otros temas —
  `font_down(UI_INFO_FONT, VISION_FONT_STEPS)` = `sony_14`, el mismo idioma que
  `NF()` de `src/cluster_column.c` para columnas de ancho fijo y que la celda del VFO
  de UI-012. Medido: `CW-R` pasa de **56 a 42 px** y `USB-D` de **66 a 52 px** en la
  celda de 56. Las otras cuatro celdas y el VFO no se tocan y los temas que no son
  VISION quedan **byte-idénticos**, porque el cambio está detrás de
  `theme_shape.vision`.
- **Cómo lo reproduce el arnés**: el contrato `typography_text_overflow` sobre la
  inspección estructural. Sobre A (`9c031c8`, el commit que sólo suma los fixtures)
  falla con `text 'USB-D' does not fit its 56x26 box (WRAP)` en
  `main-tema-vision-usbd`; sobre B (`7b644a7`, el fix) el audit da **PASS** (0
  findings) y contracts **PASS** (180 checks). Nota medida: `main-tema-vision-cwr`
  reproduce el pegado **pixel a pixel** (la tinta del modo va de 74 a 127 y la de
  `AUTO` de 131 a 185, idénticas a la captura de la radio) pero **no** dispara el
  contrato: `CW-R` mide el ancho exacto de la celda, así que no hace `WRAP` y el
  contrato (que exige que el texto crezca más allá de la caja) no puede medirlo. El
  fixture que sí lo hace es `main-tema-vision-usbd`; por eso el registro apunta a él
  y la evidencia lista los dos.
- **Evidencia**: `tools/ab_testing/reports/AB-UI04-EXP5/` (screenshots A/B, `diff` y
  `overlay` de los tres fixtures VISION, `results.json`); `audit 9c031c8`: FAIL
  `typography_text_overflow [main-tema-vision-usbd]`; `audit 7b644a7` y
  `contracts 7b644a7`: PASS; `compare 9c031c8..7b644a7`: 15 de 18 fixtures
  byte-idénticas y los 3 diffs confinados a `barra_superior`.

---

## UI-016 — Ajustes: el campo Año recortaba 4 px el valor de 4 dígitos

- **Severidad**: minor.
- **Fixture**: `settings-home`, región `ajustes_anio`.
- **Estado**: `fixed` (AB-UI016-T1). La excepción `layout_clipped` de
  `quality/contracts.yaml` se retiró en el mismo cambio: sin waiver, una
  reaparición vuelve a fallar la corrida.
- **Síntoma**: en la fila **Day, Month, Year** de Ajustes el campo del **Año**
  dibujaba **`2026`** con las últimas 4 px del último dígito recortadas contra el
  borde de su caja (medido por la inspección estructural: la etiqueta mide 84 px de
  texto natural y su caja visible era de 80). `typography_text_overflow` **no** lo
  veía: mide contra la caja del objeto, no contra el recorte del padre; lo ve
  `layout_clipped`.
- **Causa**: `src/dialog_settings.c` (`make_date`) creaba el campo del Año como un
  `lv_spinbox` de 4 cifras con el tamaño de **dos columnas** de la grilla
  (`SMALL_2` = 93 px). El área de contenido útil era de **67 px**: 93 menos el
  padding horizontal del `lv_textarea` (11 px por lado, reservado para el cursor)
  y el borde de 2 px. Los campos de Día y Mes entran con **2 cifras** (la etiqueta
  mide 67 px), pero `2026` mide **84 px** con `sony_36` (la fuente del tema): 17 px
  no entraban y el padre recortaba los últimos 4.
- **Fix en el código** (`src/dialog_settings.c`): el Año —y sólo el Año— baja su
  padding horizontal al de la grilla (`SMALL_PAD`, el espacio entre columnas) y
  toma el ancho que piden sus 4 cifras con la fuente vigente, **medido en runtime**
  con `lv_txt_get_size` (el helper `ancho_campo_cifras`, el mismo idioma que el
  resto del código usa para medir etiquetas — `info.c`, `meter.c`, `band_info.c`:
  nada de un ancho clavado, si el tema trae otra fuente el campo se ajusta solo).
  Un `lv_spinbox` no edita texto libre y no necesita el padding del cursor de un
  `lv_textarea`. Medido: la caja pasa de **93 a 94 px** (la etiqueta queda
  `696,113 84x28` con caja visible `84x28`), entra en la grilla y **Día y Mes no se
  tocan** (siguen `704,113 84x28`/`495,113 84x28`, byte-idénticos).
- **Cómo lo reproduce el arnés**: el contrato `layout_clipped` sobre la inspección
  estructural (`ui_harness --inspect`). Sobre la revisión con el defecto
  (`89851e6`/`ccfe1f5`) el campo del Año reporta `lost 4x0`; sobre el fix el
  contrato pasa sin ninguna excepción (`contracts --revision HEAD`: **0 failures**;
  `audit --suite visual`: **0 structural findings**). El A/B (`compare`) confina el
  diff de `settings-home` a `ajustes_anio` (la región nueva de `regions.yaml`), y
  los otros 21 fixtures de la suite salen sin cambios propios del fix.
- **Evidencia**: `tools/ab_testing/reports/AB-UI016-T1/` (capturas A/B, `diff`,
  `overlay`, `results.json`); `tests/check_harness_determinism.sh` (los 22 fixtures
  x2 corridas, byte-idénticos); `tools/ab_testing/quality/contracts.yaml` (excepción
  de `layout_clipped` retirada; `expected_change` acotado a `ajustes_anio`).

---

## Estático, sin fixture

Bloque para los pares que el chequeo **estático** (`x6200-ab themes` +
`quality/theme_pairs.yaml`) marca por debajo de WCAG pero que **ningún fixture
renderiza** todavía: se listan acá como hallazgo *informativo* con su número, no
como defecto activo, porque el registro sólo acepta lo que el arnés **reproduce**
en pantalla. Un par que se funde con una captura sube a la lista de arriba.

**Estado tras UI-06 incremento 1 (issue #12, 2026-10-14): sin hallazgos.**
`x6200-ab themes` mide 63 pares sobre los 7 temas que resuelve (4 paletas
incorporadas de `src/theme.h` + Holo, Material 3 y RS) y **ninguno** queda por
debajo de su umbral (normal ≥ 4.5:1, no-texto/texto-grande ≥ 3.0:1). Los dos
temas derivados en runtime (**Outdoor-Negative** y **VISION**) quedan **UNCOVERED**
declarados con motivo (no son data: los arma `use_builtin()`), no en falta. El
sweep del propio ROM (`tests/check_wcag.py`, nueve paletas) cierra en **0 fallas**:
lo único sub-umbral son los pares que la tabla excluye por WCAG 1.4.3
(deshabilitado/incidental). Detalle y tabla por tema en
`docs/AB_TESTING.md` → *Inventario de contraste estático por tema*. Esto cubre la
parte estática de la semilla **UI-003** sin agregar un defecto nuevo.

---

## Anexo: hipótesis sin reproducir

Semillas con evidencia previa en el repo pero **sin detector** todavía: no
bloquean ni pasan, quedan listadas como `unverified` hasta que el arnés las
reproduzca. Cada una dice qué falta. Tras la corrida del issue #11 (2026-10-15)
UI-002, UI-004, UI-005 y UI-007 salieron de este anexo (ver *Cierre de las
semillas del issue #11*); quedan UI-003 y UI-006.

| ID | Tema | Evidencia previa | Qué falta para fundarla |
|---|---|---|---|
| UI-002 | Las apps solo vivían en el long-press de Ajustes | `docs/HALLAZGOS.md` 17, 29 | **cerrada (2026-10-15)**: los seis fixtures `apps-menu-1..6` abren el menú de apps y cubren las 22 celdas; no reproduce defecto. Ver *Cierre de las semillas del issue #11* |
| UI-003 | Pares texto/fondo por debajo de 4.5:1 / 3:1 | `docs/HALLAZGOS.md` 24, 26, 27, 28; `tests/check_wcag.py` | un fixture que renderice los pares que incumplen; el arnés ya calcula el contraste de cada par opaco y desde UI-02 lo corre también sobre los temas (`main-tema-*`) sin encontrar nada. Desde UI-06 incr. 1 el chequeo **estático** cubre los 7 temas resolubles (4 incorporadas + 3 JSON; 63 pares, 0 fallas) y declara UNCOVERED a los derivados — ver *Estático, sin fixture* |
| UI-004 | Ajustes: widgets inalcanzables con el foco | `docs/HALLAZGOS.md` 18; `tests/check_settings_focus.py` | **cerrada (2026-10-15)**: el inspector emite `in_keyboard_group` por nodo y el contrato `layout_focus_unreachable` corre sobre Ajustes/`bt-dialog`/`main-foco`/`apps-menu-*` con 0 hallazgos. Ver *Cierre de las semillas del issue #11* |
| UI-005 | Diálogo viejo: texto tapado por los botones | `docs/HALLAZGOS.md` 21 | **cerrada (2026-10-15)**: la fixture `bt-dialog` renderiza el diálogo real de BT; la línea de estado termina en y2=396, arriba de los botones (418). Ver *Cierre de las semillas del issue #11* |
| UI-006 | Glifo BT U+F293 inexistente en `sony_20` | `docs/HALLAZGOS.md` 19; `src/info.c` | que una pantalla vuelva a dibujar el glifo en un label; el arnés ya detecta glifos ausentes (y la pantalla actual dibuja el icono con `lv_line`) |
| UI-007 | Temas RS sin clave propia, contraste 2.58:1 | `docs/HALLAZGOS.md` 28; `docs/THEMES.md` | **cerrada (2026-10-15)**: el fixture `settings-rs-theme` (Ajustes + RS, grupo Espectro) dibuja el riel del switch con el `control_border` de RS (#6E6E6E); el par ya lo cubría el chequeo estático. Ver *Cierre de las semillas del issue #11* |
| UI-008 | CMA: 128 MB reservados para GPU no usada | `docs/RESOURCE_USAGE.md` | **cerrada como `out-of-scope` (2026-10-15)**: presupuesto de recursos del firmware, no defecto de la UI; el arnés no mide memoria del sistema. Ver la decisión en *Cierre de las semillas del issue #11* |

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
acotadas y con vencimiento, igual que UI-010. **Las cuatro ya están `fixed`** (UI-010
en AB-UI04-EXP3, UI-011 en EXP-1, UI-012 en EXP-2 y UI-013 en EXP-4) y cada
excepción se retiró con su fix: el registro no tiene hoy ningún waiver.

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
| UI-010 | Tema Outdoor: franja de frecuencia invisible | `fixed` (UI-04 / AB-UI04-EXP3) | critical | `region_min_content:frecuencia` | `main-otro-tema`; `AB-UI04-EXP3`; exclusión de región retirada |
| UI-011 | VISION: los iconos de la barra desaparecen | `fixed` (UI-04 / AB-UI04-EXP1) | major | `typography_missing_glyph` | `main-tema-vision`; `AB-UI04-EXP1`; excepción retirada |
| UI-012 | VISION: `VFO-A` no entra en su celda | `fixed` (UI-04 / AB-UI04-EXP2) | minor | `typography_text_overflow` | `main-tema-vision`; `AB-UI04-EXP2`; excepción retirada |
| UI-013 | `params.theme_name` no guarda un nombre que el menú ofrece | `fixed` (UI-04 / AB-UI04-EXP4) | major | `theme_name_fits_params` | `main-tema-outdoor-negative`; `AB-UI04-EXP4`; excepción retirada (registro sin waivers) |

**Orden propuesto para UI-04+** (severidad y alcance; lo aprueba el owner):

1. **UI-010** (critical, ya registrado): información crítica invisible en el campo.
   **Hecho** en UI-04/EXP-3 (ver arriba): la fila de frecuencia pide el rol del
   tema en vez de cablear blanco, con la exclusión de región retirada en el mismo
   cambio.
2. **UI-011** (major, VISION): es el tema pensado para vista débil y pierde los
   iconos de batería/WiFi en la barra; el arreglo es acotado (`fonts_scale`/fuentes).
   **Hecho** en UI-04/EXP-1 (ver arriba), con la excepción retirada en el mismo
   cambio.
3. **UI-013** (major): una opción de Ajustes que se elige y no se aplica, con el
   desplegable mostrando además otra selección; el arreglo toca `params`/menú.
   **Hecho** en UI-04/EXP-4 (ver arriba): `params_str_t.x` pasa a 32 bytes (el
   ancho que `theme_list()` admite por nombre) sin migración de `params.db`, con la
   excepción retirada en el mismo cambio y un test nativo que exige que todo nombre
   del menú entre en el store.
4. **UI-012** (minor, VISION): consecuencia de la geometría fija de la barra.
   **Hecho** en UI-04/EXP-2 (ver arriba): la celda del VFO vuelve al tamaño de los
   otros temas bajo VISION, con la excepción retirada en el mismo cambio.
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

---

## Fixture de Ajustes (2026-10-14) — re-evaluación de las semillas

El arnés ya **renderiza el diálogo real de Ajustes** (`src/dialog_settings.c`, por el camino
del firmware; ver `docs/AB_TESTING.md` → *Fixture del diálogo de Ajustes*). Era el habilitante
que las semillas venían esperando: re-evaluadas contra las dos fixtures nuevas
(`settings-home`, `settings-wfbat`) y sus **280 objetos** inspeccionados:

| ID | Resultado | Motivo concreto |
|---|---|---|
| UI-002 | sigue `unverified` | el fixture abre **Ajustes**, no el menú de apps (`ACTION_APP_*` de las apps): el menú es otra pantalla y sigue sin fixture |
| UI-003 | sigue `unverified` | el contraste estructural corre ahora sobre las dos fixtures de Ajustes y **no encuentra ningún par** por debajo de 4.5:1/3:1: los pares del diálogo (texto sobre el fondo del diálogo, etiquetas sobre el riel del switch) pasan con el tema por defecto. Nada que reproducir |
| UI-004 | **habilitante cerrado** | el fixture existe y la grilla de foco se construye (`dialog_item()` registra los widgets en `keyboard_group`); falta el **detector**: el inspector no emite aún la pertenencia al grupo de foco por nodo, así que no hay regla que marque "widget que no se puede alcanzar". El chequeo **estático** `tests/check_settings_focus.py` sigue cubriendo la fuente (28 widgets creados, todos con `dialog_item()`) |
| UI-005 | sigue `unverified` | el caso es de BT/WeFax/SSTV (el texto viejo apoyado en la fila de botones); Ajustes no lo reproduce |
| UI-006 | sigue `unverified` | el icono BT se dibuja con `lv_line`; el detector de glifos ausentes sigue sin nada que morder |
| UI-007 | **habilitante cerrado** | el par del riel del switch (`control_border`) vive en Ajustes y el fixture ya lo renderiza, pero con el **tema por defecto**: falta la variante **Ajustes + RS** para que el contraste se mida sobre el riel del tema RS (el chequeo estático ya resuelve el par y pasa en los 3 temas JSON) |
| UI-008 | sigue `unverified` | presupuesto de recursos del firmware; el arnés de UI no mide memoria del sistema |

Dos **hallazgos nuevos y reales** salen de estas fixtures (la spec de la corrida prohíbe tocar
Ajustes, así que quedan registrados con excepción acotada y fechada, no arreglados):

- **UI-015** (`layout_touch_target`): los ocho switches del grupo Espectro son 111×30 y el
  slider del brillo del backlight 159×13, por debajo del target de campo de 44 px (AGENTS.md).
- **UI-016** (`layout_clipped`): el campo **Año** dibuja "2026" con 84 px de texto y el área de
  contenido del spinbox le deja 80 → el último dígito pierde sus 4 px de la derecha.

Los dos están en el registro (`quality/known_issues.yaml`, `status: verified`) y cada uno tiene
su excepción acotada (regla + fixture) con vencimiento `2027-03-31` en
`quality/contracts.yaml`; el incremento de UX de Ajustes los arregla y **retira la excepción en
el mismo cambio**.

La fixture `settings-audio` (issue #26, la fila "Play,Rec gain") agrega su propia excepción de
**UI-015** por la misma geometría heredada del grupo Audio (los cuatro sliders de 13 px y el switch
de "Speaker mode", 111x30): misma regla, mismo hallazgo, otra página de la ventana.

---

## Cierre de las semillas del issue #11 (2026-10-15)

Corrida `factory/seeds-11-fixtures` (base `c4ff0c5`, 25 fixtures) sobre las cinco semillas
UI-002/004/005/007/008. Las cuatro que el arnés podía medir ya tienen fixture y **ninguna
reproduce un defecto**; la quinta es un presupuesto de recursos y queda declarada fuera del alcance.

| ID | Resultado | Evidencia |
|---|---|---|
| UI-002 | `fixed` (sin defecto reproducido) | seis fixtures `apps-menu-1..6` abren el menú de apps (la tecla APP carga `buttons_group_app`, el botón-página cicla las 6 páginas) y cubren las **22 celdas** post-About; `scenario_settle` lee la tabla real `buttons_group_app` y exige el rótulo `(APP n:6)` y las apps de la página (una página reordenada corta la corrida). Suma `tests/check_apps_menu.py` (23 apps, 23 en el menú, 6 páginas) |
| UI-004 | `fixed` (sin defecto reproducido) | el inspector (`--inspect`) emite `in_keyboard_group` por nodo (y `keyboard_group` en el bloque); el contrato nuevo `layout_focus_unreachable` (clase de control + visible + fuera del grupo) corre sobre los fixtures de Ajustes, `bt-dialog`, `main-foco` y `apps-menu-*` con **0 hallazgos**. Autoprueba: quitar `lv_group_add_obj()` de `dialog_item()` en una revisión efímera hace fallar el audit |
| UI-005 | `fixed` (sin defecto reproducido) | la fixture `bt-dialog` abre el diálogo real de BT (`main_screen_start_app(ACTION_APP_BT)` → `src/dialog_bt.c`, con `btaudio-ctl` mockeado). Medido con `--inspect`: la línea de estado "no headphones, RX off" queda en y=380 (y2=396), **arriba** de la fila de botones (y=418) → 21 px de aire; no reproduce el texto tapado. `scenario_settle` codifica ese invariante. El contrato `typography_text_truncated` encontró y se cerró un recorte real de 3 px de la **nota** del mismo diálogo (`NOTE_W`, `src/dialog_bt.c`) |
| UI-007 | `fixed` (sin defecto reproducido) | `settings-rs-theme` es la ventana de Ajustes (`settings-wfbat`) bajo el tema **RS**, scrolleda al grupo Espectro: el riel del switch se dibuja con el `control_border` de RS (**#6E6E6E**, medido en la inspección). El par ya lo cubre el chequeo estático `x6200-ab themes` (≥3:1); faltaba —y ahora está— la evidencia renderizada |
| UI-008 | `out-of-scope` | no es un defecto de UI: es un presupuesto de recursos del firmware y el arnés de UI no mide memoria del sistema |

### UI-008 — decisión (5 líneas)

UI-008 no es un defecto de la UI: el pool **CMA de 128 MB** del DTS reservado a la Mali
(unas ~3 veces lo que cuesta el configurador en Python, `docs/RESOURCE_USAGE.md`) es un presupuesto
de recursos del **firmware**, no un comportamiento de la GUI. El arnés de UI renderiza píxeles y mide layout,
tipografía, contraste y foco: **no mide memoria del sistema**, así que no puede reproducirlo por más
fixtures que se agreguen y no se le inventa un fake. Queda `out-of-scope` declarado (no es un waiver
de defecto: es la limitación del arnés con la razón por escrito). Su **monitoreo** vive donde vive el
presupuesto, [`docs/RESOURCE_USAGE.md`](RESOURCE_USAGE.md), que lo mide y ordena su reducción. Por
eso el registro lo lleva `status: out-of-scope` (sin fixture) y lo cierra esta corrida: el issue #11
queda con las cinco semillas resueltas.
