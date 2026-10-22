# Evidencia — #221 · Settings claros, textos operator-facing en inglés y contraste verificado

- **Issue**: `iariap/x6200_gui#221` (hijo 5/6 de la épica `#216`).
- **Rama**: `factory/next-221` (worktree `state/worktrees/x6200/next-221`).
- **Base**: `b798f13b` = tip de `origin/lu2dsx/x6200-rom`.
- **Código**: commit `86764045` (diff acotado, sin push: el PR lo abre el orquestador).
- **No se tocó**: temas JSON, defaults, IDs persistidos, acciones ni contenido DSP.

Alcance ejecutado: sólo los cuatro archivos del contrato (`src/dialog_settings.c`,
`src/dfl_inline.c`, `src/dialog_gps.c`, `src/dialog_propagation.c`) + el fixture
focalizado `tools/ui_harness/fixtures/dfl-editor-reset.scn` y la aserción del arnés
`tools/ui_harness/src/screens.c` (necesaria: el arnés codificaba el rótulo viejo del
botón DFL).

## 1. Cambios por archivo

| Archivo | Cambio |
| --- | --- |
| `src/dialog_settings.c` | label de la fila de timeouts: `Timeout Clock, Power, TX` → **`Clock / Power / TX (s)`** (+ comentario de por qué la unidad es segundos: `src/clock.c` multiplica ×1000). Mismo orden de los tres spinboxes (time/power/tx), mismos rangos 1..59, 1..59, 0..10, mismos callbacks, posiciones, columnas, foco, persistencia y formato de 2 dígitos. `Timeout, Brightness` intacto. |
| `src/dfl_inline.c` | `Restablecer` → **`Reset`**; `Esquema` → **`Schematic`**. IDs/zonas táctiles (`DFL_UI_RESET`, `DFL_UI_GRAPH`), política de reset y valores DSP intactos. La gráfica sigue siendo *esquemática* (se renombró la etiqueta, no se afirmó medición). |
| `src/dialog_gps.c` | `sin datos` → **`No data`** (antigüedad sin fix). |
| `src/dialog_propagation.c` | los cuatro textos exactos del contrato: `Sin QTH configurado (Ajustes > QTH): no se puede dar la direccion.` → `Set QTH in Settings to calculate direction.`; `Reloj sin NTP: el turno de los beacons puede estar corrido.` → `Clock not synchronized: beacon timing may be inaccurate.`; ` Sin spots recientes del cluster: sin datos para esta banda.` → ` No recent cluster spots for this band.`; `SIN DATOS` → `NO DATA`. |
| `tools/ui_harness/src/screens.c` | la aserción `label_on_screen(..., "Restablecer")` del editor DFL pasa a `"Reset"` (codifica el rótulo nuevo; sin esto el gate de determinismo/contratos rompía). |
| `tools/ui_harness/fixtures/dfl-editor-reset.scn` | comentario: `pulsar Restablecer` → `pulsar Reset`. |

## 2. Auditoría estática de textos (antes / después)

Salida literal de `/tmp/next221/audit_strings.py` (extrae el primer literal de cada
llamada visible: `lv_label_set_text`, `lv_dropdown/roller_set_options`,
`lv_msgbox_add_text`, `return "..."` de `verdict_es`, `snprintf(...)` en una línea).

### Settings (`src/dialog_settings.c`) — 36 literales, TODOS en inglés antes y después

Antes == Después salvo la fila de timeouts:

```
- 'Timeout Clock, Power, TX'
+ 'Clock / Power / TX (s)'
```

