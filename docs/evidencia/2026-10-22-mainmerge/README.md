# Sincronización `main` ← trunk (`lu2dsx/x6200-rom`) — merge real

Corrida del **2026-10-22** ordenada por el owner: dejar `main` (el espejo del
upstream de R1CBU) al día con la línea mantenida de la ROM, para poder tomar
issues nuevos sobre `main`.

| | |
|---|---|
| Worktree | `/home/pablo/desa/asf/state/worktrees/x6200/trunk-to-main` |
| Rama | `factory/trunk-to-main` |
| Base | `e1476ece` (`origin/main`, tip del upstream) |
| Mergeado | `d4fb57f8` (`origin/lu2dsx/x6200-rom`, 691 commits) |
| Merge base | `6a29687d` |
| Commit del merge | `895bfbac` — 2 padres (`e1476ece`, `d4fb57f8`) |
| Identidad | `Pablo Iaria <iariap@gmail.com>` (author y committer) |
| Push | **no** (el PR a `main` lo abre el orquestador) |

Los 3 commits de upstream que `main` tenía y el trunk no:

| Commit | Título | Archivos |
|---|---|---|
| `c74df4a3` | Fixes for 1.0.7 BASE, some changes | 10 |
| `d6776bd9` | Add CW zoom control | 6 |
| `e1476ece` | Bump version | 1 |

Total: **12 archivos** (`git diff --name-status 6a29687d origin/main`).

---

## 1. El conflicto: `src/main.h`

`git merge origin/lu2dsx/x6200-rom` da **un** conflicto, exactamente el que
anticipaba el dry-run (`git merge-tree`): la línea `VERSION`.

```c
<<<<<<< HEAD
#define VERSION "v0.1.2"
=======
#define VERSION "v0.1.2-lu2dsx"
>>>>>>> origin/lu2dsx/x6200-rom
```

Resolución (**fork**):

```c
/* Version del proyecto (v0.1.2 = 1.0.7 BASE) + sufijo de la ROM de LU2DSX.
 * Sincronizacion trunk->main 2026-10-22: upstream subio v0.1.1 -> v0.1.2 en
 * este mismo archivo; el fork ya usaba v0.1.2 y agrega el sufijo que lo
 * identifica como ROM. Se conserva la cadena del fork (identidad de build,
 * ver tests/check_build_identity.py). */
#define VERSION "v0.1.2-lu2dsx"
```

Por qué no se pierde nada de upstream: el aporte de `e1476ece` es el **número**
(`v0.1.1` → `v0.1.2`), y el fork ya estaba en `v0.1.2`: su cadena es
`v0.1.2` + el sufijo que identifica la ROM. La identidad de build es un
contrato del repo (`tests/check_build_identity.py`, sello `tools/fuente-md5.sh`)
y el sufijo es lo que distingue un binario de la ROM de uno del upstream.

El resto de `src/main.h` no lo tocó upstream (`mfk_inner`, `lastbreath_mark_clean_now()`
vienen del fork y quedaron intactos).

---

## 2. Los otros 11 archivos de upstream, uno por uno

Veredicto sobre el delta **de upstream** aplicado al árbol del fork. La
columna «Delta vs fork» es lo que quedó en `HEAD` respecto de
`origin/lu2dsx/x6200-rom` (o sea, el aporte real de upstream al árbol).

