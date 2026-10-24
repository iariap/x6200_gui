# #286 — UI polish 2: contraste renderizado (band plan + superficie del dialogo)

Corrida: `feat/ui-polish-contrast-284` en `state/worktrees/x6200/ui-polish-contrast-284`.
Base: **`107c1536`** (`origin/main`, con #285 ya mergeado; rebasado el 2026-10-23). Fecha: 2026-10-23.
Epica #284 · contrato #286 · humana #287 (sigue **BLOCKED** hasta que este mergee).

Alcance tocado: `src/styles.c` (`dialog_style`), `src/band_plan.c` (fondo de la etiqueta),
`tests/check_wcag.py` (modelo estatico del band plan + invariantes), `tests/wcag_rendered_baseline.json`
(**0 entradas**). **No** se toco `src/clock.c`, `src/info.c`, `src/ui_tokens.h`, el generador de
fuentes (#285), `src/dialog.c` ni `src/dialog_bt.c`.

## 1. Que se cambio

**A — `src/styles.c::dialog_style`** (la superficie que comparten TODAS las apps via `dialog_init`):

```c
lv_style_set_bg_color(&dialog_style, TC(dialog));
lv_style_set_bg_opa(&dialog_style, LV_OPA_COVER);
lv_style_set_bg_img_recolor(&dialog_style, TC(dialog));        /* opcion 2 (orden del coordinador) */
lv_style_set_bg_img_recolor_opa(&dialog_style, LV_OPA_COVER);
lv_style_set_text_color(&dialog_style, TC(text));
```

Geometria (x2/y66 796x348), `bg_img_opa` y los estados de item/`:focus` quedan igual. El
`bg_color` opaco solo **no** alcanzaba (medido, §5): la imagen del tema
`images/dialog_dark.bin` (795x347, alpha 243/255, tinta gris azulada fija) mandaba en el color
visible y pesaba ~95%; el recoloreo con el rol del tema -el idioma que ya usa `btn_active_style`-
hace que la superficie **sea** `TC(dialog)` sin token ni paleta nueva.

**B — `src/band_plan.c`**: cada `segs[i].label` pinta su **propio fondo opaco** con el **color
exacto del segmento** (+2 px a los costados, +1 px arriba/abajo, sin borde ni radio, sin input).
`#define OPA LV_OPA_80` de la franja, la paleta, los datos, los anchos, las posiciones, los textos
y la elegibilidad `lv_txt_get_width(...)+6 <= vis[i].w` quedan igual.

## 2. Las 11 entradas del baseline (main, post-#285): todas resueltas

Fuente de cada medicion: `tests/check_wcag_rendered.py --all` (pixeles del render real del arnes:
tinta del arbol vivo + fondo modal bajo el glifo; matriz de 24 fixtures = 6 temas x 4 estados).
`antes` = ratio registrado en el baseline de `main` (`107c1536`, 11 entradas);
`despues` = `wcag-rendered-opcion2-all.log` (esta rama rebasada).

| # | fixture | path | texto | antes | despues | umbral | estado |
|---|---|---|---|---|---|---|---|
| 1 | `bt-dialog-tema-aurora` | `/43/0` | `Bluetooth devices` | 4.31:1 | **10.33:1** | 4.5 | RESUELTO |
| 2 | `bt-dialog-tema-redshift` | `/43/0` | `Bluetooth devices` | 4.14:1 | **11.40:1** | 4.5 | RESUELTO |
| 3 | `bt-dialog-tema-redshift` | `/43/2` | `no headphones, RX off` | 4.2:1 | **7.59:1** | 4.5 | RESUELTO |
| 4 | `bt-dialog-tema-sunlit` | `/43/0` | `Bluetooth devices` | 1.71:1 | **10.35:1** | 4.5 | RESUELTO |
| 5 | `bt-dialog-tema-sunlit` | `/43/3` | `Row: pair/connect - Auto: hold to unpair` | 1.69:1 | **15.67:1** | 4.5 | RESUELTO |
| 6 | `main-tema-aurora` | `/9/0` | `ALL MODES / SSB` | 4.29:1 | **5.92:1** | 4.5 | RESUELTO |
| 7 | `main-tema-cathode` | `/10/0` | `ALL MODES / SSB` | 4.19:1 | **5.92:1** | 4.5 | RESUELTO |
| 8 | `main-tema-redshift` | `/9/0` | `ALL MODES / SSB` | 4.07:1 | **5.92:1** | 4.5 | RESUELTO |
| 9 | `main-tema-sprint` | `/10/0` | `ALL MODES / SSB` | 4.23:1 | **5.92:1** | 4.5 | RESUELTO |
| 10 | `main-tema-sunlit` | `/5/0` | `CW` | 3.78:1 | **5.37:1** | 4.5 | RESUELTO |
| 11 | `main-tema-trail` | `/9/0` | `ALL MODES / SSB` | 4.38:1 | **5.92:1** | 4.5 | RESUELTO |

Las 7 de la banda de estado de Redshift que estaban en el baseline original ya las resolvio **#285**
en `main`, asi que no aparecen. Ademas, los **cuatro labels del dialogo BT** quedan >=4.5:1 en los
seis temas (los otros no estaban en el baseline porque ya pasaban):

| label | Aurora | Cathode | Redshift | Sprint | Sunlit | Trail |
|---|---|---|---|---|---|---|
| `Bluetooth devices` | 10.33 | 11.8 | 11.4 | 11.64 | 10.35 | 11.63 |
| `no headphones, RX off` | 10.13 | 7.5 | 7.59 | 10.43 | 9.58 | 10.52 |
| `Row: pair/connect - Auto: hold to unpair` | 14.77 | 12.57 | 11.43 | 15.54 | 15.67 | 12.91 |

Minimo de los 18: **7.5:1**. Nota de medicion: en `main` habia **4 labels "sin tinta"** (glifo no
detectable: tinta oscura del tema sobre el bitmap oscuro; la violacion existia sin numero) y quedan
2; `no headphones, RX off` de Sunlit es uno de los que paso a medirse (9.58:1).

## 3. Que color tiene ahora la superficie del dialogo (medido, los 6 temas)

Modal de los pixeles de la superficie pelada (`bt-dialog-tema-<t>`, banda x300..700 y80..100):

| tema | ANTES | DESPUES (opcion 2) | `TC(dialog)` del tema |
|---|---|---|---|
| Aurora | `(66,88,103)` gris del bitmap | `(34,26,51)` (8000/8000 px) | `(34,26,51)` = `#221A33` |
| Cathode | `(69,92,107)` | `(40,25,14)` (8000/8000 px) | `(40,25,14)` = `#28190E` |
| Redshift | `(68,90,106)` | `(20,8,9)` (8000/8000 px) | `(20,8,9)` = `#140809` |
| Sprint | `(69,93,108)` | `(12,30,56)` (8000/8000 px) | `(12,30,56)` = `#0C1E38` |
| **Sunlit** | `(77,100,113)` | **`(255,255,255)`** (8000/8000 px) | `(255,255,255)` = `#FFFFFF` |
| Trail | `(68,92,104)` | `(26,42,30)` (8000/8000 px) | `(26,42,30)` = `#1A2A1E` |

La ventana deja de ser un gris fijo y pasa a ser la superficie del tema (clara en Sunlit, oscura en
los otros cinco). Ver `dialogo-bt-*.png` (los 6) y `zoom-sangrado-bt-sunlit.png`.

## 4. Test negativo (sangrado del Home a traves de la superficie del dialogo)

`bleed_through.py`: misma fixture con **semilla 20 vs 21** (mueve espectro/waterfall del Home y
nada del dialogo); cuenta los pixeles del interior del dialogo que difieren.

| fixtures reales (x 6 temas) | ANTES (base) | DESPUES (opcion 2, post-rebase) |
|---|---|---|
| `bt-dialog-tema-*` (Bluetooth) | 6660 .. 14153 px | **0** |
| `settings-tema-*` (Ajustes) | 37345 .. 63970 px | **0** |

Total **365653 px -> 0** (`bleed-base.json` / `bleed-opcion2.json`). **Limite declarado**: el arnes
de host renderiza **2 de los 25 dialogos reales** (Settings y Bluetooth); los otros 22 son mocks
inertes (`tests/check_dialog_coverage.py`). La parte multi-app (GPS, Status, WiFi, DFL, CW,
SWRScan en ARM) queda **BLOCKED**:

```
$ LAB=/tmp/opencode/x6200-op2-qemu CONT=x6200-op2-qemu VOL=x6200-op2-build tests/qemu/run_apps_qemu.sh --check
  FALTA: falta el volumen privado x6200-op2-build (un build de la GUI; CONSTRUIR=si + BUILDROOT=<clon> para construirlo)
  FALTA: falta el chroot privado (/tmp/opencode/x6200-op2-qemu/chroot): pasar ROOTFS=<rootfs.tar privada> o dejar el volumen con target/ (se arma solo)
  binfmt qemu-arm: ok
SKIP: barrido QEMU sin entorno privado (2 requisito(s) faltante(s))     -> rc=77
```

La imagen `x6200-buildroot` trae `/workspace/build` vacio (sin toolchain ni `target/`): armar un
volumen privado exige construir Buildroot entero (horas). No se declara PASS ni hecho.

## 5. Por que la opcion 2

La primera pasada del contrato (solo `bg_color` + `bg_opa COVER`) **no** resolvia los 5 pares del
dialogo BT: el fondo modal bajo los labels se movia **+1/+2 niveles** y los ratios quedaban iguales
(1.69/1.71). Causa medida: la superficie visible es la imagen del tema (`images/dialog_dark.bin`,
795x347, **alpha 243/255**, tinta fija en los 6 temas), asi que el rol del tema pintado **debajo**
pesa (1-alpha) ~**4.7%** (`(despues-antes)/(TC(dialog)-pantalla) ~ 0.047`). El recoloreo (opcion 2,
orden del coordinador) usa el idioma que ya existe en el archivo (`btn_active_style`).

## 6. Baseline: 11 -> 0 entradas

En el rebase el conflicto del JSON se resolvio **regenerando**, no mergeando texto:
`git checkout origin/main -- tests/wcag_rendered_baseline.json` (11 entradas) y despues
`python3 tests/check_wcag_rendered.py --update-baseline` con el arbol rebasado. Diff
(`baseline.diff.txt` / `baseline-update-opcion2.log`):

```
- bt-dialog-tema-aurora              text  /43/0  ratio 4.31:1  'Bluetooth devices'
- bt-dialog-tema-redshift            text  /43/0  ratio 4.14:1  'Bluetooth devices'
- bt-dialog-tema-redshift            text  /43/2  ratio 4.2:1   'no headphones, RX off'
- bt-dialog-tema-sunlit              text  /43/0  ratio 1.71:1  'Bluetooth devices'
- bt-dialog-tema-sunlit              text  /43/3  ratio 1.69:1  'Row: pair/connect - Auto'
- main-tema-aurora                   text  /9/0   ratio 4.29:1  'ALL MODES / SSB'
- main-tema-cathode                  text  /10/0  ratio 4.19:1  'ALL MODES / SSB'
- main-tema-redshift                 text  /9/0   ratio 4.07:1  'ALL MODES / SSB'
- main-tema-sprint                   text  /10/0  ratio 4.23:1  'ALL MODES / SSB'
- main-tema-sunlit                   text  /5/0   ratio 3.78:1  'CW'
- main-tema-trail                    text  /9/0   ratio 4.38:1  'ALL MODES / SSB'
baseline escrito: tests/wcag_rendered_baseline.json (0 entradas, +0/-11/~0)
```

`+0` = **ninguna violacion nueva**; `~0` = **ningun ratio bendecido en silencio**. Salieron las 11
que el cambio resuelve de verdad (5 del dialogo BT + 6 del band plan) y no quedo ninguna entrada
que #286 resuelva. Umbrales intactos (4.5:1), sin exenciones ni blanket-skip.

## 7. Gates: comandos y exit codes (post-rebase)

Comandos EXACTOS del job `functional` de `.github/workflows/ab-testing.yml` (en `tools/ab_testing`):

| comando | salida | rc |
|---|---|---|
| `uv run ruff check .` | `All checks passed!` | **0** |
| `uv run ruff format --check .` | `200 files already formatted` | **0** |
| `uv run ty check src tests` | `All checks passed!` (precede un `WARN No python files found under the given path(s)` de ty, igual en `main`) | **0** |
| `uv run pytest -m "not e2e"` | **`233 passed, 13 deselected`** | **0** |

| comando | resultado |
|---|---|
| `python3 tests/check_wcag.py` | **0** — 1062 pares OK, 0 fallas |
| `python3 tests/check_wcag_rendered.py --all` | **0** — `violaciones: 0`; `baseline OBSOLETO` para las 11 antes de regenerar (esperado) y PASS con 0 entradas despues |
| `python3 …/themes/bleed_through.py` | **0** — 0 px de sangrado (365653 px en la base) |
| `tests/check_harness_natural.sh` | **0** — PASS. **`flushed_pixels` delta = 0**: 12308896 en mi corrida y en el artefacto comprometido `tools/ab_testing/reports/NATURAL-ECOFINAL/natural.json`; `natural.totals` 11208000 identico. **No hubo que tocar `SETTLE_PX`, el artefacto ni los pins del pytest.** |
| `tests/check_harness_determinism.sh` | **0** |
| `tests/check_harness_wakeups.sh` | **0** |
| `sh tests/run_all_tests.sh --quick` | **0** — "22 OK, 0 fallos" (wcag OK, wcag_rendered OK; `qemu_apps` SKIP con recursos privados; codecs SKIP por la imagen minima): `run-all-quick-opcion2.log` |
| `uv run x6200-ab contracts --revision HEAD --repo . --json` (job `contracts` del CI) | **PASS** — `checks: 2118`, `failures: 0`, `exit_code: 0` (§8) |
| `tests/qemu/run_apps_qemu.sh --check` | **77** — SKIP/BLOCKED (§4) |

Sin recorte ni glifos faltantes: en el arbol `--inspect` de las 24 fixtures, **0 truncados, 0 con
overflow, 0 con glifos faltantes** (`matriz-truncation.txt`); "ocluidos" sin cambios (128); y todos
los pixeles que se mueven caen dentro del rectangulo del dialogo (x2..797, y66..413) o de la franja
del band plan (y142..156) (`op2-pixeles.txt`).

## 8. Contrato `region_min_content`: medir el waterfall donde se VE (#286)

El CI del PR fallo en **un** contrato (`ui-contracts`):

```
status: FAIL · checks: 2118 · failures: 1
FAIL region_min_content:waterfall [bt-dialog-tema-redshift]: content 0.0163 >= 0.0200
```

Causa: el contrato pregunta "el waterfall **se esta dibujando**" como fraccion de pixeles
que se apartan del color modal de la region, y lo medía sobre la region **entera**
(`waterfall` = `[0,196,800,222]`). Con la ventana de las apps **opaca** (lo que pide esta
epica) los pixeles que la ventana tapa ya no son visibles: en esa fixture el numero cayo de
**20.2% a 1.63%** midiendo una placa lisa que el usuario no ve. El contrato estaba calibrado
para la ventana translucida anterior.

**Opcion elegida: 1 — medir el AREA VISIBLE** (no se toco el umbral: sigue `0.02`, ni se
desactivo el contrato, ni skip, ni exencion). Concretamente:

* `quality/contracts.yaml`: `modal_surface: [2, 66, 796, 348]` (la caja de
  `src/dialog.c::dialog_init` -> `dialog_style`, la misma que la region `ajustes_dialogo`) y
  `visible_under_modal: [waterfall]`, la lista de regiones que se miden sobre su parte
  visible cuando la ventana esta en pantalla.
* `contracts.py`: la ventana se detecta en el arbol `--inspect` (un objeto con esa caja
  visible **exacta** y superficie propia: fondo opaco o la imagen del tema). Sin arbol no se
  asume nada y la region se mide entera, como antes. Una region sin un solo pixel visible se
  **declara** ("fully covered ... not measured"), no se falla.
* `visual_diff.py`: `region_content_fraction(..., exclude=)` y `region_visible_mask`; la
  mediana y el conteo se calculan solo sobre los pixeles visibles.

**Por que solo el waterfall**: es la banda que la ventana tapa (`y196..413` de `y196..418`) y
lo que queda visible son las mismas filas del waterfall, asi que el contrato sigue
preguntando exactamente donde el usuario mira. `barra_superior`, `s_metro` y `botones` estan
fuera de la ventana; `espectro` y `frecuencia` conservan contenido propio de la ventana en la
parte tapada, asi que siguen midiendo la region entera (su comportamiento **no** cambia).

**Medido** (`contracts-ci.json`): `bt-dialog-tema-redshift` **0.0163 -> 0.0480** (umbral
0.02); las otras fixtures con ventana: aurora 0.0941, cathode 0.4916, sprint 0.4236, sunlit
0.1251, trail 0.0652.

### Control negativo (obligatorio, con la imagen REAL)

`negative_control_waterfall.py` (captura real + arbol `--inspect` real):

```
1) captura REAL sin tocar   : ok=True  content 0.0480 >= 0.0200
2) waterfall entero aplanado : ok=False  content 0.0000 >= 0.0200  (color plano (10, 5, 5))
  control negativo: OK (la parte visible sigue mandando)
```

Es decir: excluir el area tapada **no** convierte "el waterfall esta en blanco" en un pase.
Lo mismo queda codificado como test permanente en `tools/ab_testing/tests/test_contracts.py`
(`test_modal_surface_does_not_hide_a_blank_region` y companeros: 4 tests nuevos, 237 passed
en `pytest -m "not e2e"`).

### Comando EXACTO del job `contracts` del CI

```
cd tools/ab_testing
uv run x6200-ab contracts --revision "$(git rev-parse HEAD)" --repo <repo> --json | tee contracts.json
```

Resultado: **`status: PASS`, `exit_code: 0`, `revision: 45e72738`, `scenarios: 205`,
`checks: 2118`, `failures: 0`, `unchanged_defects: 0`** (mismo numero de checks que antes: no
se agrego ni se quito ninguno). JSON completo en `contracts-ci.json`.

### Las fixtures SIN ventana no cambian por este fix

`isolate_contract_scope.py` corre las dos semanticas (con y sin `visible_under_modal`) sobre
las **mismas 205 capturas**, asi la unica variable es el cambio de contrato:

```
capturas: 205  (con ventana modal: 44, sin: 161)
findings de region_min_content que cambian SOLO por el cambio de contrato:
  fixtures SIN ventana modal : 0
  fixtures CON ventana modal : 44      (una por fixture, siempre `region_min_content:waterfall`)
```

Las 44 son todas las apps (no solo `bt-dialog-*`/`settings-*`: el GPS, Status, WiFi, DFL, CW,
SWRScan, etc. abren la misma ventana). Entre `main` y esta rama los valores de `espectro`
tambien se mueven ~1e-4 en las fixtures de pantalla principal, y eso **no** es de este fix:
es el render del band plan de #286-B (los pills cambian pixeles dentro de `espectro`); la
corrida de aislamiento (mismas capturas) da **0** diferencias.

## 9. Costo de render post-rebase (`--bench`, 21 frames, main `107c1536` vs main+#286)

| fixture | flushes | flushed px | render p50 main | +#286 | delta | p95 main | +#286 |
|---|---|---|---|---|---|---|---|
| `bt-dialog-tema-sunlit` | 33 = 33 | identico | 6.14 ms | 8.01 ms | **+30.4%** | 6.20 | 8.39 |
| `settings-tema-sunlit` | 35 = 35 | identico | 6.24 ms | 8.03 ms | **+28.7%** | 6.27 | 8.24 |
| `main-tema-aurora` (sin dialogo) | 29 = 29 | identico | 3.17 ms | 3.21 ms | +1.4% | 3.21 | 3.24 |

**Proxies deterministas identicos** (flushes, flushed px; redrawn px 384000). El reloj de pared del
host x86 sube ~+29-30% en los frames con dialogo: la pasada de recoloreo cuesta mas que un blend de
imagen plano (medido antes del rebase en +18-20% sobre la base vieja; el numero depende del estado
del host, el proxy determinista no se mueve). En el A/B eso no es FAIL: p95 con ambos proxies en
0.0% se reporta INCONCLUSIVE. El frame budget de la Cortex-A7 sigue **PENDING** (no hay unidad).

## 10. Como reproducir

```bash
python3 tests/check_wcag.py
python3 tests/check_wcag_rendered.py --all
python3 docs/evidencia/2026-10-23-ui-polish/themes/bleed_through.py --json /tmp/bleed.json
cd tools/ab_testing && uv sync && uv run ruff check . && uv run ruff format --check . \
  && uv run ty check src tests && uv run pytest -m "not e2e"
# contrato del CI (jobs `contracts`), control negativo y aislamiento del alcance
cd tools/ab_testing && uv run x6200-ab contracts --revision "$(git rev-parse HEAD)" \
  --repo "$(git rev-parse --show-toplevel)" --json | tee contracts.json
cd tools/ab_testing && uv run python \
  ../../docs/evidencia/2026-10-23-ui-polish/themes/negative_control_waterfall.py
```

## 11. Pendiente

* **QEMU/ARM de las 25 apps**: BLOCKED (§4).
* **Humana #287**: BLOCKED hasta este merge.
* El **rebase sobre #285** ya esta hecho: `origin/main` = `107c1536`.