El resto (Theme, Font, Clock view, Day, Month, Year, Hour, Min, Sec, Timeout,
Brightness, Buttons brightness, S-meter needle, Band plan, Waterfall auto min, max,
Voice mode, Voice rate, pitch, volume, Charger, …, los 4 desplegables de opciones y
`detents/s`) ya estaba en inglés: Settings fue normalizado en el #109 y lleva su
regla escrita en el código (`src/dialog_settings.c:123`, "Settings es solo ingles
(regla del owner)"). Los únicos tokens en español en `dialog_settings.c` son
**comentarios** y **diagnóstico de log** (`LV_LOG_USER`, "Setting without a label",
números de grupo), no cadenas operator-facing. No hace falta selector de idioma: no
se agregó ninguno.

### Otros archivos tocados

```
src/dfl_inline.c         - 'Restablecer'   + 'Reset'
                         - 'Esquema'       + 'Schematic'
src/dialog_gps.c         - 'sin datos'     + 'No data'
src/dialog_propagation.c - 'SIN DATOS'     + 'NO DATA'
                         - 'Sin QTH configurado (Ajustes > QTH): no se puede dar la direccion.'
                           + 'Set QTH in Settings to calculate direction.'
                         - 'Reloj sin NTP: el turno de los beacons puede estar corrido.'
                           + 'Clock not synchronized: beacon timing may be inaccurate.'
                         - ' Sin spots recientes del cluster: sin datos para esta banda.'
                           + ' No recent cluster spots for this band.'
```

**Alcance declarado (no es un defecto oculto)**: `dialog_propagation.c` conserva
otras cadenas en español que **no** están en el contrato de 4 sustituciones:
`Paso 1/4 | Preparando`, `Paso 2/4 | Sincronizando`, `Paso 3/4 | Midiendo`,
`SIN BEACONS IBP`, `la banda actual`, `No se pudo crear el hilo del barrido.` y las
notas multilínea (`Sin beacons IBP en %s…`, `Cancelado por el operador…`, `El equipo
dejo de estar en RX…`, `No se escucho ningun beacon…`, `Umbrales: abierta…`). El
contrato del issue pide sustituir **sólo** esos cuatro textos y prohíbe ampliar
alcance; la regla "Settings en inglés" del owner aplica a **Settings**, no a esta
app. Se reportan como observación para quien decida (no se tocaron).

**Residual en el mismo camino de GPS**: `src/gps_format.c:50` también devuelve
`"sin datos"` (para una antigüedad fuera de rango) y `tests/native/test_gps_format.c`
lo fija; el contrato lista **sólo** `src/dialog_gps.c` y prohíbe tocar otro archivo de
producción, así que no se modificó. Queda anotado para un follow-up (no es Settings).

## 3. El literal del reloj: medición de encaje (800×480, seis temas)

Método: `ui_harness --inspect` sobre el grupo **Clock** de Ajustes con cada uno de los
seis temas (`screen settings-grupo` + `settings_grupo Clock` + `theme <T>`; fixtures
temporales de medición, **no** commiteadas). El `lv_label` se dimensiona por contenido;
la columna de etiquetas va de x=22 al borde de la zona de controles (x=499).

| Tema | Antes: `Timeout Clock, Power, TX` | Después: `Clock / Power / TX (s)` | overflow | truncated |
| --- | --- | --- | --- | --- |
| Aurora   | 297×28 | 253×28 | false | false |
| Cathode  | 331×29 | 280×29 | false | false |
| Redshift | 293×29 | 244×29 | false | false |
| Sprint   | 264×28 | 228×28 | false | false |
| Sunlit   | 332×29 | 270×29 | false | false |
| Trail    | 331×29 | 280×29 | false | false |

El literal nuevo es **más angosto** en los seis temas (peor caso Trail/Cathode: 280 px,
x=22..302), muy por debajo de x=499: no hay truncamiento, recorte ni solapamiento con
los spinboxes. La fila (`row_dsc = 54`) y la caja de los tres spinboxes no se movieron
(ver §5: el diff de píxeles queda dentro de la fila y=272..300).

## 4. Rótulos del editor DFL (`--inspect`)

`Reset`: `Restablecer` 73×15 → `Reset` 35×15, centrado en la misma caja de 128×44
(`DFL_UI_RESET`). `Schematic`: `Esquema` 72×11 (natural 39) → `Schematic` 72×11
(natural 44, Sunlit 52): mismo `text_box` de 72, sin truncar, alineado a la derecha del
gráfico. Ninguno de los dos cambia de posición ni de zona táctil.

## 5. A/B de píxeles (revisiones A = `b798f13b`, B = `8676404`)

Comando (herramienta del repo):

```
uv run x6200-ab compare --a b798f13b --b 86764045 \
    --repo <worktree> --reports-dir <tmp>/ab-reports --experiment-id AB-next221 --json
```

Veredicto: **PASS** (`exit 0`). 204 escenarios capturados en ambas revisiones; **35**
con píxeles movidos y **todos dentro de regiones ya declaradas** en
`tools/ab_testing/quality/contracts.yaml` (`expected_changes`):

- `settings-home`: **2510 px**, dentro de `ajustes_dialogo` (la fila del reloj,
  bbox medida x=23..318 y=275..297).
- `dfl-editor-*` (31 fixtures): **457..685 px** cada una, dentro de `dfl_inline`
  (botón Reset y rótulo Schematic, bbox x=396..489 y≈287..386).
- `dfl-mirror-*` y `dfl-presets-graph-tema-*`/`dfl-presets-vol`: idem.

Gates del reporte: Functional **PASS**, Visual Regression **PASS** ("no unexplained
visual regressions"), Quality Contract **PASS** (2106 checks), Performance **PASS**
(proxies deterministas: `flushes` y `flushed_pixels` con Δ 0.0% en todos los
escenarios), Hardware **BLOCKED**, Energy **BLOCKED** (host, no radio).

## 6. Contraste verificado (no a ojo)

- **Estático, todos los temas servidos**: `uv run x6200-ab themes` → `PASS: 10
  palette(s) …, 90 pair(s) checked, 0 failure(s)`. Incluye los pares del tema Sunlit
  y los objetos no-texto (arco del S-meter, columna del waterfall, línea central,
  barras de CW tune) sobre los renders dorados.
- **Objetos no-texto medidos en Sunlit** (de la salida de `x6200-ab themes`):
  `s_meter_arc` 3.66:1, `waterfall_signal` 8.89:1 (`main-tema-sunlit`), 9.16:1
  (`main-tema-sunlit-agua`), `waterfall_center_line` 14.51:1, `cw_tune_bars` 5.02:1 —
  todos ≥ 3.0:1 (umbral ya existente).
- **Contratos del arnés sobre B**: `x6200-ab contracts --revision 8676404` →
  `PASS`, `checks: 2106`, `failures: []`, sin cambios de presupuesto ni golden.
- **Inspección de `main-tema-sunlit` + sonda de píxeles de los rótulos sobre la
  cascada** (indicación de F-10, "revisar contraste de rótulos en la cascada"): sobre
  el render real, cada rótulo declara su `text_color` del tema y el fondo medido es el
  de la superficie que LVGL dibuja detrás:

  | Rótulo (tema Sunlit) | color | fondo medido | ratio |
  | --- | --- | --- | --- |
  | `DFL USB` | `#053F80` | `#ECF4F9` | 9.30:1 |
  | `Schematic` | `#33475B` | `#EAF3F8` | 8.51:1 |
  | `LOW` | `#112537` | `#EFF6FA` | 14.31:1 |
  | `0 Hz` | `#112537` | `#EEF6FA` | 14.29:1 |
  | `BW` | `#33475B` | `#ECF4F9` | 8.61:1 |
  | `Cancelar` | `#112537` | `#F2F5F2` | 14.23:1 |

  Todos superan 4.5:1 (texto normal). **No se midió ninguna violación de los umbrales
  ya existentes** → por contrato **no se cambia la paleta** ni el fondo/opa de la
  etiqueta, y no se agrega un 7º tema. La indicación de la captura de #197 (F-10) queda
  como **REVIEW_REQUIRED / impresión visual**: no es fallo WCAG reproducido por el
  arnés. La legibilidad al sol/LCD real es **PENDING_HARDWARE** (no hay captura física).

## 7. Gates (salida real)

| Gate | Comando | Resultado |
| --- | --- | --- |
| Nativos (host) | `sh tests/native/run_native_tests.sh` | **OK** — 141 comprobaciones, 0 fallos |
| Nativos (paridad CI) | `sh tests/native/run_native_tests.sh --ci-parity` | **OK** — 141 comprobaciones, 0 fallos; `lvgl/` oculto |
| Suite completa | `sh tests/run_all_tests.sh` | **OK** — 19 OK, 0 fallos. SKIP: `wspr`, `sstv`, `wefax`, `js8` (la imagen `x6200-uiharness` no trae las deps de codec), `webserver` (sin checkout `.x6200_webserver`) |
| Estáticos | `check_cfg_fills`, `check_actions`, `check_settings_focus`, `check_settings_grid`, `check_apps_menu`, `check_app_handlers`, `check_manual_apps`, `check_wcag`, `check_cw_tune_feed`, `check_build_identity`, `check_autotune_v2` | **OK** (todos rc=0) |
| Determinismo | `bash tests/check_harness_determinism.sh` | **PASS** — PNGs idénticos 2× por fixture |
| Natural | `bash tests/check_harness_natural.sh` | **PASS** — perfil natural determinista, acotado |
| Wakeups | `bash tests/check_harness_wakeups.sh` | **PASS** — perfil de wakeups medido y delimitado |
| Waterfall incremental | `bash tests/check_waterfall_incremental.sh` | **PASS** — 195 checks, 0 fallas (+ ASan/UBSan) |
| Auditoría de pruebas | `python3 tests/audita-pruebas.py` | **OK** (estructura y evidencia en orden; "para revisar a mano" preexistentes) |
| Temas (contraste) | `uv run x6200-ab themes` | **PASS** — 90 pares, 0 fallas |
| Contratos | `uv run x6200-ab contracts --revision 8676404` | **PASS** — 2106 checks, 0 fallas |
| A/B visual | `uv run x6200-ab compare --a b798f13b --b 8676404` | **PASS** — 35 fixtures movidas, todas en regiones declaradas |
| Python del arnés | `uv sync` + `ruff check` + `ruff format --check` + `ty check src tests` + `pytest -m "not e2e"` | **OK** — 233 passed, 13 deselected |
| Matriz | `scripts/apps/x6200/check-matrix.sh origin/lu2dsx/x6200-rom HEAD <worktree>` | **rc=0** — todos los checks `ok` salvo los `[SKIP]` por paths (temas JSON, `tools/ab_testing/**/*.py`) y el banco QEMU |

## 8. PENDING / BLOCKED razonados

- **Banco QEMU (`BLOCKED`)**: el preflight de `tools/captura-banco.sh --check` reporta
  que faltan el volumen `x6200-build-output`, el chroot, el binario `x6200-remote` y
  libs del banco. Son **recursos privados** de otras sesiones en vuelo; la política de
  laboratorio de la épica prohíbe usarlos/compartirlos, así que el banco se declara
  `BLOCKED` y **no** se corre. Los **22 diálogos inertes en host** incluyen GPS y
  Propagación: sus textos no se pueden capturar en el arnés de host →
  **PENDING_QEMU** para la captura de esos dos diálogos (el cambio de strings es
  textual y no toca el camino de código; se verificó por lectura y por el diff).
- **Legibilidad al sol / LCD real**: **PENDING_HARDWARE** (no hay medición física).
- **Temas JSON / paleta**: no se tocaron (no había violación medible; ver §6).
- **Manual / docs en español**: `docs/manual/MANUAL.md`, `MANUAL.html` y
  `docs/settings-order.md` siguen citando `Timeout Clock, Power, TX`; el contrato
  prohíbe traducir el manual entero y el alcance es "visuales locales + tests". Queda
  como observación para el hijo de docs (#222).
