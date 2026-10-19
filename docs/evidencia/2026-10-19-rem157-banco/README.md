# Evidencia #157 — `state.pwr` real y mapeo de modos exacto (control remoto)

Fecha: 2026-10-19. Rama: `factory/remote-state-157`. Commit de código: `9309774b`
(base `8b4d44ef`, trunk tip). Binario ARM del banco: `md5 98f18ff4d6ae` (13.803.904 B).

Dos defectos de la **lectura del estado** del socket:

1. **`state.pwr` congelado en 50**: `state_line()` reportaba `cfg_sm.p_pwr.get()`, y
   `Param::get()` devuelve el `value_` en memoria (5, fijado en el constructor); el valor
   real está en `cfg_sm.p_pwr.getf()` → `subject_get_float(cfg.pwr.val)`. El camino de
   comandos ya estaba bien: `apply_cmd` de `CMD_PWR` hace `p_pwr.set(val/10.0f)`, que va por
   el overload float (`setf_`) al subject `cfg.pwr`.
2. **`{"cmd":"mode","val":"NFM"}` seleccionaba SAM**: `mode_names[]` tenía 8 entradas con
   `"NFM"` en el índice 7, pero el enum es `x6200_mode_sam = 7` / `x6200_mode_nfm = 8`
   (`aether_radio/x6200_control/low/control.h:88-99`). Además el bounds check
   (`mode <= x6100_mode_nfm`) leía `mode_names[8]` **fuera del array** en NFM
   (`src/remote_server.cpp:623`).

## Fix

`src/remote_server.cpp`:

- `mode_names[]` pasa a las 10 entradas del enum, en orden:
  `LSB, LSB-D, USB, USB-D, CW, CW-R, AM, SAM, NFM, WFM`.
- El bounds del state line usa el tamaño de la tabla (`MODE_NAMES_N`), nunca el enum.
- `state_line()` lee la potencia con `cfg_sm.p_pwr.getf()`.

`tools/ui_harness/src/scenario.c`: sólo se actualiza el comentario que citaba la tabla
vieja (LSB..NFM) a la nueva (LSB..WFM).

## TDD nativo (determinista, sin radio)

Suite nueva `tests/native/test_remote.cpp` (+ `stubs_remote/`, `remote_stubs.[c|cpp]`,
`remote_test_main.c`). Incluye el `remote_server.cpp` staged para ver sus funciones
estáticas (`state_line`/`parse_cmd`/`apply_cmd`) y enlaza el **`settings_manager.cpp` y
`subjects.cpp` reales**, así que `Param::get()/getf()` se comportan igual que en la radio.

- `tdd-red.log`: el test contra el `src/remote_server.cpp` de la base (`8b4d44ef`):
  **15 fallos / 118 comprobaciones** y **SIGSEGV** al leer `mode_names[9]` (la lectura fuera
  de rango del modo WFM). Es el RED real de los dos defectos.
- `tdd-green.log`: el mismo test con el fix: **118 comprobaciones, 0 fallos**, exit 0.

Casos que cubre:

1. `pwr`: varios valores por `parse_cmd`+`apply_cmd`; el subject `cfg.pwr` y el `get`
   reportan el valor aplicado (1, 5, 10, 50, 100); `0`/`101` siguen rechazados.
2. `mode`: ida y vuelta de los **10** modos por `parse_cmd`+`apply_cmd`+`state_line`
   (NFM→8, SAM→7, sin off-by-one).
3. Bounds: `state_line` mapea 0..9 y devuelve `"?"` para 10 y −1 (nunca lee fuera de la
   tabla).
4. `mode` con un nombre desconocido sigue dando `"Invalid mode"`.

## Gates (`gates.log`)

| Gate | Resultado |
|---|---|
| Suite nativa completa + `-fanalyzer` | exit 0 (`native-tests.log`; bloque nuevo: 118/0) |
| Estáticos de la app (10) + `bash -n` | 10/10 ok (`check-matrix.log`) |
| Determinismo del arnés de UI | **PASS** (PNG idénticos por fixture; `determinism.log`) |
| A/B `AB-REM157` (`8b4d44ef` vs `9309774b`) | **PASS**, 89 escenarios **pixel-idénticos**, 913 contratos (`ab-rem157.log`) |
| `contracts` | PASS — 913 checks, 0 fallos (`contracts.log`) |
| `audit` | 11 issues conocidos, **0 reproducidos**; 9810 objetos, 0 hallazgos estructurales (`audit.log`) |
| `themes` | PASS (WCAG 2.1 AA sobre las paletas servidas; `themes.log`) |
| `pytest` (unit) | **240 passed** (`pytest.log`) |
| `pytest -m e2e` | **13 passed** (`pytest-e2e.log`) |
| `ruff check` / `ruff format --check` / `ty check` | todo OK (`ruff-format-ty.log`) |
| Cross-compile ARM (toolchain buildroot) | `100% Built target x6200_gui`, `remote_server.cpp.o` incluido (`arm-cross-build.log`); binario `md5 98f18ff4d6ae` |

