# #251 — Tipografía y separación de iconos/estados en la barra superior (HOME, barra izquierda)

Corrida del **2026-10-23** del hijo 2/2 de la épica
[#249](https://github.com/iariap/x6200_gui/issues/249), issue
[#251](https://github.com/iariap/x6200_gui/issues/251).

| | |
|---|---|
| Worktree | `/home/pablo/desa/asf/state/worktrees/x6200/home-status-legibility-251` |
| Rama | `factory/home-status-legibility-251` (el issue pide `feat/home-status-legibility`) |
| Base | `8549891e` = HEAD de `origin/main` (revalidado al iniciar y al cerrar; no avanzó) |
| Commits | `HEAD` del worktree (ver §7), identidad `Pablo Iaria <iariap@gmail.com>` |
| Push | **no** (el PR contra `main` lo abre el orquestador) |
| Aislamiento | recursos propios: worktree propio, `docker run --rm`, build del arnés en `tools/ui_harness/build` (gitignored); **cero** contenedores dejados |
| Radio | **no se tocó** (sin SSH/flash/deploy/TX/RF/`sudo`). QEMU/blanco de hardware: §6 |

Épica #249: base y destino **`main`**; **no** se usó `lu2dsx/x6200-rom` en ningún
paso (ni merge, ni rebase, ni cherry-pick). El merge de este PR depende de #250
integrado; **no** se rebasó (lo coordina el orquestador).

---

## 1. Alcance implementado (contrato del issue, al pie de la letra)

| Archivo | Cambio |
|---|---|
| `src/ui_tokens.h` | `UI_INFO_CELL_GAP` 2 → **6**, `UI_INFO_ROW_PITCH` 22 → **26**; tokens nuevos `UI_INFO_CELL_PAD_V` (2), `UI_INFO_FONT_BIG` (`&sony_22`), `UI_INFO_BT_H` (20); 4 `_Static_assert` de los límites |
| `src/info.c` | pad vertical **local** de la celda = `UI_INFO_CELL_PAD_V` en las seis celdas; `info_agc`/`pre_att`/`atu` dibujan con `status_font_big()` (`sony_22`, con `font_down(..., VISION_FONT_STEPS)` bajo VISION); VFO/modo conservan su `font_down` ya probado; WiFi/BT sin tocar |
| `tests/check_info_bar.py` | **nuevo**: chequeo determinista del contrato (tokens + límites + pad local + fuentes + runa de BT). RED antes del fix (log en `logs/check_info_bar-RED.log`) |
| `tests/run_all_tests.sh` | corre el chequeo nuevo |
| `docs/evidencia/2026-10-23-home251/**` | esta evidencia (PNG antes/después, `--inspect` medido, logs) |

No se tocó `src/clock.c` (#250), ni `src/styles.c`, ni el layout flex de 2 filas,
ni el S-meter, el espectro, el waterfall, las softkeys, los timers, las teclas ni
el DSP. No se agregó ningún icono, control, setting ni fuente.

Sobre el enforce: los límites de la rejilla quedan como `_Static_assert` en
`src/ui_tokens.h`, así que **cada compilación** (el build de la ROM, el arnés de
host y el job `contracts` del CI, que compila el arnés del sha del PR) los
verifica; `tests/check_info_bar.py` agrega lo que el compilador no ve (pad local,
fuentes por celda, runa de BT) y corre en `tests/run_all_tests.sh`. No se tocó
`.github/**` (fuera del alcance del issue).

### Geometría cerrada (tokens)

```
celda 56x26, rejilla 3x2, origen (15,5), gap 6 (col pitch 62), row pitch 26,
WiFi = mitad de celda (28), runa BT 14x20 con dy 8
borde derecho = 15 + 3*56 + 2*6 = 195  <= 206 (barra legacy) / 209 (simple)
fondo de la 2da fila = 5 + 26 + 26 = 57 <= 61
fondo de la runa BT = 5 + 26 + 8 + 20 = 59 <= 61
```

Los cuatro límites quedan como `_Static_assert` en `src/ui_tokens.h` (se
verifican en cada compilación, sin dependencia de tema) y el mismo cálculo lo
repite `tests/check_info_bar.py` con mensaje legible.

---

## 2. Medición REAL con `ui_harness --inspect` (la que verifica el criterio)

Comando (imagen `x6200-uiharness`, worktree montado, build en `tools/ui_harness/build`):

```sh
docker run --rm --entrypoint bash --user "$(id -u):$(id -g)" -v "$PWD:/src" \
  -w /src/tools/ui_harness/build x6200-uiharness \
  -lc './ui_harness --scenario ../fixtures/<fixture>.scn \
        --png out/<fixture>.png --json out/<fixture>.json --inspect'
```

`medir.py` (en este directorio) lee el árbol LVGL vivo del JSON y extrae las seis
celdas. Salidas crudas: `logs/medir-antes.log`, `logs/medir-despues.log`;
tablas por celda: `mediciones/antes.csv`, `mediciones/despues.csv`.

### 2.1 Antes → después (todos los fixtures, mismo resultado)

| | antes | después | contrato #251 |
|---|---|---|---|
| filas (contenedor flex) | `y=5` y `y=27`, ancho 172 | `y=5` y `y=31`, ancho 180 | pitch **26** |
| solape de filas | **4 px** (26 > 22) | **0 px** | cero solapamiento |
| gap horizontal | 2 px (`x=15,73,131`) | **6 px** (`x=15,77,139`) | **6 px** |
| borde derecho de la 3ra celda | 187 | **195** (barra 209/206) | dentro |
| fondo de la 2da fila | 53 | **57** (barra 61) | dentro |
| runa BT | `159,30 16x22` (fondo 52) | `167,34 16x22` (fondo **56**) | dentro, sigue al WiFi |
| `text_box.h` de la celda | **16** (< line-height 18: apretado) | **22** (22px útiles) | el texto de 22 px entra |
| fuente AGC / PRE-ATT / ATU | `sony_20` | **`sony_22`** | 22 donde cabe |
| fuente VFO / MODE / WIFI | `sony_20` | `sony_20` | 20 |

### 2.2 Anchos naturales medidos (celda de 56 px, nunca hay wrap)

Anchura de texto que reporta LVGL (`text_natural.w`) y caja visible, por estado.
Ninguna celda envuelve (`long_mode` WRAP con `natural <= box`), ninguna se trunca
(`text_overflow`/`text_truncated` en `false`) y en las seis `visible == caja`
(cero recorte por el padre).

| celda | fixture / estado | fuente | natural | caja | visible |
|---|---|---|---|---|---|
| VFO | `home-usb-20m` **VFO-A** / `main-tx-simulado` **SPL-A** | sony_20 | 48 / 46 | 56x22 | 56x26 |
| VFO | `home-cb` **CH 19** / `home-cb-offchannel` **CH --** | sony_20 | 41 / 40 | 56x22 | 56x26 |
| MODE | `dfl-editor-usb-d` **USB-D** (peor caso) | sony_20 | 49 | 56x22 | 56x26 |
| MODE | `home-cb` **AM** / `home-usb-20m` **USB** | sony_20 | 24 / 31 | 56x22 | 56x26 |
| AGC | `home-usb-20m` **AUTO** / `main-tx-simulado` **FAST** | sony_22 | 43 / 39 | 56x22 | 56x26 |
| PRE-ATT | `home-usb-20m` **P/A** / `main-tx-simulado` **PRE** / `home-frecuencia-extrema` **ATT** | sony_22 | 25 / 29 / 30 | 56x22 | 56x26 |
| ATU | `home-usb-20m` **ATU1** / `home-frecuencia-extrema` **ATU3** | sony_22 | 37 / 40 | 56x22 | 56x26 |
| WIFI/BT | símbolo `U+F1EB` + runa de línea | sony_20 | 18 (en 28) | 28x22 | 28x26 |

### 2.3 Radio de impacto (blast radius) — medido por píxeles

`diff_regiones.py` compara los PNG antes/después de los 12 fixtures con el umbral
del pipeline A/B (|dR|+|dG|+|dB| > 12):

```
fixture                  cambiados en la barra   fuera
dfl-editor-usb-d              3379        3379       0
home-cb-offchannel            3059        3059       0
home-cb                       3134        3134       0
home-frecuencia-extrema       3476        3476       0
home-usb-20m                  3254        3254       0
main-tema-aurora              3254        3254       0
main-tema-cathode             3447        3447       0
main-tema-redshift            2843        2843       0
main-tema-sprint              3103        3103       0
main-tema-sunlit              3508        3508       0
main-tema-trail               3439        3439       0
main-tx-simulado              3678        3678       0
```

**0 píxeles cambiados fuera de `x=0..209, y=0..61`** en los 12 fixtures: el
S-meter (arranca en x=210), el reloj/batería (x=591..), la frecuencia, el
espectro, el waterfall y las teclas quedan intactos. Log:
`logs/diff-regiones.log`.

---

## 3. Hallazgo medido: el slot 22 de las familias de la colección no es más grande

El contrato pide `sony_22` en AGC/PRE-ATT/ATU. Medido, en los **seis temas de la
colección** el cambio de slot **no agranda el glifo**: la migración de fuentes de
#9 (`tools/fonts/fonts.py`) genera las familias libres con los slots 20 y 22 **al
mismo tamaño de píxel**, así que los `.c` de ambos slots son idénticos:

```
== familias de los 6 temas: slot 20 vs slot 22 (contenido normalizado) ==
inter            slot20=3e9b8b784be3 slot22=3e9b8b784be3 IDENTICOS   (Aurora)
ibmplexsans      slot20=97494557c838 slot22=97494557c838 IDENTICOS   (Cathode, Trail)
notosans         slot20=bdbc3d6d1f27 slot22=bdbc3d6d1f27 IDENTICOS   (Sunlit)
sourcesans3      slot20=d7e3ea0efc05 slot22=d7e3ea0efc05 IDENTICOS   (Redshift)
robotocondensed  slot20=66a9a138b80c slot22=66a9a138b80c IDENTICOS   (Sprint)
== familia stock del equipo (sin familia de tema) ==
sony_20  Size: 14 px
sony_22  Size: 15 px      <- 1 px más: acá el cambio SÍ agranda
```

Doble verificación con el binario (control negativo, mismo build, misma escena):
forzar `status_font_big()` a `sony_20` y capturar `home-usb-20m` da un PNG
**byte-idéntico** al de `sony_22` (`md5 b1bab8a98c783007dd1b03d76a83725c` en los
dos); el bbox de tinta de AGC/ATU/P/A es el mismo (p.ej. `AUTO` 42x11 en ambos).
El JSON sí cambia (`text_font`), porque el objeto resuelve a otro slot.

**Qué cambia de verdad, entonces (#251, medido y visible en los seis temas):** el
aire horizontal (2→6 px), el pitch vertical (22→26, sin el solape de 4 px) y la
**caja del texto** (16→22 px, ver §2.1): antes la caja de contenido (16 px) era
**más chica que la altura de línea (18 px)** y el texto de la celda quedaba
apretado contra el padding; ahora tiene las 22 px útiles. La subida de slot queda
aplicada según el contrato y es la que da el +1 px con la familia stock del
equipo.

Cambiar a un salto de tamaño *visible en los seis temas* (p.ej. `sony_24`) es
**otra decisión del owner**: el issue la prohíbe explícitamente ("no elegir otra
fuente ni modificar controles sin nuevo contrato"). Queda como pendiente en §8.

---

## 4. Capturas antes/después (800×480)

`before/<fixture>.png` y `after/<fixture>.png`, misma escena, mismo seed, misma
revisión del arnés (solo cambian `src/info.c` y `src/ui_tokens.h`):

`main-tema-aurora`, `main-tema-cathode`, `main-tema-redshift`, `main-tema-sprint`,
`main-tema-sunlit`, `main-tema-trail` (los seis temas de la colección) y
`home-usb-20m`, `home-cb`, `home-cb-offchannel`, `main-tx-simulado`,
`home-frecuencia-extrema`, `dfl-editor-usb-d` (estados borde).

---

## 5. Matriz de estados borde

| estado del issue | fixture | cubierto |
|---|---|---|
| `VFO-A` | `home-usb-20m`, `home-frecuencia-extrema` | sí (48 px) |
| `SPL-A` | `main-tx-simulado` (split) | sí (46 px) |
| `SPL-B` | — | **BLOCKED**: ningún fixture del corpus pone `vfo B` y el banco QEMU no está disponible (§6); `SPL-A` es el mismo molde de 5 caracteres y deja 10 px de margen, así que el 1 px de diferencia de glifo no puede cambiar el resultado |
| `CH 19` / `CH --` | `home-cb` / `home-cb-offchannel` | sí (41 / 40 px) |
| `USB-D` (modo más ancho) | `dfl-editor-usb-d` | sí (49 px) |
| `CW-R` | — | **BLOCKED**: ningún fixture del corpus usa `CW-R`; el peor caso del modo es `USB-D` (49 px) y `CW-R` mide 42 px con la misma fuente (UI-014), o sea entra con más margen |
| `AUTO` / `FAST` | `home-usb-20m` / `main-tx-simulado` | sí (43 / 39 px) |
| `P/A` / `PRE` / `ATT` | `home-usb-20m` / `main-tx-simulado` / `home-frecuencia-extrema` | sí (25 / 29 / 30) |
| `ATU1` / `ATU3` | `home-usb-20m` / `home-frecuencia-extrema` | sí (37 / 40) |
| CB | `home-cb`, `home-cb-offchannel` | sí |
| WiFi/BT on/off | — | **BLOCKED en HOST**: `wifi_get_status()` devuelve `WIFI_OFF` fijo y `bt_state_on()/bt_state_connected()` devuelven `false` fijo (`tools/ui_harness/mocks/mock_ui_deps.c`), no hay clave de escenario que los mueva. La **geometría** de la celda (28x26) y de la runa (14x20, dy 8) sí queda medida (§2); los estados de color y el `on/off` quedan para QEMU/radio (§8) |
| 6 temas | `main-tema-*` | sí |
| VISION | — | **BLOCKED** (abajo) |

### VISION

`theme_shape.vision` **no tiene setter alcanzable** en este árbol: ningún tema de
los seis, ninguna base de `theme.c` (`base_shapes[]`) y ninguna clave de escenario
lo pone en `true` (el contrato `theme_shape.vision` quedó sin uso al retirarse el
tema VISION en #74). No hay fixture que renderice VISION, así que su `--inspect`
**no se puede producir** y no se inventa un PASS.

Lo que sí queda cubierto: el patrón exacto del contrato
`font_down(UI_INFO_FONT_BIG, VISION_FONT_STEPS)` está en `info.c` y lo verifica
`tests/check_info_bar.py` en cada corrida; y la aritmética de `font_down` es la ya
probada por UI-012/UI-014 sobre las celdas de VFO/modo. Reproducirlo con imagen
necesita reactivar VISION (nuevo contrato del owner) o el banco QEMU.

---

## 6. Gates

Todo lo de abajo es salida real; SKIP/BLOCKED llevan su razón y **nunca** se
cuentan como PASS.

| Gate | Comando | Resultado |
|---|---|---|
| Chequeo focalizado nuevo | `python3 tests/check_info_bar.py` | **OK** (`logs/check_info_bar.log`); RED antes del fix: `logs/check_info_bar-RED.log` |
| Nativos | `sh tests/native/run_native_tests.sh` | **OK** `138 comprobaciones, 0 fallos` (`logs/native.log`) |
| Paridad CI | `sh tests/native/run_native_tests.sh --ci-parity` | **OK** `138 comprobaciones, 0 fallos` + `ci-parity: OK (build host sin lvgl/)` (`logs/native-ci-parity.log`) |
| Suite completa quick | `sh tests/run_all_tests.sh --quick` | ver §6.1 |
| Determinismo del arnés | `sh tests/check_harness_determinism.sh` | ver §6.2 |
| Natural | `sh tests/check_harness_natural.sh` | ver §6.2 |
| Wakeups | `sh tests/check_harness_wakeups.sh` | ver §6.2 |
| WCAG | `python3 tests/check_wcag.py` | ver §6.1 |
| Ajustes (foco / grilla) | `python3 tests/check_settings_focus.py`, `python3 tests/check_settings_grid.py` | ver §6.1 |
| Geometría (inspect) | `x6200-ab contracts --revision <sha> --repo <worktree>` | ver §6.3 |
| Temas | `x6200-ab themes` | ver §6.3 |
| Matriz del harness | `scripts/apps/x6200/check-matrix.sh origin/main HEAD <worktree>` | ver §6.4 |

### 6.1 `run_all_tests.sh --quick`

`18 OK, 0 fallos` (`logs/run_all_quick.log`). Incluye el chequeo nuevo
(`info_bar: OK`), `wcag: OK` (`todos los pares cumplen WCAG 2.1 (AA)`),
`settings_focus: OK`, `settings_grid: OK`, `host: OK` (`138 comprobaciones, 0
fallos`), `host_ci_parity: OK` y `drawbuf_partial: OK` (`PASS: partial draw
buffer renders the identical physical framebuffer`).

SKIP con razón (no PASS):

- `wspr`, `sstv`, `wefax`: `SKIP: la imagen x6200-uiharness no trae
  /usr/include/fftw3.h` (`build full: docker build -t x6200-uiharness
  tools/ui_harness`); son suites de codecs, no tocan la UI.
- `js8`: omitido por `--quick`.
- `webserver`: no hay checkout `.x6200_webserver`/volumen de buildroot en este host.

También corren sueltos, con la misma salida: `python3 tests/check_info_bar.py`
(OK), `python3 tests/check_wcag.py` (OK), `python3 tests/check_settings_focus.py`
(OK) y `python3 tests/check_settings_grid.py` (OK).

### 6.2 determinismo / natural / wakeups

`logs/harness-gates.log` (los tres con `rc=0`; el diff no toca
`tools/ui_harness/**`, así que la matriz los reporta SKIP, pero se corrieron
igual porque el orquestador los pedía):

```
PASS: deterministic (identical PNGs for every fixture)          determinism rc=0
PASS: natural bench deterministic, profiled and delimited (main-natural)   natural rc=0
PASS: wakeup profile measured, sourced and delimited (main-natural)        wakeups rc=0
  loop: 14 wakeups with the data drive (11 without it) in 1000 ms; sleeps 10-200 ms
```

### 6.3 `x6200-ab themes` / `contracts`

- `uv run x6200-ab themes` → `rc=0`: todos los objetos no-texto medidos sobre
  los golden siguen sobre el mínimo (`s_meter_arc`, `waterfall_signal`,
  `waterfall_center_line`, `focus_indicator`, `cw_tune_bars`); log
  `logs/ab-themes.log`. **No se regeneró ningún golden**: el cambio no mueve
  ninguno de los objetos que ese chequeo mide (su README sólo pide regenerarlos
  cuando se mueve un objeto medido), y el diff de píxeles queda acotado a la barra
  izquierda (§2.3).
- `uv run pytest -m "not e2e"` (inventario/contratos del framework A/B, que la
  matriz corre porque cambió `tools/ab_testing/`): `227 passed, 13 deselected`
  (`logs/ab-pytest.log`).
- Se declaró el cambio cosmético en `quality/contracts.yaml`
  (`expected_changes`, región `barra_superior`, autor `home-status-legibility-251`,
  hasta 2027-03-31) siguiendo el mismo idioma que UI-011/UI-012/UI-014: la región
  existe en `regions.yaml` (0,0,800,62) y el cambio cae entero dentro de ella.
- `x6200-ab contracts --revision <sha>`: ver §6.4 (lo corre la matriz).

### 6.4 Matriz del harness

`scripts/apps/x6200/check-matrix.sh origin/main HEAD <worktree>` → **exit 1** por
**un solo** check, `check_dialog_coverage`, que es un **desajuste preexistente
entre el harness y `main`**: el script referencia
`tests/check_dialog_coverage.py`, que existe en el trunk (`origin/lu2dsx/x6200-rom`)
pero **no en `main`** (por eso está fuera de la épica #249), y el bloque se
dispara con cualquier cambio en `src/`, `tests/**` o `tools/ui_harness/**`:

```
==> [check] check_dialog_coverage
    FAIL:
    /usr/bin/python3: can't open file '.../tests/check_dialog_coverage.py': [Errno 2] No such file or directory
```

Evidencia del desajuste (no del diff):

```
$ git ls-tree --name-only origin/main tests/ | grep -i dialog          # (vacío)
$ git ls-tree --name-only origin/lu2dsx/x6200-rom tests/ | grep -i dialog
tests/check_dialog_coverage.py
```

Ningún otro check de la matriz falló (log completo: `logs/check-matrix.log`):

```
==> [check] native tests (tests/native/run_native_tests.sh)                 ok
==> [check] check_cfg_fills / check_actions / check_settings_focus          ok
==> [check] check_apps_menu / check_app_handlers / check_manual_apps        ok
==> [check] check_wcag / check_cw_tune_feed / check_settings_grid           ok
==> [check] check_autotune_v2 / check_build_identity                        ok
==> [check] check_dialog_coverage                                           FAIL (arriba)
==> [check] pytest -m 'not e2e' (ab_testing)                                ok
==> [check] x6200-ab themes                                                 ok
==> [check] x6200-ab contracts (04f98b79…)                                  ok   <- el gate de geometría (#inspect)
==> [check] bash -n tests/run_all_tests.sh                                  ok
==> [SKIP] check_wcag (temas de la imagen): sin cambios en rootfs/**/themes/*.json
==> [SKIP] ruff del framework A/B: sin cambios en tools/ab_testing/**/*.py
==> [SKIP] harness determinism gate: sin cambios en tools/ui_harness/
==> [check] banco QEMU: preflight (tools/captura-banco.sh --check)          BLOCKED (entorno)
```

`x6200-ab contracts` evaluó el sha `04f98b79…` (materializa el worktree del sha,
compila el arnés en `x6200-uiharness` y corre **todos** los fixtures con las reglas
de layout/typography sobre `--inspect`) y dio **ok**: es el "inspect de geometría"
del pedido. Salida propia en `logs/ab-contracts.log`.

**Banco QEMU (QEMU_USER, ARM real, sin radio): BLOCKED** por entorno, con la salida
real del preflight (no es un PASS y no se corrió):

```
FALTA: falta el volumen x6200-build-output (build del GUI)
FALTA: falta el chroot (../.vfix3): extraer el rootfs de la imagen, ver docs/TESTING.md nivel 4
FALTA: falta tools/remote/x6200-remote (compilar con tools/remote/compila.sh)
== 5 cosa(s) faltan: el banco no puede correr ==
```

Por eso la QA visual RX/TX queda **NO verificada en ARM/QEMU** (§8): lo que hay es
la simulación de HOST (arnés) y ningún contacto con la radio.

---

## 7. Commits

Identidad `Pablo Iaria <iariap@gmail.com>` (config del repo, sin `-c user.name`).
Por rutas explícitas (nunca `git add -A`); `.asf-active` queda **sin commitear**.
**Sin push**: el PR contra `main` lo abre el orquestador.

| Commit | Qué |
|---|---|
| 1 — **`04f98b79`** | El cambio: `src/info.c`, `src/ui_tokens.h`, `tests/check_info_bar.py`, `tests/run_all_tests.sh`, `tools/ab_testing/quality/contracts.yaml` y la primera evidencia |
| 2 — HEAD (`git log -1 --format=%h` del worktree) | Sólo documentación del harness: README final de la evidencia + `logs/{check-matrix,ab-contracts,ab-themes,ab-pytest,harness-gates,native*}.log`, y el formato `ruff` de `tests/check_info_bar.py` |

Los gates de §6.3/§6.4 (matriz y `x6200-ab contracts`) evaluaron el commit 1
(`04f98b79…` — es el sha que aparece en `logs/check-matrix.log` y
`logs/ab-contracts.log`); el commit 2 no toca `src/`, y del lado de `tests/` sólo
trae el formato de `check_info_bar.py`, cuyo chequeo se volvió a correr después
(OK).

---

## 8. Pendientes / PENDING_HARDWARE

- **VISION** no se puede renderizar en HOST (no hay setter alcanzable) → su
  `--inspect` queda pendiente hasta que exista un contrato que reactive el modo o
  hasta que corra el banco QEMU.
- **`SPL-B`, `CW-R`, WiFi/BT on-off**: sin fixture en el corpus (ver §5). El banco
  QEMU —que podría reproducirlos con el rootfs/la radio— está **BLOCKED** por
  entorno (§6.4): faltan el chroot `.vfix3`, el volumen `x6200-build-output` y el
  cliente de control en este host.
- **QA visual RX/TX (NO RF)**: lo medido es el **arnés de host** (binario de host,
  framebuffer en RAM): estado RX en los fixtures de pantalla principal y **TX** en
  `main-tx-simulado` (visor TX + split). El **QEMU_USER** (binario ARM + rootfs)
  quedó BLOCKED por entorno, así que la lectura en pantalla de la radio **no** está
  verificada acá: no se declara nada sobre la radio.
- **Salto de tamaño visible en los seis temas** (§3): si el owner quiere que el
  "22" se vea más grande en la colección, hace falta otro contrato (slot mayor o un
  mapeo de familias que no colapse 20/22).
- **Paridad BASE 1.0.7 / radio 1.0.6**: este cambio es cosmético y no toca
  `X6200Control` ni el firmware; **no** se afirma compatibilidad ni un GO de
  release (advertencia de #249 / PR #223).
- Merge: depende de #250 integrado; el rebase sobre el `main` nuevo y el gate
  combinado los coordina el orquestador (**acá no se rebasó**, por instrucción).

---

## 9. Reproducir

```sh
# 1) arnés (host, contenedor efímero; no deja contenedores)
REV=$(git rev-parse HEAD)
docker run --rm --entrypoint bash --user "$(id -u):$(id -g)" -v "$PWD:/src" \
  -w /src/tools/ui_harness x6200-uiharness \
  -lc "mkdir -p build && cd build && cmake .. -DHARNESS_REVISION=$REV && make -j\$(nproc)"

# 2) captura + inspect por fixture (ver §2), con el build en tools/ui_harness/build
# 3) medición
python3 docs/evidencia/2026-10-23-home251/medir.py <dir-json> [--csv salida.csv]
python3 docs/evidencia/2026-10-23-home251/diff_regiones.py <dir-antes> <dir-despues>
# 4) contrato determinista
python3 tests/check_info_bar.py
```

