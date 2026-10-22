# #217 — CW keyboard idle render

**Issue:** [iariap/x6200_gui#217](https://github.com/iariap/x6200_gui/issues/217)
(hijo 1/6 de la épica [#216](https://github.com/iariap/x6200_gui/issues/216)).
**Rama:** `factory/next-217` · **base:** `b798f13b57ae4c6e0ec5a935a9dba2d4b2ca16bd`
(tip de `origin/lu2dsx/x6200-rom`, con el hardening #211 ya integrado) ·
**HEAD de código:** `438df0dfbd2b9115989e1d6aead375c0e575ad5e` (la matriz de
§5 corre sobre este commit; esta evidencia es el commit de `docs/` que le sigue).

Objetivo: que `src/dialog_cw_kbd.c` no repinte la UI con el diálogo en reposo,
sin alterar el keyer ni el decodificador de CW (condición explícita del issue).

## 1. Diff por archivo

| Archivo | Qué |
|---|---|
| `src/dialog_cw_kbd.c` | +71/−13: `update_view()` compara cada texto/estilo contra el último valor efectivamente dibujado y sólo escribe el label cuando cambia; `drawn_reset()` en `construct_cb()` y `destruct_cb()`. Único archivo de producción tocado. |
| `tests/native/test_cw_kbd.c` | test nuevo (297 líneas): 52 comprobaciones. |
| `tests/native/cwkbd_stubs.c` / `.h` | stubs del diálogo + **contadores de invalidaciones**. |
| `tests/native/cwkbd_test_main.c` | runner del binario nuevo. |
| `tests/native/stubs_cwkbd/**` | 13 headers stub (LVGL + deps del firmware) para estagear la fuente real. |
| `tests/native/run_native_tests.sh` | stage `cwkbd` en la suite nativa (39 líneas). |

Ningún otro archivo de producción (`src/`) cambia:

```
$ git diff --name-only b798f13b HEAD -- src/
src/dialog_cw_kbd.c
```

## 2. Método de medición (HOST)

El diálogo CW **no** está compilado en el `ui_harness` (es uno de los diálogos
inertes de `tools/ui_harness/mocks/mock_main_screen.c`), así que la medición se
hace con la **fuente real** `src/dialog_cw_kbd.c` estageada contra stubs
(`tests/native/stubs_cwkbd/`, el patrón de `cw_encoder`/`swrscan`), y el
instrumento son los contadores de `cwkbd_stubs.c`:

- `label_writes` = llamadas a `lv_label_set_text` / `lv_label_set_text_fmt`.
- `style_writes` = llamadas a `lv_obj_set_style_*`.
- Ambas son, por definición, **una invalidación LVGL del objeto**: `update_view()`
  corre en el timer de 100 ms y es lo único que escribe durante los ticks.

El estímulo es **100 ticks idénticos del propio timer** (10 s de tiempo de
diálogo) sin cambio de modo, tecla, cola ni tema — exactamente el escenario del
issue. El mismo binario de test se compiló contra la fuente **base** y contra la
**corregida** (mismos stubs), y las cadenas renderizadas se pinnean byte a byte.

## 3. Antes / después

100 ticks idénticos, diálogo en reposo:

| | label_writes | style_writes | invalidaciones/tick | por segundo |
|---|---|---|---|---|
| **base** `b798f13b` | 400 | 200 | 6.0 | 60 |
| **corregido** `438df0df` | **0** | **0** | **0.0** | **0** |

Salida real (recortada):

```
$ ./cwkbd_tests            # fuente BASE
  [medicion] reposo, 100 ticks identicos: label_writes=400 style_writes=200
FALLO ...:85: cwkbd_stub_label_writes() == 0
FALLO ...:86: cwkbd_stub_style_writes() == 0
...
52 comprobaciones, 18 fallos
exit=1

$ ./cwkbd_tests            # fuente CORREGIDA
  [medicion] reposo, 100 ticks identicos: label_writes=0 style_writes=0
  [medicion] cambio de tema: label_writes=3 style_writes=2
52 comprobaciones, 0 fallos
exit=0
```

Y con estímulo real, la corrección también **reduce** los repintados (no deja de
reaccionar):

| cambio | label_writes por tick (base) | corregido |
|---|---|---|
| WPM | 4 | 1 (sólo el header) |
| estado NOT CW / ON AIR | 4 | 1 (sólo el chip) |
| cambio de tema | 4 | 3 una vez, después 0 |

Logs completos: `cwkbd-base.txt`, `cwkbd-fixed.txt`; CSV con hashes de fuente y
binario: `mediciones.csv`.

**Qué no se mide (y por qué).** No se reporta área redibujada en px ni capturas
PNG+árbol LVGL: el `ui_harness` no aloja este diálogo y alojarlo obligaría a
editar el mock genérico de diálogos (`mocks/mock_main_screen.c`) y a compilar
`cw_encoder`/`keyboard`/`textarea_window`, fuera del set de archivos permitido
para este hijo (`src/dialog_cw_kbd.c` + pruebas). La parte del "0 cambios
visuales" que sí se puede pinnear —el texto exacto de `log_lbl` en estados
READY/con cola/con historial, y los de `head`/`head_right`/`state_lbl`— se fija
en el test y es idéntica en base y en la corrección. La captura real por QEMU es
del pipeline de #222 (`PENDING`).

## 4. Constancia de que el keyer y el decodificador no cambian

- El único cambio de producción es la **capa de aplicación LVGL** en
  `update_view()`: no se toca el timer (sigue en 100 ms), ni la lógica de cola,
  ni `cw_encoder_*`, ni el timing, ni el estado TX, ni la entrada por teclado, ni
  la cancelación, ni los mensajes remotos.
- `tests/native/test_cw_kbd.c::test_cw_kbd_keyer_semantics` (pasa en base **y**
  en la corrección): fuera de CW la línea se rechaza sin encolar ni keyear; en CW
  va en mayúsculas, se descarta `#`, un espacio final y **un** `cw_encoder_send`;
  100 ticks de render **no** vuelven a keyear; `cw_stop` limpia la cola y llama a
  `cw_encoder_stop`; `cw_dec` llega a `cfg.cw_decoder`; el objeto de estado remoto
  sigue reportando los valores del keyer.
- Suites del keyer y del decoder en el árbol corregido: `test_cw_encoder` (keyer,
  #116/#160) **33 comprobaciones, 0 fallos**; `test_cw_decoder` (#51) dentro de la
  suite principal **44738 comprobaciones, 0 fallos** (ver `log-native.txt`).

## 5. Gates (salida real)

| Gate | Resultado | Log |
|---|---|---|
| `sh tests/native/run_native_tests.sh` | **PASS** (exit 0); CW keyboard 52/52 | `log-native.txt` |
| `sh tests/native/run_native_tests.sh --ci-parity` | **PASS** (exit 0, build sin `lvgl/`) | `log-ci-parity.txt` |
| ASan/UBSan del test nuevo (imagen `x6200-uiharness`) | **PASS** (`ASAN_OPTIONS=detect_leaks=1`, exit 0) | `log-cwkbd-asan.txt` |
| `sh tests/check_harness_determinism.sh` | **PASS** | `log-check_harness_determinism.txt` |
| `sh tests/check_harness_natural.sh` | **PASS** | `log-check_harness_natural.txt` |
| `sh tests/check_harness_wakeups.sh` | **PASS** | `log-check_harness_wakeups.txt` |
| `sh tests/check_waterfall_incremental.sh` | **PASS** (195 checks + ASan/UBSan) | `log-check_waterfall_incremental.txt` |
| `python3 tests/audita-pruebas.py` | **OK** | `log-audita-pruebas.txt` |
| `sh tests/run_all_tests.sh --quick` | **19 OK, 0 fallos** | `log-run-all-quick.txt` |
| `scripts/apps/x6200/check-matrix.sh origin/lu2dsx/x6200-rom HEAD <wt>` | **exit 0**, todos los `[check]` ok | `log-check-matrix.txt` |

SKIP / BLOCKED con su razón (nunca PASS):

- **ASan/UBSan en el runner nativo del host:** `== sanitizers: NO disponibles en
  este toolchain (se omite) ==` — falta el runtime (`libasan.so.8`/`libubsan.so.1`
  no linkean en este host). Es condición del toolchain, idéntica para todas las
  etapas (también en la base). El test nuevo **sí** se corrió bajo ASan/UBSan con
  `-fno-sanitize-recover` + `detect_leaks=1` dentro de `x6200-uiharness`, que sí
  trae el runtime → PASS (`log-cwkbd-asan.txt`).
- **Banco QEMU (preflight): BLOCKED.** Falta el volumen `x6200-build-output`, el
  chroot `/…/.vfix3` y `tools/remote/x6200-remote`. La épica prohíbe usar/compartir
  ese volumen (recurso de otras sesiones) y el banco QEMU es pipeline separado
  (#222). Ver el bloque BLOCKED en `log-check-matrix.txt`.
- **`run_all_tests.sh` SKIPs** (registrados, no PASS): `wspr`, `sstv`, `wefax`
  (la imagen `x6200-uiharness` del host no trae `fftw3.h`/`sndfile.h`), `js8`
  (`--quick`) y `webserver` (falta el checkout/volumen). Ninguno toca este cambio.
- **`check_wcag (temas de la imagen)`, `ruff`/`pytest del framework A/B`,
  `harness determinism gate`**: `[SKIP]` de la matriz por paths (no hay cambios en
  `rootfs/**/themes/*.json`, `tools/ab_testing/**/*.py` ni `tools/ui_harness/`).
- **Capturas PNG + árbol LVGL y ratio CW/home en QEMU:** no se hacen acá (el
  diálogo no está en el harness; ver §3) → `PENDING` del pipeline de #222.
- **Controles físicos / energía (watts):** `PENDING_HARDWARE`. No se afirma ahorro
  de batería sin medidor; el número medido son invalidaciones, no vatios.

## 6. Observaciones preexistentes (no introducidas por este cambio)

- `-Wformat-truncation` en `msgs_load()` (`dialog_cw_kbd.c:83,88`, buffer
  `p_label[3][16]`): **5 warnings idénticos en base y en la corrección**
  (`-O1 -Wall -Wextra`). No se toca: es lógica ajena al issue (páginas de mensajes),
  no hay cambio de comportamiento observable y la épica pide no ampliar el diff.
- El repo emite warnings upstream en otras fuentes (p. ej. `remote_server.cpp`);
  la suite nativa no los trata como error y no se sumaron nuevos.

## 7. Pendientes

- PR: lo abre el orquestador (`[P2][CW] Avoid idle UI redraws without changing
  keying`, `Refs #216` + `Closes #217`, contra `lu2dsx/x6200-rom`). **Este hijo no
  pushea ni mergea.**
- QEMU_USER (3 ventanas de 60 s, ratio CW/home ≤2,5×, capturas) → pipeline #222 /
  banco del owner.
- QA físico y medición de energía → `PENDING_HARDWARE`.
