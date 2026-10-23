# #286 — UI polish 2: contraste renderizado (band plan + superficie del dialogo)

Corrida: `feat/ui-polish-contrast-284` en `state/worktrees/x6200/ui-polish-contrast-284`,
base **`28785016`** (`origin/main`, CI opt-in #289). Fecha: 2026-10-23.
Epica #284 · contrato #286 · humana #287 (sigue **BLOCKED** hasta que #285 y este mergeen).

Alcance tocado: `src/styles.c` (`dialog_style`), `src/band_plan.c` (fondo de la etiqueta),
`tests/check_wcag.py` (modelo estatico del band plan + invariantes), `tests/wcag_rendered_baseline.json`
(7 entradas), `tests/check_wcag_rendered.py` (el diff del baseline imprime los cambios de ratio).
**No** se toco `src/clock.c`, `src/info.c`, `src/ui_tokens.h`, el generador de fuentes (#285),
`src/dialog.c` ni `src/dialog_bt.c`.

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
`bg_color` opaco solo **no** alcanzaba (medido, ver §5): la imagen del tema
`images/dialog_dark.bin` (795x347, alpha 243/255, tinta gris azulada fija) mandaba en el color
visible y pesaba ~95%; el recoloreo con el rol del tema -el idioma que ya usa `btn_active_style`-
hace que la superficie **sea** `TC(dialog)` sin token ni paleta nueva.

**B — `src/band_plan.c`**: cada `segs[i].label` pinta su **propio fondo opaco** con el **color
exacto del segmento** (+2 px a los costados, +1 px arriba/abajo, sin borde ni radio, sin input).
`#define OPA LV_OPA_80` de la franja, la paleta, los datos, los anchos, las posiciones, los textos
y la elegibilidad `lv_txt_get_width(...)+6 <= vis[i].w` quedan igual.

## 2. Las 18 entradas del baseline: antes -> despues

Fuente de cada medicion: `tests/check_wcag_rendered.py` (pixeles del render real del arnes: tinta
del arbol vivo + fondo modal medido bajo el glifo; matriz de 24 fixtures = 6 temas x 4 estados).
`antes` = ratio registrado en el baseline (el estado base medido hoy esta en
`wcag-rendered-antes.log`; 4 pares de Redshift `76%`/`50%` ya medían distinto: 4.08 / 3.78).
`despues` = corrida final con la opcion 2 (`wcag-rendered-opcion2-all.log`).

| # | fixture | tema | path | texto | antes | despues | estado | dueno |
|---|---|---|---|---|---|---|---|---|
| 1 | `bt-dialog-tema-aurora` | Aurora | `/43/0` | `Bluetooth devices` | 4.31:1 | **10.33:1** | RESUELTO | #286-A |
| 2 | `bt-dialog-tema-redshift` | Redshift | `/37/1` | ` 13.8 V` | 4.0:1 | 4.0:1 | **VIOLA** | #285 |
| 3 | `bt-dialog-tema-redshift` | Redshift | `/37/2` | `76%` | 3.8:1 | 4.08:1 | **VIOLA** | #285 |
| 4 | `bt-dialog-tema-redshift` | Redshift | `/43/0` | `Bluetooth devices` | 4.14:1 | **11.40:1** | RESUELTO | #286-A |
| 5 | `bt-dialog-tema-redshift` | Redshift | `/43/2` | `no headphones, RX off` | 4.2:1 | **7.59:1** | RESUELTO | #286-A |
| 6 | `bt-dialog-tema-sunlit` | Sunlit | `/43/0` | `Bluetooth devices` | 1.71:1 | **10.35:1** | RESUELTO | #286-A |
| 7 | `bt-dialog-tema-sunlit` | Sunlit | `/43/3` | `Row: pair/connect - Auto: hold to unpair` | 1.69:1 | **15.67:1** | RESUELTO | #286-A |
| 8 | `main-tema-aurora` | Aurora | `/9/0` | `ALL MODES / SSB` | 4.29:1 | **5.92:1** | RESUELTO | #286-B |
| 9 | `main-tema-cathode` | Cathode | `/10/0` | `ALL MODES / SSB` | 4.19:1 | **5.92:1** | RESUELTO | #286-B |
| 10 | `main-tema-redshift` | Redshift | `/37/1` | ` 13.8 V` | 4.0:1 | 4.0:1 | **VIOLA** | #285 |
| 11 | `main-tema-redshift` | Redshift | `/37/2` | `76%` | 3.8:1 | 4.08:1 | **VIOLA** | #285 |
| 12 | `main-tema-redshift` | Redshift | `/9/0` | `ALL MODES / SSB` | 4.07:1 | **5.92:1** | RESUELTO | #286-B |
| 13 | `main-tema-sprint` | Sprint | `/10/0` | `ALL MODES / SSB` | 4.23:1 | **5.92:1** | RESUELTO | #286-B |
| 14 | `main-tema-sunlit` | Sunlit | `/5/0` | `CW` | 3.78:1 | **5.37:1** | RESUELTO | #286-B |
| 15 | `main-tema-trail` | Trail | `/9/0` | `ALL MODES / SSB` | 4.38:1 | **5.92:1** | RESUELTO | #286-B |
| 16 | `settings-grupo-operation-tema-redshift` | Redshift | `/37/1` | ` 13.8 V` | 4.0:1 | 4.0:1 | **VIOLA** | #285 |
| 17 | `settings-grupo-operation-tema-redshift` | Redshift | `/37/2` | `76%` | 3.8:1 | 4.08:1 | **VIOLA** | #285 |
| 18 | `settings-tema-redshift` | Redshift | `/37/2` | `50%` | 3.8:1 | 3.78:1 | **VIOLA** | #285 |

**11 de 18 resueltas**: las 6 del band plan (B) y las 5 del dialogo BT (A). Las 7 que quedan
(3.78-4.08:1) son las de la **banda de estado de Redshift, de #285**, que corre en paralelo.

Ademas de las 5 del baseline, los **mismos cuatro labels del dialogo BT** ahora dan >=4.5:1 en los
seis temas (no estaban en el baseline porque ya pasaban), medido en
`wcag-rendered-opcion2-all.log`: `Bluetooth devices` 10.33 Aurora / 11.8 Cathode / 11.4 Redshift /
11.64 Sprint / 10.35 Sunlit / 11.63 Trail; `no headphones, RX off` 10.13 / 7.5 / 7.59 / 10.43 /
9.58 / 10.52; `Row: pair/connect...` 14.77 / 12.57 / 11.43 / 15.54 / 15.67 / 12.91.

## 3. Que color tiene ahora la superficie del dialogo (medido, los 6 temas)

Modal de los pixeles de la superficie pelada del dialogo (`bt-dialog-tema-<t>`, banda x300..700
y80..100, `--inspect`-libre, contado sobre el PNG):

| tema | ANTES (base) | DESPUES (opcion 2) | `TC(dialog)` del tema |
|---|---|---|---|
| Aurora | `(66,88,103)` gris del bitmap | `(34,26,51)` (8000/8000 px) | `(34,26,51)` = `#221A33` |
| Cathode | `(69,92,107)` | `(40,25,14)` (8000/8000 px) | `(40,25,14)` = `#28190E` |
| Redshift | `(68,90,106)` | `(20,8,9)` (8000/8000 px) | `(20,8,9)` = `#140809` |
| Sprint | `(69,93,108)` | `(12,30,56)` (8000/8000 px) | `(12,30,56)` = `#0C1E38` |
| **Sunlit** | `(77,100,113)` | **`(255,255,255)`** (8000/8000 px) | `(255,255,255)` = `#FFFFFF` |
| Trail | `(68,92,104)` | `(26,42,30)` (8000/8000 px) | `(26,42,30)` = `#1A2A1E` |

El modal es **exactamente el rol** en los seis: la ventana deja de ser un gris fijo y pasa a ser la
superficie del tema (clara en Sunlit, oscura en los otros cinco). Ver `dialogo-bt-*.png` (los 6) y
`zoom-sangrado-bt-sunlit.png`.

## 4. Test negativo (sangrado del Home a traves de la superficie del dialogo)

`bleed_through.py`: renderiza la misma fixture **cambiando solo la semilla** del generador de senal
(20 vs 21, que mueve espectro/waterfall del Home y **nada** del dialogo) y cuenta los pixeles del
interior del dialogo que difieren: si la superficie es opaca tienen que ser **0**.

| fixtures reales (x 6 temas) | ANTES (base) | DESPUES (opcion 2) |
|---|---|---|
| `bt-dialog-tema-*` (Bluetooth) | 6660 (Redshift) .. 14153 (Trail) | **0** |
| `settings-tema-*` (Ajustes) | 37345 (Cathode) .. 63970 (Aurora) | **0** |

Total **365653 px -> 0** (`bleed-base.json` / `bleed-opcion2.json`; logs
`bleed-through-antes.log` / `bleed-through-despues.log`).

**Limite declarado**: el arnes de host renderiza **2 de los 25 dialogos reales** (Settings y
Bluetooth); los otros 22 son mocks inertes (`tests/check_dialog_coverage.py`). La parte multi-app
del test negativo (GPS, Status, WiFi, DFL, CW, SWRScan en ARM) queda **BLOCKED**:

```
$ LAB=/tmp/opencode/x6200-op2-qemu CONT=x6200-op2-qemu VOL=x6200-op2-build tests/qemu/run_apps_qemu.sh --check
  FALTA: falta el volumen privado x6200-op2-build (un build de la GUI; CONSTRUIR=si + BUILDROOT=<clon> para construirlo)
  FALTA: falta el chroot privado (/tmp/opencode/x6200-op2-qemu/chroot): pasar ROOTFS=<rootfs.tar privada> o dejar el volumen con target/ (se arma solo)
  binfmt qemu-arm: ok
SKIP: barrido QEMU sin entorno privado (2 requisito(s) faltante(s))     -> rc=77
```

La imagen `x6200-buildroot` trae el arbol de Buildroot configurado pero **sin toolchain ni
`target/`** (`/workspace/build` vacio): armar un volumen privado exige construir Buildroot entero
(horas). No se declara PASS ni hecho.

## 5. Por que la opcion 2 (y que paso con la primera pasada)

La primera pasada del contrato (solo `bg_color` + `bg_opa COVER`) **no** resolvia los 5 pares del
dialogo BT: medido entonces, el fondo modal bajo los labels se movia **+1/+2 niveles** (Sunlit
`#516775` -> `#516877`) y los ratios quedaban iguales (1.69/1.71). La causa: la superficie visible
es la imagen del tema (`images/dialog_dark.bin`, 795x347, **alpha 243/255**, tinta fija en los 6
temas), asi que el rol del tema que se pinta **debajo** pesa (1-alpha) ~**4.7%** (estimado por
pixel: `(despues-antes)/(TC(dialog)-pantalla) ~ 0.047`). Ese diagnostico funda el recoloreo
(opcion 2, orden del coordinador): el idioma ya existe en este archivo (`btn_active_style` usa
`bg_img_recolor` + `bg_img_recolor_opa`).

## 6. Baseline: 18 -> 7 entradas

`python3 tests/check_wcag_rendered.py --update-baseline` (diff completo en `baseline.diff.txt`):

```
- main-tema-aurora    /9/0   ratio 4.29:1  'ALL MODES / SSB'   <- resueltos por #286-B (primera pasada)
- main-tema-cathode   /10/0  ratio 4.19:1  'ALL MODES / SSB'
- main-tema-redshift  /9/0   ratio 4.07:1  'ALL MODES / SSB'
- main-tema-sprint    /10/0  ratio 4.23:1  'ALL MODES / SSB'
- main-tema-sunlit    /5/0   ratio 3.78:1  'CW'
- main-tema-trail     /9/0   ratio 4.38:1  'ALL MODES / SSB'
- bt-dialog-tema-aurora   /43/0  ratio 4.29:1  'Bluetooth devices'      <- resueltos por #286-A
- bt-dialog-tema-redshift /43/0  ratio 4.15:1  'Bluetooth devices'         (opcion 2)
- bt-dialog-tema-redshift /43/2  ratio 4.19:1  'no headphones, RX off'
- bt-dialog-tema-sunlit   /43/0  ratio 1.71:1  'Bluetooth devices'
- bt-dialog-tema-sunlit   /43/3  ratio 1.69:1  'Row: pair/connect - Auto'
baseline escrito: tests/wcag_rendered_baseline.json (7 entradas, +0/-6 /*-5*/)
```

Salieron **solo los pares que el cambio resuelve de verdad** y **quedan exactamente las 7 de
#285**: ningun par que #286 resuelva queda en el baseline (requisito del contrato, comprobado
mirando las 7 filas). No se debilito ningun umbral (siguen 4.5:1), no hay exencion, blanket-skip
ni "sin tinta"/"ocluido" forzado, y **no hay ninguna violacion nueva** (`+0`).

Nota sobre el rojo preexistente de `main`: `settings-tema-redshift /37/2` registraba 3.8:1 y mide
3.78:1 (drift del baseline, se corrige en `factory/gate-tolerance`). El `--update-baseline` de esta
corrida lo normaliza al valor medido; la logica del gate no se toco por eso.

## 7. Gates: comandos y exit codes

| comando | resultado |
|---|---|
| `python3 tests/check_wcag.py` | **0** — 1062 pares OK, 0 fallas; el par `band_plan_label` mide 9.12:1 y las invariantes (fondo opaco del dialogo, `TC(text)`, **recoloreo `TC(dialog)` opaco**, pill del band plan) pasan y **fallan contra el codigo de `HEAD`** |
| `python3 tests/check_wcag_rendered.py --all` | **1** antes del update (baseline obsoleto por 11 pares resueltos) / **0** despues: "solo violaciones preexistentes del baseline (7 entradas): OK" |
| `python3 docs/.../themes/bleed_through.py` | **0** despues (0 px) / **1** antes (365653 px) |
| `tests/check_harness_determinism.sh` | **0** — "PASS: deterministic (identical PNGs for every fixture)" |
| `tests/check_harness_wakeups.sh` | **0** — "PASS: wakeup profile measured, sourced and delimited" |
| `tests/check_harness_natural.sh` | **1** — **PREEXISTENTE**: falla igual en `HEAD` con los mismos numeros (`flushed_pixels = 12308624`, `natural.totals` identicos: `harness-natural-ANTES.log` vs `harness-natural.log` / `harness-natural-opcion2.log`) |
| `sh tests/run_all_tests.sh --quick` | **0** — ver `run-all-quick-opcion2.log` (en la primera pasada: "22 OK, 0 fallos") |
| `tests/qemu/run_apps_qemu.sh --check` | **77** — SKIP/BLOCKED con la causa exacta (§4) |

Sin recorte ni glifos faltantes: sobre el arbol `--inspect` de las 24 fixtures, **0 labels
truncados, 0 con overflow, 0 con glifos faltantes**; el conteo de "ocluidos" no cambia (128) y las
etiquetas del band plan crecen (texto+4 px) dentro de su segmento porque la elegibilidad
`texto+6 <= w` se preserva. Ademas, los pixeles que se mueven estan **todos** dentro del
rectangulo del dialogo (x2..797, y66..413) o de la franja del band plan (y142..156): nada mas se
toca (`op2-pixeles.txt`).

## 8. Costo de render (`--bench`, 21 frames medidos, 3 de warmup)

| fixture | flushes | flushed px | redrawn px | render p50 ANTES | opcion 2 | delta | p95 ANTES | opcion 2 | draw p50 ANTES | opcion 2 |
|---|---|---|---|---|---|---|---|---|---|---|
| `bt-dialog-tema-sunlit` | 33 = 33 | 10374119 = idem | 384000 = idem | 6.46 ms | 7.78 ms | **+20.4%** | 6.67 | 8.29 | 6.06 | 7.39 |
| `settings-tema-sunlit` | 35 = 35 | 9933174 = idem | idem | 6.49 ms | 7.69 ms | **+18.5%** | 6.83 | 7.77 | 6.08 | 7.31 |
| `main-tema-aurora` (sin dialogo) | 29 = 29 | 9932624 = idem | idem | 3.37 ms | 3.13 ms | -7.0% | 3.63 | 3.19 | 2.95 | 2.74 |

Los **proxies deterministas** (flushes, flushed px, redrawn px) quedan **identicos**: el cambio no
mueve area ni cantidad de flushes. El reloj de pared del host (x86, no la Cortex-A7) sube
**~+19-20%** en las ventanas con dialogo: el pasada de recoloreo cuesta mas que un blend de imagen
plano. En el A/B eso no es un FAIL (p95 con ambos proxies deterministas en 0.0% se reporta
INCONCLUSIVE), pero es un dato real para el owner. El frame budget de la radio sigue **PENDING** (no
hay unidad fisica).

## 9. Como reproducir

```bash
python3 tests/check_wcag.py
python3 tests/check_wcag_rendered.py --all
python3 docs/evidencia/2026-10-23-ui-polish/themes/bleed_through.py --json /tmp/bleed-op2.json
docker run --rm --entrypoint bash --user "$(id -u):$(id -g)" -v "$PWD:/src" \
  -w /src/tools/ui_harness/build x6200-uiharness -lc \
  "./ui_harness --scenario ../fixtures/bt-dialog-tema-sunlit.scn \
   --png out/x.png --json out/x.json --bench --warmup 3 --repeats 21"
```

Los renders de este informe son los que deja el gate en
`tools/ui_harness/build/out/wcag-rendered/png/`; las copias `antes/` y `despues/` son
byte-identicas a esos archivos (antes = la base `28785016`, despues = la opcion 2).

## 10. Pendiente

* **Rebase sobre #285**: `origin/main` sigue en `28785016` (el worktree de #285 tiene cambios sin
  commitear). Cuando mergee: rebasar, re-correr los gates y `--update-baseline` -> los 7 pares de
  Redshift salen y el baseline queda en **0 entradas**.
* **QEMU/ARM de las 25 apps**: BLOCKED (§4).
* **Humana #287**: BLOCKED hasta los dos merges.
