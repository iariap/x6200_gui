# #173 PR 2 — Editor DFL completo (diálogo, quinta celda, acción y reset)

Corrida `factory/dfl-editor-173`. **Base**: `ce77df7a` (trunk tip con el PR 1).
**Commits de código**: `7184892a` (editor + modelo + acceso), `217701c8` (waivers
UI-017 del editor) y `c9428204` (el editor entra por el dispatcher de apps y el
control remoto, para el banco). **HEAD de la corrida**: `e4f2b594` (más la
evidencia `a0dda436` y el fix del stub del test remoto). Alcance **PR 2** del §6
del issue #173.

## 1. Exploración previa (§8) — con líneas

1. **HEAD real**: `ce77df7a` (trunk tip con PR 1, `lu2dsx/x6200-rom`). Sin cambios
   del owner sobre `spectrum.c`/`waterfall.c`/`buttons.cpp`/`main_screen.c` desde
   ese commit.
2. **Framework de diálogo**: `dialog_t` (`src/dialog.h`), ciclo de vida en
   `src/dialog.c::dialog_construct()/dialog_destruct()`; `btn_page` es la página
   que se carga mientras el diálogo está abierto y `prev_page` la que se
   restaura al cerrar. `dialog_eq.cpp` es el patrón (descriptor estático, un solo
   `dialog->run`). `restore_radio` decide si el frame guarda/restaura el estado de
   la radio (issue #94); el editor lo deja en `false`.
3. **Controles reales del filtro**: `src/cfg/mode.c::cfg_mode_set_low_filter()`
   (línea 317) y `cfg_mode_set_high_filter()` (340) son los setters; el mapeo por
   modo y los bordes reales salen de `update_real_filters()` (550).
   `src/vol.c` fija los pasos: LOW **10 Hz** (79), HIGH **50 Hz** y **10 Hz** en
   CW/CWR (96-107), BW **20 Hz** (122). `MAX_FILTER_FREQ` = 10000
   (`src/cfg/mode.h`). `on_cur_filter_bw_change()` (447) es el mapeo BW→LOW/HIGH.
   `mode_default_values()`/`xmode_2_db()` (`src/cfg/mode.private.h`) son la tabla
   de predeterminados por modo.
4. **Ciclo de vida seguro (antecedente #131)**: los observers del filtro/modo son
   **diferidos** (`subject_add_delayed_observer`); un cambio desde otro hilo se
   encola y corre en el hilo de GUI (`observer_delayed_notify_all`). El diálogo
   borra sus observers en `destruct_cb` y anula sus punteros a objetos antes de
   que `dialog_destruct()` borre el `obj`.
5. **Acceso físico**: pulsación corta DFL = `KEYPAD_DFL` RELEASE →
   `buttons_load_page_group(buttons_group_dfl)` (main_screen.c:872-880); larga =
   `main_screen_action(params.long_dfl)`. El dispatcher de apps es
   `main_screen_start_app()` (228).
6. **Fixtures deterministas**: el arnés ya tenía `filter_low`/`filter_high`
   (PR 1); el editor lee `cfg_cur.filter.low/high/bw` y `cfg_cur.mode`.
7. **Impedimento**: ninguno que obligue a ampliar alcance. Dos ajustes propios del
   entorno, documentados en §8.

## 2. El diálogo y su geometría (§4)

`src/dialog_dfl.c/.h` (nuevos, registrados en `src/CMakeLists.txt`). Modal
**720×206 en x=40, y=202**, `dialog_init()` + superficie sólida con roles del tema
(`TC(popup)`/`TC(popup_border)`, opaca a propósito para tapar el waterfall). El
bbox del contrato se respetó **sin recortes** en 800×480 (verificado con el audit
estructural y las capturas): header a y=206..250, contenido 258..390, barra
inferior intacta en y=418..479. **No tapa** la barra superior ni la fila de
frecuencia.

Contenido:
- **Header**: “DFL - Digital Filter”, modo actual (`info_params_mode_label_get()`),
  botón **X** (44×44) y botón **Default** (100×44) para restablecer.
- **Gráfico esquemático** (no medición DSP): ejes y curva de banda pasante con
  dos **handles grandes**. Etiquetado **“Esquema de banda pasante”**. El eje es la
  **frecuencia del filtro** (Hz de low/high, con el cero visible), **no** las
  coordenadas del waterfall. El dominio se ajusta al modo: SSB `[0, 10000]`;
  CW/CWR y AM/SAM/NFM/WFM simétricos `[-10000, +10000]`.
- **LOW / HIGH / BW** con el valor entero exacto en Hz leído de los Subjects,
  botones **−/+** (44×44) y arrastre de los handles.

## 3. Controles: pasos, límites y validación (§4)

- Pasos reales de `src/vol.c`: LOW **10 Hz**, HIGH **50 Hz** (10 en CW/CWR), BW
  **20 Hz**. La cuantización es `align_int` (`x - x % step`), igual que vol.c.
  Valores guardados no múltiplos (p.ej. LOW=225) **no se redondean al abrir**;
  sólo se cuantizan tras una modificación efectiva.
- LOW/HIGH por **`cfg_mode_set_low_filter()`/`cfg_mode_set_high_filter()`** (no se
  escriben los límites reales a mano). LOW no editable en AM/SAM/NFM/WFM (misma
  regla del setter); LOW negativo permitido en CW/CWR; `high > low` y
  `high <= MAX_FILTER_FREQ` por el setter.
- BW: se calcula el par LOW/HIGH resultante con el **espejo de
  `on_cur_filter_bw_change()`** y se valida **antes** de escribir
  (`dfl_bw_legal`: HIGH ≤ MAX, HIGH > LOW, LOW no negativo en SSB). Si no es
  legal, se **bloquea** y se conserva el estado.
- **Sin valores optimistas**: el label y la gráfica se actualizan releyendo el
  Subject; si el setter rechaza, el Subject no cambia y la UI sigue mostrando el
  valor confirmado (el arnés lo codifica: HIGH saturado en 10000 con dos `+` de
  más). **Nunca** se escriben low/high/bw recursivamente (el refresh sólo lee).

## 4. Restablecer predeterminado (§4)

`dfl_reset_default()` usa **`mode_default_values(xmode_2_db(modo), …)`** y
actualiza **sólo LOW y HIGH** (nada de frecuencia, AGC, zoom ni otros modos). Se
confirman los valores de la tabla antes de escribir (`mode_default_values` debe
devolver `true`). El **orden de escritura** se elige para pasar las guardas de los
dos setters (`dfl_reset_low_first`): si `def_high > cur_low` se sube HIGH primero;
si no, se baja LOW primero. Así el par queda aplicado aunque el estado actual esté
fuera de rango.

## 5. Acceso: quinta celda + acción (§3)

- **Quinta celda** de la página DFL: botón **“DFL editor”** (normal) y **“Cerrar
  DFL”** (editor abierto). Las primeras cuatro (Filter low/high/BW/Eq) conservan
  texto, función y control de encoder. Páginas expuestas en `src/buttons.h`
  (`buttons_page_dfl_1`, `buttons_page_dfl_editor`) sin duplicar
  `button_item_t`.
- **`ACTION_APP_DFL_EDITOR` al final** de `press_action_t` (valor 131; **sin
  renumerar**, hueco 121 preservado; `_Static_assert` extendido). Default **sólo
  para instalaciones sin valor previo** (`params.c`: `.long_dfl`); un equipo con
  `long_dfl` guardado conserva el suyo. “Battery info” sigue elegible.
- **Settings → DFL long press**: opción “ APP DFL editor” en la lista.
- **`main_screen_action()`**: caso nuevo que delega en `main_screen_start_app()`
  (como el resto de las ventanas: cierra la app abierta y no deja huérfanas).
  **Pulsación corta física DFL intacta**; **larga** abre el editor sólo si
  `params.long_dfl == ACTION_APP_DFL_EDITOR`. Se agregó “DFLEditor” a
  `app_names[]`/`actions[]` de `remote_server.cpp` para que el banco (y el
  webserver) puedan abrirlo.

## 6. Sincronización y lifecycle (§4)

- Observers **diferidos** sobre `cfg_cur.mode` y `cfg_cur.filter.low/high/bw`:
  cambios de modo/VFO/encoder/botones originales/otro camino actualizan labels,
  gráfica y overlay.
- **TX**: al abrir se lee `radio_get_state()`; `EVENT_RADIO_TX`/`EVENT_RADIO_RX`
  (reenviados por `main_screen_radio_cb` → `dialog_send`) deshabilitan **visiblemente**
  los controles de edición y dejan **cerrar**; el estado RX no se degrada al
  volver a RX (los Subjects no se tocan).
- **Sin reapertura anidada**: `dialog_construct` sólo construye si `!run`; abrir
  otra vez pasa por `apps_close()` (cierra y reconstruye). Cerrar **no deshace**
  lo aplicado.
- Sin punteros a objetos borrados: `destruct_cb` borra observers y anula punteros
  antes de `lv_obj_del`.

## 7. Pruebas y gates (salida real)

### 7.1 Nativo (contenedor `x6200-uiharness`, ASan/UBSan)

- `tests/native/run_native_tests.sh`: **todo OK, 0 fallos** (`logs/native-container.log`).
  El bloque del editor DFL: **94 comprobaciones, 0 fallos** (pasos, cuantización,
  dominio, **golden de geometría y handles**, BW y legalidad, orden del reset).
- `gcc -fanalyzer` sobre el modelo: sin observaciones (lo corre el script).

### 7.2 Arnés de UI y determinismo

- 11 fixtures `dfl-editor-*` (USB-D, LSB-D, 6 temas, TX, interacción y 30
  ciclos) capturan **OK**; `scenario_settle` codifica encabezado, controles,
  “Esquema de banda pasante”, los valores de los labels y los resultados de la
  interacción (LOW=240, HIGH=10000 saturado).
- `tests/check_harness_determinism.sh`: **PASS** (PNG idénticos en dos corridas de
  cada fixture, incluidos los `dfl-editor-*`) (`logs/determinism.log`).
- `tests/check_harness_natural.sh`: **PASS**; `tests/check_drawbuf_partial.sh`:
  **PASS**; `tests/check_waterfall_incremental.sh`: **PASS 195 checks**.
- `x6200-ab audit` (inspección estructural REAL, 12353 objetos, 112 escenarios):
  **0 hallazgos** (`logs/ab-audit.log`).
- `x6200-ab themes` (WCAG estático sobre los 6 temas): **OK** (`logs/ab-themes.log`).

### 7.3 A/B (`AB-DFL173-2`)

`uv run x6200-ab compare --a ce77df7a --b factory/dfl-editor-173 --experiment-id AB-DFL173-2`:

```text
AB-DFL173-2: PASS (exit 0)
  Functional         PASS   101 scenarios captured on both revisions
  Visual Regression  PASS   captures are pixel-identical to the baseline
  Quality Contract   PASS   1035 contract checks passed on the candidate
  Performance        PASS   render-cost proxy within budget
  Hardware/Energy    BLOCKED (advisory)
```

**Confinamiento del diff**: la única modificación de pixeles posible fuera del
editor sería la quinta celda de la página DFL, que ninguna fixture previa
renderiza; los 101 escenarios compartidos quedan **pixel-idénticos** al baseline
(cero cambios en header, S-meter, espectro, frecuencia, band plan y las cinco
teclas). Las fixtures nuevas (`dfl-editor-*`) muestran **sólo** el modal y la
quinta celda (`Cerrar DFL`). Reporte completo en
`tools/ab_testing/reports/AB-DFL173-2/` y `logs/ab-compare.log`.

### 7.4 Estáticos y calidad

- `uv run ruff check .`, `ruff format --check .`, `ty check`: OK.
- `pytest -m "not e2e"`: **227 passed, 13 deselected**.
- `pytest -m e2e` (con `TMPDIR` en disco): **13 passed**.
- `tests/check_actions.py`, `check_apps_menu.py`, `check_app_handlers.py`,
  `check_cfg_fills.py`, `check_settings_focus.py`, `check_manual_apps.py`,
  `check_wcag.py`, `check_cw_tune_feed.py`, `check_build_identity.py`,
  `audita-pruebas.py`, `test_swrscan_curve_evidence.py`: OK.

### 7.5 ARM cross-compile + banco QEMU (gate #86)

Volumen de build **privado del issue** (`x6200-build-output-dfl173`, reusado por
PR 1/PR 2 por espacio en disco) y chroot privado
(`/home/pablo/desa/asf/state/dfl173-chroot`).

```text
binario: md5 7b623b231417
fuente:  md5 29f0e8f882ccf39f2cbb55ef3a983176   (sello .fuente.md5 del volumen)
boot_identity (main.c:142): fuente: 29f0e8f882ccf39f2cbb55ef3a983176   -> COINCIDE
```

El banco corre el **binario ARM real** con `qemu-arm-static` (cortex-a7), pasa el
gate fail-closed de firmware del port (3/3 marcas) y **no dejó `crash.txt`**.
Capturas (`banco/`):

| Captura | Tema | Modo | Editor |
|---|---|---|---|
| `banco-usbd-aurora-oscuro.png` | Aurora (oscuro) | USB-D | 225/3525/3300 Hz |
| `banco-lsbd-aurora-oscuro.png` | Aurora (oscuro) | LSB-D | 225/3525/3300 Hz |
| `banco-usbd-sunlit-claro.png` | Sunlit (claro) | USB-D | 225/3525/3300 Hz |

El editor se abrió por el **control remoto** (`{"cmd":"app","text":"DFLEditor"}`),
el mismo camino que usa el webserver. Log de arranque: `logs/banco-out.txt`.

### 7.6 Capturas 800×480 antes/después

`capturas/dfl-editor-usb-d.png` (editor, USB-D), `capturas/dfl-editor-lsb-d.png`
(espejo), `capturas/dfl-editor-usb-d-tema-*.png` (6 temas),
`capturas/dfl-editor-tx.png` (controles deshabilitados durante TX) y
`capturas/dfl-editor-interaction.png` (tras `+`/arrastre/rechazo). El “antes” es
el baseline del A/B (`ce77df7a`, sin el editor).

## 8. Qué se cambió y qué no

**Se cambió**: `src/dialog_dfl.c/.h` y `src/dfl_model.h` (nuevos);
`src/buttons.cpp`/`.h` (quinta celda + página del editor); `src/main_screen.c`
(caso de la acción + `app_label`); `src/params/params.h`/`.c` (enum + default);
`src/dialog_settings.c` (opción de long press); `src/remote_server.cpp`
(“DFLEditor”); `src/CMakeLists.txt`; el arnés (`screens.c`, `mock_cfg.c`,
`CMakeLists.txt`, fixtures) y los tests nativos (`test_dfl_editor.c` +
`stubs_dfl/`), `stubs_remote/params/params.h`, `check_apps_menu.py`,
`test_scenarios.py`, `test_theme_checks.py`, `contracts.yaml`.

**No se tocó**: RF/TX/DSP, MCU, FFT, CAT, audio, notch, el waterfall, el espectro
del PR 1, la paleta de los temas, la línea central del #140, ni la geometría de
`main_screen`. **Sin SHIFT, sin presets inventados, sin filtros falsificados, sin
TX filter avanzado ni botones extra.**

## 9. Comandos reproducibles

```sh
# nativo (contenedor)
docker run --rm --entrypoint bash -v "$PWD:/src" -w /src x6200-uiharness \
    sh tests/native/run_native_tests.sh /tmp/native

# arnés
cd tools/ui_harness && REV=$(git rev-parse HEAD) && docker run --rm --entrypoint bash \
    -v "$PWD/../..:/src" -w /src/tools/ui_harness x6200-uiharness \
    -lc "mkdir -p build && cd build && cmake .. -DHARNESS_REVISION=$REV && make -j\$(nproc)"
cd tools/ui_harness/build && ./ui_harness --scenario ../fixtures/dfl-editor-usb-d.scn \
    --png out.png --json out.json --inspect

# A/B y calidad
cd tools/ab_testing
uv run x6200-ab compare --a ce77df7a --b factory/dfl-editor-173 --experiment-id AB-DFL173-2
uv run x6200-ab audit  --revision factory/dfl-editor-173
uv run x6200-ab themes
uv run ruff check . && uv run ruff format --check . && uv run ty check
uv run pytest -m "not e2e" && TMPDIR=/var/tmp uv run pytest -m e2e

# determinismo
tests/check_harness_determinism.sh

# ARM + banco QEMU (volumen y chroot privados del issue)
VOLUMEN=x6200-build-output-dfl173 BUILDROOT=/home/pablo/desa/x6200/buildroot_new/AetherX6200Buildroot \
    tools/compila-gui.sh 4
CHROOT=/home/pablo/desa/asf/state/dfl173-chroot VOLUMEN=x6200-build-output-dfl173 \
    tools/captura-banco.sh banco.png '{"cmd":"app","text":"DFLEditor"}'
```

## 10. PENDIENTES / decisiones para el owner

1. **Encabezado con guion ASCII**: el contrato dice “DFL — Digital Filter” (em
   dash, U+2014). Se usó **“DFL - Digital Filter”** para no arriesgar
   `typography_missing_glyph` en las familias de fuente (el audit lo marcaría como
   caja vacía). Si el owner quiere el em dash, es un cambio de una línea **si** las
   fuentes lo traen.
2. **El editor entra por `main_screen_start_app()` y por el control remoto**
   (además de la quinta celda y el long press). Es lo que permite abrirlo por el
   banco/webserver y evita ventanas huérfanas; no agrega UX nueva (es el mismo
   dispatcher de las demás ventanas).
3. **Volumen de build reusado** (`x6200-build-output-dfl173`): es privado del
   issue #173 (PR 1 y PR 2) por límite de disco del host (99% ocupado). No se
   compartió con otras corridas.
4. **Validación física en la radio** (`requires-radio`): el banco QEMU no tiene
   FFT ni audio reales; las capturas ARM prueban el **render** del binario real,
   no la recepción ni que el filtro DSP cambie.
5. **BW en el arnés**: el mock de `cfg` no corre los observers de `src/cfg/mode.c`,
   así que en el arnés el BW no recentra LOW/HIGH (sólo cambia su propio Subject).
   La lógica de BW (objetivos y legalidad) se prueba en el **test nativo**, que es
   el oráculo del mapeo real.
6. **`check_manual_apps.py`** avisa que `ACTION_APP_DFL_EDITOR` no tiene nombre
   mapeado ni sección en el manual; es informativo (el chequeo pasa). El editor no
   es una app del menú; si el owner quiere una sección en el manual, se agrega.

## 11. Cómo deshacer

Revertir `7184892a`, `217701c8` y `c9428204`: se quitan `src/dialog_dfl.*`,
`src/dfl_model.h`, la quinta celda y la página del editor, la acción
`ACTION_APP_DFL_EDITOR` (y su entrada en Settings/remote), los fixtures/tests y
los waivers. No hay migración de datos ni cambio de paleta; `long_dfl` de un
equipo existente no se toca (el default sólo aplica sin valor guardado).
