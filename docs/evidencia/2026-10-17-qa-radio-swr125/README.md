# QA en la radio — parábola de ROE del #130 (cierre requires-radio del #125)

**Fecha**: 2026-10-17 (hora de la radio) · **Rama**: `lu2dsx/x6200-rom` · **Commit del fix**: `63dbf7a4`
(merge del PR #130, "SWR Scan: la parabola de ROE siempre abre hacia arriba")
**Radio**: 192.168.0.236, ROM `c640645d810c`, antena real de 40 m (HF-008), tema del owner `Redshift`
**Binario**: build limpio desde el tip → `md5 b1ba67520079` (13.753.408 B), **idéntico al que ya tenía la radio**
**Gate #86**: `tools/verifica-firmware-port.py` → `OK … ELF32 ARM, 13753408 B, 3/3 marcas` (exit 0)

## Qué se validó

El fix del #130 (`src/roe_curve.c`: tras el ajuste libre, si `a<0` se espeja al valle con la misma
columna del vértice y la base en el mínimo medido) sobre la radio real. El criterio del owner
(comentario de re-test del #125): **la curva dibujada tiene que ser una U** — base en el cuarto
inferior del chart (`y≥316`), en columna interior, y **ambos bordes subiendo** (puntas hacia +∞).

## Método

1. Clon propio `x6200_qa` al tip del trunk; `RECONSTRUIR=si tools/compila-gui.sh` (build **limpio**).
2. `tools/prueba-gui-en-radio.sh` → el gate #86 corre solo y valida el binario antes de subirlo.
   La radio ya corría `b1ba67520079` (el mismo md5), así que el deploy fue idempotente; la GUI se
   relanzó (el equipo **no** se reinició: el uptime siguió subiendo).
3. Radio en **CW**, dial al centro de la resonancia, **SWRScan** abierto, **1 pasada (Quick)**,
   **TX a 0.1 W** (lo fuerza `radio_start_swrscan()`: `SWRSCAN_TX_POWER_DW=1`). **ATU**: la radio la
   tenía **habilitada** (`atu=1`, indicador `ATU1` encendido) — igual que en la captura del owner
   `26-roe-multipasada-final.png`; el control remoto **no expone** un comando de ATU, así que **no se
   tocó** (regla del skill: no tocar los ajustes de la radio). Sin XPA125B.
4. Captura read-only con `tools/captura-radio.sh` (framebuffer `/dev/fb0`, rotado 270°).
5. Aserción `tests/swrscan_curve_evidence.py` (banda del chart `y=69..399`).

## Resultado — la curva en la radio ES una U ✓

Medición de píxeles sobre las capturas, extrayendo el trazo de la curva por su color real en el tema
del owner (`RGB(255,179,160)`; ver `medida-curva.txt`):

| Captura | dial / span / escala | valle (y) | col. valle | bordes izq / der (y) | suben (px) | cuarto inferior | interior |
|---|---|---|---|---|---|---|---|
| `01-swrscan-50k-dial7000-U.png` | 7.000 / 50 kHz / Linear | **398** | 255 | 304 / 230 | **+94 / +168** | ✓ | ✓ |
| `02-swrscan-50k-dial7000-U-2da.png` (2ª pasada) | 7.000 / 50 kHz / Linear | **398** | 232 | 318 / 224 | **+80 / +174** | ✓ | ✓ |
| `03-swrscan-200k-log-dial7016-U.png` | 7.016,334 / 200 kHz / Log | **398** | 238 | 258 / 165 | **+140 / +233** | ✓ | ✓ |
| `04-swrscan-500k-log-dial7016-U.png` | 7.016,334 / 500 kHz / Log | **398** | 273 | 227 / 167 | **+171 / +231** | ✓ | ✓ |
| `06-swrscan-500k-dial7030-ref.png` | 7.030 / 500 kHz / Linear | **398** | 224 | 341 / 283 | +57 / +115 | ✓ | ✓ |
| `05-swrscan-50k-dial7020-fuera-resonancia.png` | 7.020 / 50 kHz / Linear | 398 | **10 (borde)** | 398 / 388 | 0 / +10 | ✓ | ✗ |

**Lectura**:

- Con el dial **en la resonancia** (~7.000 MHz en esta antena hoy) la curva es una **U limpia**: base
  en el piso (`y=398` → ROE ≈ 1.0), valle **interior** (x≈230–280) y **las dos puntas suben** (94–233 px).
  Reproducido en la 2ª pasada y también con span 200 kHz/Log (las condiciones exactas de la captura
  del domo del owner).
- Con el dial **fuera de la resonancia** (7.020 / 7.040), los datos medidos son monótonos: el mínimo
  cae en el borde izquierdo y la curva dibujada es una rama de parábola, no una U con valle interior.
  **No es una inversión** (la curva abre hacia arriba): es que el rango barrido no contiene el valle.
  El fix sólo espeja el ajuste cuando `a<0`; un ajuste que ya abre hacia arriba se dibuja tal cual
  (`src/roe_curve.c:166-185`).

## La aserción del #125 NO es ejecutable en la radio tal como está documentada ✗

El paso 3 del re-test del #125 dice:
`python3 tests/swrscan_curve_evidence.py /tmp/swrscan-u.png` (1 argumento). **Ese comando no corre**:
el script pide **2** capturas (`len(argv) != 3` → imprime el docstring y sale con **exit 2**).

Con 2 argumentos (el fixture del arnés + la captura real de la radio) la comprobación de la radio
**falla igual**: `FAIL valle real del owner -> valle: no encontre la curva dibujada` (exit 1). La causa
está medida: `curve_mask()` exige `min(r,g,b) >= 180`, y la curva del tema del owner (`Redshift`) es
`RGB(255,179,160)` → `min = 160 < 180`, así que **no la detecta**. El fixture del arnés sí pasa porque
su curva es `RGB(248,238,255)` (`min=238`). El propio docstring del script dice **"No OCR, no radio."**
Salida cruda en `asercion-script.txt`.

- **RED/GREEN del arnés (trunk), que sí es lo que el script mide**: `03-harness-domo-valle.png` +
  `04-harness-valle-real.png` → **PASS** (el domo del owner → valle: base `y=398` en `x=284`, bordes
  suben 207/277 px; el valle real queda valle: 20/12 px). El fix está verificado en el arnés.
- **En la radio**, la medición independiente de arriba (con el color real de la curva) confirma la U.
  El script, tal cual, no la puede leer.

→ Hallazgo cargado como **issue nuevo** (no se arregla: el entregable es el issue).

## Hallazgos laterales (no del fix, ya reportados)

- **`Error: timeout de RF`**: en **todas** las pasadas la sesión termina en `Error: timeout de RF` con
  `n≈43-45/51` — **nunca `Completed`**. El watchdog de la pasada (`SWRSCAN_PASS_TIMEOUT_MS = 1785+500 =
  2285 ms` para 51 puntos = 44,8 ms/punto) se dispara antes de terminar porque el tiempo real por punto
  en hardware es mayor que el `SWRSCAN_DWELL_MS=35` asumido (medido: TX entre ~2,3 y ~2,6 s). Ya estaba
  anotado en el #119 y en el registro del otro QA; **sigue abierto**.
- **Crash SIGSEGV (1 vez, no reproducido)**: durante la corrida apareció
  `CRASHLOG sig=11 fallo=0xf363 pc=0xb6186ae8 lr=0xb618d4f0 sp=0xbed77ba0 base=0x47ea8c` justo después
  de `on_vfo_mode_change: mode=2` + carga de `filter_low/high`; la GUI se relanzó sola (pid 2728→3931).
  **Ya está reportado en el issue #131** (mismo evento). Un barrido dirigido de las 23 apps
  (abrir 6 s / cerrar / comparar pid) **no lo reprodujo**: pid 3931 estable, CRASHLOG 0. Nota: el log
  vivo post-relanzamiento arranca limpio, por eso `verifica-radio` dice "sin senales de crash" — el
  CRASHLOG está en el `app_log` guardado (`x6200_log_2026-10-17_22-38-46.txt`).

## Barrido de apps (no regresión)

`PERMANENCIA=14 tools/prueba-apps-radio.sh` → **23/23 ok, 0 caídas** (incluye **SWRScan**, que ahora
**sí** se abre por el control remoto: el #78 quedó resuelto). Detalle en `apps-sweep.log`.

## Estado final de la radio (restaurada)

- dial `7.030.000`, modo `LSB`, banda `40m CW`, `pwr` 5.0 W, app cerrada, **RX**.
- `swrscan_span=500000`, `swrscan_linear=1` (restaurados a como estaban).
- `cw_tune=cw_decoder=key_train=0`; tema `Redshift` (no se tocó); **ATU sin tocar** (`atu=1`).
- md5 desplegado `b1ba67520079`; GUI pid 3931; equipo sin reinicio (uptime continuo). Ver `estados.txt`.

## Archivos

| Archivo | Qué es |
|---|---|
| `01..04-*.png` | capturas de la radio con la curva en U (números en la tabla) |
| `05-*.png` | captura con el dial fuera de la resonancia (rama, no U interior) |
| `06-*.png` | captura de referencia (span 500 kHz / dial 7.030) |
| `07-restaurado.png` | estado final restaurado |
| `medida-curva.txt` | medición de píxeles de cada captura (valle, bordes, criterios) |
| `asercion-script.txt` | salida cruda del script del #125 (1 arg, 2 args arnés, 2 args con la radio) |
| `deploy.log` | build/deploy por WiFi con el gate #86 |
| `verifica-radio.txt` | `tools/verifica-radio.sh` (TODO OK) |
| `apps-sweep.log` | barrido de las 23 apps |
| `md5.txt` | commit, hash de fuentes, md5 del build, gate #86, md5 desplegado |
| `estados.txt` | estado del radio, params y el CRASHLOG del #131 |
| `rx-checks.txt` | chequeos de RX: volumen 0→30→0, dígitos del dial (7.010/7.020/7.030), toggle de NB (#114, la radio sobrevivió: uptime continuo, pid 3931) |
