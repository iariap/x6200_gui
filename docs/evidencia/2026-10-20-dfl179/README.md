# #179 — Editor DFL inline (corrección del #173)

Corrida `factory/dfl-inline-179`. **Base**: `9d72bde1` (trunk tip,
`lu2dsx/x6200-rom`). **HEAD de la corrida**: `123b9e7a`. Un solo commit de
código+arnés+evidencia. `docs/evidencia/2026-10-20-dfl179/`.

> **Validación física: FUERA de esta corrida.** El owner hace la prueba en su
> radio después del merge (decisión vinculante del 20/10). El agente NO la
> exige ni la declara: **queda pendiente prueba física posterior del owner**.

## 0. Qué se corrigió y por qué

El #173 (PR #176/#177) implementó el editor de filtro DFL como una **ventana
modal** (`dialog_construct`), con título, marco, sombra, botón X y botones
+/−; el owner reportó que parecía «una ventana del SO» y que los valores no se
podían cambiar. El #177 además había enganchado la **rueda principal (rotary0)**
al filtro vía `dialog_rotary()`, y el hit-test de los handles
(`dfl_nearest_handle()`) **siempre** devolvía un handle, aun tocando lejos.

Esta corrida **reemplaza el modal por un panel NATIVO inline** dentro del
rectángulo del waterfall, y la edición pasa a ser **solo con el encoder de
VOLUMEN (rotary1, arriba a la izquierda)** y con touch. La rueda principal
(rotary0) **sigue sintonizando** y nunca toca el filtro.

## 1. Ruteo del encoder VOL (rotary1) — cómo se edita sin tocar VFO ni volumen

El camino real del encoder de volumen es `src/main_screen.c::spectrum_key_cb()`
(LV_EVENT_KEY sobre el espectro), que recibe `KEY_VOL_LEFT/RIGHT_EDIT` y
`KEY_VOL_LEFT/RIGHT_SELECT`. El editor intercepta **solo** esas cuatro teclas
cuando está expandido, **antes** de `vol_update()` / `vol_change_mode()`:

```c
// src/main_screen.c::spectrum_key_cb()
case KEY_VOL_LEFT_EDIT:
case '[':
    if (key == KEY_VOL_LEFT_EDIT && dfl_inline_on_vol_key(KEY_VOL_LEFT_EDIT)) {
        break;                 // consumido por DFL: NO llega a vol_update()
    }
    vol_update(-1, false);
    break;
case KEY_VOL_RIGHT_EDIT:
case ']':
    if (key == KEY_VOL_RIGHT_EDIT && dfl_inline_on_vol_key(KEY_VOL_RIGHT_EDIT)) {
        break;
    }
    vol_update(+1, false);
    break;
case KEY_VOL_LEFT_SELECT:
case '{':
    if (key == KEY_VOL_LEFT_SELECT && dfl_inline_on_vol_key(KEY_VOL_LEFT_SELECT)) {
        break;
    }
    vol_change_mode(-1);
    break;
case KEY_VOL_RIGHT_SELECT:
case '}':
    if (key == KEY_VOL_RIGHT_SELECT && dfl_inline_on_vol_key(KEY_VOL_RIGHT_SELECT)) {
        break;
    }
    vol_change_mode(+1);
    break;
```

`dfl_inline_on_vol_key()` (`src/dfl_inline.c`) devuelve `true` **solo** con el
panel expandido y el evento consumido:

```c
bool dfl_inline_on_vol_key(uint32_t key) {
    if (!expanded) {
        return false;
    }
    switch (key) {
        case KEY_VOL_LEFT_EDIT:  if (!tx_active) { dfl_rotary_apply(sel, -1); dfl_refresh(); } return true;
        case KEY_VOL_RIGHT_EDIT: if (!tx_active) { dfl_rotary_apply(sel, +1); dfl_refresh(); } return true;
        case KEY_VOL_LEFT_SELECT:  if (!tx_active) { cycle_sel(-1); } return true;   // LOW/HIGH/BW
        case KEY_VOL_RIGHT_SELECT: if (!tx_active) { cycle_sel(+1); } return true;
        default: return false;
    }
}
```

