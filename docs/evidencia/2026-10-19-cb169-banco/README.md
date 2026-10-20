# CB 27 MHz (#169): el canal en la fila de frecuencia y la barra CB del plan de banda

Rama `factory/cb-ui-169`, base `73fdd055` (trunk tip), HEAD de la corrida `19ef1ebe`.
**Solo UI**: no se tocó el dial, el TX, la potencia, el interlock, la migración ni la tabla de
canales. **No se transmitió, no se desplegó y no se flasheó.** `info.c` quedó intacto (la celda VFO
sigue con `CH nn`; redundancia aceptada).

## Requisito A — el canal en la fila de frecuencia

En CB la fila grande muestra la frecuencia del grid, el canal y el grid:

| Label | Ancho | Fuente en CB | Texto en CB | Fuera de CB |
|---|---|---|---|---|
| `freq[0]` | 150 px | `&sony_30` (normal) | `27.185` = `MHz.kHz` | borde inferior del span |
| `freq[1]` | 500 px | `&sony_38` (normal) | `CH 19` / `CH --` | frecuencia grande |
| `freq[2]` | 150 px | `&sony_30` (normal) | `CB C` (grid) | borde superior del span |

- La fuente normal en los tres hace que `CH`, `CB` y `--` se vean también en los temas con
  `font_digits` (Cathode, Sprint): la familia segmentada es solo dígitos.
- El «8» fantasma (`freq_shadow`) se oculta en CB y vuelve a llenarse al salir.
- Fuente y fantasma se cambian **solo en la transición** CB ↔ no-CB (`cb_row_fonts()` + un
  `static bool cb_row_on`), no en cada cambio de frecuencia.
- En CB **no** se toca `msg_tiny`. `update_freq_boundaries()` llama **siempre** a
  `band_plan_update()` y **retorna antes** de escribir los extremos si está en CB (los escribe
  `on_fg_freq_change()`).
