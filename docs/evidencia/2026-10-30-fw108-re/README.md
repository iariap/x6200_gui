# Fase B #376 — RE estático de `UNVERIFIED_VENDOR_PAYLOAD`: uplink SLIP y control BASE/audio-MUX

- **Epic:** [#374](https://github.com/iariap/x6200_gui/issues/374) · **Fase A:** [#375](https://github.com/iariap/x6200_gui/issues/375) → `ARTIFACT_UNAVAILABLE` (STOP) · **Fase B:** [#376](https://github.com/iariap/x6200_gui/issues/376)
- **Autorización acotada del coordinador (2026-10-30, #376 y #374):** sólo RE estático offline y read-only del payload recibido, etiquetado **`UNVERIFIED_VENDOR_PAYLOAD`**. Esta autorización **no levanta el STOP de integridad**, no acredita «1.0.8 oficial», no habilita producción ni #377/#378.
- **Base:** `main` real `6850d671d430a1693dbcaf593579a3d98b037995` (verificado con `git fetch fork main`; el `origin` del clone `x6200_qa` tiene fetchspec roto).
- **Worktree/branch:** `/home/pablo/desa/x6200/x6200-fw108-re` / `audit/x6200-fw108-re`.
- **Naturaleza del PR:** **docs-only**. No se modifica `src/`, no se toca la ROM, no se ejecuta ningún binario del fabricante, no se accede a radio/I²C/UART/MUX/PTT/ATU, no se descarga firmware.

## Entradas (hashes verificados en esta sesión)

| Artefacto | Ruta (scratch, fuera de Git) | SHA256 |
|---|---|---|
| GUI 1.0.8 (payload **no autenticado**) | `fw108-scratch/ext108/fs108/x6200_ui_v100` | `26623266bfcd3ed4d2e08b03b9f023cef809579f5ed497899670fc01f66e9b2b` |
| GUI 1.0.6 (referencia auténtica) | `fw108-scratch/ref106/fs106/x6200_ui_v100` | `b76331c5aa859ae98dc1819b1079a3e40489e9986c9656e0dc2c553aeb28871b` |
| `.xgf` 1.0.8 | `fw108-scratch/ext108/fs108/x6200_mcu_20260820141208.xgf` | `9a19f97a5ad9e141a3663d222f984073bf0b18b38d4662604372a0036f9daa23` |
| `.xgf` 1.0.6 | `fw108-scratch/ref106/fs106/x6200_mcu_20250111134652.xgf` | `dca90387b70d9550448006310a69ea65625bec87f402c10df789deeb13ff12f3` |
| rootfs ext4 payload 1.0.8 (`parts/1.img`) | `fw108-scratch/ext108/parts/1.img` | `3c475b16d48acb23f727cbc3d6d3b62f103bbc26bd3ce7f2e8f1b3be27ff4764` |

Contexto de integridad (Fase A, PR #379): el ZIP 1.0.8 es íntegro pero su `sdcard.img` **no coincide** con el `Image File SHA256` que declara su release note ⇒ **la autenticidad del payload no está acreditada**. Todo lo de esta fase es análisis del *contenido recibido*, no del «1.0.8 oficial».

## Herramientas usadas (estático, sin ejecución de binarios vendor)

- `arm-linux-readelf` / `arm-linux-nm` / `arm-linux-objdump` del toolchain Buildroot (`rom-build/clone-buildroot/build/host/bin/`).
- Escáner de xrefs propio sobre **capstone 5.0.7** (literal pools, `movw/movt`, `add pc`) — `fw108-scratch/re376/xrefs.py`.
- **REA 6.7.0** (`tools/rea.sh`, `doctor`/`capabilities` OK) con provider **Ghidra 12.1.4** + JDK21: `decompile --provider ghidra` para pseudocódigo de `BaseComm::start`, `MainWidget::slot_slaveReportMessage`, `BaseCommIIC::start`, `BaseCommIIC::updateWorkParams` y `BaseCommIIC::parseParamItem` (evidencia en scratch, hashes abajo).
- Python 3 con `struct`/`hashlib` para tablas CRC y fixtures; `gh`/`git` para entrega.

No se usó ejecución dinámica del binario, ni `ldd`/`strace` sobre el mismo, ni emulación.

## Resultados principales (detalle y offsets en los docs enlazados)

### 1. Uplink serie BASE→Linux ([`slip-wire-contract.md`](slip-wire-contract.md))

- **1.0.8 añade SLIP real** (funciones nuevas `slip_encode`@`0xf7424` / `slip_decode`@`0xf7494`, archivo `slip.c`): trama = `escape(payload)` + **un** `END 0xC0`; escapes RFC1055 `C0→DB DC`, `DB→DB DD`; sin `END` inicial emitido por el encoder. El decoder tolera `END` consecutivos con salida vacía, descarta `DB` huérfano de forma laxa (`DB xx → xx`), devuelve `-1` sólo por overflow de salida y **no lleva cota de entrada**.
- El **único** caller es `BaseComm::start`@`0xde4bc` (`bl slip_decode`@`0xde9a0`); `slip_encode` **no tiene callers** en la GUI 1.0.8 (código muerto en la APP).
- Tras decodificar exige **exactamente 552 bytes** (`cmp r0,#0x228`, `0xde9a4`), copia el payload a `BaseComm+0x9` y valida CRC32 con **la misma tabla de 256 entradas** y **el mismo algoritmo por palabra u32 LE** que el contrato legacy (`refs/X6200Control/src/low/flow.c`, `xtable sha256 0053f121…`); el CRC va en `payload[548..552)` = `0x224` y se calcula sobre `payload[0..548)`.
- **Layout decodificado = `x6200_flow_t` legacy v0.1.1** (offset por offset: `magic 0x000`, `samples[512] uint8 0x004`, `flags 0x204`, `dbm/tx_power/vswr/alc_level 0x208..0x20b`, `vext/vbat/batcap/reserved_2 0x20c..0x20f`, `atu_params 0x210`, `reserved_3[3] 0x214`, `hkey 0x220`, `crc 0x224`; `sizeof=552`).
- **Matiz de integridad del contrato:** la APP 1.0.8 **no valida el magic** (sólo lo imprime en el log de fallo de CRC); el valor `0xAA5555AA` es **hipótesis** heredada del payload legacy y de la GUI 1.0.6. Por eso el gate exploratorio es **`PARTIAL`**, no `PROVEN`.
- **1.0.6 no tiene SLIP**: sincroniza buscando el magic `0xAA5555AA` en el buffer y usa una trama de **2088 bytes** (`samples[512]` eran `float` = 2048 B), con el mismo CRC (tabla idéntica, CRC en offset `2084`, sobre `2080` B). El cambio 1.0.6→1.0.8 coincide con el commit `0ee1ae3` «Fix flow on 1.0.7 base» de `X6200Control` (`float samples[512]` → `uint8_t samples[512]`) y con la nota de 1.0.7 «baseband uplink data to use SLIP encoding».

### 2. Audio/MUX / BASE ([`audio-mux-diff.md`](audio-mux-diff.md))

- Downlink = **I²C** (`/dev/i2c-0`, `open(O_RDWR)`), no serie: `BaseCommIIC::iicWrite`/`iicRead`@`0xe2840`/`0xe2740` usan `ioctl(fd, I2C_RDWR=0x707)` con dirección de 1 o 2 bytes (big-endian) según `AddressMode`.
- La APP escribe un **bloque de workparams de 216 B (`0xd8`)** en la dirección I²C `0x0000`; el **word 12 (bytes `0x30..0x34`)** es el registro de flags: la APP 1.0.8 escribe/borra bits `0x2`, `0x8`, `0x10`, `0x20`, `0x1000`, `0x2000`, `0x4000`, `0x8000`, `0x10000`, **`0x40000`** (dos ramas), `0x100000`, `0x200000`, `0x400000`, `0x800000` (`BaseCommIIC::parseParamItem`, offsets `pack+0x30`). `updateWorkParams` fija de forma explícita `wparam/sple→bit0x2` y `wparam/atue→bit0x1000`.
- **Cruce con nuestra RE previa ([HALLAZGOS #77](https://github.com/iariap/x6200_gui/blob/main/docs/HALLAZGOS.md)):** nuestro port llama `reg12` a la palabra `0x0030` con `iptt=0x40000` y `voice_rec=0x8` en read-modify-write; la APP vendor 1.0.8 escribe **la misma palabra** y **ambos bits** (aunque el mapeo param→bit exacto queda `PARTIAL`). ⇒ `mux_contract_signal=FOUND_UNVERIFIED`.
- **No hay ACK/readback ni restore:** `iicWrite` no relee ni valida el bloque; `BaseCommIIC::start` sólo comprueba el retorno de `ioctl` y, como «supervisor», **reescribe el bloque completo cada ~1 s** mientras la APP vive (no es readback físico). Al salir sólo cierra el fd (`BaseCommIIC::stop` = flag); no hay restore de `voice_rec`/PTT/ATU tras cierre o crash ni watchdog/heartbeat.
- **1.0.6 vs 1.0.8: el camino de control BASE no cambió.** `BaseCommIIC::{start,iicRead,iicWrite,parseParamItem,updateWorkParams,updateRxFilters,setCalibrateData,cwDecodeWPM}` tienen **el mismo tamaño** y **secuencia de instrucciones idéntica normalizada** en ambas GUIs (sólo difieren literales de relocación/GOT). Lo que cambió es el uplink (SLIP) y el panel de audio (`PanelCall` nuevo, `SpectrogramWidget` OpenGL; se retiran `XComm`/`XScopeGL`).
- **`.xgf` opaco:** 524288 B, entropía 7.47–7.75 bits/byte, 256 símbolos, sin cabecera reconocible; no se descifra ⇒ **`MCU_OPAQUE`**.
- ⇒ **`MUX_SAFETY_STILL_BLOCKED`** (siguen faltando U1 verdad de estado, U2 arbitraje/interlocks, U3 restore independiente ≤250 ms, U4 prueba física; #362 no se desbloquea por analogía).

### 3. Fixtures sintéticos ([`fixtures-method.md`](fixtures-method.md))

Dos fixtures (más casos límite) **inventados**, con hex y SHA256, generados con la transcripción del código 1.0.8 y la tabla CRC extraída del binario. **Nunca se presentan como capturas de radio.**

### 4. Gate de salida

Ver [`gates.json`](gates.json): `payload_authenticity=UNVERIFIED`, `slip_payload_gate=SLIP_FORMAT_UNVERIFIED`, `wire_contract_for_received_payload=PARTIAL`, `mux_contract_signal=FOUND_UNVERIFIED`, `production_ready=false`, `compat_377_allowed=false`.

## UNKNOWNs y límites (explicitados, no maquillados)

1. **Autenticidad del payload:** no acreditada (STOP de Fase A vigente). Todo el análisis describe el *payload recibido en custodia*, no el 1.0.8 oficial.
2. **Magic en el cable:** la APP 1.0.8 no lo valida; `0xAA5555AA` es hipótesis (legacy v0.1.1 + GUI 1.0.6). No hay captura real ni firmware BASE decodificado que lo confirme.
3. **Bytes reales de trama:** no se capturó tráfico (prohibido); el contrato se reconstruye del código de la APP + fixtures sintéticos.
4. **Mapeo param→bit de `parseParamItem`:** `PARTIAL`; resueltos `atue→0x1000` y `sple→0x2`; el resto (incl. qué parámetro enciende `0x8`/`0x40000`) queda `UNKNOWN`.
5. **Comportamiento runtime del parser de la APP:** el manejo de buffer (`QByteArray::remove(0, index)` deja el `END` sin consumir y la resincronización depende de 6 lecturas no-bloqueantes vacías ≈30 ms) se documenta como **hallazgo de código**; su efecto práctico no se validó dinámicamente (prohibido ejecutar el binario).
6. **MCU/BASE:** `.xgf` opaco; no se afirma nada sobre el firmware del MCU salvo que no está decodificado.
7. **Build exacto:** `X6200Control` v0.1.1 es referencia de contrato legacy; no se afirma que sea exactamente la librería enlazada en la ROM del equipo.

## Contenido de este directorio

| Archivo | Contenido |
|---|---|
| `README.md` | este resumen, entradas/hashes, resultados, UNKNOWNs |
| `slip-wire-contract.md` | contrato SLIP/uplink 1.0.8 con offsets, pseudocódigo, comparación 1.0.6 y veredicto de equivalencia |
| `audio-mux-diff.md` | I²C/BASE, workparams, reg12/flags, ACK/readback/restore, matriz 1.0.6 vs 1.0.8 |
| `symbols-xrefs.csv` | binario/versión/dirección/tamaño/símbolo/evidencia y xrefs clave |
| `fixtures-method.md` | método reproducible y fixtures sintéticos con hex/SHA256 |
| `gates.json` | gate de salida de la Fase B |
| `commands.log` | comandos reales ejecutados + exit codes (sanitizado, sin binarios propietarios) |

## Reproducibilidad

Los binarios y las decompilaciones completas viven **fuera de Git** en `/home/pablo/desa/x6200/fw108-scratch/re376/` (no se publican binarios propietarios ni volcados). Los comandos claves y sus exit codes están en [`commands.log`](commands.log); los hashes de las herramientas y de los artefactos intermedios están en los docs. Verificación de entrega: `git diff --check`, docs-only y `check-matrix.sh` (ver `commands.log`).

## Declaración final de estado

Esta fase produce **evidencia y un PR documental**. No es un GO de producción, no autoriza instalación/despliegue, no desbloquea [#377](https://github.com/iariap/x6200_gui/issues/377) (sigue bloqueado) ni inicia [#378](https://github.com/iariap/x6200_gui/issues/378), no cambia `docs/HALLAZGOS.md` por hipótesis y no altera el gate de [#362](https://github.com/iariap/x6200_gui/issues/362).
