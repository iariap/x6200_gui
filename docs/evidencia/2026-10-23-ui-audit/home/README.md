# UI AUDIT A — HOME 800×480: reloj, batería, Bluetooth y display de radio (issue #270, épica #269)

Corrida de **recolección de evidencia visual** del hijo **A** de la épica
[#269](https://github.com/iariap/x6200_gui/issues/269), issue
[#270](https://github.com/iariap/x6200_gui/issues/270). **Sólo documentación y
medición**: no se tocó `src/`, tests, temas, fuentes, settings persistidos,
controles, lógica de radio, RF ni firmware.

| | |
|---|---|
| Worktree | `/home/pablo/desa/asf/state/worktrees/x6200/ui-audit-home` |
| Rama | `factory/ui-audit-home` — **el issue pide `audit/ui-home-20261023`**; el harness usa una rama por corrida (la épica admite `audit/ui-<scope>-<fecha>`, y en este harness la rama de la corrida se llama así por convención del worktree) |
| **SHA congelado (medido en cada captura)** | `d0ff5cd196b43ba0190d4ccad03f559a2954daf6` (`revision` del JSON de las 36 capturas con `--json`; verificado contra el SHA pinneado por la épica) |
| Fecha de corrida | 2026-10-23 |
| Push | **no** (el PR contra `main` lo abre el orquestador) |
| Radio / hardware | **no** (sin SSH, flash, deploy, TX/RF, `sudo`). QEMU: no se usó (es alcance de C) |
| Aislamiento | contenedor `x6200-uiharness` efímero (`--rm`), build **fuera** del worktree (`/tmp/opencode/ui-audit-home/build-a`), cero contenedores propios dejados |

**Cambio respecto del prompt de la épica**: el build del arnés se hizo
out-of-source en un scratch propio (`/tmp/opencode/ui-audit-home`) en vez de
`tools/ui_harness/build` **dentro** del worktree, para que el único cambio en el
árbol sea este directorio de evidencia. Los comandos exactos quedan en
`logs/comandos.log`, así que la reproducibilidad no depende de dónde esté el
build.

---

## 1. Qué se midió y con qué (herramientas reales, no simuladas)

| Herramienta | Rol en esta corrida |
|---|---|
| `tools/ui_harness` (LVGL 8.3.11 real → PNG 800×480 headless) | la captura y el árbol LVGL vivo (`--inspect`) |
| `src/coverage.c` (vía el JSON) | la clasificación REAL/PARTIAL/SYNTHETIC/UNSUPPORTED, **por captura** |
| `medir.py` (este directorio) | geometría del árbol + medición de PIXELES (tinta, fondo, contraste WCAG 2.1, cobertura de tinta) |
| `crops.py` (este directorio) | recortes ampliados con vecino más cercano (ayuda de lectura) |
| `manifiesto.py` (este directorio) | `manifest.csv` desde los logs + los JSON + las mediciones |
| `tests/check_info_bar.py`, `tests/check_wcag.py`, `tests/run_all_tests.sh --quick` | diagnóstico de base (no se modificaron) |

### Comando de build (idéntico para todas las capturas)

```sh
docker run --rm --entrypoint bash --user "$(id -u):$(id -g)" \
  -v "$WT:/src" -v /tmp/opencode/ui-audit-home:/scratch \
  -w /src/tools/ui_harness x6200-uiharness \
  -lc "cmake -S /src/tools/ui_harness -B /scratch/build-a \
        -DHARNESS_REVISION=d0ff5cd196b43ba0190d4ccad03f559a2954daf6 -DCMAKE_BUILD_TYPE=Release \
        && cmake --build /scratch/build-a -j 6"
# -> /scratch/build-a/ui_harness  (host x86_64, LVGL 8.3.11 = submodulo lvgl @ 74d0a816)
```

### Comando de captura (uno por estado, exacto, en `logs/comandos.log`)

```sh
docker run --rm --entrypoint bash --user "$(id -u):$(id -g)" \
  -v "$WT:/src" -v /tmp/opencode/ui-audit-home:/scratch \
  -w /scratch/build-a x6200-uiharness \
  -lc "./ui_harness --scenario /src/tools/ui_harness/fixtures/<fixture>.scn \
        --png /src/docs/evidencia/2026-10-23-ui-audit/home/png/<out>.png \
        --json /src/docs/evidencia/2026-10-23-ui-audit/home/json/<out>.json --inspect"
```

`--inspect` es un flag del JSON que **no cambia un pixel** (README del arnés).
Medido acá también: `png/home-rx-base.png` y `png/home-rx-base-sin-inspect.png`
(misma corrida, sin `--inspect` ni `--json`) tienen **el mismo sha256**
(`1007699c29c6568959a6ccc6b5ff202dcdb4a980e879a1bf516141be2e824440`), y
`home-rx-base` vs `home-rx-tema-aurora` también (mismo estado de radio, tema
resuelto al default Aurora: `theme.requested = Aurora`).

**Determinismo verificado**: `capturar.sh` corrió **tres veces** las 37 capturas
completas y las 37 dieron el **mismo sha256** en las tres (comparación 1↔3:
0 diferencias; el script reescribe `logs/capturas.tsv` con el sha de cada PNG).

### Cómo se corre la medición (reproducible)

```sh
cd docs/evidencia/2026-10-23-ui-audit/home
python3 medir.py         # -> mediciones/*.csv  (+ arbol-<out>.csv por captura)
python3 crops.py         # -> crops/*.png
python3 manifiesto.py    # -> manifest.csv
```

Dependencias: `python3` con `numpy` y `Pillow` (las mismas que usan los tests
del repo). Todo lo demás es stdlib.

### Método de la medición de píxeles (declarado, no implícito)

* **fondo** de un elemento = el color modal de los pixeles de su caja que están
  *lejos* del color de texto declarado (el 40 % más lejano). En una celda con
  fondo opaco es el color de la celda; en una etiqueta sobre el bitmap de la
  barra es el bitmap.
* **cobertura de tinta** de un pixel = `mean((px − fondo) / (color_declarado − fondo))`
  por canal (con `|den| > 32`), recortada a `[0,1]`. Es la fracción del color
  declarado que ese pixel alcanza. **No depende de la distancia absoluta**: una
  familia con antialiasing fuerte (Redshift, `sourcesans3`) nunca llega a 1.0 y
  una sin él sí, y el criterio los trata igual.
* **tinta** = cobertura ≥ 0.45 (`COBERTURA_TINTA`); **fondo** = cobertura < 0.25.
* **contraste** = fórmula WCAG 2.1 (la misma de `tests/check_wcag.py`), medido
  entre el color declarado y el fondo medido; y `ratio_renderizado` = contraste
  entre el **pixel más fuerte del trazo** y ese fondo (lo que el ojo recibe).
* **`ratio_glifo_p05 / mediana`** = contraste contra el fondo local de cada
  columna con tinta (muestreado ±4 px arriba/abajo del trazo), p05 y mediana.
  Es el peor caso realista por columna, no un promedio de la caja.
* **`cobertura_tinta_max`** = el máximo de cobertura del elemento (1.0 = el
  trazo llega al color nominal).
* Nada de esto es OCR: el texto y la fuente salen del árbol LVGL
  (`lv_label_get_text`, `lv_obj_get_style_text_font`), y la tinta se identifica
  con el color que el propio árbol declara.

---

## 2. Inventario de capturas (37)

`manifest.csv` es la tabla completa (columnas de la épica: `screen, state,
theme, source_coverage, sha, command, exit_code, png, width, height, issue_id,
severity, source_path, observation`). Resumen:

| Grupo | Capturas | Archivos |
|---|---|---|
| HOME base RX (Aurora) | 1 (+1 control de bytes sin `--inspect`) | `home-rx-base`, `home-rx-base-sin-inspect` |
| Tema explícito, RX | 6 (Aurora, Cathode, Redshift, Sprint, Sunlit, Trail) | `home-rx-tema-*` |
| Tema explícito, TX (split) | 6 | `home-tx-tema-*` |
| Batería / alimentación (Aurora) | 14 | `home-rx-bateria-{0,7,10,11,20,30,36,50,85,99,100,desconocida}`, `home-rx-fuente-externa-cargando`, `home-rx-bateria-112-sin-fuente` |
| Batería, mismo símbolo en otro tema | 5 | `home-rx-bateria-85-{cathode,redshift,sprint,sunlit,trail}` |
| Frecuencia / band plan | 1 | `home-rx-frecuencia-extrema` |
| Espectro/waterfall | 2 | `home-rx-espectro-agua`, `home-rx-natural` |
| Familia de dígitos segmentada | 1 | `home-rx-digitos-dseg7` |

**Las 37 corridas dieron `exit_code 0` y las 37 imágenes son 800×480** (sin
escalar, PNG RGB). Cobertura declarada por el arnés (idéntica para todas las
capturas de pantalla `main`, del JSON de cada corrida):

* **REAL**: `main_screen.c`, `theme.c`, `styles.c`, `fonts/`, `info.c`,
  `clock.c`, `meter.c`, `tx_info.c`, `band_plan.c`+`band_plan_data.c`,
  `buttons.cpp`, `pannel.c`, `msg.c`+`msg_tiny.c`, `adaptive_tuning.c`,
  `cfg/subjects.cpp`, `cfg/settings_manager.cpp`
* **PARTIAL**: `spectrum.c`, `waterfall.c` (componente real; las magnitudes salen
  del generador determinista del arnés, **no de un FFT ni de la radio**),
  `band_info.c` (catálogo del arnés), `cw_tune_ui.c`
* **SYNTHETIC**: `cfg` (catálogo), `params`, `radio`
* **UNSUPPORTED**: `cluster_column.c`, `spot_marks.c`, los diálogos
  (`dialog_*.c`) — en estas capturas **no** se renderiza ningún diálogo: los
  diálogos mockeados del arnés están marcados como tales y **no son prueba de UI
  real**.

Los cuatro estados de batería derivados (`scenarios/ui-audit-bateria-*.scn`)
existen porque el lenguaje de fixtures no cubre 0/50/100/desconocido: son
**fixtures nuevos**, con las claves documentadas del arnés (`battery_cap`,
`battery_v`, `external_v`, `charging`) y **dentro de este directorio**, porque el
contrato del issue prohíbe escribir fuera de él. La captura sigue siendo del
**componente real** (`clock.c`); lo que cambia es el valor que el arnés le pasa
a `clock_update_power()` (el mismo camino que `radio.c` en la radio).

---

## 3. Geometría medida del árbol LVGL vivo (Aurora, `home-rx-base`)

Cajas absolutas (x, y, w, h) del `inspection` del arnés. Crudo:
`mediciones/arbol-home-rx-base.csv`; por elemento y captura:
`mediciones/elementos.csv`.

| Elemento | Caja | Fuente | Texto / `text_natural` | Notas |
|---|---|---|---|---|
| barra de info (`info_style`) | `0,0,209,61` | — | — | bitmap `images/top_short_dark.bin`, `bg_opa 0` (el bitmap es el fondo) |
| fila 1 (flex) | `15,5,180,26` | — | — | pitch vertical 26 = alto de celda → **0 px de solape** entre filas |
| fila 2 (flex) | `15,31,180,26` | — | — | borde inferior 57 ≤ 61 ✔ |
| celda VFO | `15,5,56,26` | `sony_20` | `VFO-A`, 48×18 | tinta `21,13,68,23`; 8 px de aire a la derecha |
| celda modo | `77,5,56,26` | `sony_20` | `USB`, 31×18 | |
| celda AGC | `139,5,56,26` | `sony_22` | `AUTO`, 43×18 | |
| celda PRE/ATT | `15,31,56,26` | `sony_22` | `P/A`, 25×18 | |
| celda ATU | `77,31,56,26` | `sony_22` | `ATU1`, 37×18 | |
| celda WiFi | `139,31,28,26` | `sony_20` | `U+F1EB`, 18×18 | media celda: 28 px (izquierda) |
| **runa BT** (`lv_line`) | `167,34,16,22` | — | 7 puntos, trazo 2 px | **el objeto mide 16×22**, no 14×20: `ui_tokens.h` documenta la runa de 14×20 (`UI_INFO_BT_H 20`, dy 8) y el objeto real es 2 px más ancho y más alto (el grosor del trazo). Borde inferior 56 ≤ 61 ✔ |
| panel del reloj (`clock_style`) | `591,0,209,61` | — | — | bitmap, alineado al borde derecho |
| hora | `591,5,209,31` | `sony_38` | `00:00:00`, 110×31 | tinta `643,14,749,31` (**alto de tinta 18 px**), centrada |
| fuente externa | `599,41,57,18` | `sony_20` | `U+F1E6 13.8 V`, 57×18 | oculta si `v_ext < 3.0 V` |
| porcentaje | `768,46,24,13` | `sony_16` | `76%`, 24×13 | **se solapa con la caja del icono** (ver §5) |
| icono de batería | `756,41,16,18` | `sony_20` | `U+F241`, 16×18 | |
| fila de frecuencia | 3 labels `0/150/650`, `y=160`, alto 36 | `sony_30` / `sony_38` | `13.882` / `14.074.000` / `14.266` | tinta central `338,174,464,191`; laterales 279/285 px de tinta |
| S-meter (`meter_style`) | `210,0,380,61` | — | — | |
| `tx_info` (sólo TX) | `210,0,380,123` | — | — | visible con `rxtx tx`; **no** pisa la barra ni el reloj (209 < 210 y 590 < 591) |
| espectro | `0,0,800,160` | — | — | |
| waterfall | `0,240,800,240` | — | — | |
| softkeys | 5 × `0..640,418,160,62` | — | — | |

**Los anchos de texto cambian con la familia del tema** (el arnés aplica
`theme.font` como hace `main()`): `VFO-A` mide 48 px en Aurora (`inter`), 49 en
Cathode/Trail (`ibmplexsans`), 41 en Redshift (`sourcesans3`), 37 en Sprint
(`robotocondensed`), 49 en Sunlit (`notosans`). En las seis entra en la celda de
56 px (el borde más ajustado es `ATU1` a 42 px en Cathode/Sunlit → 14 px de
aire). **Ningún `text_overflow` ni `text_truncated` en ninguna captura** (columna
de `mediciones/elementos.csv`).

---

## 4. Contraste medido por tema (lo que el tema declara == lo que se dibuja)

`ratio_modal` = contraste WCAG entre el color declarado y el fondo medido del
elemento. Crudo: `mediciones/elementos.csv`.

| Tema (fuente) | celda `VFO-A` | celda WiFi | reloj `HH:MM:SS` | `76%` | `🔌 13.8 V` | runa BT (apagada) |
|---|---|---|---|---|---|---|
| Aurora (`inter`) | 13.39 | 9.18 | 11.48 | 5.56 | 5.96 | **1.39** |
| Cathode (`ibmplexsans`) | 12.91 | 7.70 | 9.00 | 4.82 | 5.54 | **1.41** |
| Redshift (`sourcesans3`) | 11.43 | 7.59 | 6.08 | **3.80** | **3.80** | **1.76** |
| Sprint (`robotocondensed`) | 14.66 | 9.84 | 11.50 | 5.91 | 5.91 | **no medible** (ver §5.4) |
| Sunlit (`notosans`) | 12.31 | 7.52 | **3.12** | **4.67** | **3.89** | **no medible** |
| Trail (`ibmplexsans`) | 11.77 | 9.59 | 10.94 | 5.24 | 5.30 | **1.39** |

* Las **celdas de la barra de info** cumplen AA (≥ 4.5) en los seis temas, con la
  WiFi entre 7.5 y 9.8. El fondo medido de la celda coincide con `TC(bar)` del
  tema (p. ej. Aurora `#2C213E` = `(44,33,62)`), que es lo que la celda pinta con
  `bg_opa COVER`.
* El **panel del reloj** (arriba a la derecha) no tiene fondo opaco: los textos
  van sobre el bitmap `top_short_dark.bin` compuesto con `TC(bg)`. Ahí el
  contraste medido baja en los temas oscuros por familia con AA (Redshift) y, muy
  fuerte, en **Sunlit**: el tema es claro (`base Outdoor`, `bg` claro) pero el
  bitmap del panel es el oscuro → el texto oscuro (`#162436`) queda sobre un
  gris medio y mide **3.12:1** (la hora) y **3.89:1** (`🔌 13.8 V`). Ver §5.2/§5.3.
* Recortes para leer la tabla: `crops/reloj-panel-temas-x3.png` (el panel del reloj
  en los 6 temas, apilado), `crops/barra-info-temas-x3.png` (la barra de info en
  los 6 temas) y `crops/barra-superior-temas-x2.png` (la barra superior completa).
  Son ampliaciones 3×/2× sin interpolación: **no** son prueba de legibilidad
  física (§7.3).

---

## 5. Defectos medidos (con su evidencia)

> Separación explícita: **5.1–5.4 son mediciones** (geometría del árbol +
> píxeles). **§6** son hipótesis/subjetivo, marcadas como tales. Nada de acá
> implica legibilidad física: es el render de host 800×480 (§7.3).

### 5.1 `P1` — El porcentaje de batería se dibuja ENCIMA del icono (colisión de glifos), todos los temas

**Qué se midió** (no es una impresión: es geometría de cajas + tinta):

1. Las cajas de las dos etiquetas **se solapan**, en los seis temas, en 1–10 px
   (`mediciones/reloj_bateria.csv`, `solape_cajas_px`; y
   `mediciones/solapamientos.csv`, que las lista como único solape parcial de la
   barra superior). Ejemplos con `76%`: Aurora 4 px, Cathode 4, Redshift 7,
   Sprint 3, Trail 4 (Sunlit 0). Con `100%` (Aurora) el solape sube a **10 px**.
2. Hay **tinta del porcentaje dentro de la caja del icono**. Se midió sin
   atribuir glifos por color (icono y texto comparten `TC(text)`): por cada
   (tema, símbolo de batería) se tomó la **intersección de las máscaras de tinta
   entre estados del mismo símbolo con distinto texto** (p. ej. `76%` vs `85%`,
   `20%` vs `30%`) — lo común es el icono, lo que aparece sólo en algunos estados
   es texto. `mediciones/colision_bateria.csv`, columna
   `tinta_extra_dentro_caja_icono_px`:

   | tema | estados con colisión | px de texto dentro de la caja del icono |
   |---|---|---|
   | Aurora | `10,20,30,50,70,76,84,85,88,99,100` % | 2 – 8 |
   | Cathode | `70,76,85` % | 4 – 6 |
   | Redshift | `70,76,85` % | 6 – 9 |
   | Sprint | `70,76,85` % | 3 |
   | Trail | `70,76,85` % | 3 – 6 |
   | Sunlit | — | 0 (las cajas no se solapan) |

   **Cota inferior**: dos estados que comparten el dígito inicial (70 % y 76 %)
   dejan parte del trazo en la intersección, así que el número real es ≥ el de la
   tabla.
3. Los estados donde **no** se midió tinta de texto dentro del icono son `0%`,
   `7%`, `11%` y `36%` (sus cajas arrancan más a la derecha: `0%` en x=774, `7%`
   en 775, `11%` en 771 con 1 px de solape, `36%` en 767 con 5 px de solape pero
   sin tinta adentro); **con `10%` en adelante** la colisión está medida. La
   colisión crece con el ancho del texto: el solape de cajas va de **0 px**
   (`7%`, `0%`) a **10 px** (`100%`).
4. Confirmación visual (ampliada, sólo lectura de píxeles):
   `crops/comparativa-pct-x12.png` (los 10 estados de Aurora en fila),
   `crops/bateria-temas-x8.png` (los 6 temas, mismo estado 76 %),
   `crops/bateria-100%-x8.png` (Aurora al 100 %, donde el solape de cajas es el
   mayor: 10 px).

**Mecanismo (código, no hipótesis)**: `src/clock.c` alinea el icono con
`lv_obj_align_to(lb_bat_icon, lb_bat_pct, LV_ALIGN_OUT_LEFT_BOTTOM, -STATUS_GAP, 0)`
con `STATUS_GAP 4`. En LVGL 8.3.11 (`lvgl/src/core/lv_obj_pos.c`) `LV_ALIGN_OUT_LEFT_*`
calcula `x = -lv_obj_get_width(obj)` y después **suma** `x_ofs`, así que pasar el
offset en negativo **acerca** el icono 4 px dentro de la caja del porcentaje en
vez de dejar 4 px de aire. Además la etiqueta del porcentaje se auto-dimensiona
con el texto (`LV_SIZE_CONTENT`), así que su borde izquierdo se corre hacia la
izquierda cuando el texto tiene más glifos (`100%` = 30 px → x 762) mientras el
icono queda donde se resolvió su alineación: el aire entre ambos **no se
conserva** entre estados.

**Severidad `P1`**: la parte que se pierde es el **primer dígito** del
porcentaje (en `76%` el `7`, en `100%` el `1`), y el operador lee la línea de
estado de un vistazo. El icono sigue indicando el nivel, así que no es `P0`.

### 5.2 `P2` — El porcentaje de batería en **Redshift** se dibuja con ~40 % de cobertura: contraste medido 1.7–1.9:1

En Redshift (`sourcesans3`) los trazos de 1 px del porcentaje (fuente `sony_16`)
**nunca alcanzan el color declarado**: `cobertura_tinta_max` 0.36–0.41 y
`ratio_renderizado_pct_fuera_icono` **1.72–1.90:1** (Aurora: cobertura 1.0,
5.56:1). El contraste nominal del tema (el par `text` vs el fondo medido) sería
5.42:1, así que la pérdida es del renderizado de la familia, no del tema.
Evidencia: `mediciones/reloj_bateria.csv`
(`cobertura_tinta_pct_fuera_icono`, `ratio_renderizado_pct_fuera_icono`),
`mediciones/elementos.csv`, y el recorte `crops/bateria-temas-x8.png` (fila 3).
Afecta a las tres capturas de Redshift (`home-rx-tema-redshift`,
`home-tx-tema-redshift`, `home-rx-bateria-85-redshift`).

### 5.3 `P2` — El panel del reloj en **Sunlit** queda oscuro sobre oscuro: hora 3.12:1, `🔌 13.8 V` 3.89:1

Sunlit es un tema claro (`base Outdoor`, `bg` claro) pero el panel de arriba a la
derecha se dibuja con el bitmap **oscuro** (`images/top_short_dark.bin`, el mismo
en los seis temas, `src/styles.c`) compuesto con el `TC(bg)` del tema. El texto
del tema es oscuro (`#162436`): el fondo medido bajo el trazo es `(99,113,123)`
(hora) → **3.12:1**; y `(111,129,143)` para `🔌 13.8 V` → **3.89:1**. La hora es
texto grande (31 px de caja, tinta de 20 px de alto) y WCAG AA le pide 3.0:1 —
**3.12 pasa por muy poco**; el p05 por columna es 2.4, o sea que hay columnas por
debajo de 3.0. La línea de estado (20 px, texto normal) pide 4.5 y mide 3.89 →
**falla**. El porcentaje mide 4.67 (nominal) con el trazo a 4.33.
Evidencia: `mediciones/elementos.csv` (captura `home-rx-tema-sunlit`),
`crops/bateria-temas-x8.png` (fila 5), `crops/reloj-panel-temas-x3.png`.

### 5.4 `P3` — La runa de Bluetooth en estado *apagado* es indistinguible del fondo de la barra (1.4–1.9:1)

La runa se dibuja siempre (es un `lv_line` de 7 puntos, `src/info.c`), y su color
sale del estado: apagado → `TC(icon_off)`, encendido sin auriculares →
`TC(text_dim)`, con A2DP → `TC(text)`. **En el arnés sólo es reproducible el
estado apagado** (`mocks/mock_ui_deps.c` fija `bt_state_on() == false`,
`bt_state_connected() == false`, `wifi_get_status() == WIFI_OFF`, y el lenguaje de
fixtures no tiene clave de BT/WiFi) — ver §7.1.

Medido (contraste del trazo `icon_off` contra el bitmap de la barra, caja
`167,34,16,22`): Aurora **1.39**, Cathode **1.41**, Redshift **1.76**, Trail
**1.39**; en **Sprint y Sunlit** la medición declara *no medible* porque el color
del trazo y el fondo quedan a menos de 32/255 en **todos** los canales (es decir,
la runa y el fondo son el mismo color a efectos prácticos). En las 37 capturas,
el 100 % de los píxeles de fondo de esa caja da menos de 3:1 contra el trazo.

`icon_off` está en la lista de exentos de `tests/check_wcag.py`
(`EXENTOS = {..., "icon_off", ...}`, WCAG 1.4.3 "disabled/incidental"), así que
esto **no** es una falla de la paleta: es la medición del único estado que el
arnés puede renderizar. Lo que falta —cuánto sube la runa cuando está encendida
o conectada— **no se puede medir acá** (§7.1). Del lado de la paleta, contra el
mismo fondo medido, los otros dos colores declarados darían: `text_dim` 3.81–6.22
según tema (Aurora 5.53, Cathode 3.95, Redshift 3.81, Trail 6.22) y `text`
5.74–8.06 (Aurora 8.06, Cathode 6.63, Redshift 5.74, Trail 7.63).

### 5.5 Sin defecto medido

* **Filas de la barra de info**: pitch 26 con celdas de 26 y gap 6 → 0 px de
  solape, y 0 px de recorte (`text_overflow`/`text_truncated` falsos en las 6
  celdas × 6 temas). El texto más ancho de la barra es `VFO-A`/`SPL-A` con 48–49
  px en Aurora/Cathode/Sunlit/Trail y 36–46 px en Sprint/Redshift, dentro de la
  celda de 56 px (7–20 px de aire). **Cero glifos faltantes y cero en fallback**
  en todos los elementos del alcance A: el ícono de WiFi `U+F1EB`, el de batería
  `U+F240..F244` y el de fuente `U+F1E6` resuelven en las fuentes de cada tema.
* **Espectro y waterfall no están en blanco**: banda limpia del espectro
  (`0,62,209,97`) con 130–169 colores distintos y 4.5–19 % de píxeles no modales
  (el 19 % es `home-rx-espectro-agua`, con `spectrum_filled true`); banda limpia
  del waterfall (`100,290,600,170`) con 3 648–6 663 colores distintos y **93 %**
  de píxeles no modales en todas las capturas. `mediciones/regiones.csv`. **Las
  magnitudes son sintéticas** (generador determinista por semilla): esto prueba
  que el widget dibuja, no que reciba RF.
* **TX vs RX**: con `rxtx tx` aparece el panel `tx_info` (`210,0,380,123`) y la
  barra de info, el reloj y el S-meter quedan intactos (no hay intersección de
  cajas: 209 < 210 y 590 < 591). La fila de frecuencia, el espectro y las
  softkeys se mantienen.
* **Frecuencia/band plan**: en `14.350 MHz` (borde de 20 m) los tres labels
  siguen sin recorte (`home-rx-frecuencia-extrema`).

---

## 6. Hipótesis (subjetivo, para que el owner decida — NO es medición)

Separadas a propósito del §5, como pide la épica. Ninguna de estas se usó para
asignar severidad.

* El glifo de Bluetooth (una `lv_line` de 7 puntos, 16×22 px de objeto, trazo de
  2 px) leído a 800×480 tiene una forma que depende mucho del antialiasing; el
  dueño ya lo mencionó como "feo". Acá **no se propone rediseño**: la medición de
  §5.4 dice que en los estados que no se pueden renderizar no hay dato.
* La jerarquía visual del panel del reloj: la hora ocupa 209×31 y el estado
  inferior 18 px; la relación entre ambos parece deliberada (`src/clock.c`
  documenta el contrato). Sin medición que la contradiga, es apreciación.
* El `%` del porcentaje queda separado de los dígitos por 1–2 px más que en otras
  fuentes (`%` con avance ancho). Es una propiedad de las familias, no un defecto
  medido.

---

## 7. BLOCKED / UNSUPPORTED / UNKNOWN (con la razón exacta)

### 7.1 Estados de BT/WiFi activo — **UNSUPPORTED (BLOCKED)**

El arnés de host **fija** esos estados: `mocks/mock_ui_deps.c` tiene
`bt_state_on()` → `false`, `bt_state_discoverable()` → `false`,
`bt_state_connected()` → `false`, `bt_state_mute()` → `false` y
`wifi_get_status()` → `WIFI_OFF`; y el parser de escenarios
(`tools/ui_harness/src/scenario.c`) **no tiene ninguna clave** de BT/WiFi (lista
cerrada: un `key` desconocido hace fallar la corrida). Por eso:

* **capturado**: BT apagado (runa en `TC(icon_off)`) y WiFi apagado (ícono en
  `TC(text_dim)`), en los 6 temas y en RX/TX.
* **no capturado**: BT encendido, BT conectado (A2DP), WiFi `STARTING` /
  `DISCONNECTED` / `CONNECTING` / `CONNECTED`. **Evidencia parcial disponible**:
  la geometría de la runa (§3, medible siempre) y el contraste *declarado* de
  `text_dim`/`text` contra el fondo medido de la barra (§5.4). El camino real
  (banco de radio / QEMU) es hardware o recursos de otra corrida: fuera de este
  contrato.

### 7.2 Barra "legacy" de 206 px (`params.theme == THEME_LEGACY`) — **UNSUPPORTED**

El arnés llama siempre `styles_init(THEME_SIMPLE)` (`tools/ui_harness/src/harness_main.c`),
que es el default del firmware (`src/params/params.c`: `.theme = { .x = THEME_SIMPLE }`),
así que todas las capturas tienen el bitmap **de 209 px** (`top_short_dark.bin`).
La variante de 206 px (`top_short.bin`, un `params.theme` persistido distinto) no
es seleccionable desde un fixture. Los límites de `src/ui_tokens.h` están
verificados contra 206 (el más ajustado) por `_Static_assert` en compilación, así
que el caso queda cubierto por el contrato, pero **no hay PNG de esa variante**.

### 7.3 Legibilidad física / representación a tamaño físico de 4" — **UNKNOWN**

No hay en el repositorio (buscado en `docs/`, `doc/`, `rootfs/`) el **área activa
del panel en mm** de la X6200, así que cualquier "tamaño físico real" sería una
dimensión inventada. Esta corrida reporta: (a) los PNG 800×480 **a 1:1 sin
escalar**; (b) los recortes ampliados **declarados como ampliados** (vecino más
cercano) y sin ninguna afirmación de legibilidad sobre el LCD real; (c) el alto
de tinta en px de cada texto (§3). Para una representación a tamaño físico hace
falta que el owner aporte el área activa del panel.

### 7.4 Otros límites declarados

* **Diálogos y overlays sobre HOME**: no se evaluaron (son alcance de B; y los 22
  diálogos mockeados del arnés **no** son prueba de UI real).
* **Mapas de dígitos / VISION**: `theme_shape.vision` no es seleccionable por
  fixture (el arnés no setea VISION) → **UNSUPPORTED** en esta corrida.
* **`x1` (multiplicador de tuning adaptativo)**: etiqueta presente pero oculta en
  todos los fixtures de HOME → no capturada.
* **RF/audio/TX real**: no (sin hardware). El estado TX es el del mock
  (`mocks/mock_radio.c`), con contadores en el JSON (`radio_calls`).
* **QEMU / binario ARM / banco de la radio**: fuera del contrato (alcance de C y/o
  hardware).

---

## 8. Tests corridos (diagnóstico de base: no se modificó ninguno)

| Comando | Exit | Resultado |
|---|---|---|
| `python3 tests/check_info_bar.py` | **0** | contrato de la barra (#251) OK: 6 celdas, pad 2, pitch 26, sin solape |
| `python3 tests/check_wcag.py` | **0** | 1062 pares OK, 170 exentos, 90 informativos, **0 fallas** (9 paletas) |
| `sh tests/run_all_tests.sh --quick` | **1** | 20 OK, **1 falla preexistente** (ver abajo); SKIP: `qemu_apps` (sin entorno privado), `wspr`/`sstv`/`wefax` (la imagen `x6200-uiharness` local no trae `fftw3.h`/`sndfile.h`; el `full` build sí) |

Logs completos: `logs/test-check_info_bar.log`, `logs/test-check_wcag.log`,
`logs/test-run_all_tests-quick.log`.

La única falla es **preexistente y ajena a esta corrida**:
`manual_apps` → "apps en el firmware: 24 | secciones de apps en el manual: 24 |
SIN SECCION en el manual: `ACTION_APP_DFL_EDITOR -> 'DFL editor'`". Este PR **no
modifica** `src/`, `tests/` ni el manual: `git status --porcelain` en el worktree
sólo muestra `.asf-active` (sin commitear) y este directorio de evidencia.

**No se declara ningún test como pasado sin su log.**

---

## 9. Archivos de esta evidencia

```text
docs/evidencia/2026-10-23-ui-audit/home/
  README.md               este documento
  manifest.csv            tabla por captura (columnas de la épica #269 §6)
  capturar.sh             corre las 37 capturas (comando exacto, exit codes)
  medir.py                geometría del árbol + mediciones de píxeles
  crops.py                recortes ampliados (referencia visual)
  manifiesto.py           genera manifest.csv de los logs/JSON/mediciones
  scenarios/              11 fixtures nuevos (4 estados de batería + 7 pares de símbolo)
  png/                    37 PNG 800x480, sin escalar (la evidencia)
  json/                   36 JSON del arnés (cobertura + inspection del árbol)
  crops/                  21 recortes ampliados (ayuda de lectura)
  mediciones/
    elementos.csv         un registro por elemento del alcance A y captura
    arbol-<out>.csv       el árbol LVGL completo de cada captura
    solapamientos.csv     intersecciones de cajas (sólo solapes parciales)
    reloj_bateria.csv     icono vs porcentaje: cajas, tinta, cobertura, contraste
    colision_bateria.csv  intersección de máscaras por símbolo: texto dentro del icono
    regiones.csv          contenido de las bandas limpias de espectro/waterfall
    png.csv               modo/tamaño/sha256 de cada PNG
  logs/                  37 logs de corrida + comandos.log + capturas.tsv + 3 logs de tests
```

### Reproducir todo desde cero

```sh
WT=<worktree de la corrida>            # rama factory/ui-audit-home, SHA d0ff5cd1
# 1. build out-of-source (ver §1)
# 2. capturas
sh "$WT/docs/evidencia/2026-10-23-ui-audit/home/capturar.sh" "$WT"
# 3. mediciones + manifiesto
cd "$WT/docs/evidencia/2026-10-23-ui-audit/home"
python3 medir.py && python3 crops.py && python3 manifiesto.py
```

---

## 10. Límites de esta evidencia (resumen para revisión)

1. Es un render **de host** del LVGL real, a 800×480, con radio/FFT/parámetros
   mockeados: **no** prueba comportamiento del firmware en el hardware ni la
   legibilidad sobre el LCD de la radio (§7.3).
2. Espectro y waterfall son `PARTIAL`: el widget es real, las magnitudes no.
3. De la barra superior se midió geometría, tipografía, píxeles y contraste; **no**
   se infiere nada sobre el consumo, el RF ni la interacción (teclas/touch).
4. Los defectos `P1`/`P2` de §5 son **mediciones de píxeles y de árbol**, no
   opiniones: cada uno cita la tabla y el comando que lo produce. Ninguna
   corrección fue aplicada (es una auditoría).
