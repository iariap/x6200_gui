# UI POLISH 1 — HOME (#285, épica #284): reloj/batería legibles y Bluetooth de verdad

Corrida de **implementación** del hijo 1 de la épica #284. El contrato es el
[issue #285](https://github.com/iariap/x6200_gui/issues/285) y se siguió al pie de
la letra; **la única desviación** (una inconsistencia interna del contrato, no una
decisión de diseño) está en §7.1 y se reporta con su medición.

| | |
|---|---|
| Worktree | `/home/pablo/desa/asf/state/worktrees/x6200/ui-polish-home-284` |
| Rama | `feat/ui-polish-home-284` (la que fija la épica) |
| Base | `origin/main` = `28785016c03d7bcdb06f99ef47edd9593f2333c4` (re-validado con `git fetch`) |
| Push | **no** (el PR lo abre el orquestador contra `main`, `Refs #284; Closes #285`) |
| Radio / hardware | **no** (sin SSH, flash, deploy, TX/RF, `sudo`; nada de RF/DSP) |
| Aislamiento | contenedores `--rm` **efímeros** con scratch propio (`/tmp/opencode/ui-polish-home-284`); el build del arnés va **fuera del worktree**; cero contenedores propios al terminar (verificado) |

**Alcance implementado** (una línea por decisión del contrato):

1. **Runa de Bluetooth de DOS trazos** (`src/info.c`): brazos (path A, 7 puntos,
   15×21) + **espina vertical** (path B, `{8,1}..{8,21}`), ambos `lv_line` de 2 px
   con extremos redondeados, posicionados como una sola unidad en la media celda
   derecha de la celda WiFi/BT (`UI_INFO_BT_X_PAD 6`), con las guardas de igualdad
   por trazo y el timer de 2 s intactos.
2. **Reloj con `font_digit(&sony_44)`** y `CLOCK_LINE_Y` 5 → 3; `HH:MM:SS` 24 h
   permanente, cadencia y formato sin cambios.
3. **Porcentaje de batería con `&sony_22`** (icono y fuente externa siguen en
   `&sony_20`); `STATUS_GAP 4`, insets 8 px y la re-ancla dinámica de #279
   preservada.
4. **Banda de estado opaca** (`y40..60`, full-width, `TC(bar)`), hija del panel y
   creada **antes** que hora/fantasma/estado (queda debajo).
5. **Franja segmentada `sony_44`** por el **generador pinneado**
   (`tools/fonts/candidates.json`: `segmented_slots [30,38] → [30,38,44]`) →
   `dseg7_44.c`, `dseg7mini_44.c`, `dseg14_44.c` generados + `font_families.*`.

**No se tocó**: S-meter, espectro, waterfall, band plan, dígitos de frecuencia,
softkeys, panel 206/209×61, paleta de temas, lógica de radio, TX/RX, BT/A2DP,
`src/dialog_bt.c` (es de #286), persistencia ni `font_digits`.

---

## 0. Cómo se reproduce todo (comandos exactos y exit codes)

```sh
WT=/home/pablo/desa/asf/state/worktrees/x6200/ui-polish-home-284
SCRATCH=/tmp/opencode/ui-polish-home-284          # build + JSON crudos (fuera del worktree)
IMG=x6200-uiharness

# 1. El arnés, DOS binarios: base y con el cambio (out-of-source, build propio)
docker run --rm --entrypoint bash --user "$(id -u):$(id -g)" -v "$WT:/src" \
  -v "$SCRATCH:/scratch" -w /src/tools/ui_harness $IMG -lc \
  "cmake -S /src/tools/ui_harness -B /scratch/build-base -DHARNESS_REVISION=28785016… && cmake --build /scratch/build-base -j8"
# idem /scratch/build-post con el árbol ya modificado

# 2. Capturas antes/después (162 escenarios de la matriz, 18 PNG curados por fase)
cd "$WT/docs/evidencia/2026-10-23-ui-polish/home"
python3 capturar.py --phase before --build "$SCRATCH/build-base" --scratch "$SCRATCH"
python3 capturar.py --phase after  --build "$SCRATCH/build-post" --scratch "$SCRATCH"

# 3. Medición (geometría del árbol LVGL vivo + tinta/contraste sobre los píxeles)
python3 medir.py --phase before --scratch "$SCRATCH"     # -> mediciones/*-before.csv
python3 medir.py --phase after  --scratch "$SCRATCH"     # -> mediciones/*-after.csv
python3 crops.py                                          # -> crops/ (ayuda de lectura)
python3 medir_flushes.py --before "$SCRATCH/build-base" --after "$SCRATCH/build-post" \
  --scratch "$SCRATCH/perf"                               # -> mediciones/flushes-suite.csv
```

| Comando | Exit | Log |
|---|---|---|
| `python3 tools/fonts/fonts.py fetch --licenses` | 0 | `logs/fonts-fetch.log` |
| `python3 tools/fonts/fonts.py generate` | 0 | `logs/fonts-generate.log` |
| `python3 tools/fonts/fonts.py verify` | 0 | `logs/fonts-verify.log` |
| `python3 tests/check_info_bar.py` | 0 | `logs/check-info-bar.log` |
| `python3 tests/check_wcag.py` | 0 | `logs/check-wcag.log` |
| `python3 tests/check_wcag_rendered.py` | **1** (7 entradas del baseline resueltas: "baseline OBSOLETO") | §5 |
| `python3 tests/check_wcag_rendered.py --update-baseline` | 0 (**+0 / −7**) | `logs/wcag-rendered-update-baseline.log` |
| `python3 tests/check_wcag_rendered.py` (re-run) | 0 | `logs/wcag-rendered.log` |
| `sh tests/check_harness_natural.sh` | **1** el primer intento (`SETTLE_PX` medido distinto) → **0** | `logs/check-harness-natural.log` (§6) |
| `sh tests/check_harness_wakeups.sh` | 0 | `logs/check-harness-wakeups.log` |
| `sh tests/check_harness_determinism.sh` | 0 | `logs/harness-determinism.log` |
| `python3 medir_flushes.py …` | 0 (205 fixtures, 0 fuera de presupuesto) | `logs/medir-flushes.log` |
| `sh tests/run_all_tests.sh --quick` | 0 (**22 OK, 0 fallos**) | `logs/run_all_tests-quick.log` (§5, §9) |
| test nativo del reloj (`gcc … && ./clock_tests`, con `-ftrivial-auto-var-init=pattern`) | 0 (**137 comprobaciones, 0 fallos**) | `logs/native-clock.log` |
| `fixtures-gaps/*.scn` con el binario nuevo | `net-*`/`vision`/`bar-legacy` **OK**; los 3 diálogos FALLA (necesitan el binario `ui_harness_dialogs`) | `logs/gaps-fixtures.log` |

`--inspect` está usado en **todas** las capturas: la geometría sale del árbol LVGL
vivo, no de la imagen. La tinta y el contraste salen de los píxeles (método
declarado en `medir.py`, el mismo de `tests/check_wcag_rendered.py`).

---

## 1. Capturas antes/después (800×480, nativas, sin escalar)

`png-before/` y `png-after/` tienen 18 PNG cada una, misma lista de escenarios:
los 6 temas, los 3 estados de BT (`bt_state off|on|connected`, claves de
`fixtures-gaps`), los 6 porcentajes (`--%`, 0, 9, 76, 99, 100), fuente ausente,
carga y la familia segmentada (`digits dseg7`). Los escenarios están en
`scenarios/` (generados por `capturar.py`): la matriz de 144 estados (6 temas ×
6 pct × 2 fuentes × 2 cargas) más 18 escenarios curados, 162 en total. El JSON de
cada uno (con el árbol `--inspect`) queda en el scratch —es de donde salen las
mediciones— porque el contrato pide los PNG de contraste, no 288 imágenes.

Recortes ampliados (vecino más cercano; **no** son prueba de legibilidad física):

* `crops/runa-bt-antes-despues-x8.png` — el zigzag de un trazo (arriba), la runa
  con espina apagada/encendida/conectada (abajo, 3 estados).
* `crops/reloj-<tema>-antes-despues-x3.png` — el panel del reloj por tema.
* `crops/reloj-temas-despues-x3.png`, `crops/banda-porcentajes-despues-x5.png`.

---

## 2. Matriz geométrica (árbol LVGL vivo, `--inspect`)

Medición del panel del reloj y de la runa, en las **162 capturas** por fase
(`mediciones/reloj_estado-*.csv`, `mediciones/runa_bt-*.csv`,
`mediciones/colisiones-*.csv`). Cajas absolutas `(x0,y0,x1,y1)`; el ancho/alto de
la hora es el de su **tinta** (el label mide todo el panel, 209 px).

| | antes | después | contrato |
|---|---|---|---|
| hora: caja | `(591,5,800,y)`, alto 19–31 (19–20 segmentadas) | `(591,3,800,36)`, alto 21–34 (21–23 segmentadas) | dentro de `x591..799`, `y ≤ 39`, ≥ 2 px de margen |
| hora: tinta (alto × ancho) | 18×107 (Aurora) … 21×122 (Trail) | **20×114 … 24×129** | "estrictamente más alta y más ancha" ✅ en los 6 temas (y en dseg: 20×100 → 22×112) |
| banda de estado | **no existe** | `(591,40,800,61)`, opaca, full-width, no clickeable, no scrollable | exactamente una, `y40..60`, `TC(bar)` |
| fuente externa | `(599,41,…)` | `(599,41,656,59)` | banda `y41..60`, inset 8 px |
| icono de batería | `(748,41,764,59)` (Aurora) | `(740,41,756,59)` | 16 px, `sony_20` |
| porcentaje | `(768,46,792,59)` 24×13 | `(760,41,792,59)` 32×18 | `sony_22` |
| hueco icono↔pct | 4 px caja / 5–7 px tinta | 4 px caja / **6** px tinta | ≥ 4 px |
| hueco ext↔icono | 81–106 px | 72–99 px | ≥ 6 px |
| `text_overflow` / `text_truncated` | 0 | **0** | 0 |
| colisiones (cualquier hallazgo) | 162/162 (sin banda) | **0/162** | 0 |
| runa BT: trazos | 1 (`lv_line` 167,34,16,22) | **2** (brazos `173,34,190,57`, espina `173,34,183,57`) | dos trazos, espina vertical |
| runa BT: dentro de x167..195/y31..61 | 1 trazo | **2/2** ✅ | con el trazo de 2 px contado |
| espina vertical (píxeles, ver §3) | 0.35 de columna máxima (**no** hay espina) | 0.88–0.92 (**sí**) | la runa, no un zigzag |

El `banda_box` `(…40…61)` es la mitad inferior del panel de 61 px: la esquina
derecha exclusiva 800 coincide con el borde del panel y 61 con el del bitmap
(`clock_style`).

---

## 3. La espina de la runa: verificación sobre la fuente y sobre los píxeles

El contrato pide "regression check a nivel fuente que afirme que la espina
vertical existe y un screenshot que pruebe una runa de Bluetooth, no un zigzag".
Van los tres niveles:

1. **Fuente** (`tests/check_info_bar.py`, exit 0): `bt_spine_pts[]` tiene 2 puntos
   **con la misma x** (`8`) y va de `y1` a `y21`; hay exactamente **2**
   `lv_line_create()`; los dos llevan `UI_INFO_BT_LINE_W` y
   `lv_obj_set_style_line_rounded`; la x sale de
   `UI_INFO_ORIGIN_X + UI_INFO_WIFI_COL*UI_INFO_COL_PITCH + UI_INFO_WIFI_W +
   UI_INFO_BT_X_PAD` y la y de `UI_INFO_WIFI_ROW*UI_INFO_ROW_PITCH +
   UI_INFO_BT_DY`; los tokens (`UI_INFO_BT_ARM_POINTS 7`,
   `UI_INFO_BT_SPINE_POINTS 2`, `UI_INFO_BT_W 15`, `UI_INFO_BT_H 21`,
   `UI_INFO_BT_X_PAD 6`) coinciden con los arrays.
2. **Árbol vivo** (`mediciones/runa_bt-after.csv`, 18/18 capturas): 2 trazos, uno
   más angosto (la espina, 10 px de objeto = 8 + trazo), **mismo origen**, dentro
   de la caja de la media celda con el trazo contado. El `scenario_settle` del
   arnés (`tools/ui_harness/src/screens.c`, actualizado a los DOS trazos)
   corre en cada fixture `net-*` y corta la corrida si no hay 2 trazos con anchos
   distintos y el color del estado inyectado: los 8 fixtures `net-*` pasan
   (`logs/gaps-fixtures.log`).
3. **Píxeles** (`vertical_spine()` en `medir.py`): dentro de la caja de la runa,
   alguna columna tiene que cubrir a lo alto la runa (≥ 80 % de las filas con un
   píxel distinto del fondo **de la misma fila** a la derecha del rune; el bitmap
   del panel tiene gradiente, así que un color modal único confunde bitmap con
   tinta). Medido: **0.35 antes** (zigzag solo, ninguna columna pasa de 0.52) vs
   **0.88–0.92 después** (2 columnas a lo alto, x=180–181 = centro de la runa).
   El umbral 12 es el mismo margen que el A/B usa para ruido/antialiasing
   (`budgets.yaml::pixel_tolerance`).

---

## 4. Matriz de contraste por tema (medida sobre el render)

Misma fórmula y método que `tests/check_wcag_rendered.py` (fondo local muestreado
±4 px del trazo, mediana de los pares). `ratio` es la mediana; `p05` el peor 5 %.
Fuente: `mediciones/reloj_estado-*.csv` (columnas `*_ratio`, `*_ratio_p05`,
`*_fondo`).

| tema (fuente) | `13.8 V` antes → después | p05 antes → después | `76%` antes → después | p05 antes → después | hora antes → después |
|---|---|---|---|---|---|
| Aurora (inter) | 7.10 → **13.39** | 5.32 → 13.39 | 5.58 → **13.39** | 5.01 → 13.39 | 9.99 → 10.01 |
| Cathode (ibmplexsans) | 4.76 → **12.91** | 4.50 → 12.91 | 4.94 → **12.91** | 4.64 → 12.91 | 9.25 → 9.36 |
| **Redshift** (sourcesans3) | **4.00 → 11.43** | **3.60 → 11.43** | **4.08 → 11.43** | **3.69 → 11.43** | 6.37 → 6.40 |
| Sprint (robotocondensed) | 5.95 → **14.66** | 5.58 → 14.66 | 5.92 → **14.66** | 5.60 → 14.66 | 11.34 → 11.59 |
| Sunlit (notosans) | 4.67 → **12.31** | 3.69 → 12.31 | 4.65 → **12.31** | 3.88 → 12.31 | 3.10 → 3.12 |
| Trail (ibmplexsans) | 5.24 → **11.77** | 4.95 → 11.77 | 5.37 → **11.77** | 4.19 → 11.77 | 8.53 → 9.76 |

El fondo medido bajo la banda pasa a ser **exactamente el rol `bar` del tema**
(p. ej. Aurora `(44,33,62)` = `#2C213E`) en los 162 estados, y por eso la mediana
y el p05 coinciden: la banda es opaca y uniforme. **Redshift** (el defecto P2 de la
auditoría #270) pasa de 4.0/3.8 a 11.43 y **Sunlit** de 4.67/3.89 a 12.31 (el
texto chico de la fuente externa era el otro par bajo AA).

La hora **no** cambia de fondo (sigue sobre el bitmap del panel, como el contrato
pide) y su ratio queda igual: Sunlit 3.10 → 3.12 (texto grande, AA pide 3.0;
la auditoría ya lo había medido así y #285 no lo toca).

---

## 5. Gates de contraste y de tests

* `python3 tests/check_wcag.py` → **0**: 1062 pares OK, 170 exentos, 90
  informativos, **0 fallas** (pares estáticos por rol: la banda usa un rol
  existente, no un color nuevo).
* `python3 tests/check_wcag_rendered.py` → **1** con la lista exacta:
  * **7 entradas del baseline resueltas** (las de Redshift del reloj/estado) →
    `--update-baseline` imprime `+0 / −7` y el archivo pasa de 18 a **11**
    entradas (`git diff --stat`: 105 líneas borradas, 0 agregadas);
  * **0 violaciones nuevas y 0 regresiones**;
  * quedan **11** violaciones preexistentes (5 del diálogo de BT + 6 del band
    plan, "ALL MODES / SSB" y "CW"), que son las de **#286**.
* `python3 tests/check_info_bar.py` → 0 (contrato #251 intacto + runa #285).
* Test nativo del reloj → 137 comprobaciones, 0 fallos (incluye la banda: orden
  de creación, parenthood, flags, `TC(bar)` opaco, `y_ofs 40`, alto 21,
  `LV_PCT(100)`; `font_digit(&sony_44)` en hora y fantasma; `&sony_22` en el
  porcentaje; y las guardas OPT-4: 100 ticks idénticos no escriben **ni fondo**).
* `sh tests/run_all_tests.sh --quick` → **0**: **22 OK, 0 fallos**; omitidos
  declarados por el runner: `qemu_apps` (necesita el chroot privado propio),
  `wspr`/`sstv`/`wefax` (la imagen local no trae `fftw3.h`/`sndfile.h`; la imagen
  `full` sí), `js8` (`--quick`) y `webserver` (sin checkout). Ningún test se
  declara pasado sin su log.

---

## 6. Costo de render (el proxy de `flushes`), medido de verdad

`medir_flushes.py` corre **la suite entera** (205 fixtures de
`tools/ui_harness/fixtures/`) con los dos binarios y compara `metrics.flushes` y
`metrics.flushed_pixels` contra los presupuestos del gate de Performance de
`x6200-ab` (`budgets.yaml`: +20 % flushes / +10 % px). Crudo:
`mediciones/flushes-suite.csv`.

| | resultado |
|---|---|
| flushes | **0.00 %** en las 205 fixtures (idéntico, ninguna se movió) |
| `flushed_pixels` | **máximo +0.04 %** (`dfl-usb-d-tema-sunlit`, `main-tema-sunlit*`), presupuesto +10 % |
| fuera de presupuesto | **0** |
| framebuffer distinto | 205/205 (es el cambio visual pedido: la banda y el reloj) |

El reloj invalida más por tick (la etiqueta creció 2–3 px), y se midió con el
mismo parche local de `__wrap_time` que el #250 usa para esto (no versionado):
**10486 px** (`214×49`, `main-natural`, antes 10272) y **7704 px** (`214×36`,
`main-frecuencia-dseg`, antes 7490) → **+214 px por tick** (+2.1 % / +2.9 %). No
aparece en el histograma del banco natural porque el banco fija `time()` y la
guarda de igualdad (#150) apaga la invalidación: en la radio cambia una vez por
segundo.

`tests/check_harness_natural.sh` **falló el primer intento** por el número
medido, no por una regresión: el *settle* pasó de `1100352` a **`1100896` px**
(+544 px, +0.05 %; la unión de invalidaciones del arranque incluye al reloj, que
cambió). Se re-midió sobre este binario y el control `--display-off` lo verifica
por diferencia (`SETTLE_PX − 384000 = 716896`). Se actualizó la constante **con
su número medido** en `tests/natural_evidence.py`; **ningún umbral se relajó**.

`tests/check_harness_wakeups.sh` → 0 (14 wakeups con drive / 11 sin él, sin
hilo de tick). `tests/check_harness_determinism.sh` → 0 (205 fixtures × 2 con el
mismo sha256).

---

## 7. Desviaciones, PENDING y BLOCKED

### 7.1 Δ **y de la runa de BT**: el contrato se contradice y se siguió su parte verificable

El contrato pide dos cosas **incompatibles** para la misma runa:

* "use **the existing y equation** `UI_INFO_ORIGIN_Y + UI_INFO_WIFI_ROW*UI_INFO_ROW_PITCH + UI_INFO_BT_DY`" → `y0 = 5 + 26 + 8 = **39**`;
* "Update compile-time geometric bounds to ensure both objects fit within
  x167..195 and **y31..61, accounting for 2px stroke**" → con `y0 = 39`,
  `39 + 21 (alto) + 2 (trazo) = **62 > 61**: **no entra**;
* y el checklist: "Two-stroke Bluetooth with centre spine, **centred in 28px
  half-cell**" → la celda mide `y31..57`; con `y0 = 39` la runa cae `y40..60`,
  **3 px por debajo del fondo opaco de la celda**.

La ecuación que el contrato llama "la existente" es la del `_Static_assert` viejo
(`UI_INFO_ORIGIN_Y + …`), pero el **código** usaba `UI_INFO_WIFI_ROW*UI_INFO_ROW_PITCH
+ UI_INFO_BT_DY` (`y0 = 34`): ahí está la inconsistencia (el assert y el código ya
discrepaban). Con `y0 = 34` **todo el contrato verificable se cumple**:
`34 + 21 + 2 = 57 ≤ 61` y `173 + 15 + 2 = 190 ≤ 195`, y la runa queda centrada en
la celda (medido: objeto `173,34 → 190,57`, celda `x167..195 / y31..57`). La
auditoría #270 (la fuente del contrato) tampoco reporta ningún defecto vertical:
mide la runa en `167,34,16,22` y su único hallazgo es el corrimiento **horizontal**,
que es exactamente lo que #285 corrige con `UI_INFO_BT_X_PAD 6` (`x 167 → 173`,
centro 181).

**Decisión**: se implementó `y0 = 34` (la ecuación "existente" **del código**),
porque es la única lectura que satisface las otras dos cláusulas vinculantes del
mismo contrato (los límites con el trazo contado y el centrado en la media
celda), y los `_Static_assert` de `src/ui_tokens.h` la verifican en compilación
(`UI_INFO_BT_Y0 + UI_INFO_BT_H + UI_INFO_BT_LINE_W ≤ 61`). **Si el orquestador
prefiere el literal `ORIGIN_Y + …`, es un `+ UI_INFO_ORIGIN_Y` en una línea de
`src/ui_tokens.h` y otra en `src/info.c`**; con ese literal los asserts
compilados fallan (62 > 61) y la runa baja 5 px, quedando fuera del fondo de la
celda. No se relajó ningún umbral para esquivarlo.

### 7.2 PENDING / BLOCKED (con la causa exacta)

* **Legibilidad física en el LCD de 4" — PENDING por contrato.** No existe en el
  repo el área activa del panel en mm, así que cualquier afirmación de tamaño
  físico sería inventada. Lo entregado: PNG 800×480 a 1:1, recortes
  **declarados** como ampliados, y el alto de tinta en px de cada texto (§2).
* **Las 11 violaciones restantes del baseline de contraste — de #286.** No se
  tocó ninguna: quedan listadas en `logs/wcag-rendered.log` y en el baseline.
* **Las 5 violaciones del diálogo de BT (`bt-dialog-*`)** no se pueden resolver
  desde #285: `src/dialog_bt.c` es explícitamente **no-goal** ("no changes to
  `src/dialog_bt.c` (belongs to #286)"). La banda de estado **sí** resuelve las
  que el diálogo deja ver del reloj (`bt-dialog-tema-redshift /37/1` y `/37/2`
  desaparecieron del baseline).
* **`uv run x6200-ab compare/contracts` (el framework A/B completo) — no corrido
  en esta corrida (PENDING).** Su *proxy* de Performance es exactamente el de §6 y
  se midió directamente sobre las 205 fixtures con los dos binarios y los mismos
  presupuestos; correr el framework implicaría crear worktrees y builds propios
  por revisión (fuera del alcance del worktree de la corrida). El orquestador
  puede pedirlo; no hay ninguna medición inventada en su lugar.
* **Los 3 fixtures de diálogo de `fixtures-gaps/`** (freq/CW/voz) necesitan el
  binario `ui_harness_dialogs` (`-DHARNESS_REAL_DIALOGS=1`), igual que antes de
  este cambio; con el binario por defecto cortan (declarado en el README del
  arnés). No es una regresión de #285.
* **Los estados `bt_state` inyectados son SYNTHETIC** (el arnés no compila
  `src/bt_state.c`/`src/wifi.cpp`): prueban layout/color del estado inyectado, no
  la función real de BT/WiFi (sin radio). Igual que en la auditoría #270.
* **Sanitizers**: el host de esta corrida no tiene `libasan`/`libubsan`
  (`ld: cannot find /usr/lib64/libasan.so.8.0.0`), así que el stage nativo corre
  con el fallback documentado del runner (compilación normal + analizador
  estático); el test del reloj se corrió con
  `-ftrivial-auto-var-init=pattern`, que es el veneno que hace determinista el
  UI-009.

---

## 8. El generador de fuentes: corrió, con el diff esperado y nada más

`tools/fonts/candidates.json`: `segmented_slots [30,38] → [30,38,44]` (y el
comentario del manifiesto + `tools/fonts/README.md` actualizados para nombrar el
`sony_44` del reloj). Después:

```sh
python3 tools/fonts/fonts.py fetch --licenses   # inputs pinneados, sha256 verificados
python3 tools/fonts/fonts.py generate           # lv_font_conv 1.5.3 (pinned)
python3 tools/fonts/fonts.py verify             # -> ok, 3 files por familia segmentada
```

El árbol generado cambia **solo** lo esperado (reproducibilidad probada: los
otros ~300 `.c` de familias quedaron **byte-idénticos**):

```text
 src/fonts/font_families.c   | 6 +++---
 src/fonts/font_families.h   | 2 +-
 src/fonts/font_names.h      | 3 +++
 3 files changed, 7 insertions(+), 4 deletions(-)
 ?? src/fonts/dseg7_44.c  ?? src/fonts/dseg7mini_44.c  ?? src/fonts/dseg14_44.c
```

`FONT_SEGMENTED_SLOTS(X)` pasa a `X(30) X(38) X(44)` y las tres filas
`font_digit_families` apuntan al slot 44. Los `.c` nuevos llevan su `Opts` con
`--lv-fallback sony_44` y `match: ink` (ink 21 = target 21 en los tres):

| archivo | tamaño lv_font_conv | fallback | line_height |
|---|---|---|---|
| `dseg7_44.c` | 22 px | `sony_44` | 23 |
| `dseg7mini_44.c` | 23 px | `sony_44` | 24 |
| `dseg14_44.c` | 21 px | `sony_44` | 21 |

Nada de esto se escribió a mano: el ID persistido `font_digits` y las fuentes de
frecuencia no se tocaron.

---

## 9. `run_all_tests.sh --quick` y estado final del árbol

`sh tests/run_all_tests.sh --quick` → **0**: **22 OK, 0 fallos**, con los omitidos
declarados por el runner (`qemu_apps`, `wspr`, `sstv`, `wefax`, `js8`,
`webserver`; el motivo de cada uno, en `logs/run_all_tests-quick.log`). Ningún
test se declara pasado sin su log.

Archivos de esta evidencia:

```text
docs/evidencia/2026-10-23-ui-polish/home/
  README.md          este documento
  capturar.py        genera los 162 escenarios y captura la fase (--phase before|after)
  medir.py           geometria del arbol + tinta + contraste WCAG (-> mediciones/)
  medir_flushes.py   el proxy de flushes sobre las 205 fixtures (-> mediciones/flushes-suite.csv)
  crops.py           recortes ampliados (ayuda de lectura)
  scenarios/         162 .scn de la matriz
  png-before/        18 PNG 800x480 (antes)
  png-after/         18 PNG 800x480 (despues)
  crops/             9 recortes ampliados
  logs/              logs de cada comando (fonts, gates, tests, gaps)
                     + rerun-gates.sh (la corrida secuencial de los gates sobre el arbol congelado)
  mediciones/        CSV por fase + flushes-suite.csv
  capturar-*.cmd     los comandos exactos que corrieron los contenedores
```

Comandos y logs: todos los de la tabla de §0 quedaron en `logs/` con su exit
code. Los scripts (`capturar.py`, `medir.py`, `medir_flushes.py`, `crops.py`) y
los 162 escenarios están en este directorio, así que la corrida se reproduce
entera sin depender del scratch.

---

## 10. El diff (qué se toca y qué no)

`git diff --stat` del cambio (sin este directorio de evidencia):

```text
 src/clock.c                          |   67 +-   reloj sony_44, pct sony_22, banda de estado
 src/info.c                           |   81 +-   runa de BT: brazos + espina (dos lv_line)
 src/ui_tokens.h                      |   42 +-   tokens de la runa + _Static_assert con el trazo
 src/fonts/dseg14_44.c                | 1689 +    generado (nuevo)
 src/fonts/dseg7_44.c                 | 1403 +    generado (nuevo)
 src/fonts/dseg7mini_44.c             | 1461 +    generado (nuevo)
 src/fonts/font_families.c            |    6 +-   FONT_SEGMENTED_SLOTS X(30) X(38) X(44)
 src/fonts/font_families.h            |    2 +-
 src/fonts/font_names.h               |    3 +    (tabla del inspector)
 tests/check_info_bar.py              |  101 +-   contrato de la runa de BT
 tests/native/clock_stubs.c           |   44 +-   la banda + sony_22/44 + TC(bar)
 tests/native/clock_stubs.h           |   32 +-
 tests/native/stubs_clock/lvgl/lvgl.h |    3 +
 tests/native/stubs_clock/styles.h    |    4 +-
 tests/native/stubs_clock/theme.h     |    1 +
 tests/native/test_clock.c            |   38 +-
 tests/natural_evidence.py            |   70 +-   SETTLE_PX re-medido + BT por region
 tests/wcag_rendered_baseline.json    |  105 -    -7 entradas (las 7 de Redshift)
 tools/fonts/README.md                |    5 +-
 tools/fonts/candidates.json          |    5 +-   segmented_slots [30,38] -> [30,38,44]
 tools/ui_harness/src/screens.c       |   68 +-   el settle pasa a los DOS trazos
```

**Fuera del diff**: `src/dialog_bt.c` (#286), `src/styles.c`, la paleta de temas,
`src/spectrum.c`/`waterfall.c`/`band_plan.c`/`meter.c`, `lv_conf.h`, `rootfs/`,
`sql/` y todo lo de RF/DSP/persistencia. `docs/AB_TESTING.md` se dejó intacto a
propósito: es una **bitácora fechada** de la migración de tokens de UI-05
(2026-10-14) y reescribirla falsearía el registro.

### Trazabilidad del SHA

Las capturas (`--phase before` y `--phase after`) sellan
`revision = 28785016c03d7bcdb06f99ef47edd9593f2333c4` (el `HEAD` base con el que se
capturaron; el commit del cambio se creó después, así que el sello del arnés es el
de la base). Después se rebasó sobre el `main` nuevo (**`b7660ed0`**, §11), que
solo cambia el gate de contraste y su baseline: el gate rebasado re-renderizó los
6 temas y los PNG son byte-idénticos a `png-after/`, así que el árbol capturado y
el del commit final son el mismo render. Los gates de §5/§6/§11 se re-corrieron
sobre el árbol ya rebasado.

---

## 11. Rebase sobre el `main` nuevo (#290) y regeneración del baseline

`main` se movió (`28785016` → **`b7660ed0`**, el fix del gate de contraste #290:
`tests/check_wcag_rendered.py` compila desde un checkout limpio y `--update-baseline`
imprime también los ratios que **cambian** (`~`); el baseline de `main` quedó con
**18** entradas, cuatro de ellas re-baselinadas porque #279 movió el render). La
rama se rebasó sobre ese `main` para que el CI corra **una sola vez** sobre el
estado final (en `pull_request` GitHub usa el workflow de la rama head: una rama
sin rebasar corre los gates viejos).

* **Un solo conflicto**: `tests/wcag_rendered_baseline.json` (los dos lo tocamos).
  Se resolvió **regenerando**, no mergeando texto:
  1. `git checkout origin/main -- tests/wcag_rendered_baseline.json` (se partió de
     las 18 de `main`, con las 4 de #290 ya re-baselinadas);
  2. `python3 tests/check_wcag_rendered.py --update-baseline` →
     **`- bt-dialog-tema-redshift /37/1 (4.0)`, `- bt-dialog-tema-redshift /37/2 (4.08)`,
     `- main-tema-redshift /37/1 (4.0)`, `- main-tema-redshift /37/2 (4.08)`,
     `- settings-grupo-operation-tema-redshift /37/1 (4.0)`,
     `- settings-grupo-operation-tema-redshift /37/2 (4.08)`,
     `- settings-tema-redshift /37/2 '50%' (3.78)`** → **`11 entradas, +0/-7/~0`**.
* `tests/check_wcag.py` **no se tocó** (el fix del gate es de #290 y ya está en main).
* **El render es el mismo**: el gate, ya rebasado, re-renderizó los 6 temas y los
  PNG son **byte-idénticos** a `png-after/home-<tema>.png` (los 6, sha256 iguales),
  así que la matriz de §2/§4 (tinta de la hora más alta/ancha, contraste por tema)
  sigue valiendo tal cual.
* Gates re-corridos sobre el árbol rebasado:

  | comando | exit | resultado |
  |---|---|---|
  | `python3 tests/check_info_bar.py` | **0** | contrato #251 + runa #285 |
  | `python3 tests/check_wcag.py` | **0** | 1062 pares OK, 0 fallas |
  | `python3 tests/check_wcag_rendered.py` | **0** | `PASS`, 11 preexistentes (= las de #286), 0 nuevas, 0 regresiones |
  | `sh tests/run_all_tests.sh --quick` | **0** | 22 OK, 0 fallos (mismos SKIP declarados) |

---

## 12. El CI (`UI Functional Tests`) y la coherencia de la evidencia natural

El primer run del PR (run `40037883146`) pasó **Visual Regression**, **Performance**
(13m58s), **Quality Contracts** y el gate de contraste, y falló **Functional** en el
paso `Python framework (ruff, format, ty, unit tests)`:

```text
FAILED tools/ab_testing/tests/test_natural.py::test_evidence_is_profiled_delimited_and_deterministic
E  AssertionError: FAIL: metrics.flushed_pixels = 12308352
   (the settled capture must keep its 1100896 px and the natural frames add their own)
1 failed, 232 passed, 13 deselected
```

### 12.1 De dónde sale el valor esperado (son **tres** lugares, no uno)

| dónde | qué es | estado antes del arreglo |
|---|---|---|
| `tests/natural_evidence.py::SETTLE_PX` | la **constante** del checker | actualizada por mí a `1100896` |
| `tools/ab_testing/reports/NATURAL-ECOFINAL/*.json` | el **artefacto de evidencia comprometido** (regenerable) | **viejo**: `metrics.flushed_pixels = 12308352` = `1100352` (settle) + `11208000` (frames) |
| `tools/ab_testing/tests/test_natural.py` | **pins** del framework (pytest) sobre ese artefacto | **viejos**: `1100352`, `716352`, `clock 10272 px` |

Yo había actualizado solo la constante: el checker (que corre el gate local **y** el
pytest del CI) pasó a exigir `1100896 + frames` contra un artefacto que seguía
teniendo el settle viejo ⇒ el CI falló. Los pins del pytest estaban viejos por lo
mismo.

**Por qué mi `run_all_tests.sh --quick` no lo vio**: ese comando **no** corre el
pytest del framework; el job del CI corre `uv run pytest -m "not e2e"` (el log del
CI dice `13 deselected`, y el `--quick` no incluye ese paso). Desde ahora, el
comando del CI se corre tal cual (§12.3).

### 12.2 El arreglo (regenerar el artefacto + pins coherentes, no "para que pase")

1. **Artefacto regenerado** con el comando documentado (el mismo de las re-baselines
   anteriores): `sh tests/check_harness_natural.sh` (rebuild + los 5 modos del gate)
   más `--bench-natural` sobre `main-natural-eco`, copiando las 7 salidas
   (JSON+PNG) a `tools/ab_testing/reports/NATURAL-ECOFINAL/`, con el bloque nuevo
   en `sha256.txt` y la sección de re-baseline en su `README.md`.
2. **Pins del pytest** actualizados **con los números medidos**:
   `1100352 → 1100896`, `716352 → 716896`, `clock 10272 px → 10486 px`, y los
   docstrings/comentarios con la procedencia (`AB-HOME285`).
3. **Dos números de documentación** que la corrida también movía y estaban viejos
   desde #250: el comentario del gate `tests/check_harness_natural.sh` (reloj
   10272 → 10486) y la lista de rectángulos del `tools/ui_harness/README.md`
   (reloj `580,0 220x72`/15840 → `586,0 214x49`/10486; el rectángulo del BT ya no
   aparece por la guarda de igualdad de OPT-4 #150).

### 12.3 El delta medido (y por qué es el de mi cambio)

Nada del perfil natural se movió; lo único que se movió es el **settle de la
captura** (`metrics`), y **+544 px en los siete modos, con los mismos flushes**:

| modo | antes (`65f7842c`) | después (`3cc409e9`) | delta |
|---|---|---|---|
| `natural`/`run1` | 72 / 12308352 | **72 / 12308896** | +544 px, 0 flushes |
| `forced` | 9 / 2252352 | **9 / 2252896** | +544 |
| `static` | 13 / 1290752 | **13 / 1291296** | +544 |
| `cli` | 21 / 3656352 | **21 / 3656896** | +544 |
| `off` | 5 / 716352 | **5 / 716896** | +544 |
| `main-natural-eco` | 24 / 3762602 | **24 / 3763146** | +544 |
| `natural.totals` (los 40 frames) | 11208000 px / 66 rects / 66 flushes | **idéntico** | 0 |

**Explicación medida, no hipótesis**: los 40 frames naturales **no** dependen del
reloj (con `time()` fijo el texto no cambia y la guarda de igualdad del #150 apaga
la invalidación), así que el perfil no se mueve; lo que cambia es la invalidación
de **arranque** del panel del reloj: la etiqueta de la hora es 2 px más alta
(`sony_44`) y la banda de estado es una superficie nueva, y esa unión es 544 px
más grande. El costo **por tick** cuando el segundo cambia (medido con el reloj de
pared avanzando con el reloj virtual) es **+214 px** (10272 → 10486 px, `214×49`),
o sea +2.1% de la invalidación del reloj, una vez por segundo en la radio. El
proxy de flushes sobre las 205 fixtures (§6) sigue en **0.00% de flushes / +0.04%
de px**, así que el +544 px del settle no es una regresión de operación: es un
costo de arranque de 544 px (0.05% de la captura).

### 12.4 El comando del CI, corrido tal cual

`working-directory: tools/ab_testing` (`logs/ci-python-framework.log`):

| comando | exit | salida |
|---|---|---|
| `uv sync` | 0 | deps del grupo dev instaladas |
| `uv run ruff check .` | **0** | `All checks passed!` |
| `uv run ruff format --check .` | **0** | `200 files already formatted` |
| `uv run ty check src tests` | **0** | `All checks passed!` |
| `uv run pytest -m "not e2e"` | **0** | **`233 passed, 13 deselected`** (0 failed) |

Y el resto de los gates locales, re-corridos sobre el árbol del arreglo (el arreglo
solo toca la evidencia, los pins del framework y documentación: **ningún binario de
la ROM cambia**): `check_info_bar` **0**, `check_wcag` **0**,
`check_wcag_rendered` **0** (11 preexistentes, PASS), natural/wakeups/determinism
**PASS**, el proxy de flushes **0 fuera de presupuesto** y
`sh tests/run_all_tests.sh --quick` **0** (22 OK, 0 fallos).