- **EDIT** ajusta el parámetro seleccionado con los pasos vigentes
  (`dfl_rotary_apply`, espejo de `src/vol.c`: LOW 10 Hz, HIGH 50 Hz y 10 Hz en
  CW/CWR, BW 20 Hz). **No** se llama `vol_update()`: el volumen no cambia.
- **SELECT** recorre cíclicamente LOW → HIGH → BW, **omitiendo LOW** si el modo
  no lo permite (AM/SAM/NFM/WFM). No edita valores.
- `dfl_rotary_apply()` escribe por los **setters vigentes**
  (`cfg_mode_set_low_filter`/`cfg_mode_set_high_filter` y el Subject del BW con
  validación previa); **nunca** toca `cfg_cur.fg_freq`.
- Al **expandir** se guarda el estado del encoder VOL (`vol_get_mode()` y
  `vol->mode`) y se fuerza `ROT_VOL_EDIT_MODE`; al **colapsar** se **restaura**
  el modo y el control previos.

### Eliminación del enganche de rotary0

`src/main_screen.c::main_screen_rotary_enhanced_cb()` seguía llamando a
`dialog_rotary(enhanced->raw_diff)` (el enganche del #177). Esa llamada **se
conserva** (otras apps la usan), pero el editor DFL **ya no es un diálogo**, así
que `dialog_rotary()` no tiene callback de DFL y **no consume** el giro:
rotary0 sigue sintonizando (VFO o canal CB, respetando `freq_lock`) y nunca
modifica LOW/HIGH/BW. Test dedicado: `dfl-editor-rotary0` (§5).

## 2. Layout expandido (números MEDIDOS con `--inspect`)

El panel es **hermano del waterfall** (hijo de la pantalla), creado después de
la imagen para dibujarse encima; **no** es un diálogo y **no** altera el
invariante de la línea central (#140). Geometría derivada del objeto real (no
hardcodea 240):

```c
lv_area_t wa; lv_obj_get_coords(waterfall_widget, &wa);
lv_coord_t p_h = (wa.y2 - wa.y1 + 1) - 28 /* micro-waterfall */ - buttons_bar_height();
panel = lv_obj_create(lv_obj_get_parent(waterfall_widget));
lv_obj_set_pos(panel, wa.x1, wa.y1 + 28);
lv_obj_set_size(panel, wa.x2 - wa.x1 + 1, p_h);
```

Medido en `dfl-editor-usb-d` (`--inspect`, JSON en `capturas/`):

| Elemento | x | y | w | h |
|---|---|---|---|---|
| waterfall (objeto) | 0 | 240 | 800 | 240 |
| **micro-waterfall vivo** | 0 | 240 | 800 | **28** |
| **panel inline** | 0 | **268** | 800 | **150** |
| cabecera `DFL USB-D` | 8 | 284 | 320 | 15 |
| `Restablecer` (botón, target 128×44) | 367 | 272 | 128 | 44 |
| gráfico | 8 | 320 | 487 | 96 |
| fila LOW | 505 | 272 | 286 | 44 |
| fila HIGH | 505 | 320 | 286 | 44 |
| fila BW | 505 | 368 | 286 | 44 |
| barra inferior expandida | 0 | 418 | 800 | 62 |

- **Micro-waterfall obligatorio**: los 28 px superiores del waterfall **real**
  (las filas más recientes, que entran por arriba) siguen visibles; el panel
  opaco se dibuja **solo desde y_local=28**. **No** es un segundo buffer ni una
  copia: el waterfall sigue rasterizando debajo (`src/waterfall.c` intacto).
- **Sin** X, título de ventana, marco, sombra ni scroll. Fondo `TC(panel)`,
  separador de 1 px en x=499, filas derechas con `TC(sel_bg)`/`TC(sel_border)` +
  **barra lateral** + valor destacado (foco que no depende solo del color).
- Gráfico: línea de banda pasante de 2 px (`TC(accent)`), relleno `TC(vfo_line)`
  al 20 %, con headroom (meseta a 1/4 de la altura, no de borde a borde), línea
  base y **cero visible**, ticks 2 (min/max), handles visuales de 8 px, y la
  marca chica `Esquema`.

## 3. Escala adaptativa (C.3) y golden

`src/dfl_model.h`: dominios «agradables» **1000 / 2000 / 5000 / 10000 Hz**. Se
elige el menor que contenga ambos límites (SSB `[0, M]`; CW/CWR y simétricos
`[-M, +M]`). **GOLDEN**: USB-D 225..3525 → **0..5000 Hz**. Se fija al
expandir/cambiar de modo y solo **crece** con histéresis
(`dfl_domain_grow`, nunca reduce en cada detent). Los límites de firmware
(`MAX_FILTER_FREQ`) **no** son este dominio visual. Conversión Hz↔px reversible
(`dfl_graph_x`/`dfl_graph_hz`), testeada.

## 4. Hit-test de handles

`dfl_model.h::dfl_hit_test(x, low_x, high_x)`: devuelve un handle **solo** si
está a `DFL_HANDLE_MAX_DIST = 22` px o menos de su x (zona útil ≥44×44); si
ambos están lejos, devuelve `DFL_HANDLE_NONE` y **un toque lejano no modifica
nada**. Reemplaza a `dfl_nearest_handle()` (que siempre devolvía un handle).
Función **pura y testeada** (incluye toques al límite y fuera). El arrastre
mueve el extremo por los setters vigentes y **selecciona** su fila.

## 5. Tests (salidas reales)

### 5.1 Nativos (`tests/native/run_native_tests.sh`, host gcc)

- Modelo puro del editor: **130 comprobaciones, 0 fallos** (pasos, escala
  adaptativa + golden 225..3525→0..5000, geometría, **hit-test ≤22 px**, BW y
  reset). `gcc -fanalyzer`: sin observaciones.
- Motor de la rueda (`dfl_rotary`): **68 comprobaciones, 0 fallos** (pasos por
  modo, clamps, LOW no editable en AM/SAM/NFM/WFM, BW ilegal rechazado, valor
  desde el Subject). Log: `logs/native.log`.

### 5.2 Arnés de UI (VOL, rotary0, touch) — `scenario_settle` corta si no lo ve

`dfl-editor-interaction`: el fixture empuja **KEY_VOL_* al espectro enfocado**
(el camino real de `spectrum_key_cb`) y verifica:

- VOL EDIT sobre LOW (paso 10): 225 → 230 → 240.
- fila HIGH tocada → selecciona; VOL (paso 50): 3525 → 3550.
- fila BW tocada → selecciona; VOL (paso 20): 3300 → 3320.
- SELECT cicla BW → LOW; EDIT baja LOW 240 → 230.
- arrastre cerca del handle de HIGH (x=340) → 3500 y **selecciona HIGH**.
- toque **lejano** (x=479, hit-test > 22 px) → **no cambia nada**.
- **`cfg_cur.fg_freq` y el volumen (`radio_change_vol`) NO cambian** al editar
  el filtro con VOL.

`dfl-editor-rotary0` (escenario nuevo): gira **rotary0** por el evento real
(`EVENT_ROTARY_ENHANCED`) con el panel expandido y verifica que **sí sintoniza**
y **NO toca los filtros**, también con **`freq_lock`** y en **CB** (27.185 MHz).
`dfl-editor-tx`: durante TX el filtro RX no cambia. `dfl-editor-reset`:
Restablecer aplica solo LOW/HIGH (USB-D → lsb_dig: 50/2950). `dfl-editor-cycles`:
**30 ciclos** expandir/contraer por ESC / quinta celda "Waterfall" / cierre
directo, con cambios de modo y frecuencia, sin desfases.

### 5.3 Capturas 800×480 (todos los estados y los 6 temas)

`capturas/`: DFL cerrado (`banco/banco-home.png` en el banco; home del arnés en
las fixtures `main-*`), **página DFL normal** (`dfl-page-tema-*`), **expandido**
(`dfl-editor-usb-d-tema-*`), **LOW/HIGH/BW seleccionado**
(`dfl-editor-usb-d-tema-*` / `dfl-editor-sel-high-*` / `dfl-editor-sel-bw-*`),
**antes/después de girar VOL** (`dfl-editor-interaction`), **Restablecer**
(`dfl-editor-reset`), **colapsar** (`dfl-editor-cycles`). Los seis temas
(Aurora, Redshift, Sunlit, Sprint, Cathode, Trail). `scenario_settle` verifica
que **no hay popup/X/título/marco**, que el panel arranca 28 px debajo del
waterfall y que la barra está en la página expandida.

## 6. Gates

| Gate | Resultado | Evidencia |
|---|---|---|
| Nativos (host) | **OK** (40288 + 130 + 68, 0 fallos) | `logs/native.log` |
| `-fanalyzer` (modelo y rueda) | **OK** (sin observaciones) | `logs/native.log` |
| Estáticos (`check_*`, `audita-pruebas`) | **OK** | `logs/check_*.log`, `logs/audita.log` |
| Determinismo del arnés | **PASS** (PNG idénticos ×2) | `logs/determinism.log` |
| `x6200-ab themes` (WCAG 6 temas + no-text) | **OK** | `logs/ab-themes.log` |
| ruff / format / ty | **OK** | `logs/ruff.log`, `logs/rufffmt.log`, `logs/ty.log` |
| pytest (no e2e) | **227 passed** | `logs/pytest.log` |
| pytest e2e (TMPDIR en disco) | **13 passed** | `logs/pytest-e2e.log` |
| A/B `AB-DFL179` | **PASS** (Functional/Visual/Contract/Perf) | `logs/ab-compare.log`, `ab-DFL179/` |
| `x6200-ab audit` (estructural) | **0 hallazgos** (19399 objetos, 142 escenarios) | `logs/ab-audit.log` |
| ARM cross-compile + banco QEMU (#86) | **OK** (sin crash, sello #165 OK) | `logs/banco-out.txt`, `banco/` |

### 6.1 A/B `AB-DFL179` (declaración del diff)

```text
AB-DFL179: PASS (exit 0)
  Functional         PASS   122 scenarios captured on both revisions
  Visual Regression  PASS   no unexplained visual regressions
  Quality Contract   PASS   1274 contract checks passed on the candidate
  Performance        PASS   render-cost proxy within budget
  Hardware           BLOCKED (advisory)
  Energy             BLOCKED (advisory)
```

**Confinamiento**: las únicas fixtures con píxeles distintos son las
`dfl-editor-*` (la región `dfl_inline`: panel 800×150 en y=268 + barra
expandida y=418..479). Los 122 escenarios compartidos quedan **byte-idénticos**
fuera de esa región (header, S-meter, espectro, frecuencia, band plan y las
cinco teclas). El candidato agrega fixtures nuevas (`dfl-page-tema-*`,
`dfl-editor-sel-*`, `dfl-editor-rotary0`, `dfl-editor-reset`) que el A/B
captura solo en el candidato.

### 6.2 ARM cross-compile + banco QEMU (gate #86)

```text
version: 123b9e7a DFL inline (issue #179): panel nativo sobre el wate
fuente:  md5 53880eb1851f4cca315f432846d6a8df   (sello .fuente.md5, #165)
binario: md5 4610cd43a2bf   qemu-cpu: cortex-a7
boot_identity: fuente: 53880eb1851f4cca315f432846d6a8df  -> COINCIDE con el sello
gate fail-closed #86: OK (ELF32 ARM, 13875308 B, 3/3 marcas)
crash.txt: NO
```

Capturas del binario ARM real bajo `qemu-arm-static` (`banco/`), abriendo el
editor por el **control remoto** (`{"cmd":"app","text":"DFLEditor"}`, el camino
del webserver):

| Captura | Modo | Estado |
|---|---|---|
| `banco-home.png` | USB-D | DFL cerrado (waterfall íntegro) |
| `banco-usbd-aurora.png` | USB-D | editor inline expandido |
| `banco-lsbd-editor.png` | LSB-D | editor inline (espejo) |

El tema del banco es el de la radio (Sunlit por defecto); el remote no expone
cambio de tema, así que la matriz de seis temas se cubre con el arnés (§5.3).

## 7. Lifecycle y no-regresión

- **Solo el hilo de GUI toca LVGL**: `dfl_inline_expand/collapse/on_vol_key` y
  los callbacks de eventos corren en el hilo de GUI; los observers son
  **diferidos** (`subject_add_delayed_observer`) y se **desregistran** en
  `dfl_inline_shutdown()`; el refresh solo corre con el panel expandido.
- **Sin punteros colgados**: el panel es hijo de la pantalla (se borra con
  ella); `dfl_inline_init()` es idempotente y resetea punteros.
- **No toca la gestión global de diálogos** (`src/dialog.c/.h` intacto) ni roba
  inputs a otras apps; `apps_stop()` contrae el DFL al cambiar de app/página.
- **`ACTION_APP_DFL_EDITOR` conserva su ID persistido** (sigue siendo 131,
  `params.long_dfl` intacto) aunque ya no sea app ni diálogo. La **pulsación
  corta DFL** mantiene su comportamiento y **contrae** si está expandido; la
  **larga** alterna solo si `params.long_dfl == ACTION_APP_DFL_EDITOR`.
- **Invariantes respetadas**: no se tocó RF/TX/DSP, MCU, FFT, CAT, audio, notch
  DFN, bandas ni apps ajenas; no se migró LVGL; se mantienen las **cinco teclas**
  y el overlay RX del PR #174 (`spectrum_filter_band` + ticks del waterfall).

## 8. Qué se cambió

- `src/dfl_inline.c/.h` (**nuevos**): panel inline.
- `src/dialog_dfl.c/.h`: **eliminados**.
- `src/dfl_model.h`: escala adaptativa (`dfl_domain_fit/grow`), `dfl_hit_test`.
- `src/main_screen.c`: ruteo de VOL, ESC/DFL corto contraen, `dfl_inline_init`,
  TX/RX, `apps_stop` contrae, sin `dialog_construct` para DFL.
- `src/buttons.cpp/.h`: página expandida (LOW/HIGH/BW sin cifras + Eq +
  Waterfall), `buttons_bar_height()`, `buttons_dfl_expanded_sync()`.
- `src/CMakeLists.txt`, `tools/ui_harness/CMakeLists.txt`: inventario de fuentes.
- `tests/native/test_dfl_editor.c` + `run_native_tests.sh`: golden nuevo.
- `tools/ui_harness/src/screens.c` + fixtures: interacción VOL/rotary0/touch,
  capturas de los seis temas.
- `tools/ab_testing/quality/{contracts,regions}.yaml`: región `dfl_inline` y
  waivers UI-017 de las fixtures nuevas de Sprint/Cathode.
- `tests/check_wcag.py`: comentario del rol.

## 9. PENDIENTES / desviaciones documentadas

1. **Prueba física posterior del owner** (decisión vinculante del 20/10): el
   agente **no** valida hardware. Queda **pendiente**.
2. **Separador de la cabecera**: el contrato escribe `DFL · <modo>`, pero las
   fuentes del tema son **ASCII** (cmap 32..126, `src/fonts/sony_18.c`): el
   carácter `·` (U+00B7) no tiene glifo y sale como caja. Se usa `DFL USB-D`
   (espacio) — misma información, sin glifo inexistente.
3. **Volumen/chroot del build**: el chroot es **privado** de la corrida
   (`state/dfl179-chroot`), pero el **volumen** de build se **reusó**
   (`x6200-build-output-dfl173`): un volumen nuevo pesa ~20 GB y el disco del
   host está al 100 % (no entra). El binario se compiló **limpio** desde este
   HEAD (sello de fuentes `53880eb1…` = el del `.fuente.md5`).
4. El banco usó el tema por defecto de la radio (Sunlit); el cambio de tema no
   está expuesto por el socket de control. La matriz de temas se cubre con el
   arnés (`capturas/`).