**Pixel-idéntico**: el A/B es PASS con capturas idénticas porque el arnés de UI no compila
`remote_server.cpp`; el cambio del arnés es un comentario. El gate visual del #157 no aplica
(el defecto es del protocolo del socket, no de píxeles).

El A/B se corrió con el **volumen de build privado** `x6200-build-output-rem157` y un
**chroot privado** `/home/pablo/desa/x6200/.vrem157`, para no pisar el volumen compartido
`x6200-build-output` ni el chroot `.vfix3` (estaban en uso por otras corridas). Detalle
importante del rebuild: `compila-gui.sh` borra los `.o` pero el rsync de buildroot (`-au`)
**no** repuso mi `remote_server.cpp` (el destino era más nuevo) y el primer binario salió con
el código viejo; el build válido se hizo con `make x6200-gui-dirclean && make x6200-gui-rebuild`
(verificado: la copia del build tiene la tabla nueva y `p_pwr.getf`).

## Banco QEMU (sin radio)

`banco-get.png`: la GUI ARM real (`98f18ff4d6ae`) en QEMU (cortex-a7), con el modo en NFM.
`banco-shot.log`: salida de `tools/captura-banco.sh` (binario verificado por el **gate #86**:
`OK: ... es el firmware del port`, 3/3 marcas; captura útil, sin `crash.txt`).

`banco-transcript.txt` — probado **por socket** con el cliente `tools/remote/x6200-remote`:

- **`pwr`**: en CB C (27 MHz) el tope de potencia del #155 recorta a 4.0 W, así que
  `pwr 50`/`pwr 100` → el `get` reporta **`pwr=40`** (¡la potencia *real*, no el pedido!);
  `pwr 1/10/33/7` se reflejan tal cual. En 40m SSB, `pwr 100` → **`pwr=100`**, `pwr 50` → 50.
  Antes el `get` decía 50 siempre, sin importar el valor ni la banda.
- **`mode`**: los 10 modos por socket ida y vuelta exacta (LSB, LSB-D, USB, USB-D, CW, CW-R,
  AM, SAM, NFM, WFM). Antes `SAM`/`WFM` daban `Invalid mode` y `NFM` seleccionaba SAM.
- **Rechazos**: `mode BOGUS` → `Invalid mode`; `pwr 0`/`pwr 101` → `Invalid value`.

**No medible sin radio**: el banco no tiene *flow* ni MCU, así que no se puede confirmar que
la potencia realmente salga por RF ni que NFM module de verdad. Eso es `requires-radio`.

## requires-radio (pendiente, no ejecutado)

Con el binario del commit `9309774b` desplegado en la radio (192.168.0.236) y una carga
ficticia/antena:

1. `{"cmd":"pwr","val":100}` → `{"cmd":"get"}` debe reportar el setpoint que la radio
   realmente aplica en la banda activa (en CB, el tope del #155; fuera de CB, 100). Confirmar
   con wattímetro que la potencia de TX sigue al setpoint (y que en CB no pasa de 4 W).
2. `{"cmd":"mode","val":"NFM"}` → `{"cmd":"get"}` debe decir `"mode":"NFM"` y la radio debe
   quedar **realmente en NFM** (verificar en el panel: filtro 0..3500, y audio de FM).
3. `{"cmd":"mode","val":"SAM"}` → la radio debe quedar en SAM (no en NFM).
4. Repetir la ida y vuelta de los 10 modos y confirmar en el panel.

## Archivos

| Archivo | Qué es |
|---|---|
| `tdd-red.log` | test nuevo contra el `remote_server.cpp` de la base: 15 fallos + SIGSEGV |
| `tdd-green.log` | el mismo test con el fix: 118/0 |
| `native-tests.log` | suite nativa completa + `-fanalyzer` |
| `check-matrix.log` | estáticos de la app (10) + `bash -n` |
| `determinism.log` | gate de determinismo del arnés |
| `ab-rem157.log` | resumen del A/B `AB-REM157` (reporte en `tools/ab_testing/reports/AB-REM157`) |
| `contracts.log`, `audit.log`, `themes.log` | contratos, auditoría y contraste de temas |
| `pytest.log`, `pytest-e2e.log` | pytest unit + e2e |
| `ruff-format-ty.log` | ruff check/format + ty |
| `arm-cross-build.log` | cross-compile ARM (dirclean + rebuild) |
| `banco-shot.log`, `banco-get.png` | captura del banco QEMU (gate #86 OK) |
| `banco-transcript.txt` | barrido por socket de `pwr` y `mode` |