| # | Archivo | Veredicto | Evidencia |
|---|---|---|---|
| 1 | `src/main.h` | **conflicto → fork** (`VERSION`) | §1 |
| 2 | `src/cfg/cfg.h` | compatible | Renombra `cfg_item_t fft_dec` → `fft_zoom_cw` y, en `cfg_cur_t`, saca `Subject *zoom` y agrega `Subject *fft_dec` junto a `fft_width`. El fork sólo había **agregado** items (WSPR/cluster/WeFax/CW trainer) en otras posiciones: el orden `cfg_t` ↔ `fill_cfg_item()` se mantiene y `check_cfg_fills` da ok |
| 3 | `src/cfg/cfg.c` | compatible | Mueve la creación de `cfg_cur.fft_width` y el observer `update_fft_width` a `mode.c`. Las guardas del fork en `init_items()`/`load_items_from_db()` y los `boot_mark()` quedan iguales. No queda ninguna referencia al símbolo movido (grep de `update_fft_width` en `cfg.c`: vacío) |
| 4 | `src/cfg/mode.c` | compatible (el fork **no lo había tocado**) | Entra completo: crea `cfg_cur.fft_width`/`fft_dec`, engancha `update_fft_width` a `cfg_cur.fft_dec` y a `cfg.fft_zoom_cw`, y `mode_default_values()` devuelve **ancho** (`FFT_FULL_WIDTH`, `/4` en CW) en vez del factor de zoom. Los consumidores del fork del subject `cfg_cur.fft_width` (`spectrum.c`, `waterfall.c`, `band_info.c`, `main_screen.c`) no cambian de contrato |
| 5 | `src/cfg/mode.private.h` | compatible | `cfg_mode_t.zoom` → `fft_dec`. Además se renombró el parámetro del prototipo `mode_default_values(..., uint32_t *fft_width)` (upstream renombró el `.c` y dejó el `.h` con `zoom`: cosmético, cero efecto) |
| 6 | `src/dsp.cpp` | compatible **con reserva** (§4) | Entra el promedio del PSD en potencia, el mínimo con ventana y offsets por ancho, `meter_set_noise(min)` y `min -= 30`. El fork sólo había agregado 10 líneas al final (`dsp_set_spectrum/waterfall_enabled`), intactas. Contrato con `spectrum.c` (`spectrum_update_min/max`, `cfg_cur.fft_width`) y con `meter.c` (`meter_set_noise`, declarado en `meter.h`) igual que upstream |
| 7 | `src/meter.c` | compatible **con reserva** (§4) | Entra `meter_update()` con los offsets PRE/ATT (saca el `db -= 8` y el `noise_level = spectrum_get_min()`) y `meter_set_noise()`. Los bloques propios del fork (aguja, guarda de igualdad OPT-4 #150, `drawn_*`) no se tocan; la guarda sigue mirando `noise_level`, que ahora lo fija el DSP. Sólo se corrigió un comentario que quedó mintiendo (`spectrum_get_min()` → `meter_set_noise()`) |
| 8 | `src/meter.h` | compatible | Agrega `void meter_set_noise(float db);`. El fork no lo tocaba |
| 9 | `src/mfk.cpp` | compatible | El `case MFK_FFT_DECIM` pasa de `cfg.fft_dec.val` a `cfg_cur.fft_dec` (vivo) + `cfg_cur.fft_width` para el rótulo; se agrega `case MFK_CW_ZOOM`. Los casos propios del fork (GPS/voice/etc.) quedan. Un solo `case MFK_CW_ZOOM` (grep: 1) |
| 10 | `src/mfk.h` | **incompatible → fork** (posición del enum) | §3 |
| 11 | `src/buttons.cpp` | compatible | `btn_zoom` pasa a `make_btn(fft_dec_label_getter, MFK_FFT_DECIM, &cfg_cur.fft_width)` («Span: N kHz») y aparece `btn_cw_zoom` en `page_cw_decoder_1` (5 botones; `BUTTONS == 5`). La sobrecarga `make_btn(label_fn, mfk, Subject**)` ya existía en el fork, y el resto de su barra (749 líneas propias) no se toca |
| 12 | `src/radio.c` | compatible **con reserva** (§4) | El `radio_tick()` del fork era **idéntico** al de la base en esa zona (no lo había tocado), así que el hunk entra limpio: copia `samples[i] = pack->samples[i] - 126.0f`, `dbm = -pack->dbm + 4` y los observers de `cfg_cur.fft_dec` / `cfg.fft_zoom_cw`. El resto del archivo (474 líneas propias: batería, CB, hkey, poweroff) queda intacto |

Fuera de los 12: **`tools/ui_harness/mocks/mock_cfg.c`** (código del fork) se
rompía con el merge, así que se adaptó:

- `cfg_cur.zoom` ya no existe → `cfg_cur.fft_dec` (mismo rol).
- el arnés tiene que crear `cfg.fft_zoom_cw.val` (lo leen `btn_cw_zoom` y
  `cw_zoom_label_getter()`; sin él, la página CW del arnés lee un subject NULL).
- `mode_default_values()` del mock devolvía el factor de zoom (`1`, `4`); ahora
  devuelve el ancho (`FFT_FULL_WIDTH`, `/4` en CW) como el firmware, si no el
  mock deja de ser espejo.

---

## 3. `src/mfk.h`: por qué el fork gana la posición del enum

Upstream inserta `MFK_CW_ZOOM` **a media lista**:

```c
     MFK_KEY_RATIO,
+    MFK_CW_ZOOM,
 
     MFK_DNF,
```

Eso corre en uno **todos los ids siguientes** (DNF … ATT/PRE/COMP, y
`MFK_CW_DECODER_AUTO`). Los ids de `mfk_mode_t` no son sólo símbolos: son
**bits persistidos**:

- `src/params/params.h:112` → `uint64_t mfk_modes;`
- `src/params/params.c:47` → default `(1 << MFK_FFT_DECIM) | (1 << MFK_SPECTRUM_BETA) | …`
- `src/params/params.c:293-294` → se carga de params.db (`mfk_modes`)
- `src/params/params.c:491` → se guarda en params.db
- `src/mfk.cpp:789` → `loop_modes(dir, mfk_mode, params.mfk_modes, MFK_LAST-1)`

Con la inserción en el medio, un usuario que actualiza la ROM y tenía, p. ej.,
`DNF` asignado, después del upgrade ve otra tecla en ese lugar (y el bit del
final, `MFK_CW_DECODER_AUTO`, se corre también). El propio archivo ya tenía la
regla escrita (`/* Append-only: los ids son bits de params.mfk_modes y se
persisten. */`, aplicada al agregar `MFK_CW_DECODER_AUTO`).

Resolución: **se conserva la feature** (`MFK_CW_ZOOM` sigue existiendo, con su
`case` en `mfk.cpp`, su botón y su item de cfg) pero va **al final** de la
lista, después de `MFK_CW_DECODER_AUTO` y antes de `MFK_LAST`:

```c
    /* Append-only: los ids son bits de params.mfk_modes y se persisten. */
    MFK_CW_DECODER_AUTO,
    /* CW zoom viene del upstream v0.1.2, que lo inserto a media lista (entre
     * MFK_KEY_RATIO y MFK_DNF). Aca va al final a proposito: insertarlo en el
     * medio corre en uno todos los ids siguientes y reinterpreta el bitmask
     * `params.mfk_modes` que ya esta guardado en params.db ... */
    MFK_CW_ZOOM,

    MFK_LAST,
```

Ningún id existente cambia de valor, así que el `mfk_modes` que ya está en la
radio sigue significando lo mismo.

Medido sobre los tres archivos (`origin/main:src/mfk.h`,
`origin/lu2dsx/x6200-rom:src/mfk.h` y el mergeado):

| | fork | con la inserción de upstream | merge (esta resolución) |
|---|---|---|---|
| ids asignables a una tecla (antes de `MFK_LAST`) | 40 | 41 | **41** |
| ids asignables que **cambian** de valor | — | **23** | **0** |
| `MFK_CW_ZOOM` | — | 16 (a media lista) | **40** = `MFK_LAST-1` |
| `MFK_LAST` | 40 | 41 | 41 |
| ids de app (`MFK_RTTY_*`, después de `MFK_LAST`) | 41-44 | 42-45 | 42-45 (**+1**) |

Los 40 ids asignables del fork conservan su valor exacto (verificado
comparando valores, no posiciones). `MFK_CW_ZOOM` queda **dentro** del rango
que recorre el encoder (`mfk.cpp:789`, `loop_modes(..., MFK_LAST-1)`), así que
se puede asignar igual que cualquier otra tecla. Lo único que corre +1 es el
sentinel `MFK_LAST` (simbólico) y los 4 ids de app que están **fuera** del
rango del loop (no se persisten: `params.mfk_modes` sólo guarda bits de ids
menores que `MFK_LAST`), así que no tienen efecto visible.

---

## 4. Reserva (no bloquea el merge, sí el flash): el _flow_ de BASE 1.0.7

Los 3 commits de upstream son **«Fixes for 1.0.7 BASE»**: cambian cómo se
interpreta el *flow* que manda el STM32 (espectro/waterfall/S-meter/batería).
Ese cambio va **en pareja** con la lib de control y la BASE OEM
(`docs/UPSTREAM_REBASE_PLAN.md:42,129-146`).

Evidencia dura de la pareja:

| Pieza | Qué dice |
|---|---|
| Buildroot de la ROM | `aether-x6200-control.mk`: `AETHER_X6200_CONTROL_VERSION = v0.1.0` (en `rom-build/clone-buildroot` y en `AetherX6200Buildroot`) |
| lib v0.1.0 (la que compila la ROM) | `flow.h`: `float samples[512];` (sysroot ARM de los dos árboles de build) |
| lib v0.1.1 («Fix flow on 1.0.7 base») | `git diff v0.1.0 v0.1.1` en `X6200Control`: `float samples[512]` → **`uint8 samples[512]`** (+ apertura de TX por calibración) |
| GUI v0.1.2 (este merge) | `radio.c`: `samples[i] = pack->samples[i] - 126.0f;` y `dbm = -(int16_t)pack->dbm + 4` — o sea, samples **8 bits centrados en 126** |
| Radio del owner | `docs/evidencia/2026-10-19-f0-cb-tx/README.md:7`: **`BASE fw version: 1.0.6`** (medido 2026-10-19) |
| Plan del propio fork | `docs/UPSTREAM_REBASE_PLAN.md:134-135`: «Si tu radio tiene base ≥ 1.0.7 → v0.1.2 es la versión correcta. Si tiene base 1.0.3 / 1.0.6 … v0.1.2 puede mostrar mal espectro/S-meter/batería» |

Consecuencias, sin maquillar:

- **El merge en sí es correcto y compila**: `x6200_control_fft_zoom_cw_set()`
  ya existe en la lib v0.1.0 que linkea la ROM (verificado con
  `nm -D libaether_x6200_control.so.0.15.3 | grep fft`: exporta
  `x6200_control_fft_dec_set` y `x6200_control_fft_zoom_cw_set`), así que el
  observer nuevo de `radio.c` no rompe el link.
- **Pero el árbol de `main` queda calibrado para BASE ≥ 1.0.7**, y la radio
  está en BASE 1.0.6 con la lib v0.1.0 (`float`). Antes de flashear hay que
  alinear la pareja (buildroot → control **v0.1.1** + BASE OEM **1.0.7**,
  que es lo que pide `UPSTREAM_REBASE_PLAN.md:196`) **o** validar en la radio
  espectro/waterfall/S-meter; el banco QEMU no puede verlo (corre sin radio).
- Si se quiere seguir soportando BASE 1.0.6 con este `main`, el lugar para
  decidirlo es un issue aparte: las dos mitades (tipo de `samples` en la lib y
  offset en la GUI) tienen que moverse juntas, no a mano en un merge.

Nota menor de migración (heredada de upstream, no del merge): el ancho por
modo se persiste con otra clave (`spectrum_factor` → `fft_dec`), así que el
span guardado por modo vuelve al default del modo tras actualizar; y el item
global `fft_dec` que siembra `sql/params.csv` queda huérfano (la carga es por
nombre, se ignora; el item nuevo `fft_zoom_cw` usa su default 0).

---

## 5. Verificación

### 5.1 `git diff --stat origin/lu2dsx/x6200-rom..HEAD` (sólo el delta de upstream)

```
 src/buttons.cpp                   | 19 +++++++-
 src/cfg/cfg.c                     | 15 ++-----
 src/cfg/cfg.h                     |  4 +-
 src/cfg/mode.c                    | 57 +++++++++++++++---------
 src/cfg/mode.private.h            |  4 +-
 src/dsp.cpp                       | 91 +++++++++++++++++++++++++++++++++------
 src/main.h                        |  5 +++
 src/meter.c                       | 20 ++++++---
 src/meter.h                       |  1 +
 src/mfk.cpp                       | 22 +++++++++-
 src/mfk.h                         |  7 +++
 src/radio.c                       | 15 +++++--
 tools/ui_harness/mocks/mock_cfg.c | 17 +++++---
 13 files changed, 209 insertions(+), 68 deletions(-)
```

Los 12 archivos de upstream + `mock_cfg.c` (adaptación del arnés, §2). Nada
más cambia respecto del trunk: **no hay pérdida de trabajo del fork**.

Ese `--stat` es el del **commit del merge** (`895bfbac`). Con el commit de esta
evidencia encima el mismo comando agrega **sólo** los 6 archivos de
`docs/evidencia/2026-10-22-mainmerge/` (§5.7), o sea documentación de la
corrida, nada del árbol del firmware:

```
$ git diff --stat origin/lu2dsx/x6200-rom..895bfbac | tail -1
 13 files changed, 209 insertions(+), 68 deletions(-)
$ git diff --name-status origin/lu2dsx/x6200-rom..HEAD | grep '^D'
(ninguno: 0 borrados respecto del trunk)
$ git diff --name-only origin/lu2dsx/x6200-rom..HEAD | grep -v '^src/\|^tools/' | sort
docs/evidencia/2026-10-22-mainmerge/banco-captura.log
docs/evidencia/2026-10-22-mainmerge/banco-home.png
docs/evidencia/2026-10-22-mainmerge/banco-out.txt
docs/evidencia/2026-10-22-mainmerge/matrix.log
docs/evidencia/2026-10-22-mainmerge/native-tests.log
docs/evidencia/2026-10-22-mainmerge/README.md
```

### 5.2 Nada del fork se perdió

```
$ git diff --stat origin/main..HEAD | tail -1
 22539 files changed, 4577174 insertions(+), 28324 deletions(-)

$ git diff --name-status origin/main..HEAD | grep '^D'
D   src/voice.cpp
D   u-boot/u-boot-sunxi-with-spl.bin

$ git diff --name-status origin/lu2dsx/x6200-rom..HEAD | grep '^D'
(ninguno)
```

Los dos borrados son **del fork** (no del merge): `src/voice.cpp` lo reemplazó
el fork con su propia implementación (`src/voice.h` sí existe y `src/voice.cpp`
no estaba en el árbol del trunk tampoco), y `u-boot/u-boot-sunxi-with-spl.bin`
es un blob binario que el fork sacó del repo. Ningún archivo del fork se borra
por efecto del merge (`git diff` contra el trunk no reporta ningún `D`).

### 5.3 Sello de fuentes

`tools/fuente-md5.sh` corre igual y determinista (mismo hash desde dos `cwd`
distintos, dos veces):

```
$ bash tools/fuente-md5.sh                                   # desde el worktree
1d0d8c494a92f894999208bfbdcd627c
$ cd /tmp && bash .../tools/fuente-md5.sh                    # desde otro cwd
1d0d8c494a92f894999208bfbdcd627c
```

`tests/check_build_identity.py` (el check que fija el contrato: `rsync
--checksum` en `compila-gui.sh`, sello en el binario, mismo algoritmo en
`fuente-md5.sh`) da **ok** dentro de la matriz.

### 5.4 Matriz de checks

Comando:

```
scripts/apps/x6200/check-matrix.sh origin/main HEAD \
  /home/pablo/desa/asf/state/worktrees/x6200/trunk-to-main
```

Salida real (`EXIT=0`):

```
==> [check] native tests (tests/native/run_native_tests.sh)
    ok
==> [check] check_cfg_fills
    ok
==> [check] check_actions
    ok
==> [check] check_settings_focus
    ok
==> [check] check_apps_menu
    ok
==> [check] check_app_handlers
    ok
==> [check] check_manual_apps
    ok
==> [check] check_wcag
    ok
==> [check] check_cw_tune_feed
    ok
==> [check] check_build_identity
    ok
==> [SKIP] check_wcag (temas de la imagen): ya corre en el bloque de firmware (src/ cambió)
==> [check] uv sync (ab_testing)
    ok
==> [check] ruff check (ab_testing)
    ok
==> [check] ruff format --check (ab_testing)
    ok
==> [check] pytest -m 'not e2e' (ab_testing)
    ok
==> [check] x6200-ab themes
    ok
==> [check] x6200-ab contracts (895bfbacf7dbb3e226534035557cc5aeab3d8322)
    ok
==> [check] bash -n tests/bootanim/start-stop-daemon.sh
    ok
==> [check] bash -n tests/check_bootanim.sh
    ok
==> [check] bash -n tests/check_drawbuf_partial.sh
    ok
==> [check] bash -n tests/check_harness_determinism.sh
    ok
==> [check] bash -n tests/check_harness_natural.sh
    ok
==> [check] bash -n tests/check_harness_swrscan_curve.sh
    ok
==> [check] bash -n tests/check_harness_wakeups.sh
    ok
==> [check] bash -n tests/check_waterfall_incremental.sh
    ok
==> [check] bash -n tests/e2e_chroot.sh
    ok
==> [check] bash -n tests/native/codec_docker.sh
    ok
==> [check] bash -n tests/native/run_js8_tests.sh
    ok
==> [check] bash -n tests/native/run_native_tests.sh
    ok
==> [check] bash -n tests/native/run_navtex_tests.sh
    ok
==> [check] bash -n tests/native/run_sstv_tests.sh
    ok
==> [check] bash -n tests/native/run_wefax_tests.sh
    ok
==> [check] bash -n tests/native/run_wspr_tests.sh
    ok
==> [check] bash -n tests/run_all_tests.sh
    ok
==> [check] bash -n tools/barrido-apps-banco.sh
    ok
==> [check] bash -n tools/bench/compila.sh
    ok
==> [check] bash -n tools/captura-banco.sh
    ok
==> [check] bash -n tools/captura-pantallas.sh
    ok
==> [check] bash -n tools/captura-radio.sh
    ok
==> [check] bash -n tools/compila-gui.sh
    ok
==> [check] bash -n tools/fuente-md5.sh
    ok
==> [check] bash -n tools/lib-radio.sh
    ok
==> [check] bash -n tools/mide-arranque-banco.sh
    ok
==> [check] bash -n tools/prueba-apps-radio.sh
    ok
==> [check] bash -n tools/prueba-gui-en-radio.sh
    ok
==> [check] bash -n tools/release/make_release_assets.sh
    ok
==> [check] bash -n tools/release/make_update_tgz.sh
    ok
==> [check] bash -n tools/release/publish_release.sh
    ok
==> [check] bash -n tools/release/test_update.sh
    ok
==> [check] bash -n tools/remote/compila.sh
    ok
==> [check] bash -n tools/verifica-radio.sh
    ok
==> [check] harness determinism gate
    ok
==> [check] banco QEMU: preflight (tools/captura-banco.sh --check)
    BLOCKED: falta entorno (corre en el host del owner, no en el runner):
    == comprobando el banco ==
      docker: ok
      imagen x6200-buildroot: ok
      volumen x6200-build-output: ok
      FALTA: falta el chroot (/home/pablo/desa/asf/state/worktrees/x6200/trunk-to-main/../.vfix3): extraer el rootfs de la imagen, ver docs/TESTING.md nivel 4
      FALTA: el chroot no parece un rootfs (sin /bin/busybox)
      FALTA: el chroot no tiene /lib
      python3: ok
      PIL: ok
      FALTA: falta /home/pablo/desa/asf/state/worktrees/x6200/trunk-to-main/tools/remote/x6200-remote (compilar con /home/pablo/desa/asf/state/worktrees/x6200/trunk-to-main/tools/remote/compila.sh)
      buildroot: ok
      libs del banco: ok
    == 4 cosa(s) faltan: el banco no puede correr ==
EXIT=0
```

Notas de la matriz (nada de esto es un pase):

- Los grupos que no dispara el diff se reportan `[SKIP]` con el motivo (p. ej.
  `check_wcag (temas de la imagen)`, que ya corre en el bloque de firmware).
- El **banco QEMU salió `BLOCKED`** en la matriz: su preflight busca el chroot
  en `$REPO/../.vfix3` (`state/worktrees/x6200/.vfix3`, que no existe en este
  worktree) y el cliente `tools/remote/x6200-remote` sin compilar. Se levantó a
  mano (§5.5) con `CHROOT` explícito, y **ahí sí corrió y pasó**.

### 5.5 Banco QEMU (ARM real, sin radio) — corrida manual

La matriz lo deja `BLOCKED` por convención de paths, así que se corrió con el
mismo script del gate y las dos variables que faltaban:

```
$ sh tools/remote/compila.sh                      # cliente de control
$ cp -a --reflink=auto /home/pablo/desa/x6200/.vfix3 \
      /home/pablo/desa/asf/state/worktrees/x6200/vfix3-mainmerge
$ BUILDROOT=/home/pablo/desa/x6200/buildroot_new/AetherX6200Buildroot \
  CHROOT=/home/pablo/desa/asf/state/worktrees/x6200/vfix3-mainmerge \
  sh tools/captura-banco.sh /tmp/banco-mainmerge-home.png
```

Salida real (`EXIT=0`):

```
== comprobando el banco ==
  docker: ok
  imagen x6200-buildroot: ok
  volumen x6200-build-output: ok
  chroot /home/pablo/desa/asf/state/worktrees/x6200/vfix3-mainmerge: ok
  python3: ok
  PIL: ok
  cliente de control: ok
  buildroot: ok
  libs del banco: ok
  qemu-arm-static ejecutando ARM: ok
  limpiando la GUI (dirclean): el rebuild reusa una copia propia del repo
  reconstruyendo la GUI: el hash de los fuentes cambio y un fuente es mas nuevo que el binario
OK: .../vfix3-mainmerge/usr/sbin/x6200_gui es el firmware del port (ELF32 ARM, 13929076 B, 3/3 marcas)
  captura: /tmp/banco-mainmerge-home.png (11455 colores distintos)
  version: 895bfbac Sincronizar main con el trunk (lu2dsx/x6200-rom)
  fuente: md5 1d0d8c494a92 (src/ + lvgl/src)
  binario: md5 50590a86e9a9   qemu-cpu: cortex-a7
  captura util (140888 bytes)
```

Qué prueba esto:

1. **El árbol mergeado compila para ARM** con el buildroot de la ROM
   (`compila-gui.sh`/package `x6200-gui`, `-j nproc`, binario
   `13929076 B`), o sea también contra la **lib de control v0.1.0** que
   tiene `float samples[512]` — el punto que más importaba comprobar después
   del merge (`radio.c` nuevo + `dsp.cpp`/`meter.c` nuevos).
2. **Arranca y dibuja**: la GUI corre bajo `qemu-arm-static` (cortex-a7) sin
   radio y la captura de la home es útil y no vacía (140.888 B; el umbral del
   gate es 4.000 B), con el sello de fuentes `1d0d8c494a92` = el mismo
   `hash` de `tools/fuente-md5.sh`.
3. El binario pasa el guard del banco (`verifica-firmware-port.py`: es el
   firmware del port, 3/3 marcas).

Del log de arranque del banco (`$CHROOT/out-banco.txt`, 304 líneas), lo único
que toca a este merge es lo esperado en una base sin esos items persistidos:

```
[Warn] cfg_params_load_item: No results for load fft_zoom_cw      (params.c:77)
[User] load_items_from_db: Can't load fft_zoom_cw (pk=0)          (cfg.c:219)
[Warn] cfg_mode_params_load_item: No results for load from mode_params with name: fft_dec and mode: 6
[User] cfg_mode_params_save_item: Saved fft_dec=0 (pk=6)
```

Es el camino normal de un alta: se usa el default (0) y se persiste. **No hay
`CRASHLOG` de excepción, ni `ERROR` de subject NULL, ni fallo de la guarda
`init_items` del fork.** (El `CRASHLOG-MOD-INIT modulos=46` es el log de
módulos del port, y los `GLib-GObject-*` y el `CAT: no se pudo abrir /dev/ttyS2`
del final son ruido conocido del banco: sin D-Bus y sin radio.)

### 5.6 Suite nativa, en detalle

`tests/native/run_native_tests.sh` (corre también dentro de la matriz, que
sólo imprime `ok`): **20 grupos, 51.895 comprobaciones, 0 fallos**, exit 0.

### 5.7 Crudos de esta corrida

En este mismo directorio, tal cual salieron:

| Archivo | Qué es |
|---|---|
| `matrix.log` | salida completa de `check-matrix.sh` (§5.4) |
| `native-tests.log` | salida completa de `tests/native/run_native_tests.sh` |
| `banco-captura.log` | salida de `tools/captura-banco.sh` (§5.5) |
| `banco-home.png` | la captura de la home del banco (140.888 B) |
| `banco-out.txt` | `out-banco.txt` del chroot: el arranque completo de la GUI en ARM |

---

## 6. Pendientes / riesgos

1. **Pareja BASE/lib (§4)** — el único riesgo real del merge: no flashear este
   `main` sobre una radio con BASE 1.0.6 + lib v0.1.0 sin validar
   espectro/waterfall/S-meter, o mejor: subir la pareja (control v0.1.1 + BASE
   1.0.7) y validar.
2. `#166`-style: el observer nuevo `x6200_control_fft_zoom_cw_set` entra por
   `subject_add_observer_and_call`, igual que los demás (toma `radio_lock`), así
   que sigue la regla del fork para no tocar la lib desde el hilo de LVGL.
3. Validación en hardware del CW zoom (botón de `page_cw_decoder_1`, `1<<n`)
   queda para la radio: el banco no tiene RF.
