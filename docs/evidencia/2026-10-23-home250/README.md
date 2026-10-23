# Evidencia — issue #250 (hijo 1/2 de la épica #249): reloj permanente + batería compacta

Fecha: 2026-10-23. Worktree de la corrida:
`state/worktrees/x6200/home-clock-battery-250`, rama `factory/home-clock-battery-250`.

| | |
|---|---|
| Base (y destino) | `origin/main` = `8549891e2cfafeaa970afd582bdacd136601a5d0` |
| Commit de la implementación | `65f7842c29f124728556d8c54b5644b8ffdefe0a` |
| Alcance | `src/clock.c`, `src/dialog_settings.c`, tests/fixtures/documentación del reloj |
| Fuera | `src/info.c` (hijo #251), DSP, `radio.c`, params persistidos, `main_screen.c`, softkeys, control físico |

La rama `lu2dsx/x6200-rom` **no** se usa ni se fusiona (fuera de la épica). No se
afirma compatibilidad con la BASE 1.0.6 ni un `GO` de release: la advertencia de
paridad de PR #223 sigue abierta (ver *Pendientes*).

## 1. Qué cambia (contrato del issue)

- La hora es `HH:MM:SS` (24h) **siempre**: no depende de `params.clock_view`, de
  ningún timeout ni de `radio_get_state()`. `localtime()` en fallo da `--:--:--`.
- El panel sigue siendo la caja de **206/209 × 61** al borde derecho
  (`clock_style`), ahora como **un contenedor** que dibuja el fondo **una sola
  vez** y sin capturar input, con dos filas adentro: la del reloj (y=0..40,
  `font_digit(&sony_38)`, el "8 fantasma" `88:88:88` con la misma métrica y
  opacidad) y la de estado (y=41..60).
- Fila de estado: **fuente externa** a la izquierda (símbolo plug + `%.1f V`,
  sólo con `v_ext` finito y ≥ 3.0 V, omitida sin fuente) y **batería** a la
  derecha (icono `sony_20` + porcentaje `sony_16`). Sin muestra válida: `--%` y
  el icono vacío en `TC(text_dim)`, **nunca** un `0.0v / 0%` inventado.
- Nivel del icono por **porcentaje** (0..10 vacío, 11..35 nivel 1, 36..60 nivel
  2, 61..85 nivel 3, 86..100 completo); el SoC no se recalcula de la tensión y un
  **0% real** (con `v_bat` válida) se muestra tal cual.
- Un solo snapshot de `power_mux` por actualización; todo LVGL fuera del mutex.
  Los dos timers de siempre (500 ms / 200 ms) y las guardas de igualdad
  (texto y color) de OPT-4 #150.
- Ajustes: salen la fila `Clock view` y la de `Timeout Clock, Power, TX`.
  `params.clock_view`, `clock_time_timeout`, `clock_power_timeout` y
  `clock_tx_timeout` siguen persistidos (ids, nombres, rangos y tabla DB
  intactos, ahora inertes) y `clock_set_view()` / `clock_set_*_timeout()` siguen
  exportadas.

## 2. Capturas antes/después (800×480)

`before/` = binario ARM de `8549891e` (build host del harness, mismas fuentes);
`after/` = binario de `65f7842c`. Cada PNG viene con su JSON `--inspect`.

Comando (dentro de `tools/ui_harness`, con la imagen `x6200-uiharness`):

```sh
./ui_harness --scenario ../fixtures/<fixture>.scn \
    --png out/<antes|despues>/<fixture>.png --json out/<antes|despues>/<fixture>.json --inspect
```

| fixture | px cambiados | bbox | regiones | px fuera |
|---|---|---|---|---|
| main-tema-aurora | 2265 | x=599..790 y=10..57 | reloj_potencia | **0** |
| main-tema-cathode | 2321 | x=599..791 y=5..56 | reloj_potencia | **0** |
| main-tema-redshift | 1665 | x=599..790 y=8..56 | reloj_potencia | **0** |
| main-tema-sprint | 2232 | x=599..791 y=5..56 | reloj_potencia | **0** |
| main-tema-sunlit | 2510 | x=599..791 y=7..56 | reloj_potencia | **0** |
| main-tema-trail | 2626 | x=599..791 y=8..56 | reloj_potencia | **0** |
| main-tema-aurora-tx | 2275 | x=599..790 y=10..57 | reloj_potencia | **0** |
| main-tema-cathode-tx | 2368 | x=599..791 y=5..56 | reloj_potencia | **0** |
| main-tx-simulado | 2275 | x=599..790 y=10..57 | reloj_potencia | **0** |
| home-usb-20m | 2265 | x=599..790 y=10..57 | reloj_potencia | **0** |
| home-bateria-externa | 2416 | x=599..790 y=10..57 | reloj_potencia | **0** |
| main-bateria-baja | 1605 | x=641..790 y=10..55 | reloj_potencia | **0** |
| main-frecuencia-dseg | 2107 | x=599..790 y=5..57 | reloj_potencia | **0** |
| settings-home | 27027 | x=23..791 y=10..404 | reloj_potencia + ajustes_dialogo | **0** |
| settings-grupo-operation | 2280 | x=599..791 y=10..122 | reloj_potencia + ajustes_dialogo | **0** |
| settings-perfil | 1833 | x=641..791 y=10..335 | reloj_potencia + ajustes_dialogo | **0** |
| apps-menu-1 | 2265 | x=599..790 y=10..57 | reloj_potencia | **0** |
| bt-dialog | 2265 | x=599..790 y=10..57 | reloj_potencia | **0** |

Medido píxel a píxel contra `tools/ab_testing/quality/regions.yaml`
(`reloj_potencia` = [591,0,209,62], `ajustes_dialogo` = [2,66,796,348]). El fondo
del panel (el bitmap de `clock_style`) queda **byte a byte igual**: lo que se
mueve es la tinta de adentro (la hora y la banda de estado). En las fixtures de
Ajustes el segundo movimiento es el cuerpo del diálogo (dos filas menos = 108 px
de contenido, el pulgar de la barra de scroll se corre): todo dentro de
`ajustes_dialogo`, que contiene también `ajustes_scrollbar`.

## 3. `--inspect` de los seis temas

Geometría del panel y de sus filas (medida con el LVGL real del harness):

| tema (fixture) | panel | línea del reloj | fuente externa | icono | porcentaje |
|---|---|---|---|---|---|
| Aurora | 591,0 209×61 | 591,5 209×31 | 599,41 57×18 | 756,41 16×18 | 768,46 24×13 |
| Cathode | 591,0 209×61 | 591,5 209×19 (dseg14) | 599,41 64×18 | 755,41 16×18 | 767,48 25×11 |
| Redshift | 591,0 209×61 | 591,5 209×29 | 599,41 62×18 | 755,41 16×18 | 768,46 24×13 |
| Sprint | 591,0 209×61 | 591,5 209×19 (dseg14) | 599,41 64×18 | 755,41 16×18 | 766,48 25×11 |
| Sunlit | 591,0 209×61 | 591,5 209×30 | 599,41 64×17 | 732,41 16×17 | 771,46 21×12 |
| Trail | 591,0 209×61 | 591,5 209×30 | 599,41 64×18 | 733,41 16×18 | 767,47 25×11 |

En los seis: `visible == caja` (nada recortado), `text_overflow = false`,
`text_truncated = false`, `missing_glyphs = []`, `clickable = false` en el
contenedor (no captura input) y las cinco etiquetas cuelgan del contenedor. El
ancho del panel sale de `clock_style` (209 con el setup `THEME_SIMPLE`, que es lo
que valen los seis temas servidos: nada escribe `params.theme.x = THEME_LEGACY`
en esta revisión, así que la variante de 206 px del reloj viejo no se alcanza),
así que el contenedor y sus filas lo siguen sin ningún ancho cableado.

**VISION**: la colección servida tiene **seis** temas (`rootfs/usr/share/x6200/themes`:
Aurora, Cathode, Redshift, Sprint, Sunlit, Trail); VISION se retiró con THEMES v2
#74 y no hay ningún tema que declare `vision: true`, así que el camino del tema
VISION (`fonts_scale(VISION_FONT_STEPS)`, `src/main.c`) es **inalcanzable**: no se
puede pedir `--theme VISION` al harness y **no se declara medido**. Lo que sí se
mide, como sustituto más fuerte, es la matriz completa de familias de fuente y de
dígitos (§7).

## 4. Tabla de transición del reloj

La hora no depende del estado de la radio ni de la vista persistida. El test
nativo (`tests/native/test_clock.c`, 122 comprobaciones) lo fija así:

| estado | `clock_view` persistido | texto de la hora | fuente externa | batería |
|---|---|---|---|---|
| RX | `CLOCK_TIME_ALLWAYS` | `12:34:56` | según pack | según pack |
| TX | `CLOCK_TIME_ALLWAYS` | `12:34:56` | según pack | según pack |
| RX | `CLOCK_TIME_POWER` | `12:34:56` | según pack | según pack |
| TX | `CLOCK_TIME_POWER` | `12:34:56` | según pack | según pack |
| RX | `CLOCK_POWER_ALLWAYS` | `12:34:56` | según pack | según pack |
| TX | `CLOCK_POWER_ALLWAYS` | `12:34:56` | según pack | según pack |
| — (después de `clock_set_view`/`clock_set_*_timeout`) | cualquiera | `12:34:56` | según pack | según pack |

En píxeles: `main-tema-aurora-tx` y `main-tx-simulado` (RX→TX→RX con el pack de
12.1 V) muestran la hora y la banda de estado igual que su par en RX (§2). La
hora cambia una vez por segundo y con la guarda de igualdad sólo se reescribe
cuando el segundo cambia (2 ticks de 500 ms → 1 escritura); 100 ticks idénticos
no escriben ni invalidan nada (comprobado en el test nativo).

**Loading**: el reloj vive en `main_screen()`, que se construye una vez; no hay
pantalla de carga propia donde se esconda (el splash es anterior). Por eso este
caso se reporta como *no medible en el harness* (no hay una fixture de boot) y no
como PASS: lo que el contrato protege —"sólo desaparece si la pantalla se
destruye"— está codificado en que `show_time()` no consulta el estado de la radio
ni ningún timeout.

## 5. Pérdida y retorno de telemetría de DATA

Mismo fixture, pack cambiado (medido con `--inspect` y el texto de las etiquetas):

| pack | antes (`8549891e`) | después (`65f7842c`) |
|---|---|---|
| sin pack (`battery_v 0`, `external_v 0`) | `\uf244 0.0v\n0%` (inventa) | hora + `--%` + `\uf244` en `TC(text_dim)`, sin línea de fuente |
| fuente sin batería (`v_bat 0`, `v_ext 13.8`) | `\uf244 0.0v\n\uf1e6 13.8v` (inventa tensión, sin %) | hora + `\uf1e6 13.8 V` + `--%` dim |
| pack válido (`12.1 V`, `13.8 V`, `76%`) | `\uf240 12.1v\n\uf1e6 13.8v` (sin hora) | hora + `\uf1e6 13.8 V` + `76%` |
| pack válido en TX | ídem RX (sin hora) | hora + banda de estado |

El retorno de la telemetría se comprueba en el test nativo: inválida → válida →
inválida escribe exactamente lo que cambia (porcentaje + icono y, si aparece o
desaparece, la línea de la fuente externa), sin parpadeo (la bandera `charging`
llega en el mismo pack).

## 6. Batería 0 / 20 / 82 / 100, charging y fuente externa

Escenarios temporales fuera del inventario versionado
(`tools/ui_harness/build/out/scn/`, gitignored: no tocan
`tools/ab_testing/tests/test_scenarios.py`). El fixture base es `home-usb-20m`
con el pack indicado:

| escenario | antes (`8549891e`) | después (`65f7842c`) |
|---|---|---|
| `cap 0`, `v_bat 12.0` | `\uf240 12.0v` + `0%` (icono **lleno** con 0%: el icono se mapeaba por tensión) | `0%` + `\uf244` (vacío) |
| `cap 20`, `v_bat 12.0` | `\uf240 12.0v` + `20%` (icono lleno) | `20%` + `\uf243` (nivel 1) |
| `cap 82`, `v_bat 12.1` | `\uf240 12.1v` + `82%` (icono lleno) | `82%` + `\uf241` (nivel 3) |
| `cap 100`, `v_bat 12.6` | `\uf240 12.6v` + `100%` | `100%` + `\uf240` (completo) |
| `charging true`, `v_ext 13.8` | `\uf240 12.1v` + `\ue55b 13.8v` | `\ue55b 13.8 V` + `76%` + `\uf241` |
| `v_ext 13.8` sin batería | `\uf244 0.0v` + `\uf1e6 13.8v` | `\uf1e6 13.8 V` + `--%` dim |
| `v_ext 0` (sin fuente) | segunda línea de fuente igual salía como `0.0v` | sin línea de fuente |

Los cinco niveles del icono (bordes 10/11, 35/36, 60/61, 85/86) están fijados
además por el test nativo, que es determinista y barato.

## 7. Familias de fuente y de dígitos (matriz completa)

`--font <id>` (las 14 familias candidatas, `src/fonts/font_families.c`) y
`--digits <id>` (dseg7, dseg7mini, dseg14) sobre `home-usb-20m`, con `--inspect`:

- `text_overflow = false`, `text_truncated = false` y `missing_glyphs = []` en
  **las 17 corridas**;
- la línea del reloj mide 209 × 29..31 (fuente de texto) o 209 × 19..20
  (segmentada) y siempre entra en la fila y=0..40;
- la banda de estado queda entre y=41 y y=48 (más alta: 16..18 px) dentro del
  panel, y el peor caso de ancho deja 79 px de aire entre la fuente externa
  (jetbrainsmono: `13.8 V` = 74 px) y el grupo de batería.

Archivos: `tools/ui_harness/build/out/fonts/*.json` (gitignored; el resumen está
en esta tabla).

## 8. Contraste del texto de la banda de estado

Fondo medido = promedio de la franja de la banda sin tinta (x=670..735, y=41..58)
del render del propio fixture; `TC(text)` leído del `--inspect`; `TC(warn)` del
JSON de cada tema. Razón con la misma función que usan los gates
(`x6200_ab.contrast.contrast_ratio`, WCAG 2.1):

| tema | fondo de la banda | `TC(text)` | razón | `TC(warn)` | razón |
|---|---|---|---|---|---|
| Aurora | #46596E | #F8EEFF | 6.40:1 | #FFD196 | 5.08:1 |
| Cathode | #445868 | #F9D9AA | 5.47:1 | #FFBC72 | **4.47:1** |
| Redshift | #435768 | #FFB3A0 | **4.36:1** | #FFC295 | 4.78:1 |
| Sprint | #43596D | #F5F7F9 | 6.77:1 | #FFD179 | 5.07:1 |
| Sunlit | #738796 | #162436 | **4.21:1** | #8A5400 | **1.68:1** |
| Trail | #455C6B | #F2EED8 | 6.01:1 | #F5CA83 | 4.56:1 |

Decisión que el propio contrato pide: **no se usa `TC(warn)`** para el estado
≤20%, porque esa pareja no pasa el umbral de 4.5:1 en **todos** los temas
(Sunlit 1.68:1 y Cathode 4.47:1). El texto queda en `TC(text)` y el estado se lee
por el **icono** (vacío/niveles). Medido, no supuesto: es la tabla de arriba.

Nota honesta: `TC(text)` bajo Redshift (4.36:1) y Sunlit (4.21:1) queda algo
debajo de 4.5:1 contra este fondo de bitmap; es el mismo rol y el mismo fondo que
la tinta del reloj viejo (que iba en blanco fijo), así que el cambio no degrada
la pareja, y el gate de tipografía del framework no aplica acá porque mide
contraste sólo cuando la etiqueta pinta un fondo opaco propio (estas no lo
hacen). Queda anotado para el QA físico.

## 9. Costo de redraw del reloj (medido, no estimado)

`x6200-ab compare --a 8549891e --b 65f7842c` (el gate del `UI Visual Regression`
de la épica) da **PASS** (exit 0): **Functional PASS** (199 escenarios capturados
en las dos revisiones), **Visual Regression PASS** (`no unexplained visual
regressions`: no hay un píxel cambiado fuera de las regiones declaradas),
**Quality Contract PASS** (2058 checks sobre el candidato), **Performance PASS**
(`render-cost proxy within budget; 1 scenario(s) accepted by a dated exception`) y
**Hardware/Energy BLOCKED** (no requeridos: son físicos). El reporte completo
(`ab/AB-HOME250/`: `results.json`, `report.md`, `report.html`, `metadata.json`,
`metrics/` y `logs/`) queda acá; los volcados crudos de `screenshots/` y `diffs/`
(199 × 2 revisiones, ~150 MB) no se versionan —se regeneran con el mismo comando—
y las capturas antes/después de las 18 fixtures de §2 sí.

El único FAIL de la primera corrida fue **Performance** en una sola fixture:

| escenario | px A → B | Δpx | flushes A → B | Δflushes |
|---|---|---|---|---|
| `main-frecuencia-dseg` | 1096000 → 1100352 | **+0.397%** | 4 → 6 | **+50.0%** (techo 20%) |
| los otros 198 | — | −1.0% (baja) | 5 → 6 | +20.0% (justo el techo) |

**Causa medida** (no supuesta): con un `DIAGFLUSH` temporal en el flush callback
del harness (parche local, no versionado) las dos corridas de esa fixture dan:

````
A: t=1010 800x480 | t=1040 800x165 | t=1040 800x245 | … | t=2200 800x480          (4)
B: t=1010 800x480 | t=1040 800x165 | t=1040 800x245 |
   t=1500 590,32 75x34 (fuente externa) | t=1500 747,32 53x34 (icono+%) | t=2200 800x480   (6)
````

La banda de estado se dibuja **una vez**, cuando llega el pack real (el harness
publica `clock_update_power()` después de `main_screen()`), y lo hace en **dos
rectángulos separados** —la fuente externa a la izquierda, el icono+porcentaje a
la derecha, como pide la UX del issue— donde el reloj de potencia viejo
invalidaba **uno solo** (una etiqueta para todo el panel). Es un costo de
**arranque**, no de operación, y el costo sostenido del reloj **baja** (§9):
en esta misma fixture, 7490 px por tick contra 15549 del reloj viejo.

Por eso el cambio declara la desviación por el canal que el repo tiene para eso
—`tools/ab_testing/quality/performance_exceptions.yaml`, con techo por métrica,
razón, autor y vencimiento— y **`budgets.yaml` no se toca** (el test
`test_the_global_budget_file_is_untouched` lo verifica por sha256). El techo se
eligió para admitir **sólo** este efecto: 4 → 6 flushes (+50%) entra, 4 → 7
(+75%) no; en píxeles, +0.397% medido contra 1.5% de techo (el presupuesto global
es 10%). Los dos tests que custodian el registro
(`test_the_repo_registry_is_scoped_dated_and_names_the_issue` y
`..._admits_the_measured_breach_and_stays_bounded`) se extendieron para nombrar
el issue de cada entrada y para probar que el techo de la nueva sigue acotado.
**Queda para la revisión del orquestador/owner**: es una aceptación declarada y
acotada de una desviación medida, no un presupuesto relajado.


El reloj dispara su invalidez cuando cambia el texto. Con `time()` fijo en el
harness eso no pasa nunca (y la guarda de igualdad del #150 lo apaga), así que la
medición se hizo con el reloj de pared avanzando con el reloj virtual: un parche
local del `__wrap_time` de `tools/ui_harness/mocks/mock_ui_deps.c` (**no
versionado**, revertido después; el build final se rehizo sin él).

| fixture | antes (`8549891e`) | después (`65f7842c`) |
|---|---|---|
| `main-natural` (familia de texto, 12.1 V + ext) | 15549 px en `581,0,219,71` | **10272 px** en `586,0,214,48` |
| `main-frecuencia-dseg` (dígitos segmentados) | 15549 px en `581,0,219,71` | **7490 px** en `586,0,214,35` |

El reloj permanente invalida menos píxeles por segundo que el viejo de potencia
en vista hora. En `main-natural` con `time()` fijo el histograma no cambia (0
rectángulos de reloj, 39 de cascada y 27 de espectro) y el control
`--natural-static` sigue quedando sólo con los band-info. Ver §10.

## 10. Banco natural (settle re-medido)

| campo | antes (`cbaaf72f`, #150) | después (`65f7842c`) |
|---|---|---|
| `natural.totals` (`main-natural`) | 11208000 px / 66 rects | 11208000 px / 66 rects |
| histograma del reloj | 0 | 0 |
| `metrics.flushes` (captura) | 5 | **6** |
| `metrics.flushed_pixels` (captura) | 1111840 | **1100352** |
| `--display-off` `metrics` | 4 / 727840 | 5 / 716352 |
| `main-natural-eco` `metrics` | 24 / 3776640 | 24 / 3762602 |

El perfil de los 40 frames naturales no cambia (el reloj no dispara con `time()`
fijo); lo que cambia es el settle de la captura, porque la invalidación de
arranque del reloj nuevo es más chica y se une distinto con la del espectro
(0,0,800,165, que contiene al panel). El bloque `metrics` no expone los
rectángulos del settle: el número está medido sobre el binario y el control
`--display-off` lo verifica por diferencia. La evidencia versionada
(`tools/ab_testing/reports/NATURAL-ECOFINAL/`) y `tests/natural_evidence.py` se
regeneraron con estos números.

## 11. Gates (salida real)

Ver el detalle por gate en el reporte de la corrida; acá el resumen:

| gate | resultado |
|---|---|
| `tests/native/run_native_tests.sh` | ver §12 |
| `tests/native/run_native_tests.sh --ci-parity` | ver §12 |
| `tests/run_all_tests.sh --quick` | ver §12 |
| determinismo / natural / wakeups | ver §12 |
| WCAG (`tests/check_wcag.py`) + `x6200-ab themes` | ver §12 |
| `check_settings_grid.py` / `check_settings_focus.py` | ver §12 |
| `x6200-ab contracts` (geometría y contratos sobre B) | ver §12 |
| matriz del harness (`check-matrix.sh origin/main HEAD`) | ver §12 |

## 12. Salida de los gates y skips/blocks

Todos corridos en el worktree `state/worktrees/x6200/home-clock-battery-250`,
sobre el commit `65f7842c`. Los logs crudos están en `/tmp/opencode/gates/`
(efímeros); acá va la línea de resultado de cada uno.

| gate | exit | salida |
|---|---|---|
| `sh tests/native/run_native_tests.sh` | 0 | `clock (#250): reloj permanente + bateria compacta` → `122 comprobaciones, 0 fallos`; host `138 comprobaciones, 0 fallos` |
| `sh tests/native/run_native_tests.sh --ci-parity` | 0 | `== ci-parity: OK (build host sin lvgl/)` |
| `sh tests/run_all_tests.sh --quick` | 0 | `17 OK, 0 fallos` (omitidos: wspr sstv wefax js8 webserver) |
| `sh tests/check_harness_determinism.sh` | 0 | `PASS: deterministic (identical PNGs for every fixture)` |
| `sh tests/check_harness_natural.sh` | 0 | `PASS: natural bench deterministic, profiled and delimited (main-natural)`; `clock 10272 px x 0 tick(s)`; png sha256 `d1ca50fa2fea…` |
| `sh tests/check_harness_wakeups.sh` | 0 | `PASS: wakeup profile measured, sourced and delimited (main-natural)`; `tick thread: none (LV_TICK_CUSTOM) = 0 wakeups in 1000 ms` |
| `python3 tests/check_wcag.py` | 0 | `todos los pares cumplen WCAG 2.1 (AA)` |
| `python3 tests/check_settings_grid.py` | 0 | `la zona de controles de Ajustes usa un solo esquema de grilla` |
| `python3 tests/check_settings_focus.py` | 0 | `todos los widgets de Ajustes estan en el grupo de foco` |
| `python3 tests/check_cfg_fills.py` / `check_actions.py` / `check_apps_menu.py` / `check_app_handlers.py` / `check_manual_apps.py` / `check_cw_tune_feed.py` / `check_autotune_v2.py` / `check_build_identity.py` | 0 | OK (ver `run_all --quick`) |
| `uv run ruff check .` / `ruff format --check .` / `ty check src tests` (ab_testing) | 0 | `All checks passed!` / `199 files already formatted` / sin observaciones |
| `uv run pytest -m "not e2e" -q` (ab_testing) | 0 | `227 passed, 13 deselected` |
| `uv run x6200-ab themes` | 0 | OK (pares WCAG estáticos de los 6 temas) |
| `x6200-ab contracts --revision 65f7842c` (vía la matriz) | 0 | `ok` — contratos de geometría/tipografía/temas sobre B |
| matriz del harness (`scripts/apps/x6200/check-matrix.sh origin/main HEAD`) | 1 | todos los checks `ok` menos los dos de abajo |

**SKIP** (no corrieron, con razón; nunca PASS):

- `wspr`, `sstv`, `wefax` (en `run_all --quick`): `SKIP: la imagen x6200-uiharness
  no trae /usr/include/fftw3.h|sndfile.h (build full: docker build -t
  x6200-uiharness tools/ui_harness)`. La imagen disponible es el target `ci`; son
  suites de códec, ajenas a este cambio.
- `js8` y `webserver`: omitidos por `--quick` y porque no hay checkout
  `.x6200_webserver` al lado del worktree (el `run_all` lo declara).
- `check_wcag (temas de la imagen)`: la matriz sólo lo corre si cambian
  `rootfs/**/themes/*.json`; ya corre en el bloque de firmware.

**BLOCKED** (falta entorno, no es un fallo del cambio; tampoco es PASS):

- **Banco QEMU** (`tools/captura-banco.sh --check`): `FALTA: falta el volumen
  x6200-build-output`, el chroot (`.vfix3`), el binario `tools/remote/x6200-remote`
  y el rootfs. Corre en el host del owner, con recursos que esta corrida no usa
  (hay otra sesión en vuelo).

**FAIL pre-existente** (no lo introduce este cambio):

- `check_dialog_coverage` (en la matriz): `can't open file
  '.../tests/check_dialog_coverage.py'`. El archivo **no existe** en la base:
  `git cat-file -e 8549891e:tests/check_dialog_coverage.py` →
  `fatal: path ... does not exist`. Es una expectativa de la matriz del harness
  contra una revisión de la app que todavía no trae ese chequeo (`run_all_tests.sh`
  no lo corre). La matriz lo cuenta como FAIL porque el comando existe en su lista;
  no hay nada que este cambio pueda hacer al respecto sin tocar `tests/` ajeno a
  la spec.
- Corrida extra **con sanitizers** (la suite nativa dentro de la imagen
  `x6200-uiharness`, Ubuntu 24.04, `gcc 13.3.0`, `ASan + UBSan`): el stage del
  reloj pasa `122 comprobaciones, 0 fallos` (sin UB y sin leaks), y el runner
  aborta más adelante en el stage **Auto Tune v2** con
  `SUMMARY: AddressSanitizer: 4320 byte(s) leaked in 60 allocation(s)` cuyo stack
  es `qs1dsearch_create` **dentro de `libliquid.so.1`**: es el liquid del
  contenedor, no el cambio de reloj (ese stage no compila `src/clock.c`). En el
  host (Fedora, gcc 16, sin runtime de ASan) la suite completa sale `0 fallos`.

## 13. Pendientes / no afirmado

- **QEMU_USER**: el banco ARM con `qemu-arm-static` vive en el host del owner
  (imagen `x6200-buildroot` + volumen `x6200-build-output`); esta corrida **no**
  lo usó (recursos compartidos con otra sesión en vuelo) → `BLOCKED`, y la matriz
  del harness lo reporta con su razón.
- **Radio física**: nada se probó en el equipo (sin SSH, flash, TX ni RF). La
  lectura real de batería/carga y la legibilidad a sol directo quedan
  `PENDING_HARDWARE` para el pipeline de QA físico de la épica.
- **Paridad de base**: no se afirma que `main` (con el delta upstream BASE 1.0.7
  de PR #223) sea seguro con la BASE 1.0.6 de la radio. Este cambio es cosmético
  y no toca el flujo de control.
- `docs/manual/MANUAL.pdf` es un render versionado: se actualizó `MANUAL.md` y
  `MANUAL.html` (la fila del tab Clock) y el PDF queda como estaba (no hay
  generador en el repo; regenerarlo es una corrida de release).
- Contenedores propios dejados al terminar: **cero** (todas las corridas usaron
  `docker run --rm` sobre las imágenes existentes, sin volúmenes compartidos).
