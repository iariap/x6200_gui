# Evidencia visual 800×480 — Ajustes, navegación, DFL y diálogos (issue #271)

Auditoría de **recolección de evidencia** (hijo B de la épica #269). **No se cambió
nada**: este PR es sólo documentación/evidencia (PNG + tablas + este README). No se
tocó `src/`, tests, temas, fuentes, Ajustes persistidos, controles, lógica de radio,
RF ni firmware.

| | |
|---|---|
| Issue | `iariap/x6200_gui#271` ([PROMPT][UI AUDIT B] Settings, DFL, CW and dialogs) |
| Épica | `iariap/x6200_gui#269` ([EPIC][UI AUDIT] Full 800×480 visual evidence before ROM release) |
| SHA congelado (base) | `d0ff5cd196b43ba0190d4ccad03f559a2954daf6` — el mismo que fijan la épica y sus hijos |
| HEAD del worktree | `d0ff5cd196b43ba0190d4ccad03f559a2954daf6` (sin cambios de código: sólo este directorio) |
| Worktree de la corrida | `state/worktrees/x6200/ui-audit-dialogs` |
| Rama de la corrida | **`factory/ui-audit-dialogs`** |
| Rama pedida por el issue/épica | `audit/ui-dialogs-20261023` (**diferencia anotada**: el worktree ya existía con la rama de fábrica; no se creó otra para no duplicar el artefacto) |
| PR | lo abre el orquestador contra `main`: `[UI AUDIT] Settings and dialogs evidence (refs #271)` |
| Capturas | **137** PNG nativos 800×480 (sin escalar), 6 temas, `--inspect` |

> **Límite explícito**: nada de esto prueba legibilidad *física* en el LCD de 4".
> Son medidas sobre el render determinista del arnés de host (LVGL 8.3.11 real) más
> el árbol de objetos vivo. Las conclusiones sobre el panel real quedan pendientes de
> la pasada de hardware.

---

## 1. Alcance medido

Alcance B de la épica: **Ajustes (todas las páginas y filas), navegación, DFL
(editor inline y sus estados), UI de CW y los diálogos**, en los estados
alcanzables por **caminos reales de la GUI**.

| Grupo | Fixtures | Capturas | Estado |
|---|---|---|---|
| Ajustes: los 8 grupos | `settings-home` (Clock), `settings-wfbat` (Spectrum), `settings-audio` (Audio), `settings-perfil` (Appearance), `settings-grupo-operation/-display/-info/-actions` | 8 | **medido** |
| Ajustes: matriz de temas | `settings-tema-{aurora,cathode,redshift,sprint,sunlit,trail}` + `settings-grupo-operation-tema-*` | 12 | **medido** |
| Navegación del firmware | `apps-menu-1..6` (menú APP, 6 páginas), `app-bands` (APP→Bands), `settings-bands` (Ajustes→Bands) | 8 | **medido** (ruta; ver §5) |
| DFL: página, editor inline y estados | `dfl-page-*`, `dfl-editor-*` (usb-d, lsb-d, tx, interaction, cycles, reset, rotary0, sel-bw, sel-high, mirror-usb/lsb), `dfl-usb-d*`, `dfl-lsb-d` | 34 | **medido** |
| DFL: presets (páginas A–F) y sus estados | `dfl-presets-*` (fav, adj, graph, more, confirm, assign, save, touch, vol, tx, nav, digi, nfm, custom, activo, cycles, rotary0, apply, mode-change) | 60 | **medido** |
| UI de CW | `cw-tune-default/-tuner/-ssb/-tema-sunlit`, `cw-autotune-off/-on` | 6 | **medido** (widget REAL; tono/decodificado sintético) |
| Diálogo Bluetooth (el otro diálogo real del firmware) | `bt-dialog` + 6 variantes de tema | 7 | **medido** |
| **Total** | | **137** | |

Inventario completo de diálogos (incluidos los 22 inertes del arnés de host):
[`tablas/dialogos.csv`](tablas/dialogos.csv).

## 2. Cómo se reprodujo

Todo el render es determinista y sale del arnés de host documentado en
`tools/ui_harness/README.md`, construido en la imagen `x6200-uiharness` **en el
mismo SHA**.

```sh
# 1. build (una vez; imagen reproducible, ya presente en el host)
REV=$(git rev-parse HEAD)                    # d0ff5cd196b43ba0190d4ccad03f559a2954daf6
docker run --rm --entrypoint bash --user "$(id -u):$(id -g)" \
  -v "$PWD:/src" -w /src/tools/ui_harness x6200-uiharness \
  -lc "mkdir -p build && cd build && cmake .. -DHARNESS_REVISION=$REV && make -j8"

# 2. las 137 capturas (una por fixture, salida y exit code por corrida)
docker run --rm --entrypoint bash --user "$(id -u):$(id -g)" \
  -v "$PWD:/src" -v "<scratch>:/scratch" -w /src/tools/ui_harness/build x6200-uiharness \
  /scratch/render-all.sh        # loop: ./ui_harness --scenario ../fixtures/<f>.scn \
                                #        --png /scratch/png/<f>.png --json /scratch/json/<f>.json --inspect
```

La lista de fixtures y el `md5` de cada PNG están en
[`logs/render-all.log`](logs/render-all.log) y [`logs/render-rc.tsv`](logs/render-rc.tsv)
(**137/137 con `exit_code = 0`**). La forma de una captura individual, tal como la
registra cada fila de `manifest.csv`, es:

```sh
docker run --rm --entrypoint bash --user "$(id -u):$(id -g)" \
  -v "<worktree>:/src" -v "<scratch>:/scratch" -w /src/tools/ui_harness/build \
  x6200-uiharness -lc "./ui_harness \
    --scenario ../fixtures/settings-home.scn \
    --png /scratch/png/settings-home.png --json /scratch/json/settings-home.json --inspect"
```

Ese mismo comando con `--scenario ../fixtures/home-usb-20m.scn` produce el **control**
que se usa en §6 (`control/home-usb-20m.png` + su JSON): `home-usb-20m` y `settings-home`
declaran **las mismas claves de escenario** (la única diferencia es `screen`, verificado
con `diff`), así que la pantalla principal detrás de la ventana es la misma imagen y la
diferencia entre las dos capturas es, exactamente, la ventana de Ajustes.

> El arnés **reescribe `A:/dev/shm/` → `A:<repo>/`** en `src/styles.c` al compilar
> (CMakeLists.txt), así que los bitmaps de estilo (`images/dialog.bin`, `btn.bin`, …)
> **sólo se resuelven dentro del contenedor**, donde el repo está montado en `/src`.
> Correr el binario fuera del contenedor pierde esos bitmaps (LVGL avisa
> `draw_bg_img: Couldn't read the background image`) y cambia los colores de las
> superficies: **todas las capturas de esta evidencia son de dentro del contenedor**.

Tablas por captura: [`manifest.csv`](manifest.csv) (una fila por captura, con SHA,
comando exacto, exit code, tema, cobertura, bounds, sha256 del PNG y severidad).

Peso de la evidencia: **≈32,7 MB de PNG** (138 archivos nativos: las 137 capturas + el
control de §6, sin reescalar ni recomprimir; los renders con espectro/cascada sintéticos
casi no comprimen) + ~4 MB de tablas. Todo es regenerable bit a bit con los comandos de
arriba; si el repositorio prefiere no cargar los 137 PNG, el subconjunto mínimo para
sostener cada hallazgo está nombrado en cada ficha de §4.

## 3. Qué se midió y qué salió

### 3a. Estructura: sin truncado, sin desborde, sin glifos faltantes

Sobre las **137 capturas** (`--inspect`, árbol LVGL vivo, sin OCR):

| Medida | Resultado |
|---|---|
| Etiquetas visibles inspeccionadas | **4437** |
| `text_truncated` | **0** |
| `text_overflow` | **0** |
| `missing_glyphs` (glifo ausente en toda la cadena de fuentes) | **0** |
| Fuente más chica del alcance | `sony_14` (34 capturas, p. ej. el rótulo `Schematic` del editor DFL, caja de 11 px de alto) |
| Cajas de 11 px de alto | 30 (100 % rótulos `sony_14` del DFL) |
| Celdas de la grilla de Ajustes | 105 celdas; ≈41 filas de contenido en 8 grupos (60 etiquetas en la columna 0, 19 de ellas celdas ocultas del legend del band plan) |

Es un **resultado negativo medido**, no una ausencia de medición: en el alcance B no
hay ninguna etiqueta recortada, desbordada ni con glifo faltante.

Que una etiqueta quede fuera del viewport de la grilla de Ajustes (**medido**: hasta 63
etiquetas por captura, todas dentro del nodo de la grilla, `scroll_dir = ver`, límite
superior del conteo de 1212) **no** es un defecto: es contenido al que el usuario llega
scrolleando, y el propio contrato del arnés lo distingue (`layout_checks.py` respeta
`scroll_dir`). En las capturas del DFL el conteo de etiquetas fuera de viewport es 0.

### 3b. Hallazgos estructurales crudos (sin waivers) y su clasificación

[`tablas/hallazgos-estructurales.csv`](tablas/hallazgos-estructurales.csv) (59 filas),
medidos con el **mismo código de reglas del proyecto** (`x6200_ab.layout_checks` /
`x6200_ab.typography_checks` sobre el `--inspect`):

| Regla | Casos | Dónde | Clasificación |
|---|---|---|---|
| `layout_touch_target` (< 44 px) | **36** | 12 fixtures de Ajustes + 6 variantes de tema | **UI-015**, defecto real pre-existente, con waiver fechado en `tools/ab_testing/quality/contracts.yaml` (hasta 2027-03-31) → **P2** |
| `layout_overlap` (dos etiquetas con texto en la misma caja) | **23** | 22 fixtures DFL (la capa "8 fantasma" de la fila de frecuencia, `88.888.888` + el valor en la caja 500×36 de `150,160`) + `dfl-presets-rotary0` (indicador `x1` del tuning adaptativo sobre el rótulo del reloj, `779,10` vs `591,5`) | **UI-017** (por diseño) y **UI-018** (documentado con waiver) → **P3** |

Detalle de los touch targets medidos ([`tablas/touch-target.csv`](tablas/touch-target.csv)):

* switches de Ajustes: **111×30** (`Bluetooth`, `Discoverable (CAT)`, `Cluster column`,
  `S-meter needle`, `Band plan`, `Waterfall battery mode`, `Speaker mode`)
* switches de la fila `Mag Freq, Info, ALC`: **63×30** (caja de dos columnas)
* sliders: **46×13 y 51×13** (las cuatro de la fila de audio), **159×13** (brillo)
* la caja útil de 44 px es la que fija `contracts.yaml::min_touch_target_px`, y el panel
  de la radio es táctil → el área táctil real medida es 30 px de alto (y 13 px en los
  sliders).

### 3c. Contraste: matrices de contraste por tema (medido sobre píxeles)

El contrato del proyecto sólo puede calcular el par texto/fondo **cuando la etiqueta
pinta un fondo opaco**; en Ajustes/DFL/CW casi ninguna lo hace (7 filas en
[`tablas/contraste-unknown.csv`](tablas/contraste-unknown.csv) documentan esa
imposibilidad estructural con su razón). Por eso acá se agrega una **medición por
píxeles** — color de tinta del árbol de objetos + fondo estimado de la propia caja en
el PNG (mediana, uniformidad y percentiles 5/95) — en
[`tablas/contraste-medido.csv`](tablas/contraste-medido.csv) (3205 filas de etiquetas
visibles). Columnas clave: `text_color_object_tree`, `bg_median_rgb`,
`bg_uniformity`, `contrast_median`, `contrast_p05_p95`, `reliable`,
`covered_by_later_surface`.

| Tema | Filas confiables (no cubiertas por una superficie posterior) | Filas con contraste mediano < 4.5:1 |
|---|---|---|
| Sunlit | 307 | **62** |
| Redshift | 299 | **50** |
| Aurora | 277 | 8 |
| Sprint | 301 | 1 |
| Cathode | 324 | **0** |
| Trail | 300 | **0** |

Y el chequeo estático de temas del proyecto (`tests/check_wcag.py`) da **0 fallas**
(1062 pares OK, 170 exentos, 90 informativos) — es decir **el gate estático está verde
mientras el render de Sunlit mide 2.38:1**, porque el gate compara roles del tema
(`text` sobre `dialog` = `#162436` sobre `#FFFFFF` = 12:1) y la ventana **no pinta ese
rol**: pinta el bitmap `images/dialog.bin` (superficie oscura, ver §4 hallazgo D-1).

## 4. Defectos (ranked, con evidencia)

### D-1 — **P1** · En el tema **Sunlit** las etiquetas de fila de Ajustes quedan ilegibles (2.38–2.39:1)

* **Medido**: `settings-tema-sunlit` → etiqueta `Theme`, tinta `#162436`, fondo medido
  `#4B606C`, **contraste 2.38:1**; `settings-grupo-operation-tema-sunlit` → `Freq
  acceleration`, **2.39:1**; `settings-tema-sunlit` → `Theme`, **2.38:1** con
  uniformidad de fondo 1.000 (la superficie no es un gradiente: el número es sólido).
  (WCAG 2.1 AA pide 4.5:1 para texto normal y el proyecto usa ese umbral en
  `tests/check_wcag.py`.)
* **Evidencia visual**: `png/settings-tema-sunlit.png` (las etiquetas de fila se ven
  oscuras sobre la superficie oscura de la ventana, mientras en la misma ventana las
  superficies que **sí** usan el rol claro del tema miden contraste alto: p. ej. la fila
  de frecuencia, tinta `#162436` sobre fondo medido `#F7F5EB` = **14.34:1**, y el interior
  de los campos del mismo tono).
* **Causa raíz medida**: el cuerpo de la ventana de Ajustes es un bitmap con alpha
  (`src/styles.c`: `lv_style_set_bg_img_src(&dialog_style, PATH "images/dialog.bin")`,
  y el nodo del diálogo llega al `--inspect` como `bg_image: true`, `bg_opa: 0`); la
  tinta de la etiqueta sale del rol del tema (`text` → `#162436` en el `base: Outdoor`).
  El interior de los campos sí pinta `dialog` (`#FFFFFF` en Sunlit). Resultado: tinta
  oscura sobre superficie oscura **sólo en las etiquetas de fila**.
* **Por qué el gate no lo ve**: `tests/check_wcag.py` confronta roles estáticos
  (122 roles de `src/theme.h` + los 10 JSON), nunca el bitmap de la superficie de la
  ventana.
* **Alcance**: Ajustes (B). El mismo mecanismo (superficie bitmap oscura + tinta oscura
  del `base: Outdoor`) se ve medido en otras superficies que **no** son de este hijo:
  banda de estado del reloj en Sunlit `00:00:00` **2.78:1** (`bt-dialog-tema-sunlit`,
  `cw-tune-tema-sunlit`, `dfl-editor-*-sunlit`) → se reporta como observación
  cross-scope para **A (#270)**.
* **No prescribe solución** (regla del issue): se documenta el hecho y la causa.

### D-2 — **P2** · Controles de Ajustes por debajo del target táctil de 44 px (UI-015, pre-existente)

36 casos medidos (§3b): switches 111×30 / 63×30 y sliders de 13 px de alto en 18
capturas (12 fixtures + 6 variantes de tema). Con waiver fechado del propio proyecto
(`contracts.yaml`, `layout_touch_target`, hasta 2027-03-31) y `x6200-ab contracts`
sigue **PASS** porque el waiver es explícito y acotado. Se rankea **P2** (degradación
visible/operativa, no bloquea: el encoder y el ancho de 111 px del switch permiten
operar) y queda como insumo para la decisión del owner.

### D-3 — **P2** · Título de la ventana Bluetooth por debajo de AA en 2 de 6 temas

* `Bluetooth devices`: **Aurora 4.34:1** (tinta `#D7C2FF` sobre la superficie del
  diálogo), **Redshift 4.11:1** (`#FFB39C`). En Sunlit el rótulo no está presente en el
  render.
* Resto de la ventana medido: `no headphones, RX off` 6.53:1 (Aurora) / 4.23:1
  (Redshift); `Row: pair/connect - Auto: hold to unpair` 9.53:1 / 6.36:1.
* Evidencia: `png/bt-dialog.png`, `png/bt-dialog-tema-redshift.png`,
  `tablas/contraste-medido.csv`.

### D-4 — **P3** · Solapamientos del DFL (UI-017 por diseño, UI-018 documentado)

* 22 fixtures DFL: la etiqueta sombra `88.888.888` y el valor del dial comparten la
  caja 500×36 en `150,160` (100 % de solape de caja). Es la capa que da el look de
  segmentos apagados (por diseño, con waiver por fixture en `contracts.yaml`).
* `dfl-presets-rotary0`: el indicador `x1` del tuning adaptativo (`779,10`) cae dentro
  de la caja del rótulo del reloj (`591,5`) — **UI-018** (waiver fechado).

### D-5 — **P3** · Celdas "muted" de los presets del DFL

`Mem 1`, `Mem 2`, `--`, `Mem 1\nvacia`, `Wide 3300 Hz`, `DX 2100 Hz` en las páginas de
presets miden **3.57–3.95:1** con la tinta del rol **`text_mute`** (`#8A7BA0` Aurora /
`#A86458` Redshift). Se reporta medido y **clasificado como exento** (WCAG 1.4.3,
"disabled/incidental": `tests/check_wcag.py` lista esa familia como exención) porque el
estado es memoria vacía/deshabilitada, no texto operable.

### 3d · Navegación y foco (medido)

* **Foco visible**: en cada fixture de Ajustes hay exactamente 1 etiqueta dentro del
  grupo de teclado (`in_keyboard_group`), y el campo enfocado se distingue del resto por
  relleno: campo `01` enfocado → tinta `#000000` sobre fondo medido `#A0A9B1`
  (**8.80:1**); su gemelo sin foco `01` → `#FFFFFF` sobre `#3F5566` (**7.77:1**). El
  foco es visible y el texto sigue legible en ambos estados.
* **`settings_goto` en el último grupo**: al entrar al grupo **Info** la grilla ya está
  en su tope de scroll, así que su fila (`Mag Freq, Info, ALC`, y=343..371) queda como
  **última fila visible** del viewport (`2,75 796×330`) en vez de arriba como en los
  otros grupos. Medido, no es un defecto (la fila y el widget enfocado quedan
  completamente dentro del viewport) — se registra como comportamiento de navegación.
* **Páginas de Ajustes con `settings-grupo-*`**: los 4 grupos `Action/Display/Info/Operation`
  entran por el tab real (`buttons_press`) y su fila testigo está a la vista.
  **Nota de metadata del arnés (no se toca)**: para el `screen: settings-grupo` el
  arreglo `coverage` del JSON **no** declara `dialog.c` / `dialog_settings.c` (cae en la
  tabla de `main_screen`), aunque `src/screens.c` abre la ventana real. La clasificación
  REAL de esas capturas está fundada en el camino de `src/screens.c` + el build, no en
  ese arreglo; queda como inconsistencia de metadata a decidir (no se modifica código).
* **DFL, orientación del gráfico (medido, [tablas/dfl-grafico.csv](tablas/dfl-grafico.csv))**:
  la meseta del editor inline (`TC(accent)`) está en `y=343` (2 px) y cubre **319 px**
  para el filtro 225–3525 Hz; en **LSB-D** la misma meseta aparece **espejada** sobre el
  centro del gráfico (USB `33..351` ↔ LSB `151..469`), es decir la regla de orientación
  del issue #182 se cumple **pixel a pixel** (`dfl-graph_mode_x`/`dfl_graph_layout` en
  `src/dfl_model.h`, caja del gráfico `8,320 487×96` = `DFL_GRAPH_X/Y/W/H` de
  `src/dfl_inline.c`). `dfl-editor-reset` mide 281 px (filtro por defecto) y
  `dfl-editor-tx` 303 px.

## 5. BLOCKED / UNSUPPORTED (con la razón exacta)

| Ítem | Estado | Razón |
|---|---|---|
| Ventana **Bands** (`src/dialog_bands.c`) | **BLOCKED** | En el arnés de host es un **descriptor inerte** (`tools/ui_harness/mocks/mock_main_screen.c`): no se renderiza. `app-bands`/`settings-bands` miden **la ruta** (`scenario_settle` verifica que la ventana de Ajustes se cierre y quede un diálogo corriendo), nunca sus píxeles. La tabla oficial lo declara `INERT` (`tests/qemu/dialog-coverage.csv`). |
| Los **22 diálogos** inertes del host (Callsign, Cluster, CW Keyboard, CW Trainer, EQ, Field Ops, Frequency entry, FT8, GPS, JS8, CW messages, …) | **BLOCKED** | Ídem: `tests/qemu/dialog-coverage.csv` los lista `INERT` con su fuente real; el barrido real es la corrida **C (#272)** con QEMU. Acá no se corre QEMU (recursos privados de otra corrida; prohibido compartirlos). |
| **SWR Scan** (`dialog_swrscan.c`) | **fuera de alcance** | Es un diálogo real del firmware pero **transmite mientras mide** (`radio_start_swrscan()`): requires-radio, y no es alcance de B. Sus fixtures `swrscan-*` no se capturaron. |
| Persistencia de presets del DFL (`params.db`) | **SYNTHETIC** | No hay base de datos en el host: `dfl_presets_store.c` escribe en el almacén de params mockeado. `save`/`assign` miden la UI, no la persistencia entre reinicios (esa parte es del banco/QEMU). |
| Tono y decodificado de CW (`src/cw.cpp`, `cw_autotune.c`) | **SYNTHETIC/UNSUPPORTED** | No se compilan (necesitan liquid-dsp y el lazo de audio). La página (CW 1:2)/(CW 2:2), su botón, su toggle y el widget `cw_tune_ui.c` son REALES; el texto del panel (`pannel.c`) es el placeholder del arnés. La convergencia del auto-ajuste es requires-radio. |
| RTC/reloj de pared, backlight, Bluetooth, mixer, wfview | **SYNTHETIC** | Servicios mockeados por el arnés (el reloj se envuelve con `--wrap=time` a una época fija). Se miden los diálogos, no el efecto en el hardware. |
| Estado del diálogo DFL **en TX real** / RF / SWR / sensibilidad del VFO | **BLOCKED** | Sin radio: prohibido SSH/flash/TX/sudo en esta corrida. |
| Legibilidad **física** en el LCD de 4" | **BLOCKED** | No se infiere de un PNG: requiere la pasada de hardware. |

## 6. Medido vs hipótesis

**Medido (hechos)**: las 137 capturas 800×480 con `exit_code=0` en el SHA congelado; 4437
etiquetas sin truncado/desborde/glifo faltante; 36 touch targets < 44 px; 23
solapamientos (22 por diseño + UI-018); la matriz de contraste por tema; la meseta del
gráfico DFL y su espejo LSB; el foco visible; los diálogos inertes y sus rutas.

**Hipótesis refutada por la medición (se registra para no repetirla)**: al mirar
`settings-home.png` a tamaño completo parecía que el contenido de la pantalla principal
"se veía a través" de la ventana de Ajustes (dígitos del dial, rótulos del panel DFL).
**Es falso**, y se midió cada parte:

* Las etiquetas del panel DFL inline están `hidden` en el árbol y **no se dibujan**:
  tomando su caja (`DFL USB`, `8,284 320×15`) y descontando las cajas que sí pinta el
  diálogo encima, quedan **1734 px** con luminancia mínima 67, mediana 71, p95 77 y
  **máxima 80** — sin tinta de texto (un glifo dibujado daría píxeles ≫ 150).
* La superficie de la ventana **sí** filtra, pero muy poco: contra el control
  `home-usb-20m` (los dos fixtures sólo difieren en la clave `screen`, verificado con
  `diff`), en la banda limpia `x=200..500, y=330..355` (excluye los controles del
  diálogo, que son los únicos que pintan ahí además de la superficie) la pendiente
  columna a columna es **0,043** (r=0,28) y el caso puntual más brillante —la columna
  `x=319`, luminancia detrás **196,8**— rinde **78,1** con ventana contra un piso de
  superficie de **70,4** → Δ≈7,7 sobre Δfondo=138 = **5,6 %**. Transmisión ≈4–6 %.
* El peor contraste del contenido filtrado contra la superficie es entonces ~1,1:1, muy
  por debajo del 3:1 no-texto: **no** compromete las etiquetas de la ventana.

**Hipótesis abiertas (no medidas, marcadas como tales)**: si en el panel real el
`base: Outdoor` del tema Sunlit se percibe igual de oscuro que en el render (depende del
LCD y del brillo: requires-hardware); y si el switch de 30 px de alto es operable con
dedo en el panel real (medida antropométrica, no de software).

**UNKNOWN declarado por diseño**: el contraste texto/fondo de las etiquetas cuyo fondo
no es opaco no es calculable desde el árbol de objetos → 7 filas con su razón en
`tablas/contraste-unknown.csv`; y los valores de los `lv_dropdown` no aparecen en el
árbol (LVGL dibuja su texto internamente), así que su tinta se midió **sólo por
píxeles** y así se declara en `contraste-medido.csv`.

## 7. Tests ejecutados (comandos exactos y resultados reales)

| Comando | Resultado | Log |
|---|---|---|
| `python3 tests/check_settings_grid.py` | **PASS** (8/8 macros, 48 filas de control, 5 formas de fila, origen único x=499), exit 0 | [`logs/tests-check_settings_grid.txt`](logs/tests-check_settings_grid.txt) |
| `python3 tests/check_settings_focus.py` | **PASS** (25 widgets de foco, 26 `dialog_item()`, 4 sliders con su parámetro, 3 campos de fecha con la misma caja), exit 0 | [`logs/tests-check_settings_focus.txt`](logs/tests-check_settings_focus.txt) |
| `python3 tests/check_wcag.py` | **PASS**: 0 fallas (1062 pares OK, 170 exentos, 90 informativos) en los 10 temas/paletas | [`logs/tests-check_wcag.txt`](logs/tests-check_wcag.txt) |
| `uv run x6200-ab contracts --revision d0ff5cd196…` | **PASS**: 2118 checks, 0 fallas, 0 defectos de baseline sin cambio (exit 0) | [`logs/x6200-ab-contracts.txt`](logs/x6200-ab-contracts.txt) |
| `sh tests/run_all_tests.sh --quick` | **20 OK, 1 FALLÓ**; omitidos: `qemu_apps` (SKIP sin entorno privado), `wspr`/`sstv`/`wefax` (SKIP: la imagen `x6200-uiharness` no trae `fftw3.h`/`sndfile.h`), `js8`, `webserver`. La falla es **pre-existente y ajena al alcance**: `manual_apps` = *el manual no tiene sección para la app `ACTION_APP_DFL_EDITOR` ('DFL editor')* (24 apps en el firmware vs 24 secciones, con ese nombre sin cubrir) | [`logs/run_all_tests-quick.txt`](logs/run_all_tests-quick.txt) |

Los tres primeros son los que el issue pide citar; `x6200-ab contracts` es el gate de
geometría sobre `--inspect`. Ninguno de los cinco modifica archivos del repo.

## 8. Estructura del directorio

```text
docs/evidencia/2026-10-23-ui-audit/dialogs/
  README.md                     este informe
  manifest.csv                  137 filas: screen,state,theme,source_coverage,sha,command,
                                exit_code,png,width,height,issue_id,severity,source_path,
                                observation (+ png_sha256, framebuffer_fnv1a, revisión)
  png/                          137 PNG nativos 800×480 (sin escalar, sin JPEG)
  control/                      home-usb-20m.png + .json: el control de §6 (misma pantalla
                                principal sin la ventana de Ajustes, para la medición de
                                transmisión de la superficie)
  tablas/
    fixtures.csv                resumen por captura (dimensiones, sha256, métricas, conteos)
    elementos-texto.csv         4437 etiquetas visibles: bounds, caja visible, fuente,
                                long_mode, caja natural del texto, colores, flags
    contraste-medido.csv        3205 pares medidos por píxeles (mediana, p05/p95, uniformidad,
                                cubierta-por-superficie-posterior, confiable)
    contraste-unknown.csv       los pares no calculables, con la razón
    hallazgos-estructurales.csv 59 hallazgos crudos (regla, fixture, detalle, path)
    touch-target.csv            los 36 casos de UI-015 con su detalle
    superficies-semitransparentes.csv  superficies con opacidad intermedia por captura
    dfl-grafico.csv             geometría y orientación del gráfico DFL (USB/LSB)
    cobertura.csv               3612 filas: cobertura declarada por el arnés (componente/nivel/nota)
    dialogos.csv                inventario de diálogos: real / inerte / sólo-ruta
  logs/
    render-all.log              salida de las 137 corridas
    render-rc.tsv               nombre, fixture, exit code, md5 por captura
    tests-*.txt, x6200-ab-contracts.txt, run_all_tests-quick.txt
```

## 9. Lo que este PR **no** hace

No modifica `src/`, tests, temas, fuentes, `ui_tokens.h`, settings, DSP, TX/RX ni
hardware; no crea issues de seguimiento; no propone dimensiones ni rediseño; no mergea
(el PR lo revisa e integra el coordinador en orden **A → B → C**); no declara validación
en hardware ni ROM.
