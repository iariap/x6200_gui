# #173 — Corrección del editor DFL: la rueda ajusta la fila seleccionada

Corrida `factory/dfl-encoder-173b`. **Base**: `960b8d66` (trunk tip con el editor
mergeado en `39539f93`). **HEAD**: `8d783dd7`. **Sin push.** No es un issue nuevo:
es la corrección que pidió el owner sobre el editor ya mergeado.

## 0. Lo que reportó el owner (20/10) y lo que se hizo

1. **La rueda (encoder) no afectaba el filtro.** Cierto: el framework despacha el
   encoder por `dialog->rotary_cb` (`src/dialog.c:175-178`) y el diálogo DFL lo
   tenía en **`NULL`**. Ahora el editor implementa `rotary_cb`, que ajusta el
   parámetro **seleccionado** con los pasos reales de `src/vol.c`.
2. **Los botones −/+ no deberían estar.** Se sacaron.

**Decisión del owner (ya tomada)**: tocar una fila (LOW / HIGH / BW) la
**selecciona** (queda resaltada con roles del tema) y la **rueda ajusta la
seleccionada**; el **arrastre del gráfico sigue** como atajo, y arrastrar un
handle **selecciona** ese parámetro.

## 1. Exploración previa (con líneas)

1. **Despacho del encoder**: `src/dialog.c:175-178` (`dialog_rotary()` llama
   `current_dialog->rotary_cb(diff)`); el camino real es
   `src/main_screen.c:1165-1213` (`main_screen_rotary_enhanced_cb` →
   `dialog_rotary(enhanced->raw_diff)`). El editor lo dejaba en `NULL`
   (`src/dialog_dfl.c:689` antes).
2. **Otros diálogos lo implementan**: `src/dialog_wspr.c:556`
   (`rotary_cb` con clamp + `msg_update_text_fmt`), `src/dialog_ft8.c:724`,
   `src/dialog_js8.c:1604`. Patrón del mensaje adoptado de ahí.
3. **Pasos reales** (`src/vol.c:75-133`): LOW **10 Hz** (79), HIGH **50 Hz** y
   **10 Hz** en CW/CWR (96-107), BW **20 Hz** (122). Cuantización `align_int`
   (`x - x % step`).
4. **Setters/validación**: `src/cfg/mode.c:317` (`cfg_mode_set_low_filter`, no-op
   en AM/SAM/NFM/WFM, LOW ≥ 0 en SSB, LOW negativo en CW/CWR) y `:340`
   (`cfg_mode_set_high_filter`, `val <= MAX_FILTER_FREQ` y `val > low`);
   `on_cur_filter_bw_change` `:447` es el mapeo BW→LOW/HIGH;
   `MAX_FILTER_FREQ` = 10000 (`src/cfg/mode.h:6`).
