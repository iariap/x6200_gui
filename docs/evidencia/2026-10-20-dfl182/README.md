# #182 — Espejo horizontal USB/LSB del editor DFL inline

Corrida `factory/dfl-mirror-182`. **Base**: `a3ff5e90` (trunk tip
`lu2dsx/x6200-rom`, con el editor inline del #179). **HEAD de la corrida**:
`3d8fdc51`. Worktree aislado `state/worktrees/x6200/dfl-mirror-182`.
Evidencia en `docs/evidencia/2026-10-20-dfl182/`.

> **Validación física: FUERA de esta corrida.** El owner hace la prueba en su
> radio después del merge. El agente NO la exige ni la declara: queda
> **pendiente prueba física posterior del owner**.

## 0. Qué se corrigió y por qué

El espectro muestra USB a la **derecha** del VFO y LSB a la **izquierda**, pero
la gráfica esquemática del editor DFL inline (#179) se dibujaba **siempre** con
LOW a la izquierda y HIGH a la derecha, en los dos modos. `draw_cb()`,
`graph_event_cb()` y `drag_apply()` usaban `dfl_graph_x()`/`dfl_graph_hz()` sin
espejo.

Esta corrida agrega el **espejo como transformación de coordenadas en el modelo
puro** (`src/dfl_model.h`) y lo usa en **todos** los caminos del editor (dibujo,
etiquetas de los extremos, cero, hit-test e inverso del arrastre). Es **solo
presentación + inverso táctil**: no cambia ningún valor de filtro, el signo de
los Subjects, ni el audio/DSP/RF. No se rediseñó el editor ni se tocaron
controles.

## 1. Los helpers y dónde se usan

Todo vive en `src/dfl_model.h` (puro, sin LVGL ni Subjects):

| Helper | Línea | Qué hace |
|---|---|---|
| `dfl_mode_is_lsb(mode)` | 261 | `true` **sólo** para `x6200_mode_lsb` / `x6200_mode_lsb_dig` |
| `dfl_graph_mirror(x, x0, w)` | 269 | refleja `x` alrededor del centro de `[x0, x0+w]`; es su propia inversa; recorta y usa `int64_t` |
| `dfl_graph_mode_x(mode, hz, d, x0, w)` | 288 | `x_asc(hz)`, espejado en LSB |
| `dfl_graph_mode_hz(mode, x, d, x0, w)` | 296 | inverso: des-espeja en LSB y llama `dfl_graph_hz()` |
| `dfl_graph_layout(mode, low, high, d, x0, w)` | 317 | geometría completa: `low_x`, `high_x`, `band_left/right` **ya ordenados**, `zero_x`, `left_hz`, `right_hz` |

`dfl_graph_x()`/`dfl_graph_hz()` **genéricos no se tocaron** (los siguen usando
los demás modos).

Usos en `src/dfl_inline.c`:

- `draw_cb()` — **línea 235**: `dfl_graph_layout()` para relleno, meseta,
  faldones, bordes, handles, cero y las etiquetas de los extremos (`L.left_hz`
  / `L.right_hz`). Ya **no** asume `low_x < high_x`: usa `band_left/right`.
- `drag_apply()` — **línea 364**: `dfl_graph_mode_hz()` (inverso espejado).
- `graph_event_cb()` (hit-test) — **línea 398**: `dfl_graph_layout()` → mismo
  `low_x`/`high_x` que el dibujo; radio `≤22 px` sin cambios.
- `on_mode_change()` — cancela `dragging = DFL_HANDLE_NONE` al cambiar de modo.

El arnés reusa la misma función pura para calcular la geometría viva del
objeto real (`tools/ui_harness/src/screens.c:792`).

## 2. Resultado por modo

| Modo | Eje izquierda→derecha | Handle izquierdo | Handle derecho |
|---|---|---|---|
| USB, USB-D | `0 → M` | LOW | HIGH |
| LSB, LSB-D | `M → 0` | **HIGH** | **LOW** |
| CW, CWR, AM, SAM, NFM, WFM | sin cambio (no se espejan) | — | — |

El espejo distingue **explícitamente** sólo `x6200_mode_lsb` y
`x6200_mode_lsb_dig`. CW/CWR y los simétricos no se espejan: rige
`src/cfg/mode.c::update_real_filters()` y el mapeo actual queda intacto (no se
supone por el nombre; CWR incluido).

Los handles conservan su **identidad semántica** (LOW sigue siendo LOW, con su
estado y su acento) aunque cambien de lado. Los textos siguen siendo
`LOW 225 Hz` / `HIGH 3525 Hz` / `BW 3300 Hz`: magnitudes positivas en Hz, sin
intercambiar labels, sin restar frecuencias, sin tocar persistencia.

## 3. GOLDEN medido

`LOW=225, HIGH=3525, BW=3300, M=5000, w=471`:

| Caso | Esperado (issue) | Medido (nativo) |
|---|---|---|
| USB `x0=0` LOW | 21 | **21** |
| USB `x0=0` HIGH | 332 | **332** |
| LSB `x0=0` HIGH | 139 | **139** |
| LSB `x0=0` LOW | 450 | **450** |
| USB área real `x0=8` | 29 / 340 | **29 / 340** |
| LSB área real `x0=8` | HIGH 147 / LOW 458 | **147 / 458** |

±1 px. Además, **en la captura 800×480** (`scenario_settle_pixels`) los handles
se dibujan exactamente ahí (color de acento `#Cdb2ff`… en Aurora, columnas de
8 px medidas con PIL):

```text
dfl-mirror-usb.png -> handles (x1,x2,centro absolutos): [(33,40,36.5), (344,351,347.5)]
dfl-mirror-lsb.png -> handles (x1,x2,centro absolutos): [(151,158,154.5), (462,469,465.5)]
```

(esperado: USB LOW 8+29=37, HIGH 8+340=348; LSB HIGH 8+147=155, LOW 8+458=466).

## 4. Dibujo (faldones, handles, etiquetas)

- **Faldones/meseta/relleno**: usan `band_left`/`band_right` ya ordenados; en
  LSB el faldón izquierdo pertenece a HIGH y el derecho a LOW, sin polígonos
  degenerados.
- **Handles**: LOW y HIGH se dibujan en `low_x`/`high_x` del layout; conservan
  su color/estado (LOW apagado si no es editable).
- **Cero y etiquetas de los extremos**: `zero_x`, `left_hz` y `right_hz` salen
  del layout. USB `0` izq / `5000` der; LSB `5000` izq / `0` der.
- Se mantiene el esquema inline: micro-waterfall de **28 px**, cinco teclas
  (`LOW · HIGH · BW · Eq · Waterfall`), marca `Esquema`, sin colores fijos ni
  cuadrícula nueva.

Comparativa lado a lado con marcas de borde:
`capturas/comparativa-usb-lsb-aurora.png`.

## 5. Contrato de touch / VOL / rotary0 (probado)

Fixtures nuevas `dfl-mirror-usb` y `dfl-mirror-lsb` (pantalla
`dfl-editor-mirror`), misma secuencia en los dos modos; `scenario_settle` corta
si no ve la **inversión real** (no sólo que aparezca la etiqueta LSB):

1. **Hit-test**: tocar el handle **izquierdo** mueve el parámetro que de verdad
   está ahí. En LSB el izquierdo es **HIGH** (3525→3500, LOW intacto); en USB el
   izquierdo es LOW (225→220, HIGH intacto).
2. **Arrastre bidireccional**: mover el handle **derecho** a la derecha, en USB
   (HIGH) **aumenta** y en LSB (LOW) **disminuye**; a la izquierda, lo opuesto.
3. **Toque lejano** (> 22 px de ambos): no cambia nada.
4. **VOL**: girar "aumentar" **aumenta** el valor numérico de la selección en
   USB **y** en LSB (la dirección del encoder no se invierte); no toca VFO ni
   volumen.
5. **rotary0**: sigue sintonizando con el panel abierto y no toca los filtros.
6. **Cambio de modo con el panel abierto**: captura un drag sin soltar, cambia
   USB→LSB, deja correr el observer diferido (cancela el drag) y manda un
   `PRESSING` colgado: **no** toca el filtro; el eje queda espejado.

`tests/native/test_dfl_editor.c` cubre además la ida/vuelta x→Hz→x en ambos
modos, el clipping, `w=0`, el dominio inválido y que los demás modos **no**
cambian.

## 6. Gates

| Gate | Resultado | Evidencia |
|---|---|---|
| Nativos (host) | **OK** — modelo DFL **215** comprobaciones, 0 fallos (suite 40288+…) | `logs/native.log` |
| `-fanalyzer` (modelo y rueda) | **OK** (sin observaciones) | `logs/native.log` |
| Estáticos (`check_*`, `audita-pruebas`, curva) | **OK** | `logs/check_*.log`, `logs/audita.log`, `logs/curve_evidence.log` |
| Determinismo del arnés | **PASS** (PNG idénticos ×2, incluidas las fixtures nuevas) | `logs/determinism.log` |
| `x6200-ab themes` (WCAG 6 temas + no-text) | **OK** | `logs/ab-themes.log` |
| ruff / format / ty | **OK** | `logs/ruff.log`, `logs/rufffmt.log`, `logs/ty.log` |
| pytest (no e2e) | **227 passed** | `logs/pytest.log` |
| pytest e2e (TMPDIR en disco) | **13 passed** (458 s) | `logs/pytest-e2e.log` |
| A/B `AB-DFL182` | **PASS** (Functional/Visual/Contract/Perf) | `logs/ab-compare.log`, `ab-DFL182/` |
| `x6200-ab audit` (estructural) | **0 hallazgos** (19615 objetos, 144 escenarios) | `logs/ab-audit.log` |
| ARM cross-compile + banco QEMU (#86) | ver §6.3 | `logs/banco-out.txt`, `banco/` |

### 6.1 A/B `AB-DFL182` (declaración del diff)

```text
experiment AB-DFL182: PASS (exit 0)
  Functional         PASS  (required) — 142 scenarios captured on both revisions
  Visual Regression  PASS  (required) — no unexplained visual regressions
  Quality Contract   PASS  (required) — 1480 contract checks passed on the candidate
  Performance        PASS  (required) — render-cost proxy within budget
  Hardware           BLOCKED (advisory) — no physical X6200 attached
  Energy             BLOCKED (advisory) — no power measurement
```

**Confinamiento**: de los 142 escenarios compartidos, **141 quedan
byte-idénticos**. El **único** con píxeles distintos es `dfl-editor-lsb-d`
(15140 px, 3.943 %, 8 cajas), atribuido a la región declarada `dfl_inline`; el
`dfl-editor-usb-d` (USB) **no cambia**. El diff (rojo) cae sólo sobre la gráfica
del panel: relleno, meseta, handles y las etiquetas `5000`/`0`
(`ab-DFL182/diffs/dfl-editor-lsb-d-diff.png`). El candidato agrega las fixtures
`dfl-mirror-usb`/`dfl-mirror-lsb`, que el A/B captura sólo del lado B.

### 6.2 Sin regresión del espectro

El arnés verifica en **cada** fixture (incluidas las nuevas) que los ticks del
filtro RX caen en `spectrum_filter_band(filter.real.from/to, fft_width, w)`
(`settle_waterfall_filter_ticks`), que es el mapeo de
`src/cfg/mode.c::update_real_filters()`: **USB-D x404..459 y LSB-D x341..396 a
span 48k** se mantienen. No se tocó `src/spectrum.c` ni `src/waterfall.c`; el
micro-waterfall de 28 px, la banda IARU, el VFO, el S-meter, las filas y la
barra inferior quedan intactos (ver la comparativa y las capturas).

### 6.3 ARM cross-compile + banco QEMU (gate #86)

```text
version: 3d8fdc51 DFL #182: registrar las fixtures dfl-mirror en la s
fuente:  md5 4e3e7f6d3232 (src/ + lvgl/src)  (sello .fuente.md5, #165)
binario: md5 88196e442ddc   qemu-cpu: cortex-a7
boot_identity: fuente: 4e3e7f6d3232689a8b70824bb0f858e3  -> COINCIDE con el sello
gate fail-closed #86: OK (ELF32 ARM, 13875308 B, 3/3 marcas)
crash.txt: NO
```

Sello de fuentes: `sh tools/fuente-md5.sh .` =
`4e3e7f6d3232689a8b70824bb0f858e3`, **idéntico** al `.fuente.md5` y al
`boot_identity` que loguea el binario al arrancar (#165).

Capturas del binario ARM real bajo `qemu-arm-static` (`banco/`), abriendo el
editor por el **control remoto** (`{"cmd":"app","text":"DFLEditor"}`):

| Captura | Modo | Estado |
|---|---|---|
| `banco-home.png` | USB-D | DFL cerrado (waterfall íntegro) |
| `banco-usbd.png` | USB-D | editor inline: eje `0` izq / `5000` der, LOW izq, HIGH der |
| `banco-lsbd.png` | LSB-D | editor inline **espejado**: eje `5000` izq / `0` der, HIGH izq, LOW der; filas `LOW 225 Hz` / `HIGH 3525 Hz` / `BW 3300 Hz` |

El tema del banco es el de la radio (Sunlit por defecto); el remote no expone
cambio de tema, así que la matriz de seis temas se cubre con el arnés
(`capturas/`). El `{"cmd":"mode"}` del control remoto cierra la app abierta (es
comportamiento del firmware, no de esta corrección), así que la captura LSB-D
reabre el editor después de cambiar de modo.

## 7. Qué se cambió

- `src/dfl_model.h`: helpers puros `dfl_mode_is_lsb`, `dfl_graph_mirror`,
  `dfl_graph_mode_x/hz` y `dfl_graph_layout`.
- `src/dfl_inline.c`: `draw_cb` (layout + etiquetas + cero), `graph_event_cb`
  (hit-test), `drag_apply` (inverso espejado) y `on_mode_change` (cancela drag).
- `tests/native/test_dfl_editor.c`: golden del espejo (`test_graph_mirror`).
- `tools/ui_harness/src/screens.c`: `dfl_live_layout`, secuencia
  `dfl-editor-mirror` en `settle_dfl_editor`, y `scenario_settle_pixels`
  (chequeo de píxeles de los handles).
- `tools/ui_harness/src/harness_main.c`, `scenario.h`: hook de chequeo de
  píxeles post-render.
- `tools/ui_harness/fixtures/dfl-mirror-usb.scn`, `dfl-mirror-lsb.scn`.
- `tools/ab_testing/tests/test_scenarios.py`: registra las fixtures nuevas.

**No se tocó** `src/cfg/mode.c`, `src/spectrum.c`, `src/waterfall.c`,
`src/dfl_rotary.c`, `src/buttons.cpp`, `src/vol.c`, RF/TX/DSP ni `lvgl/`.

## 8. PENDIENTES / desviaciones documentadas

1. **Prueba física posterior del owner**: el agente NO valida hardware. El
   control remoto del banco no gira el encoder VOL ni toca la pantalla; la
   prueba de la rueda/touch en la radio queda para el owner.
2. **Volumen de build reusado**: el chroot es **privado** de la corrida
   (`state/dfl182-chroot`), pero el volumen de build se **reusó**
   (`x6200-build-output-dfl173`): un volumen nuevo pesa ~20 GB y el disco del
   host está al límite. El binario se compiló limpio desde este HEAD (sello de
   fuentes #165).
3. El banco usa el tema por defecto de la radio; la matriz de seis temas se
   cubre con el arnés (`capturas/`).