- Se usó `ui_label_set_text()` (guardas de #150), no `lv_label_set_text()`.

`cb_plan_name()` (lógica pura, `src/cb.c`/`cb.h`) devuelve `"CB A"`..`"CB D"` y `""` para
`CB_PLAN_NONE`.

## Requisito B — la barra CB en el plan de banda

- `BP_CB` al final de `bp_type_t` (el índice indexa `colors[]`/`labels[][]`).
- Cuatro segmentos en `plan[]` entre 12 m y 10 m: 26.065–26.505 (A), 26.515–26.955 (B),
  26.965–27.405 (C), 27.415–27.855 (D).
- `colors[]` += `0x5A2CA0` (violeta); `labels[][]` += `{ "CB", NULL }`. No se tocaron `redraw()`
  ni `band_plan_segments()`.

**Contraste del label `CB` (blanco, porque `rel_lum(0x5A2CA0) = 0.065 < LUM_CORTE = 0.18`):**

| Fondo | Ratio |
|---|---|
| violeta puro `#5A2CA0` | **9,12:1** |
| violeta al 80 % de opacidad sobre fondo oscuro | **11,49:1** |
| violeta al 80 % de opacidad sobre fondo claro | **5,76:1** |

Supera AA (4,5:1) en los tres casos. La constante quedó medida por el gate `tests/check_wcag.py`
(par `hex_ffffff` sobre `hex_5a2ca0`).

## Fixtures del arnés (`tools/ui_harness`)

- `home-cb` (27.185, span 100 kHz): `27.185` · `CH 19` · `CB C`; barra llena; sin fantasma.
- `home-cb-offchannel` (27.000): `27.000` · `CH --` · `CB C`.
- `home-cb-cathode` (27.185, tema Cathode con `font_digits`): `CH 19` y `CB C` con la familia
  normal, sin fantasma.
- `home-cb-a` (26.065): `26.065` · `CH 1` · `CB A`; barra recortada a la mitad derecha.
- `home-cb-d` (27.855): `27.855` · `CH 40` · `CB D`; barra recortada a la mitad izquierda.

`scenario_settle` verifica que la fila quede escrita (con `label_suffix_on_screen`, porque el label
lleva el markup de color `#RRGGBB `). Capturas del arnés en `arnes-home-cb-*.png`.

La fila, verificada con `--inspect` en los **seis temas** (todos `27.185` · `CH 19` · `CB C`, y el
fantasma oculto en Cathode y Sprint):

| Tema | Color del texto | Fantasma |
|---|---|---|
| Aurora | `#F8EEFF` | no existe (sin `font_digits`) |
| Cathode | `#F9D9AA` | oculto |
| Redshift | `#FFB3A0` | no existe |
| Sprint | `#F5F7F9` | oculto |
| Sunlit | `#162436` | no existe |
| Trail | `#F2EED8` | no existe |

## A/B (`AB-CB169`)

`uv run x6200-ab compare --a 73fdd055 --b 19ef1ebe --experiment-id AB-CB169` → **PASS** (Functional,
Visual Regression, Quality Contract y Performance; Hardware/Energy BLOCKED por no haber radio).

Solo cambian `home-cb` y `home-cb-offchannel`. Todas las cajas de píxeles distintas caen en
`y=138..160` (franja del plan de banda) y `y=172..190` (fila de frecuencia). **Nada** de la barra de
info, el S-meter, el waterfall, los botones ni las apps. Informe completo en
`tools/ab_testing/reports/AB-CB169/`.

## Gates (salida real)

```text
$ tests/native/run_native_tests.sh                     -> 40288 comprobaciones, 0 fallos (band_plan + cb)
$ python3 tests/check_wcag.py                          -> pares OK: 1052 | FALLAS: 0
$ python3 tests/check_cfg_fills.py                     -> OK: los 70 campos se inicializan, en orden
$ python3 tests/check_actions.py                       -> todas las acciones ofrecidas tienen manejador
$ tests/check_harness_determinism.sh                   -> PASS: deterministic (identical PNGs for every fixture)
$ uv run ruff check . && ruff format --check . && ty check   -> all passed
$ uv run pytest -m "not e2e" -q                        -> 227 passed, 13 deselected
$ TMPDIR=/var/tmp/pytest-169 uv run pytest -m e2e -q   -> 13 passed
$ uv run x6200-ab contracts --revision 19ef1ebe        -> PASS — 943 checks, 0 failure(s)
$ uv run x6200-ab audit --revision 19ef1ebe            -> 0 reproduced, 0 structural findings
$ uv run x6200-ab themes                               -> 0 failures
$ uv run x6200-ab compare --a 73fdd055 --b 19ef1ebe --experiment-id AB-CB169 -> PASS
```

## Compilación ARM y banco QEMU (gate #86)

Cross-compile con el toolchain de la ROM (volumen privado `x6200-build-cb146-private`,
`make x6200-gui-dirclean` + `x6200-gui-rebuild`):

```
binario: md5 c130665e88d6   (13.859.408 B, ELF32 ARM, 3/3 marcas del port)
fuente:  md5 ff9fadfeb15a   (src/ + lvgl/src)
```

Banco con `CHROOT=/var/tmp/x6200-cb169-chroot` (privado), `VOLUMEN=x6200-build-cb146-private`,
`RECONSTRUIR=no`. Capturas:

| Captura | Frecuencia | Fila | Franja |
|---|---|---|---|
| `banco-cb-c-27.185.png` | 27.185 (CB C) | `27.185` · `CH 19` · `CB C` | violeta `CB` llena |
| `banco-cb-offchannel-27.000.png` | 27.000 | `27.000` · `CH --` · `CB C` | violeta `CB` |
| `banco-cb-a-26.065.png` | 26.065 (CB A) | `26.065` · `CH 1` · `CB A` | violeta, mitad derecha |
| `banco-cb-d-27.855.png` | 27.855 (CB D) | `27.855` · `CH 40` · `CB D` | violeta, mitad izquierda |
| `banco-cb-c-cathode-27.185.png` | 27.185, tema Cathode | `CH 19`/`CB C` en familia normal, sin fantasma | violeta `CB` |

`banco-estado.txt`: `{"band":"CB D","freq":27855000,"mode":"AM","pwr":40,"tx":false,...}` — **no se
envió ningún comando de TX**. La potencia en CB es 4.0 W (tope `cb_pwr_max`).

## PENDIENTES

- **Checklist en la radio (§5.4 del issue)**: no ejecutado en esta corrida (solo banco QEMU). Ítems
  a medir con la radio y autorización: (1) fila y franja en 27.185; (2) el dial cambia `CH n`; (3)
  grids A/B/D y barra recortada; (4) off-channel y enganche; (5) 23→24; (6) los seis temas en la
  pantalla real; (7) el ajuste *Band plan* apagado/prendido; (8) salir a 40 m; (9) `CH nn` en la
  celda VFO; (10) sin timers ni wakeups extra (diff de `lv_timer_create` y consumo).
- **F0 de #146** (RF real en 27 MHz, potencia, armónicos, PTT del micrófono): sigue
  `requires-radio`; este issue es solo UI y **no** declara el TX validado.
