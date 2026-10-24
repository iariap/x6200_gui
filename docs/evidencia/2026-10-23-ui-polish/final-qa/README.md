# QA final de la épica #284 — matriz 800×480, gates y veredicto (issue #287)

> **Ronda 2 (2026-10-24)**: el `P1` de este documento (Sunlit) fue resuelto por
> #301 y re-verificado sobre `faff19ba` en [`R2.md`](R2.md) (+ capturas en `r2/`).
> Este documento se preserva tal como se publicó en la ronda 1.

Corrida de **calificación de release, sólo evidencia**. No se toca código de
runtime: el diff de esta rama es `docs/evidencia/2026-10-23-ui-polish/final-qa/**`
y nada más.

| | |
|---|---|
| Issue | `iariap/x6200_gui#287` (hijo de la épica [#284](https://github.com/iariap/x6200_gui/issues/284)) |
| Worktree | `/home/pablo/desa/asf/state/worktrees/x6200/ui-polish-qa-284` |
| Rama | `qa/ui-polish-release-284` (sin push; el PR lo abre el orquestador) |
| **SHA candidato (congelado)** | **`1845cb77d36b93c4d8cd774fd130735f4d2f6f30`** |
| `origin/main` al medir | `1845cb77d36b93c4d8cd774fd130735f4d2f6f30` (**no avanzó**: el candidato es `origin/main`) |
| Prerrequisitos | #285 (`107c1536`), #286 (`e1807671`), #288 (`1845cb77`) mergeados |
| Fecha | 2026-10-23 |
| Radio / hardware | **no** (sin SSH, flash, deploy, TX/RF ni `sudo`); QEMU **BLOCKED** (§9) |
| Aislamiento | contenedores `--rm` propios (`x6200-qa284-*`), build y baseline **fuera** del worktree (`/tmp/opencode/…`), cero contenedores propios al terminar |

**Veredicto: NO-GO para el release del ROM.** Hay una regresión material (`P1`,
§7) introducida por #286 en el tema **Sunlit**: los valores de los controles de
Ajustes quedan **invisibles** (blanco sobre blanco). Todo el resto de la matriz
está verde. El issue de origen de la regresión es #286; esta corrida **no la
arregla** (es evidence-only).

---

## 0. Cómo leer esta evidencia

* `manifest.csv` — una fila por captura: cobertura declarada por el arnés, SHA,
  comando, exit code, sha256 del PNG, grupo y observación.
* `comandos.csv` — la matriz completa de comandos con su **exit code real**.
* `mediciones/` — las mediciones crudas (CSV) y sus scripts reproducibles.
* `png/` + `json/` — las **243** capturas 800×480 a 1:1 y el árbol LVGL vivo
  (`--inspect`, que no cambia un pixel).
* `logs/` — la salida de cada comando, con su `EXIT=`.
* `crops/` — ampliaciones 4× (vecino más cercano) **declaradas como ayuda de
  lectura**, no como evidencia de legibilidad física.

Cada PNG se etiqueta con la cobertura que el propio arnés declara
(`src/coverage.c`), no con una suposición: **REAL** (fuente real compilada),
**PARTIAL** (componente real, datos sintéticos), **SYNTHETIC** (doble explícito),
**UNSUPPORTED** (no se renderiza).

---

## 1. Entrada: la puerta estaba abierta y el SHA congelado

```sh
git fetch origin main && git rev-parse origin/main
# 1845cb77d36b93c4d8cd774fd130735f4d2f6f30
git rev-parse HEAD
# 1845cb77d36b93c4d8cd774fd130735f4d2f6f30
```

Los tres PRs de implementación están en `main` (`git log --oneline -4`):
`1845cb77` (#288), `e1807671` (#286), `107c1536` (#285). El binario del arnés se
construyó con `-DHARNESS_REVISION=1845cb77…` y **el JSON de las 243 capturas
declara esa revisión** (`manifest.csv`, columna `sha`: una sola revisión).

---

## 2. Herramientas (reales, no simuladas)

| Herramienta | Rol |
|---|---|
| `tools/ui_harness` (LVGL 8.3.11 @ `74d0a816`, PNG 800×480 headless) | la captura y el árbol LVGL vivo (`--inspect`) |
| `ui_harness_dialogs` | las 3 ventanas reales que el protocolo remoto no abre (freq, msg CW, msg voz) |
| `src/coverage.c` (en el JSON) | REAL/PARTIAL/SYNTHETIC/UNSUPPORTED por captura |
| `tests/check_wcag_rendered.py` | gate de contraste sobre la superficie renderizada (24 fixtures × 6 temas) |
| `tools/ab_testing` (`run_contracts`) | las **mismas** reglas del job `contracts` del CI, corridas sobre esta matriz |
| `medir.py`, `legibilidad.py`, `sangrado.py`, `comparar.py`, `bench.py` | geometría, tinta por widget, sangrado, A/B y costo |
| baseline `c90453fd` | árbol aparte, mismo corpus, para comparar antes/después |

Build reproducible (`construir.sh`, log `logs/build-construir.log`):

```sh
cmake -S tools/ui_harness -B <scratch>/build \
      -DHARNESS_REVISION=1845cb77d36b93c4d8cd774fd130735f4d2f6f30 \
      -DCMAKE_BUILD_TYPE=Release && cmake --build <scratch>/build -j 8
```

---

## 3. Matriz de cobertura (243 capturas)

| Grupo | Capturas | REAL | PARTIAL | SYNTHETIC | UNSUPPORTED |
|---|---|---|---|---|---|
| **home** (main screen, reloj, batería, BT/WiFi, espectro, TX) | 64 | `main_screen.c`, `theme.c`, `styles.c`, `fonts/`, `info.c`, `clock.c`, `meter.c`, `tx_info.c`, `band_plan.c`, `buttons.cpp`, `pannel.c`, `msg.c`, `adaptive_tuning.c`, `cfg/subjects.cpp`, `cfg/settings_manager.cpp` | `spectrum.c`, `waterfall.c` (magnitudes del generador, **no** de un FFT/radio), `band_info.c` (catálogo del arnés), `cw_tune_ui.c` | `cfg` (catálogo), `params`, `radio`; **BT/WiFi inyectados** por `bt_state`/`wifi_status` | `cluster_column.c`, `spot_marks.c`, diálogos |
| **diálogos** (Ajustes, BT, CW tune, DFL, SWRScan, autotune, freq/msg) | 154 | `dialog.c` + `dialog_settings.c` + `dialog_bt.c` reales; `dialog_freq.c`, `dialog_msg_cw.c`, `dialog_msg_voice.c` reales (binario `ui_harness_dialogs`); `swrscan`/`dfl`/`cw_tune` (panel) reales | datos de SWR/DFL/autotune desde fixtures deterministas | `params`, `radio`, `cfg` | los otros **22** diálogos de app son descriptores **inertes** del arnés (`tests/check_dialog_coverage.py`): la captura mide la **ruta**, no la ventana |
| **apps-menu** (menú de apps, app-bands, cierre) | 9 | `buttons.cpp` + dispatcher reales | — | `params` | contenido de cada app |
| **otros** (foco, feedback bloqueado, bench) | 16 | `main_screen.c`, `msg.c` | — | `radio` | — |
| **QEMU / apps 25×3** | **0** | — | — | — | **BLOCKED** (§9) |

Estados obligatorios del contrato, todos capturados:

| Estado pedido | Capturas | Cobertura |
|---|---|---|
| **BT OFF / ON / CONNECTED** | `gaps-net-bt-off`, `gaps-net-bt-on`, `gaps-net-bt-connected` (+ combinados `gaps-net-bt-wifi-*`) | **SYNTHETIC** (doble de `bt_state`, `src/info.c` real) |
| **WiFi OFF / ON / CONNECTED** | `gaps-net-wifi-off`, `gaps-net-wifi-on`, `gaps-net-wifi-connected` (+ combinados) | **SYNTHETIC** |
| **Reloj idle / TX, 44 px** | `main-tema-{6}`, `main-tema-{6}-tx`, `finalqa-reloj-tx` | **REAL** (`clock.c`) |
| **DSEG 44** (dseg7 / dseg7mini / dseg14 × 6 temas + TX) | `finalqa-reloj-{dseg7,dseg7mini,dseg14}-tema-{6}`, `finalqa-reloj-dseg7-tx`, `main-frecuencia-dseg` | **REAL** |
| **Batería `--` / 0 / 100 / cargando / tensión externa / sin externa** | `finalqa-bateria-desconocida`, `-0`, `-100`, `-cargando`, `-sin-externa`, `home-bateria-externa`, `main-bateria-baja` | **REAL** (`clock.c`; el valor lo inyecta el escenario, el camino es el de `radio.c`) |
| **Presets DFL** | 62 capturas `dfl-presets-*`, `dfl-editor-*`, `dfl-usb-d*`, `dfl-page-*` | **REAL** (panel inline) |
| **Tabs de Ajustes** | `settings-{audio,bands,perfil,home,wfbat,grupo-*}`, `settings-tema-{6}` | **REAL** |
| **CW** | `cw-tune-*`, `cw-autotune-*`, `main-frecuencia-dseg` (ghost), app CW (QEMU: BLOCKED) | **REAL** / PARTIAL |
| **Menús y apps** | `apps-menu-1..6`, `app-bands`, `app-close-home`, `swrscan-*` | **REAL** (menú) / **UNSUPPORTED** (contenido de app) |

**Field Ops dice X6200**: `src/dialog_field.c:1842` → `lv_label_set_text(x, "X6200")`
y **0 literales visibles con `X6100`** (`verificar_copy.py`, §6). El ADIF
(`src/field/flog.c`, `PROGRAMID X6100-IU1WXY`) **no cambia** (no aparece en el
diff). La captura renderizada de la ventana Field Ops es **QEMU/hardware** →
`BLOCKED` (§9): la verificación de acá es de fuente + estructura, no de píxeles.

**Propagation / SWRScan en inglés**: `verificar_copy.py` → **0 literales con
marcadores de español** en las 3 unidades; el "esqueleto" del fuente (todo el
código con los literales reemplazados por `<STR>`) es **idéntico** al de
`c90453fd` salvo `char buf[32] → buf[48]` (declarado por #288 para que
`Thresholds:` no trunque) ⇒ **sólo cambió el texto**, ni datos de backend ni
acciones de radio. El panel SWRScan inline sí se renderiza y muestra
`Run`/`Stop`, `Scale: Linear`, `Span:`, `Passes:` (inglés).

---

## 4. Inspección estructural (árbol LVGL vivo, 243 capturas)

`medir.py` recorre 15.952 etiquetas; 9.053 tienen texto real y 7.598 son visibles.

| Medición | Resultado |
|---|---|
| `text_overflow` en etiquetas **visibles con texto real** | **0 / 7.598** |
| `text_truncated` en etiquetas visibles | **0 / 7.598** |
| **glifos faltantes** (`missing_glyphs`, placeholder en toda la cadena) | **0** en las 243 capturas |
| glifos resueltos en **fallback** (otra fuente de la cadena) | 713 etiquetas, **7 code points, todos de icono** (`U+F1EB` WiFi ×243, `U+F240..F244` batería, `U+F1E6` plug, `U+E55B` ×3): el fallback es el comportamiento esperado y documentado del arnés (las familias alternativas son ASCII y la familia stock lleva los iconos) |
| etiquetas visibles recortadas por un ancestro | 15, **todas** filas de la grilla de Ajustes en el borde del área scrolleable (contenido alcanzable con scroll ⇒ no es `layout_clipped` según la regla del propio proyecto, `tools/ab_testing/src/x6200_ab/layout_checks.py`) |
| pares de cajas que se intersectan (contador crudo) | 884, **884/884 explicados**: 354 del panel **DFL mockeado** del arnés (se dibuja en toda pantalla), 463 de las mismas etiquetas del mock (`LOW`/`HIGH`/`BW`/`0 Hz`/`3000 Hz`), 61 de la fila de frecuencia (el mock antepone el color al texto), **6 del "8 fantasma"** (UI-017, capa de diseño con waiver por fixture en `contracts.yaml`) |
| solapamiento de cajas **icono de batería vs porcentaje** | **0 / 243** (la colisión P1 de #269 está resuelta) |
| fuente del reloj | `sony_44` ×185, `dseg14_44` ×44, `dseg7_44` ×8, `dseg7mini_44` ×6 → **siempre 44 px**, las tres familias segmentadas incluidas |
| barra de info (celdas 56×26, pitch 26) y panel del reloj | sin recorte; `tests/check_info_bar.py` = 0 |

El veredicto estructural autoritativo es el del **propio proyecto**
(`contratos.py`, §6), no mi contador crudo: `layout_clipped`/`layout_overflow`/
`layout_off_target` no reportan nada nuevo.

---

## 5. Gate de contraste renderizado

```
python3 tests/check_wcag_rendered.py
  fixtures: 24 (matriz de 6 temas x 4 estados)
  construyendo/actualizando el arnes (revision 1845cb77d36b93c4d8cd774fd130735f4d2f6f30)...
  etiquetas medidas: 494 (exentas 0, incidentales 2, no-texto 48)
  declaradas, no medidas: ocluidas 128, sin tinta 2, sin color expuesto (lv_line/lv_arc) 96
  violaciones: 0
  solo violaciones preexistentes del baseline (0 entradas): OK
  PASS  → EXIT=0
```

* **No es un falso verde por build viejo**: el gate compila por default y su log
  dice que construyó con la revisión del candidato. El binario que usé para el
  resto de la matriz se construyó aparte, también con el SHA cableado, y el JSON
  lo declara.
* **`tests/wcag_rendered_baseline.json` = `"entries": []`** (0 entradas): las 18
  históricas de #277 y las 7 de #285/#286 están realmente resueltas, no
  silenciadas. **No se cambió ningún umbral** (4,5:1 / 3:1 / 24 px) ni la lista de
  exenciones.
* **0 sangrado** por la superficie de diálogo (796×348): `sangrado.py` renderiza
  la misma fixture con `seed 20` y `seed 21` (cambia el espectro/waterfall, no el
  diálogo) en **14 diálogos REALES** (Ajustes y Bluetooth × 6 temas + mensajes CW
  y voz) → **0 px** de diferencia adentro de la ventana, `fuga_mediana = 0.0`,
  `amplitud_max = 0.0` (`mediciones/sangrado.csv`). La superficie medida es
  **exactamente el rol `dialog` del tema** en 9/14 (en los 6 de Ajustes y en los
  de mensajes); el diálogo de Bluetooth pinta su **propio** bitmap
  (`dialog_bt.c`, fuera del alcance de #286) y también es opaco (0 sangrado).
  GPS / Status / WiFi **no se pueden renderizar en el host** (descriptores
  inertes) y QEMU está `BLOCKED` (§9): para esos tres, la prueba es estructural
  (comparten `dialog_style`) y la de píxeles queda **PENDING**.
* **Texto legible en Sunlit y todos los temas**: el gate mide `lv_label`s y da 0
  violaciones, pero **no mide** el texto que dibujan los widgets
  (`lv_dropdown`/`lv_textarea`/`lv_roller`). Ese hueco se cubre con
  `legibilidad.py`, y ahí está el `P1` (§7).

---

## 6. Comandos y exit codes (matriz completa en `comandos.csv`)

| Comando | Exit | Resultado |
|---|---|---|
| `sh docs/…/construir.sh` | **0** | 2 binarios, revisión del JSON = candidato |
| `sh docs/…/capturar.sh` | **0** | **243 capturas, 243 exit 0**, todas 800×480 |
| captura 2ª vez (determinismo) | **0** | **243/243 PNG con el mismo sha256** |
| `python3 tests/check_wcag_rendered.py` | **0** | 24×6, **0 violaciones**, baseline 0 |
| `python3 tests/check_info_bar.py` | **0** | 6 celdas, pad 2, pitch 26, sin solape |
| `python3 tests/check_settings_grid.py` | **0** | grilla con origen único `x=499` |
| `sh tests/run_all_tests.sh --quick` | **0** | **22 OK, 0 fallos** (SKIP: `qemu_apps`, `wspr`, `sstv`, `wefax`, `js8`, `webserver`) |
| `sh tests/native/run_native_tests.sh` | **0** | 597 + 57 comprobaciones, 0 fallos |
| `uv run pytest -m "not e2e"` (CI) | **0** | **237 passed, 13 deselected** |
| `uv run ruff check .` / `ruff format --check .` / `ty check` (CI) | **0** | limpio |
| `sh tests/check_harness_determinism.sh` | **0** | PASS, PNG idénticos en todo el corpus |
| `sh tests/check_harness_natural.sh` | **0** | PASS, 40 frames, 11.208.000 px encendido / 0 px apagado |
| `sh tests/check_harness_wakeups.sh` | **0** | PASS, 0 wakeups de tick (LV_TICK_CUSTOM), 31 del loop en 3 s |
| `tests/qemu/run_apps_qemu.sh --check` | **77** | **SKIP/BLOCKED**: faltan volumen privado y chroot (§9) |
| `python3 docs/…/contratos.py --solo-corpus` | **1** | 8 fallas: **6 preexistentes** (idénticas al baseline) + **2 nuevas** de calibración (§7 P2) |
| `python3 docs/…/legibilidad.py` | **1** | **7 hallazgos, todos Sunlit** (§7 P1) |
| `python3 docs/…/sangrado.py` | **0** | 0 px de sangrado en 14 diálogos reales |
| `python3 docs/…/verificar_copy.py` | **0** | PASS (sólo copy, backend intacto, X6200, inglés) |
| `python3 docs/…/medir.py`, `comparar.py`, `bench.py`, `manifiesto.py`, `crops.py` | **0** | mediciones y comparaciones |

Un `SKIP` no es un `PASS`: `qemu_apps` no corrió.

### Comparación baseline `c90453fd` → candidato (218 fixtures comunes)

| Métrica | Resultado |
|---|---|
| PNG que cambian | **218/218** (el reloj de 44 px y la fila de estado están en toda pantalla, y la superficie del diálogo cambió) |
| `flushes` por captura | **delta 0 en las 218** |
| `flushed_pixels` por captura | **+111 … +429 px** (p50 **+272**, ≈ **+0,025 %** de ~1,1 M): el reloj más alto pinta más |
| `redrawn_area_px` en `--bench` | **idéntico** en las 14 fixtures medidas |
| contratos: nuevos | 19 (los 19 son mis capturas `finalqa-reloj-*`: **capa "8 fantasma" de UI-017**, waiver por fixture en `contracts.yaml` para las fixtures del corpus; mismo árbol, misma caja) |
| contratos: regresiones | **2** (`region_min_content:frecuencia` en los 2 diálogos de mensajes, §7 P2) |
| contratos: resueltos | 0 |
| contratos: persisten | 6 (idénticos al baseline, §7 P3) |

### Costo de render (`--bench`, warm-up 3, 7 repeticiones, host x86)

| Grupo | render p50 base → actual | Δ |
|---|---|---|
| Home (7 fixtures) | 1,29–1,37 ms | **−3,7 % … +4,8 %** (ruido) |
| Ajustes / BT / SWRScan (7 fixtures) | 2,75–2,89 → 3,54–3,73 ms | **+27,4 % … +32,8 %** |

Causa **medida, no supuesta**: la pasada de **recoloreo** que #286 agrega a
`dialog_style` (`bg_img_recolor` + `bg_img_recolor_opa`). Los proxies
deterministas (`flushes`, `redrawn_area_px`) **no cambian**. Es un proxy de host
x86, **no** el costo del Cortex-A7: la pasada de hardware queda `PENDING` (§9).
No se suprimió ninguna métrica.

---

## 7. Hallazgos

### `P0` — ninguno

### `P1` — Regresión material: en **Sunlit** los valores de los controles de Ajustes son invisibles

**Qué se midió** (no es una impresión):

* `legibilidad.py` recorre los 238 widgets de texto dentro de la ventana modal y
  mide, para cada uno, los píxeles del **interior de su caja** que no son el
  color de la superficie. **7 widgets quedan con 0 píxeles de tinta**, y los 7
  son de Sunlit:
  * `settings-tema-sunlit`: los 4 `lv_dropdown` de **Font**, **Visual profile
    (Eco)**, **GEN long press** y **APP long press** (cajas `499,141`, `499,200`,
    `499,274`, `499,333`, 285×56);
  * `settings-grupo-operation-tema-sunlit`: el campo **Accel threshold** (`'07'`),
    el botón **`Pair / connect...`** y un dropdown.
* El **mismo widget, misma caja** tiene tinta en los otros temas (16 colores en la
  caja; 359–845 px de tinta). En Sunlit: 1 color, 0 px de tinta.
* **No es preexistente**: la misma medición sobre el baseline `c90453fd` da
  **0 hallazgos** (`legibilidad.py --base <baseline>`). En el baseline la
  superficie del diálogo era el bitmap **oscuro** y el texto blanco se leía; en el
  candidato la superficie es `TC(dialog)` = **`#FFFFFF`** en Sunlit y el texto
  sigue siendo blanco.
* **Causa (código, no hipótesis)**: `src/styles.c:266`
  `lv_style_set_text_color(&dialog_item_style, lv_color_white())` — la tinta de
  los widgets de `dialog_item` sigue **fija en blanco**. #286 cambió la tinta del
  *label* (`dialog_style` → `TC(text)`) pero **no** la del *widget*. En los otros
  5 temas `TC(dialog)` es oscuro y el blanco funciona; en Sunlit no.
* **Por qué el gate no lo vio**: `check_wcag_rendered.py` mide `lv_label`s; el
  texto de un `lv_dropdown`/`lv_textarea` lo dibuja el widget y **no aparece como
  etiqueta** en el árbol. El gate reporta "sin tinta 2" y lo **declara**, no lo
  falla (es el mismo agujero que #277 cerró para los labels, abierto para los
  widgets).

**Evidencia**: `mediciones/legibilidad.csv`, `crops/sunlit-valores-dropdown-x4.png`
(base vs actual), `crops/settings-valores-seis-temas-x4.png` (el mismo recorte en
los 6 temas), `crops/sunlit-operation-valores-x4.png`, y el PNG a 1:1
`png/settings-tema-sunlit.png` (las celdas de la derecha están vacías).

**Severidad `P1`**: el operador no puede leer el valor actual de 4 ajustes en el
tema claro (el que se usa para leer al sol). El control sigue funcionando y el
tema oscuro no está afectado, por eso no es `P0`.

**Responsable**: **#286** (cambió la superficie sin actualizar la tinta de
`dialog_item_style`). **Esta corrida no lo arregla** (evidence-only). Con la
regla dura del issue, esto es **NO-GO**.

### `P2` — `region_min_content:frecuencia` en los 2 diálogos de mensajes (calibración de gate, no defecto visible)

* `contratos.py` reporta `gaps-dialog-msg-cw` y `gaps-dialog-msg-voice`:
  `content 0.0050 >= 0.0200`. **Medido**: la región `frecuencia` (0,160 800×36)
  pasa de **719 colores / 5,5 % modal** en el baseline a **2 colores / 99,5 %
  modal** en el candidato — es exactamente el efecto buscado por #286 (la ventana
  ya no deja ver el Home). La regla mide la región **entera** y no sabe que una
  modal opaca la tapa; `contracts.yaml` ya tiene `visible_under_modal` para el
  waterfall y no para `frecuencia`.
* **No es visible para el usuario** y las fixtures **no están en el suite del CI**
  (`fixtures-gaps/` no entra en las 205 del pipeline), por eso el CI está verde.
* **Responsable**: #286 / dueño de `contracts.yaml`. Sugerencia (no aplicada):
  extender `visible_under_modal` a `frecuencia` para modales que la cubren por
  completo, o mover estas 2 fixtures al suite.

### `P2` — Costo de render de las pantallas con diálogo +27 % … +33 % (proxy host)

* Medido con `--bench` (§6). Causa: la pasada de recoloreo de #286. Proxies
  deterministas idénticos. **No es una regresión de comportamiento** (no cambia
  pixels ni invalidaciones), pero en el Cortex-A7 el costo relativo puede ser
  mayor: **PENDING** hasta la pasada de hardware. Ya estaba documentado por #286;
  acá se **re-mide** sobre el candidato y no se suprime.

### `P3` — Filas de Ajustes cortadas en el borde del área scrolleable (preexistente)

15 etiquetas visibles con la caja recortada en el borde inferior de la grilla
(p. ej. `Voice rate, pitch, volume`, `Charger`, y una fila de 1 px en
`settings-wfbat`). Es contenido **alcanzable con scroll** en un contenedor que
declara `scroll_dir: ver`, así que la regla del proyecto (`layout_checks.py`) **no
lo considera un defecto**. Igual en el baseline.

### `P3` — Fallas preexistentes de las reglas A/B, idénticas al baseline

`gaps-dialog-freq` (`lv_textarea` 250×35 < objetivo táctil 44 px),
`gaps-dialog-msg-cw` (`'CW'` 3,89:1), y los 4 `gaps-net-*` donde el **icono de
WiFi apagado** mide 1,20:1 (`icon_off` sobre `bar`). Las tres son **idénticas en
el baseline** y `icon_off` es un rol **exento por WCAG 1.4.3** en
`tests/check_wcag.py`/`theme_palette.yaml`; la regla `typography_contrast` del A/B
no aplica esa exención. No son regresiones de esta épica.

### `P3` — El diálogo de Bluetooth no usa la superficie `TC(dialog)`

Su superficie medida es propia (`#1B1529` en Aurora vs `#221A33` del rol), porque
`dialog_bt.c` pinta su bitmap. Es **opaco** (0 sangrado) y el gate lo mide verde
(7,5:1–15,7:1). Fuera del alcance de #286, se declara.

---

## 8. Lo que **no** se pudo medir (y por qué)

| Cosa | Estado | Causa exacta |
|---|---|---|
| Apps reales en QEMU (25 × 3 rondas) | **BLOCKED** | `run_apps_qemu.sh --check` → exit **77**: falta el **volumen privado** con `target/usr/sbin/x6200_gui` y el **chroot** privado. Los volúmenes docker existentes (`x6200-build-output`, `x6200-next222-build`, `x6200-build-cache`) están **vacíos** (0 bytes), y construirlos requiere un Buildroot completo (`CONSTRUIR=si BUILDROOT=…`, "horas"). Sin binario ARM **de este SHA** no hay captura honesta: usar uno de otra corrida mediría otro código. |
| Sangrado de GPS / Status / WiFi | **PENDING** | sus diálogos son descriptores **inertes** en el arnés de host; la prueba de píxeles requiere QEMU (§ arriba). Prueba estructural: comparten `dialog_style`. |
| Captura renderizada de Field Ops / Propagation / SWRScan (diálogos) | **PENDING** | ídem QEMU. Verificación de fuente + estructura: `verificar_copy.py` = PASS. |
| RF, audio CW real, S-meter real, waterfall con RF, DFL con la perilla | **PENDING** | hardware (§10). Nada de TX/RF en esta corrida. |
| Legibilidad física al sol, 4" | **PENDING** | no hay área activa del panel en mm en el repo; cualquier tamaño físico sería inventado. |

---

## 9. Handoff de QA físico (todo `PENDING` hasta que haya evidencia en la radio)

**Candidato**: `1845cb77d36b93c4d8cd774fd130735f4d2f6f30`.
**Checksum de los artefactos de esta evidencia**: ver `manifest.csv` (sha256 por
PNG) y `logs/build-construir.log` (binario del arnés). El ROM a flashear debe
compilarse desde ese mismo SHA y su identidad verificarse con
`tests/check_build_identity.py` (gate que ya pasa en esta corrida).

Chequeos concretos para el agente de QA en la radio — **ninguno ejecutado acá**:

1. **Legibilidad al sol (4")**: home y Ajustes en **Sunlit** y **Redshift** a
   pleno sol. En particular, confirmar en la radio el `P1` de §7 (los valores de
   Font / Visual profile / GEN long press / APP long press) — si en el ROM se ve
   igual que acá, el hallazgo se confirma en hardware.
2. **Reloj y batería del home**: hora permanente `HH:MM:SS` a 44 px en los 6
   temas; `--%` sin muestra, `0 %`, `100 %`, cargando (`⚡`), 13,8 V externa y sin
   externa; el porcentaje **no** debe pisar el icono (en el arnés: 0 solapes).
3. **BT real**: OFF / ON / CONECTADO (auriculares A2DP) — la runa y el color.
4. **WiFi real**: OFF / conectando / CONECTADO — el icono y el estado.
5. **Waterfall** con RF real: que dibuje, que no se congele y que no invada la
   fila de frecuencia.
6. **Audio CW**: side tone, keyer, volumen; sin TX no hay RF.
7. **S-meter** con señal real (y en TX).
8. **DFL con la perilla**: editar LOW/HIGH/BW, presets, guardar/asignar, y que el
   encoder no pierda pasos.
9. **Performance**: fluidez del espectro/waterfall, tiempo de apertura de apps,
   y **costo de las pantallas con diálogo** (el `+27…33 %` del proxy de host).
10. **Apagado / encendido**: persistencia de tema, fuente, DFL y ajustes; sin
    crashlog nuevo; arranque al home.
11. **Field Ops / Propagation / SWRScan**: header **X6200** y mensajes en inglés
    en la pantalla real; el ADIF sigue declarando `X6100-IU1WXY` (dato de
    protocolo, **no** UI).

---

## 10. Recomendación

**NO-GO** para aprobar el release del ROM.

* Fundamento: el `P1` de §7 es una **regresión material** (información invisible
  en un tema soportado), medida contra el baseline, con causa en el código y con
  el gate de contraste **sin cubrirla**. La regla dura del issue #287 manda
  devolverla al issue responsable (**#286**) con la evidencia y **no** arreglarla
  acá.
* Lo demás está verde: 243 capturas deterministas, gate de contraste 0
  violaciones con baseline 0, 0 sangrado en los 14 diálogos reales, 0 glifos
  faltantes/recortes en texto visible, reloj de 44 px (incluidas las 3 DSEG),
  batería sin colisión, copy de #288 correcto y sin tocar backend, y todos los
  comandos del CI en 0.
* Aun resolviendo el `P1`, el release **no** se aprueba sin el **QA físico**
  (§9): QEMU y hardware quedan `BLOCKED`/`PENDING`.

Camino sugerido: (1) #286 corrige la tinta de `dialog_item_style` (o la
superficie) para Sunlit; (2) re-correr esta misma matriz y `legibilidad.py`
(esperado: 0 hallazgos); (3) resolver/decidir el `P2` de calibración de
`region_min_content`; (4) QA físico con el SHA resultante.

---

## 11. Archivos de esta evidencia

```text
docs/evidencia/2026-10-23-ui-polish/final-qa/
  README.md                 este documento
  manifest.csv              243 capturas (cobertura, sha, comando, exit, sha256)
  comandos.csv              matriz de comandos con exit code real
  construir.sh              build out-of-source con el SHA cableado
  capturar.sh               la matriz completa (corpus + gaps + estados + DSEG)
  medir.py                  geometría/glifos/recortes/solapes del árbol --inspect
  legibilidad.py            tinta real por widget dentro de la modal (hueco del gate)
  sangrado.py               test negativo de sangrado (2 semillas) + superficie
  contratos.py              reglas del CI (tools/ab_testing) sobre esta matriz
  comparar.py               baseline c90453fd vs candidato
  bench.py                  proxy de costo de render (--bench, 2 builds)
  verificar_copy.py         #288: sólo copy, backend intacto, X6200, inglés
  crops.py                  recortes 4× (ayuda de lectura)
  manifiesto.py             genera manifest.csv
  scenarios/                6 fixtures de estado (batería --/0/100/cargando/sin
                            externa y reloj en TX) — dentro de la evidencia
  png/                      243 PNG 800×480 a 1:1
  json/                     243 JSON del arnés (cobertura + inspection)
  logs/                     salida de cada comando (con EXIT=) + capturas.tsv
  crops/                    7 recortes 4×
  mediciones/
    elementos.csv           una fila por etiqueta (caja, natural, recorte, glifos)
    hallazgos.csv           contadores por captura
    solapamientos.csv       pares de cajas que se intersectan
    alineacion.csv          hechos de alineación (barra, reloj, batería)
    cobertura.csv           cobertura declarada por captura y componente
    resumen.csv             agregado por grupo
    legibilidad.csv         tinta por widget de la modal (hallazgo P1)
    sangrado.csv            sangrado y superficie por diálogo y tema
    contratos.csv/.json     findings de las reglas del CI (243 capturas)
    contratos-corpus.csv    lo mismo, sólo las fixtures del repo (218) → 8 fallas
    comparacion-*.csv       PNG / métricas / contratos / estructura / geometría
    bench.csv               render/draw/flush p50 base vs actual
    bench/{base,actual}/*.json  bloques `benchmark` crudos de los dos builds (los PNG
                            de esas corridas son idénticos a `png/`: no se duplican)
```

### Reproducir desde cero

```sh
WT=<worktree>; SCRATCH=/tmp/opencode/ui-polish-qa-284
sh $WT/docs/evidencia/2026-10-23-ui-polish/final-qa/construir.sh "$WT" "$SCRATCH"
sh $WT/docs/evidencia/2026-10-23-ui-polish/final-qa/capturar.sh  "$WT" "$SCRATCH"
cd $WT/docs/evidencia/2026-10-23-ui-polish/final-qa
python3 medir.py && python3 legibilidad.py && python3 sangrado.py
python3 contratos.py && python3 comparar.py --base <baseline> && python3 bench.py
python3 crops.py && python3 manifiesto.py
```

El baseline (`c90453fd`) se arma igual pero en un árbol aparte:

```sh
mkdir -p /tmp/opencode/qa284-base && git -C "$WT" archive c90453fd | tar -x -C /tmp/opencode/qa284-base
cp -a "$WT/lvgl" /tmp/opencode/qa284-base/lvgl
# construir.sh + SOLO_CORPUS=1 capturar.sh sobre ese árbol (218 fixtures)
```

---

## 12. Límites de esta evidencia

1. Es un render **de host** del LVGL real a 800×480 con radio/FFT/parámetros
   mockeados: **no** prueba comportamiento del firmware en la radio ni la
   legibilidad sobre el LCD.
2. Espectro y waterfall son `PARTIAL`: el widget es real, las magnitudes no.
3. BT/WiFi son `SYNTHETIC`: mide layout y color del icono con el doble, **no** la
   función real.
4. Los 22 diálogos de app son inertes en el host: de ellos se mide la **ruta**,
   no la ventana (QEMU `BLOCKED`).
5. El costo de render es un **proxy de host x86**, no el Cortex-A7.
6. Nada de acá autoriza el release: el `P1` es NO-GO y el QA físico está
   `PENDING`.
