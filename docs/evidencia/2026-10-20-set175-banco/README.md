# #175 — Ajustes: un solo origen para la columna de widgets de la derecha

**Corrida** `factory/settings-align-175` · **Issue**: [#175](https://github.com/iariap/x6200_gui/issues/175)
**Base**: `39539f93` (trunk tip, DFL #173 PR 2) · **Commits**: `3b107743` (arnés + fixtures + waivers)
y `7af6ebb1` (el fix del layout + el check). **HEAD de la corrida**: `7af6ebb1`.

## 1. El defecto, medido

La captura de la radio (`57b11ddc058a`, tema Aurora, `docs/evidencia/2026-10-20-settings-alineacion/`)
mostró que la "columna derecha" de Ajustes tenía **dos bordes izquierdos** según la fila: los
desplegables anchos arrancaban en **x≈499** y los switches en **x≈643** (en la misma página de Audio:
"SSB voice" en 501 y "Speaker mode" en 645).

Causa (`src/dialog_settings.c`): la grilla es `col_dsc = { LV_GRID_FR(1), 45 ×6 }` (columna de
etiquetas flexible + 6 columnas de 45) y **cada fila elegía a mano su columna inicial y su span**:

| Fila | Columna | Span | x del borde izquierdo |
|---|---|---|---|
| 1 control ancho (desplegable) | 1 | 6 | 499 |
| 2 controles (celdas) | 1 y 4 | 3 + 3 | 499 / 643 |
| 1 control angosto (switch) | **4** | 3 | **643** |
| 3 campos (día/hora/transverter/voz) | 1/3/5 | 2 | 499 / 595 / 691 |
| "Timeout, Brightness" | 1 y **3** | 2 + 4 | 499 / **595** |
| "Mag Freq, Info, ALC" | 1/3/5 | **3** | 499 / 595 / 691 |

Los anchos reales (medidos con `ui_harness --inspect`, no estimados): zona de controles = columnas
1..6 = **285 px** (6×45 + 5×3), de x=499 a x=784; el spinbox de 4 cifras ("2026") mide 93 (2 columnas
de 45 + el gap, `SMALL_2`); los switches y los sliders de ganancia viven en cajas de 141 (`SMALL_3`);
el desplegable de tema (los 6 nombres) y el de texto libre usan los 285 (`SMALL_6`).

## 2. El esquema elegido (y por qué)

**Un solo origen: toda fila arranca su bloque de controles en la columna 1 (x=499).** Los spans son
fijos por cantidad de controles y **tapan la zona entera**:

```
1 control    -> span 6                    (285 px)   499..784
2 controles  -> span 3 + span 3           (141 c/u)  499..640 / 643..784
3 controles  -> span 2 + span 2 + span 2  (93 c/u)   499..592 / 595..688 / 691..784
```

Por qué esos spans: 3 campos de 93 (el ancho que necesita el spinbox de 4 cifras) + 2 gaps = 285 =
la zona exacta; 2 cajas de 141 + 1 gap = 285; un control solo usa la zona entera, que es lo que ya
hacían los desplegables. Los bordes izquierdos de la columna derecha quedan así determinados por la
**cantidad de controles de la fila**, nunca por el tipo de widget.

**Única excepción, documentada** (no tres): la fila **"Timeout, Brightness"** (`make_backlight`) es
**2 + 4**: el slider de brillo es un slider de **rango** (idle..normal) y mide `SMALL_4-30 = 159 px`,
así que no entra en la mitad de la zona (141) sin recortarse. Su caja arranca en la **columna 3
(x=595)**, que es una columna del mismo esquema (la del campo del medio de las filas de 3). Es la
única fila del diálogo cuyo segundo control necesita más de media zona.

Cambios de fila: los switches de Bluetooth/Discoverable, Cluster column, wfview, Speaker mode,
waterfall smooth scroll/battery, S-meter y Band plan pasan de la columna 4 a la 1; "Mag Freq, Info,
ALC" declara el span de las filas de 3 campos (2), que es lo que su caja ya medía (93).

**De paso**: el desplegable **"Charger"** estaba en la columna 4 con el ancho de la zona entera
(285 px) y **se recortaba 144 px** contra el borde del diálogo; al entrar en la columna 1 se ve
completo. Eso dejó a la vista su símbolo por defecto (`LV_SYMBOL_DOWN`, que no está en las fuentes:
salía como **caja vacía**), así que se le saca con `lv_dropdown_set_symbol(obj, NULL)`, igual que a
los otros desplegables de Ajustes.

## 3. La medición después (bordes izquierdos por píxeles)

`medicion/resumen-bordes.md` (crudo en `medicion/arnes-*.txt` y `medicion/banco-despues.txt`): se
recorre cada fila del diálogo y se cuenta en qué columna cae el **primer borde vertical claro**.

### Arnés (host), antes → después

| Página | | 499 (col 1, el origen) | 595 (col 3) | 643 (col 4) | 691 (col 5) |
|---|---|---|---|---|---|
| Operation | ANTES | 56 | 0 | **192** | 0 |
| Operation | DESPUÉS | **248** | 0 | 0 | 0 |
| Audio | ANTES | 253 | 0 | **56** | 0 |
| Audio | DESPUÉS | **309** | 0 | 0 | 0 |
| Spectrum | ANTES | 182 | 0 | **112** | 0 |
| Spectrum | DESPUÉS | **294** | 0 | 0 | 0 |
| Clock | ANTES | 294 | 0 | 0 | 0 |
| Clock | DESPUÉS | **294** | 0 | 0 | 0 |
| Display | ANTES | 182 | 0 | **112** | 0 |
| Display | DESPUÉS | **294** | 0 | 0 | 0 |

(Clock no cambia: su grupo no tenía ninguna fila en la columna 4 — es el control de que la medición
no "ve" lo que no cambió.)

### Banco QEMU (binario ARM real, gate #86), después

| Página | Tema | 499 | 595 | 643 | 691 |
|---|---|---|---|---|---|
| Operation / Audio / Spectrum / Clock / Display | Aurora | 231 / 309 / 293 / 282 / 282 | 0 | 0 | 0 |
| Operation / Audio / Spectrum / Clock / Display | Sunlit | 157 / 175 / 160 / 206 / 206 | 0 | 0 | 0 |

**Los tres orígenes colapsan a uno (x=499) en las dos mediciones** (host y ARM real, tema oscuro y
claro). Las 5 páginas + las 3 restantes (Appearance, Actions, Info) están en `arnes/` y `banco/`.

## 4. Evidencia

```
arnes/antes/   8 grupos, arnés del host con el código de la BASE (39539f93)   <- "antes"
arnes/despues/ 8 grupos + las 6 variantes de tema de Operation, con el fix     <- "después"
banco/         9 estados de la barra de tabs x 2 temas (Aurora oscuro, Sunlit claro), binario ARM real
medicion/      bordes por píxeles (antes/después, arnés y banco) + resumen
logs/          determinismo, nativos, fanalyzer, estáticos, A/B, pytest, compilación ARM, banco
```

El "antes" del arnés se renderizó con el mismo arnés (con los fixtures nuevos) y el
`src/dialog_settings.c` de la base; el "después" con el fix. Las capturas del banco son del
**binario ARM real** (`x6200_gui`, md5 `8fbefe64ac9d`, 13.869.700 B) corriendo con `qemu-arm-static`.

## 5. Gates (salida real en `logs/`)

| Gate | Resultado |
|---|---|
| Nativos completos (ASan/UBSan) + `-fanalyzer` | OK — `logs/nativos.log` |
| `-fanalyzer` sobre `dialog_settings.c` | mismas 5 advertencias que la base (ninguna nueva): `logs/fanalyzer-dialog_settings.log` |
| Estáticos (cfg_fills, actions, **settings_focus**, **settings_grid**, apps_menu, app_handlers, manual_apps, wcag, cw_tune_feed, build_identity) | OK — `logs/estaticos.log` |
| Determinismo del arnés (todos los fixtures ×2) | **PASS** — `logs/arnes-determinismo.log` |
| A/B `AB-SET175` (`3b107743` → `7af6ebb1`, 122 escenas) | **PASS** (Functional, Visual Regression, Quality Contract 1274, Performance; Hardware/Energy BLOCKED por diseño) — `logs/ab-compare.log` |
| `audit` sobre `7af6ebb1` | 0 hallazgos estructurales, 0 issues reproducidos — `logs/ab-audit.log` |
| `themes` (WCAG estático de los 6 temas) + ruff + format + ty | OK — `logs/ab-estaticos.log` |
| pytest (no e2e) | 227 passed — `logs/ab-pytest.log` |
| pytest e2e (`TMPDIR` en disco) | 13 passed — `logs/ab-pytest-e2e.log` |
| `tests/run_all_tests.sh --quick` (con `IMAGE=x6200-uiharness-full`) | **18 OK, 0 fallos** (`settings_grid` incluido; js8 y webserver omitidos por el runner, no fallados) — `logs/run-all-tests.log` |
| ARM cross-compile | OK — `logs/banco-compila-arm.log` |
| Banco QEMU + gate #86 | OK (3/3 marcas del port, sin `crash.txt`) — `logs/banco-smoke.log`, `logs/banco-out.txt` |

### El A/B, en detalle

`AB-SET175` compara **`3b107743` (el commit del arnés: fixtures nuevos, sin tocar `src/`) contra
`7af6ebb1` (el fix)**: así la escena incluye las páginas nuevas y el diff es, literalmente, el
cambio de layout. Las 122 escenas se capturan en las dos revisiones; **sólo cambian las de Ajustes**
(Operation, Audio, Spectrum, Display, Actions, Info y las 6 variantes de tema de Operation), y dentro
de ellas el diff cae en `ajustes_dialogo` (la región declarada del cuerpo del diálogo) y en las
bandas de la pantalla principal que lo contienen (`espectro`, `waterfall`, `frecuencia`). Ninguna
otra pantalla se mueve un píxel. El commit del arnés no toca `src/`: es idéntico en píxeles a la base.

`3b107743` agrega además los **waivers** de `layout_touch_target` para los fixtures nuevos
(`tools/ab_testing/quality/contracts.yaml`): los switches de Ajustes miden 111×30 / 63×30 y el slider
de brillo 159×13, la geometría **pre-existente** de UI-015 que ya waiveraban `settings-home/-wfbat/
-audio`; las páginas nuevas la observan por primera vez y el waiver acotado por fixture (con razón y
vencimiento) la deja a la vista sin debilitar la regla.

## 6. El banco (gate #86): volumen y chroot

- **Chroot privado**: `/home/pablo/desa/asf/state/set175-chroot` (copia del de #173; el script lo
  escribe, así que no se comparte).
- **Volumen de build**: se **reusó** `x6200-build-output-dfl173` en lugar de crear uno nuevo: el
  host está al **99% de disco** y los volúmenes de build pesan ~20 GB cada uno (un volumen nuevo
  exigiría un buildroot completo desde cero). Es la misma concesión que documentó #173. El script
  detectó el cambio de hash de fuentes, hizo `dirclean` + `rebuild` y dejó el sello.
- **Sello de fuentes** (`boot_identity`, `src/main.c`): `fuente: 3c7995da89529534d6abe17111bde6f0`
  en el log de arranque = el `.fuente.md5` de la copia del build = el `tools/fuente-md5.sh` del
  árbol. El binario del banco es el de esta corrida (md5 `8fbefe64ac9d`), no uno viejo.
- **Cómo se navegó**: el control remoto no tiene un comando para los tabs de Ajustes, pero sí
  `{"cmd":"btn","val":N}` (aprieta el botón N de la barra de abajo). Con Ajustes abierto, esa barra
  es la de los tabs del diálogo (0 Operation, 1 Audio, 2 Spectrum, 3 Clock, 4 "More >") y, ya en la
  página 2, 0 Display, 1 Appearance, 2 Actions, 3 Info. Se encadenan las capturas **sin reiniciar**
  la GUI (el `arrancar_banco` del script reinicia cuando el handshake falla, y eso cerraba el
  diálogo).
- **El tema** se fija en `/mnt/params.db` (`params.theme_name`) antes de arrancar la GUI.

## 7. Qué se cambió y qué no

**Se cambió**: `src/dialog_settings.c` (el esquema de la grilla + el `set_symbol` del Charger);
`tests/check_settings_grid.py` (nuevo); `tests/run_all_tests.sh` y `.github/workflows/ab-testing.yml`
(lo enganchan); el arnés (`tools/ui_harness/src/screens.c`, `scenario.c/h` + 10 fixtures);
`tools/ab_testing` (parser de escenarios, waivers y sus dos tests).

**No se tocó**: RF/TX/DSP, `lvgl/`, la funcionalidad de los ajustes (los mismos widgets, los mismos
handlers, el mismo orden, los mismos valores), el foco/encoder (`check_settings_focus.py` sigue
verde: 29 widgets creados, todos en el grupo de foco) ni el tamaño de ningún control (el cambio es
de **columna**, no de caja). Los 6 temas sin recortes.

## 8. PENDIENTES / decisiones para el owner

1. **El diálogo semitransparente (el extra del issue): NO se tocó.** La transparencia no es de
   Ajustes: sale de `dialog_style` (`src/styles.c`) y de la imagen de fondo del diálogo del tema, que
   usan **todos** los diálogos. Cambiarla movería píxeles en BT, FT8, SWR Scan, etc., y el A/B
   dejaría de estar confinado a Ajustes. Si el owner la quiere opaca, es un cambio de tema/estilo
   (y conviene decidir si aplica a todos los diálogos o sólo a Ajustes). Evidencia del efecto:
   `banco/banco-*-Aurora.png` (se ve el "3.3k" y la fila de frecuencia a través del diálogo).
2. **`layout_touch_target` (UI-015) sigue abierto**: los switches de Ajustes miden 111×30/63×30 y el
   slider de brillo 159×13, debajo del target de 44 px de AGENTS.md. Los waivers nuevos lo mantienen
   a la vista en `contracts`/`audit`; arreglarlo es un incremento de UX aparte (alto de los
   controles, no su columna).
3. **El desplegable de tema dibuja una caja vacía** en su extremo derecho (el `LV_SYMBOL_DOWN` que no
   está en las fuentes): es **pre-existente** (está igual en la base, el desplegable ya estaba en la
   columna 1) y no lo toca esta corrida. Se arregla con la misma línea que se le puso al Charger.
4. **Volumen de build reusado** (`x6200-build-output-dfl173`): por límite de disco del host, no por
   elección. Si el owner libera espacio, conviene un volumen propio por corrida.
5. **Validación física** (`requires-radio`): el banco QEMU prueba el **render** del binario real,
   no la recepción ni que el encoder siga igual en la radio. El foco/encoder se validan con
   `check_settings_focus.py` (estático) y con el `settings-audio` del arnés (que mueve un slider).
6. **Las 5 páginas del banco**: se capturaron los **8 grupos** (9 estados de la barra de tabs) × 2
   temas. El "More >" no cambia de grupo (sólo de página de tabs), así que la captura
   `banco-more-*.png` repite el contenido de Clock con la barra de la página 2: es el estado que el
   QA fotografió como "página 5".
