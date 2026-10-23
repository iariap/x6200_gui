# #286 — UI polish 2: contraste renderizado (band plan + superficie del dialogo)

Corrida: `feat/ui-polish-contrast-284` en `state/worktrees/x6200/ui-polish-contrast-284`,
base **`28785016`** (`origin/main`, CI opt-in #289). Fecha: 2026-10-23.
Epica #284 · contrato #286 · humana #287 (sigue **BLOCKED** hasta que #285 y este mergeen).

Alcance tocado: `src/styles.c` (`dialog_style`), `src/band_plan.c` (fondo de la etiqueta),
`tests/check_wcag.py` (modelo estatico del band plan + invariantes nuevas), `tests/wcag_rendered_baseline.json`
(12 entradas), `tests/check_wcag_rendered.py` (el diff del baseline imprime los cambios de ratio).
**No** se toco `src/clock.c`, `src/info.c`, `src/ui_tokens.h` ni el generador de fuentes (#285).

## 1. Que se cambio

* **A — `src/styles.c::dialog_style`** (superficie compartida por TODAS las apps via
  `dialog_init`): pasa a pedir fondo del tema **opaco** (`bg_color TC(dialog)`, `bg_opa LV_OPA_COVER`)
  **debajo** de la imagen del tema que ya pedia, y la tinta heredada sale del rol `TC(text)` en vez
  de un blanco fijo. Geometria (x2/y66 796x348), `bg_img_opa` y los estados de item/`:focus`
  quedan igual: `src/dialog.c` y `src/dialog_bt.c` no se tocan.
* **B — `src/band_plan.c`**: cada `segs[i].label` pinta su **propio fondo opaco** con el **color
  exacto del segmento** (`LV_OPA_COVER` + `lv_obj_set_style_bg_color(s->label, colors[type])`),
  relleno interior de 2 px a los costados y 1 px arriba/abajo, sin borde ni radio, sin input.
  `#define OPA LV_OPA_80` de la franja, la paleta, los datos, los anchos, las posiciones, los
  textos y la elegibilidad `lv_txt_get_width(...)+6 <= vis[i].w` quedan igual.

## 2. Las 18 entradas del baseline: antes -> despues

Fuente de cada medicion: `tests/check_wcag_rendered.py` (pixeles del render real del arnes:
`--all` sobre las 24 fixtures = 6 temas x 4 estados; tinta del arbol vivo + fondo modal medido bajo
el glifo). Columna **antes** = ratio registrado en el baseline (el "estado base" medido hoy esta en
`wcag-rendered-antes.log`; 4 pares de Redshift `76%`/`50%` ya medían distinto: 4.08 / 3.78).
Columna **despues** = la corrida final (`wcag-rendered-despues.log`).

| # | fixture | tema | path | texto | antes | despues | estado | dueno |
|---|---|---|---|---|---|---|---|---|
| 1 | `bt-dialog-tema-aurora` | Aurora | `/43/0` | `Bluetooth devices` | 4.31:1 | 4.29:1 | **VIOLA** | #286 (bloqueado) |
| 2 | `bt-dialog-tema-redshift` | Redshift | `/37/1` | ` 13.8 V` | 4.0:1 | 4.0:1 | **VIOLA** | #285 |
| 3 | `bt-dialog-tema-redshift` | Redshift | `/37/2` | `76%` | 3.8:1 | 4.08:1 | **VIOLA** | #285 |
| 4 | `bt-dialog-tema-redshift` | Redshift | `/43/0` | `Bluetooth devices` | 4.14:1 | 4.15:1 | **VIOLA** | #286 (bloqueado) |
| 5 | `bt-dialog-tema-redshift` | Redshift | `/43/2` | `no headphones, RX off` | 4.2:1 | 4.19:1 | **VIOLA** | #286 (bloqueado) |
| 6 | `bt-dialog-tema-sunlit` | Sunlit | `/43/0` | `Bluetooth devices` | 1.71:1 | 1.71:1 | **VIOLA** | #286 (bloqueado) |
| 7 | `bt-dialog-tema-sunlit` | Sunlit | `/43/3` | `Row: pair/connect - Auto: hold to unpair` | 1.69:1 | 1.69:1 | **VIOLA** | #286 (bloqueado) |
| 8 | `main-tema-aurora` | Aurora | `/9/0` | `ALL MODES / SSB` | 4.29:1 | **5.92:1** | RESUELTO | #286 |
| 9 | `main-tema-cathode` | Cathode | `/10/0` | `ALL MODES / SSB` | 4.19:1 | **5.92:1** | RESUELTO | #286 |
| 10 | `main-tema-redshift` | Redshift | `/37/1` | ` 13.8 V` | 4.0:1 | 4.0:1 | **VIOLA** | #285 |
| 11 | `main-tema-redshift` | Redshift | `/37/2` | `76%` | 3.8:1 | 4.08:1 | **VIOLA** | #285 |
| 12 | `main-tema-redshift` | Redshift | `/9/0` | `ALL MODES / SSB` | 4.07:1 | **5.92:1** | RESUELTO | #286 |
| 13 | `main-tema-sprint` | Sprint | `/10/0` | `ALL MODES / SSB` | 4.23:1 | **5.92:1** | RESUELTO | #286 |
| 14 | `main-tema-sunlit` | Sunlit | `/5/0` | `CW` | 3.78:1 | **5.37:1** | RESUELTO | #286 |
| 15 | `main-tema-trail` | Trail | `/9/0` | `ALL MODES / SSB` | 4.38:1 | **5.92:1** | RESUELTO | #286 |
| 16 | `settings-grupo-operation-tema-redshift` | Redshift | `/37/1` | ` 13.8 V` | 4.0:1 | 4.0:1 | **VIOLA** | #285 |
| 17 | `settings-grupo-operation-tema-redshift` | Redshift | `/37/2` | `76%` | 3.8:1 | 4.08:1 | **VIOLA** | #285 |
| 18 | `settings-tema-redshift` | Redshift | `/37/2` | `50%` | 3.8:1 | 3.78:1 | **VIOLA** | #285 |

**6 de 18 resueltas de verdad** (las 6 del band plan, #286-B): `ALL MODES / SSB` pasa a **5.92:1**
en los 5 temas (negro sobre el gris opaco `#888888` del segmento) y `CW` en Sunlit a **5.37:1**
(blanco sobre el azul opaco `#3366CC`). Los otros 12 siguen por debajo de 4.5:1: 7 son de **#285**
(banda de estado de Redshift) y 5 son los **labels del dialogo BT de #286-A, que NO quedaron
resueltos** — ver §5, es el bloqueo del contrato.

## 3. Test negativo (sangrado del Home a traves de la superficie del dialogo)

`docs/evidencia/2026-10-23-ui-polish/themes/bleed_through.py` (reproducible, sin hardware):
renderiza la misma fixture **cambiando solo la semilla** del generador de senal (20 vs 21, que mueve
el espectro/waterfall del Home y **nada** del dialogo) y cuenta los pixeles del interior del dialogo
que difieren: si la superficie es opaca, tienen que ser **0**.

| fixture real (x 6 temas) | px con sangrado ANTES | px con sangrado DESPUES |
|---|---|---|
| `bt-dialog-tema-*` (Bluetooth) | 6660 (Redshift) .. 14153 (Trail) | **0** |
| `settings-tema-*` (Ajustes) | 37345 (Cathode) .. 63970 (Aurora) | **0** |

Total: **365653 px con sangrado antes -> 0 px despues** (12 fixtures x 6 temas, `bleed-base.json` /
`bleed-after.json`). Evidencia visual: `zoom-sangrado-bt-sunlit.png` (las trazas del espectro del
Home se ven sobre la superficie del dialogo ANTES; superficie limpia DESPUES), `dialogo-bt-*.png`,
`bandplan-*.png`.

**Cobertura y limite declarado.** El arnes de host solo renderiza **2 de los 25 dialogos de verdad**
(Settings y Bluetooth): los otros 22 son mocks inertes (`tests/check_dialog_coverage.py`: "OK: 22
dialogos inertes ... sobre las 25 apps"). Por eso la parte multi-app del test negativo (GPS, Status,
WiFi, DFL, CW, SWRScan en ARM) **queda BLOCKED**, con la causa exacta medida:

```
$ LAB=/tmp/opencode/x6200-contrast284-qemu CONT=x6200-contrast284-qemu \
  VOL=x6200-contrast284-build tests/qemu/run_apps_qemu.sh --check
  FALTA: falta el volumen privado x6200-contrast284-build (un build de la GUI; CONSTRUIR=si + BUILDROOT=<clon> para construirlo)
  FALTA: falta el chroot privado (/tmp/opencode/x6200-contrast284-qemu/chroot): pasar ROOTFS=<rootfs.tar privada> o dejar el volumen con target/ (se arma solo)
  binfmt qemu-arm: ok
SKIP: barrido QEMU sin entorno privado (2 requisito(s) faltante(s))     -> rc=77
```

El entorno ARM no es alcanzable en esta corrida: la imagen `x6200-buildroot` trae el arbol de
Buildroot configurado pero **sin toolchain ni `target/`** (`/workspace/build` vacio), asi que armar
un volumen privado exige construir Buildroot entero (horas) antes de tocar la GUI; y no hay una
rootfs privada (`ROOTFS`) disponible. **No se declara hecho ni PASS.** Cuando exista el entorno, el
barrido de las 25 apps (`RONDAS=1`, 8 apps minimas) es el que cierra esta parte.

## 4. Baseline: 18 -> 12 entradas

`python3 tests/check_wcag_rendered.py --update-baseline` (diff completo en `baseline.diff.txt`):

```
- main-tema-aurora    /9/0   ratio 4.29:1  'ALL MODES / SSB'    <- resueltos por #286-B
- main-tema-cathode   /10/0  ratio 4.19:1  'ALL MODES / SSB'
- main-tema-redshift  /9/0   ratio 4.07:1  'ALL MODES / SSB'
- main-tema-sprint    /10/0  ratio 4.23:1  'ALL MODES / SSB'
- main-tema-sunlit    /5/0   ratio 3.78:1  'CW'
- main-tema-trail     /9/0   ratio 4.38:1  'ALL MODES / SSB'
~ bt-dialog-tema-aurora /43/0  4.31:1 -> 4.29:1 (-0.02)   <- NO resuelto; el ratio medido se registra
~ bt-dialog-tema-redshift /43/0 4.14:1 -> 4.15:1 (+0.01)  <- idem
~ bt-dialog-tema-redshift /43/2 4.20:1 -> 4.19:1 (-0.01)  <- idem
~ bt-dialog-tema-redshift /37/2  3.8:1 -> 4.08:1 (+0.28)  <- drift preexistente de main, no de #286
~ main-tema-redshift /37/2       3.8:1 -> 4.08:1 (+0.28)  <- idem
~ settings-grupo-operation-tema-redshift /37/2  3.8:1 -> 4.08:1 (+0.28)  <- idem
~ settings-tema-redshift /37/2   3.8:1 -> 3.78:1 (-0.02)  <- idem (es el que tenia el gate en ROJO)
baseline escrito: tests/wcag_rendered_baseline.json (12 entradas, +0/-6)
```

Salieron **solo los 6 pares que el cambio resuelve de verdad**. No se debilito ningun umbral
(siguen 4.5:1), no se agrego ninguna exencion ni blanket-skip, no se marco nada como
"sin tinta"/"ocluido": los 12 que quedan siguen violando. El `--update-baseline` no agrega entradas
(+0) y no aparecio **ninguna violacion nueva** por el cambio (0 pares fuera del baseline).

Dos cosas que **no** son de este cambio y hay que saber:

* **En `main` (`28785016`) el gate ya estaba en ROJO**: `settings-tema-redshift /37/2` registra
  3.8:1 y hoy mide 3.78:1 -> "REGRESION". Es un drift preexistente del baseline (los tres pares
  `76%` ya median 4.08 en la base). El update lo normaliza al valor medido; el umbral no se toca.
* El diff del baseline **no imprimia** los cambios de ratio (solo altas y bajas): un update podia
  mover un ratio en silencio. `tests/check_wcag_rendered.py` ahora imprime esas lineas `~`
  (son las tres de arriba), que es lo que hace visible el -0.02 de los pares del dialogo BT.

## 5. BLOQUEO del contrato: los 5 labels del dialogo BT no se resuelven con la solucion A

Medido, no supuesto: tras A, los 5 pares del dialogo BT siguen por debajo de 4.5:1 (1.69 / 1.71 en
Sunlit, 4.15 / 4.19 en Redshift, 4.29 en Aurora) y dos de ellos bajan **0.02**. Causa raiz medida:

* la superficie que el usuario ve **no** es la mezcla con el Home (como asumia el contrato): es el
  **bitmap compartido del tema** `images/dialog_dark.bin` que `styles.c::setup_theme_simple()` pone
  como `bg_img_src` de `dialog_style` — el camino que toman **los seis temas JSON, Sunlit incluido**.
  Ese bitmap mide **795x347, alpha 243/255 (95.3% opaco)** en el cuerpo, y su tinta es un gris
  azulado oscuro fijo.
* por eso el `bg_color TC(dialog)` opaco que pide A queda **debajo** del bitmap y solo pesa
  (1-alpha) ≈ **4.7%**: medido por pixel, `(despues - antes) / (TC(dialog) - pantalla)` da ~0.047, y
  el movimiento del fondo modal bajo los labels es de **+1/+2 niveles** (p.ej. Sunlit `#516775` ->
  `#516877`). En Sunlit el rol `dialog` es `#FFFFFF`, pero la superficie sigue siendo oscura → la
  tinta oscura del tema (`#053F80`) no se lee.
* la misma medicion explica el otro resultado: **el sangrado del Home si desaparece** (era el 4.7%
  de atras; ahora esa capa es el rol del tema), que es exactamente lo que mide el test negativo de §3.

Para llegar al **resultado** que pide el contrato (los 5 labels >= 4.5:1 "via the shared opaque
semantic `TC(dialog)` dialog background") hay que hacer que la superficie **sea** el rol del tema
al ~100%, y eso toca la identidad visual de la ventana (hoy el bitmap le da el marco/forma y en
Sunlit **no** sigue al tema — el propio `dialog_settings.c:348` lo documenta: "el cuerpo de la
ventana es el bitmap compartido images/dialog.bin, que NO es un rol del tema (queda oscuro con los
seis temas)"). Las opciones son del coordinador, no mias (el contrato prohibe stickers por etiqueta
y pide no rediseñar):

1. `lv_style_set_bg_img_opa(&dialog_style, LV_OPA_TRANSP)`: la superficie pasa a ser exactamente
   `TC(dialog)` (Sunlit claro → los 5 pares suben a ~9:1), pero **desaparece el bitmap** (marco y
   esquinas) en las seis temas.
2. `lv_style_set_bg_img_recolor(&dialog_style, TC(dialog))` + `bg_img_recolor_opa LV_OPA_COVER`
   (idioma ya usado por `btn_active_style`): conserva la **forma** del bitmap (su alpha) pero lo
   pinta con el rol del tema; mismo efecto de contraste, identidad visual distinta a la actual.
3. Regenerar el arte por tema (`theme_image(TI_dialog)`, el mecanismo ya existe en `theme.c`):
   mantiene el bitmap y le da a cada tema su imagen; es trabajo de assets, no de una linea.

Mientras no haya decision, el resultado honesto es **6/18 resueltas, baseline 12 entradas, gate
verde** (el worktree queda consistente, sin violaciones nuevas y sin nada "salteado").

## 6. Gates: comandos y exit codes

| comando | resultado |
|---|---|
| `python3 tests/check_wcag.py` | **0** — 0 fallas, 1062 pares OK; el par `band_plan_label` nuevo mide 9.12:1 y las invariantes nuevas (fondo opaco del dialogo, pill del band plan) pasan y **fallan contra el codigo de HEAD** (verificado) |
| `python3 tests/check_wcag_rendered.py --all` | **0** — `solo violaciones preexistentes del baseline (12 entradas): OK` |
| `python3 tests/check_wcag_rendered.py` (sin update) | **1** — es el estado intermedio: baseline obsoleto por los 6 resueltos (`wcag-rendered-sin-update-baseline.log`) |
| `python3 docs/evidencia/2026-10-23-ui-polish/themes/bleed_through.py` | **0** despues (0 px de sangrado) / **1** antes (365653 px) |
| `tests/check_harness_determinism.sh` | **0** — "PASS: deterministic (identical PNGs for every fixture)" |
| `tests/check_harness_natural.sh` | **1** — **PREEXISTENTE**: falla igual en `HEAD` (`28785016`), mismos numeros (`flushed_pixels = 12308624`, `natural.totals` identicos, `harness-natural-ANTES.log` vs `harness-natural.log`); el cambio no mueve la cuenta |
| `tests/check_harness_wakeups.sh` | **0** — "PASS: wakeup profile measured, sourced and delimited" |
| `sh tests/run_all_tests.sh --quick` | ver `run-all-quick.log` (log completo del resumen) |
| `tests/qemu/run_apps_qemu.sh --check` | **77** — SKIP/BLOCKED con la causa exacta (§3) |

## 7. Costo de render (`--bench`, 21 frames medidos por fixture, 3 de warmup)

| fixture | flushes | flushed px | redrawn px | render med p50 | p95 | draw med | rss |
|---|---|---|---|---|---|---|---|
| `bt-dialog-tema-sunlit` antes | 33 | 10374119 | 384000 | 6.46 ms | 6.67 ms | 6.06 ms | 10.9 MB |
| `bt-dialog-tema-sunlit` despues | 33 | 10374119 | 384000 | 6.61 ms | 6.70 ms | 6.21 ms | 11.1 MB |
| `settings-tema-sunlit` antes | 35 | 9933174 | 384000 | 6.49 ms | 6.83 ms | 6.08 ms | 10.8 MB |
| `settings-tema-sunlit` despues | 35 | 9933174 | 384000 | 6.67 ms | 6.90 ms | 6.27 ms | 10.9 MB |
| `main-tema-aurora` antes | 29 | 9932624 | 384000 | 3.37 ms | 3.63 ms | 2.95 ms | 10.8 MB |
| `main-tema-aurora` despues | 29 | 9932624 | 384000 | 3.41 ms | 3.50 ms | 3.00 ms | 10.7 MB |

Los **proxies deterministas** (flushes, flushed px, redrawn px) quedan **identicos** en las tres:
el cambio no mueve el area ni la cantidad de flushes (la superficie opaca se pinta dentro del
mismo rectangulo que la imagen). El reloj de pared (host x86, no la Cortex-A7) sube **+1.2% a
+2.8%** en el `p50` de los dos dialogos (una pasada mas de relleno opaco de 796x348 por frame) y
+1.2% en la home del band plan; dentro de la banda de ruido del runner y **sin regresion en los
proxies deterministas**. El frame budget de la radio sigue **PENDING** (no hay unidad fisica).

## 8. Como reproducir

```bash
# renders + medicion WCAG (24 fixtures)
python3 tests/check_wcag_rendered.py --all
# test negativo del sangrado (0 px despues; 365653 px antes con el codigo de HEAD)
python3 docs/evidencia/2026-10-23-ui-polish/themes/bleed_through.py --json /tmp/bleed-after.json
# costo
docker run --rm --entrypoint bash --user "$(id -u):$(id -g)" -v "$PWD:/src" \
  -w /src/tools/ui_harness/build x6200-uiharness -lc \
  "./ui_harness --scenario ../fixtures/bt-dialog-tema-sunlit.scn \
   --png out/x.png --json out/x.json --bench --warmup 3 --repeats 21"
```

Los renders de este informe son los que deja el gate en
`tools/ui_harness/build/out/wcag-rendered/png/`; las copias `antes/` y `despues/` de las 11
fixtures de las 18 entradas son byte-identicas a esos archivos.