5. **Lifecycle (#131)**: los observers del filtro/modo son diferidos y el
   `destruct_cb` los borra y anula los punteros a objetos; se mantiene igual.

## 2. El cableado del `rotary_cb`

- **`src/dialog_dfl.c:177-184`** — `rotary_cb(int32_t diff)`: si hay TX o
  `diff == 0` no hace nada (la rueda tampoco edita durante TX); si no,
  `dfl_rotary_apply(sel, diff)` + `dfl_refresh()`. El valor y el mensaje salen del
  Subject, nunca de una posición optimista.
- **`src/dialog_dfl.c:750`** — descriptor: `.rotary_cb = rotary_cb` (antes `NULL`).
- **`src/dfl_rotary.c/.h`** (nuevos; **sin LVGL**, sólo Subjects + setters + msg):
  - `dfl_rotary_apply()` (`:75`) despacha por `sel` y emite
    `msg_update_text_fmt("DFL LOW|HIGH|BW %d Hz", v)`.
  - `rotary_low/high/bw` (`:50/:61/:68`) usan **`dfl_sel_step()`** (los pasos de
    `src/vol.c`, en `src/dfl_model.h:92`) y `dfl_quantize()`.
  - La aplicación pasa **siempre** por los caminos vigentes:
    `dfl_set_low` → `cfg_mode_set_low_filter` (`:16`); `dfl_set_high` →
    `cfg_mode_set_high_filter` (`:21`); `dfl_set_bw` (`:29`) calcula el par
    LOW/HIGH con `dfl_bw_targets` y lo valida con `dfl_bw_legal` **antes** de
    escribir el Subject del BW. Cada setter **devuelve el valor del Subject**
    (`subject_get_int`), no el pedido.
  - **No hay límites escritos a mano**: la validación es de los setters del
    firmware (LOW/HIGH) y del espejo del mapeo del BW (que ya vivía en el modelo).

## 3. Selección por fila y su default

- Cada fila es un objeto **tocable 260×44** (`control_row`,
  `src/dialog_dfl.c:597`) con `user_data` `DFL_UI_LOW_ROW`/`HIGH_ROW`/`BW_ROW`;
  al tocarla, `row_cb` (`:462`) fija `sel` y refresca.
- **Resaltado con roles del tema** (`dfl_row_refresh`, `:500`): la fila
  seleccionada usa `TC(sel_bg)` de fondo, `TC(sel_border)` de borde y
  `TC(sel_text)` para etiqueta y valor; las demás quedan transparentes con
  `TC(text_dim)`/`TC(text)` (`TC(text_mute)` cuando no son editables). Durante TX
  ninguna queda resaltada y los valores van atenuados.
- **Selección inicial**: `dfl_default_sel()` (`src/dfl_model.h:109`), fijada al
  abrir (`src/dialog_dfl.c:686`).
  - **LOW** en los modos donde es editable (USB/USB-D/LSB/LSB-D/CW/CWR): es el
    borde que más se ajusta.
  - **HIGH** en **AM/SAM/NFM/WFM**, donde LOW **no es editable** (queda en 0 y el
    setter lo rechaza): la rueda arranca en el borde útil y BW queda a un toque.
    (Se eligió HIGH y no BW porque el BW es un valor derivado del par.)
- **El arrastre del gráfico sigue** y **selecciona** el handle tocado
  (`src/dialog_dfl.c:419-427`), así la rueda sigue con el parámetro recién
  arrastrado.

## 4. Se sacaron los botones −/+

- `src/dialog_dfl.h`: el enum de ids pasa a `DFL_UI_LOW_ROW`/`HIGH_ROW`/`BW_ROW`
  (se van `LOW/HIGH/BW_MINUS/PLUS`).
- `src/dialog_dfl.c`: `control_row` ya no crea botones; la fila queda **etiqueta +
  valor**. `button_cb` sólo atiende `Default` (RESET) y `X` (CLOSE). No se agregó
  ningún otro control.

## 5. TDD (nativo del modelo)

- **Nuevo** `tests/native/test_dfl_rotary.c` (**68 comprobaciones**): pasos por
  modo (incluido CW/CWR), clamps (`HIGH ≤ MAX`, `HIGH > LOW`, `LOW ≥ 0` en SSB),
  `LOW` no editable en AM/SAM/NFM/WFM, `BW` ilegal rechazado (por arriba de MAX y
  por debajo del paso mínimo) y que **el valor aplicado (retorno y mensaje) sale
  del Subject** (incluido el rechazo: se pide 10050 y el mensaje dice 10000).
  Compila el módulo real contra un **espejo exacto** de las guardas de
  `src/cfg/mode.c` (`tests/native/dfl_rotary_stubs.c`) y stubs de cfg/msg
  (`tests/native/stubs_dfl_rotary/`).
- **Extendido** `tests/native/test_dfl_editor.c` (**+14** comprobaciones):
  `dfl_sel_step()` por parámetro y `dfl_default_sel()` por modo.

Salida real (`logs/native-host.log`, contenedor `x6200-uiharness-full`, ASan/UBSan):

```text
== Editor DFL (#173 PR 2): modelo puro (pasos, geometria golden, BW, reset) ==
108 comprobaciones, 0 fallos
== Editor DFL (#173): motor de la rueda (pasos, clamps, BW, Subject) ==
68 comprobaciones, 0 fallos
```

`gcc -fanalyzer` sobre `src/dfl_rotary.c` y sobre el test del modelo: **sin
observaciones**.

## 6. Arnés de UI

- `dfl-editor-interaction` ahora ejercita **la rueda y la selección por fila** por
  el **despachador real** (`dfl_rotary_ui` → `dialog_rotary()`,
  `tools/ui_harness/src/screens.c:825`), el mismo que usa la perilla:
  LOW (default) rueda +2 → **240**; tocar la fila HIGH y girar → **3550** (paso 50,
  cuantiza); tocar BW y girar → **3320** (paso 20, par legal); arrastre → 7000 y
  luego recorte a **10000**; rueda +1 sobre HIGH (10050) **rechazada** → queda
  10000. `settle_dfl_editor` lo afirma (`screens.c:832`).
- Los 11 fixtures `dfl-editor-*` capturan **OK** (USB-D, LSB-D, 6 temas, TX,
  interacción y 30 ciclos). Capturas en `capturas/`.

## 7. Gates (salida real)

- **Nativos + `-fanalyzer`**: `logs/native-host.log` — todo OK, 0 fallos.
- **Estáticos del repo** (`check_wcag`, `check_actions`, `check_app_handlers`,
  `check_apps_menu`, `check_settings_focus`, `check_manual_apps`,
  `check_cw_tune_feed`, `check_build_identity`, `check_cfg_fills`): `logs/static.log`
  — todos OK. `check_wcag` verifica el par de la **fila seleccionada**
  (`sel_text` sobre `sel_bg`, 6.13–14.67:1 ≥ 4.5) en las 4 paletas base y los 6
  temas JSON.
- **Determinismo** (`tests/check_harness_determinism.sh`): `logs/determinism.log`
  — `PASS: deterministic (identical PNGs for every fixture)`.
- **A/B `AB-DFL173C`**: `logs/ab-compare.md` / `logs/ab-results.json` — **PASS**
  (Functional 112 escenarios, Visual Regression sin regresiones no explicadas,
  Quality Contract 1147 checks, Performance dentro del presupuesto; Hardware y
  Energy BLOCKED advisory). Detalle en §9.
- **`x6200-ab audit`**: `logs/ab-audit.log` —
  `0 structural finding(s)` sobre 112 escenarios (12254 objetos, inspección REAL).
- **`x6200-ab contracts`**: `logs/ab-contracts.log` — `PASS — 1147 checks, 0
  failure(s)`.
- **`x6200-ab themes`**: `logs/ab-themes.log` — OK.
- **ruff / format / ty / pytest**: `logs/static.log`, `logs/pytest.log`,
  `logs/pytest-e2e.log` — `ruff check` y `ruff format --check` OK, `ty check`
  OK, `pytest -m "not e2e"` **227 passed**, `pytest -m e2e` (con `TMPDIR` en
  disco) **13 passed**.
- **Sintaxis shell** de los `.sh` tocados: `bash -n tests/native/run_native_tests.sh`
  OK (`logs/static.log`).

## 8. ARM cross-compile + banco QEMU (gate #86)

Volumen de build **privado del issue** (`x6200-build-output-dfl173`) y chroot
privado (`/home/pablo/desa/asf/state/dfl173-chroot`). El banco hace
`dirclean`+`rebuild` (el hash de fuentes cambió) y corre el **binario ARM real**
con `qemu-arm-static` (cortex-a7); pasa el gate fail-closed del firmware del port
(3/3 marcas).

```text
version: 8d783dd7 DFL #173: la rueda ajusta la fila seleccionada (sin
fuente:  md5 f1a21d6f8a02 (src/ + lvgl/src)
binario: md5 c8eda34bac1a
sello .fuente.md5 del chroot: f1a21d6f8a02732c5070bac3e28e8db2
boot_identity (main.c:142): fuente: f1a21d6f8a02732c5070bac3e28e8db2  -> COINCIDE
crash.txt: no
```

Log completo: `logs/banco.log`; arranque de la GUI: `logs/banco-out.txt`.

Capturas (`banco/`, editor abierto por control remoto
`{"cmd":"app","text":"DFLEditor"}`, el mismo camino que el webserver):

| Captura | Tema | Modo | Fila seleccionada |
|---|---|---|---|
| `banco-usbd-aurora-oscuro.png` | Aurora (oscuro) | USB-D | **LOW** |
| `banco-lsbd-aurora-oscuro.png` | Aurora (oscuro) | LSB-D | **LOW** |
| `banco-usbd-sunlit-claro.png` | Sunlit (claro) | USB-D | **LOW** |
| `banco-lsbd-sunlit-claro.png` | Sunlit (claro) | LSB-D | **LOW** |
| `banco-am-aurora-oscuro.png` | Aurora (oscuro) | AM | **HIGH** (LOW no editable) |
| `banco-am-sunlit-claro.png` | Sunlit (claro) | AM | **HIGH** (LOW no editable) |

En todas se ve el editor **sin botones −/+** (fila = etiqueta + valor) y la fila
seleccionada **resaltada**. En AM se ve además que LOW queda **atenuado** (no
editable) y el dominio simétrico del gráfico.

## 9. A/B `AB-DFL173C` — diff confinado al modal

`uv run x6200-ab compare --a 960b8d66 --b factory/dfl-encoder-173b --experiment-id AB-DFL173C`

```text
AB-DFL173C: PASS (exit 0)
  Functional         PASS   112 scenarios captured on both revisions
  Visual Regression  PASS   no unexplained visual regressions
  Quality Contract   PASS   1147 contract checks passed on the candidate
  Performance        PASS   render-cost proxy within budget
  Hardware/Energy    BLOCKED (advisory)
```

**Confinamiento**: de 112 escenarios, **sólo los 11 `dfl-editor-*` cambian**; los
otros 101 quedan **byte-idénticos**. En las fixtures estáticas (USB-D, LSB-D, los
6 temas, TX, 30 ciclos) los píxeles que se mueven están **todos dentro del cuerpo
del modal** (cajas medidas x=494..754, y=258..390 ⊂ `dfl_editor_dialogo`
[40,202,720,206]; p.ej. `dfl-editor-usb-d`: 17736 px). Para eso se agregó la
región `dfl_editor_dialogo` (`quality/regions.yaml`) y su `expected_changes`
(`quality/contracts.yaml`, `AB-DFL173C`), que aísla el modal de `waterfall`.

En `dfl-editor-interaction` el diff también toca `espectro` y `botones`: es
**esperado**, la fixture cambia los valores del filtro (LOW 240 / HIGH 10000 / BW
3320) y la banda del espectro y las celdas inferiores de la página DFL los
muestran. No es una regresión: es el efecto del cambio de valores que la propia
fixture ejerce.

## 10. Qué se cambió y qué no

**Se cambió**: `src/dialog_dfl.c/.h`, `src/dfl_model.h`, `src/CMakeLists.txt`;
**nuevos** `src/dfl_rotary.c/.h`; tests nativos (`test_dfl_rotary.c` + stubs,
`test_dfl_editor.c`, `run_native_tests.sh`); arnés (`screens.c`, `CMakeLists.txt`);
calidad A/B (`regions.yaml`, `contracts.yaml`) y `tests/check_wcag.py` (nota del
par de la fila).

**No se tocó**: RF/TX/DSP, `lvgl/`, el waterfall ni el espectro, la paleta de los
temas, `restore_radio` (sigue `false`), el cierre por ESC, el `btn_page`, el
"Restablecer predeterminado", la X, el deshabilitado en TX ni los observers
diferidos. **Sin** SHIFT, presets, botones extra ni comandos nuevos en el control
remoto.

## 11. PENDIENTES / decisiones para el owner

1. **BW seleccionado en el banco**: el control remoto sólo puede pulsar las 5
   celdas de abajo (no toca dentro del modal) y los `/dev/input` del banco van a
   `/dev/null`, así que **no se puede seleccionar BW en el banco**. Queda cubierto
   por el **arnés** (la fixture de interacción selecciona BW y gira la rueda) y por
   el **test nativo**. Si el owner quiere el estado BW en el banco, hay que agregar
   un comando de encoder al control remoto (fuera del alcance de esta corrección).
2. **Validación física en la radio** (`requires-radio`): **receta** — abrir el
   editor (pulsación larga DFL o quinta celda "DFL editor"), **girar la rueda con
   cada fila seleccionada** (tocar LOW / HIGH / BW) y verificar que el valor en
   pantalla y en el equipo cambia con los pasos de `src/vol.c` (LOW 10 Hz; HIGH
   50 Hz y 10 Hz en CW/CWR; BW 20 Hz), que el mensaje muestra el valor aplicado y
   que en TX la rueda no edita. El banco QEMU no tiene FFT ni audio ni encoder.
3. **Disco del host al 100%**: el A/B necesitó worktrees de revisión; se
   pre-crearon con **sparse checkout** (excluyendo `tools/ab_testing/reports`, 2.1
   GB por worktree) para no agotar el disco. No se borró nada de otras corridas.
4. **Encabezado con guion ASCII** ("DFL - Digital Filter"): se mantiene la decisión
   del PR 2 (evitar `typography_missing_glyph`).
5. **`check_manual_apps.py`**: sigue avisando que `ACTION_APP_DFL_EDITOR` no tiene
   sección en el manual (informativo; el chequeo pasa). Sin cambios.

## 12. Cómo deshacer

Revertir `8d783dd7`: se quitan `src/dfl_rotary.*`, la selección por fila y el
`rotary_cb`, vuelven los botones −/+ (no recomendado) y se revierten los tests, el
arnés y las entradas de calidad A/B. No hay migración de datos ni cambio de paleta.
