# Control BASE/audio-MUX en GUI 1.0.8 vs 1.0.6 (payload `UNVERIFIED_VENDOR_PAYLOAD`)

- **Binario 1.0.8:** `x6200_ui_v100` SHA256 `26623266bfcd3ed4d2e08b03b9f023cef809579f5ed497899670fc01f66e9b2b` (paquete **no autenticado**).
- **Binario 1.0.6:** `x6200_ui_v100` SHA256 `b76331c5aa859ae98dc1819b1079a3e40489e9986c9656e0dc2c553aeb28871b` (referencia auténtica).
- **Método:** estático (objdump ARM, nm, escáner capstone, REA 6.7.0/Ghidra 12.1.4). **Sin ejecutar binarios, sin I²C/UART/radio, sin `sudo`/mount, sin descargar firmware.**
- Marcas: **[HECHO]** verificado en código/offset; **[HIPÓTESIS]** inferencia; **[UNKNOWN]** no resuelto.
- Referencia de nuestro port para el registro compartido: `docs/HALLAZGOS.md` #77/#78 (reg 12 = palabra `0x0030`, `iptt=0x40000`, `voice_rec=0x8`, sin readback acreditado).

---

## 1. Downlink: no es serie, es I²C [HECHO]

### 1.1 Apertura y dispositivo

- La cadena **`/dev/i2c-0`** está en `.rodata` (1.0.8 vaddr `0x111ffb`; 1.0.6 vaddr `0x10d244`).
- `BaseCommIIC` se construye desde `MainWidget::MainWidget` (`bl BaseCommIIC::BaseCommIIC` en `0x07b290`); el constructor `0x0e56bc` recibe el path como `QString` y `BaseCommIIC::start`@`0x0e5a88` hace `open(path, O_RDWR)` (`open64(..,2)`) guardando el fd en el global `BaseCommIIC::fd` (`0x162e70`) y la dirección en `BaseCommIIC::addr` (`0x162e6c`).
- **No hay `QSerialPort` en la ruta BASE**; el uplink serie es `BaseComm` (`/dev/ttyS1`, ver `slip-wire-contract.md`). Son dos canales distintos.

### 1.2 Primitivas I²C (estáticas) [HECHO]

`BaseCommIIC::iicWrite`@`0x0e2840` (tamaño `0x178`) y `BaseCommIIC::iicRead`@`0x0e2740` (`0x100`), ambas **`static`** (sin `this`; coherente con el `__PRETTY_FUNCTION__` en `.rodata` `0x121fa3`):

```
iicWrite(uint32_t address, AddressMode mode, uint8_t *data, uint32_t len)
iicRead (uint32_t address, AddressMode mode, uint8_t *data, uint32_t len)
```

- `mode == 0` ⇒ dirección de **1 byte** (`uxtb`); `mode != 0` ⇒ dirección de **2 bytes big-endian** (`rev16`). En todas las llamadas observadas de `BaseCommIIC` el modo es **1** (16-bit address).
- Construye un `struct i2c_msg` en stack (`addr`, `flags`, `len`, `buf`) y ejecuta **`ioctl(fd, 0x0707, &msg)`** = `I2C_RDWR`.
- `iicWrite` reserva `addrlen+len` (`malloc`), copia dirección+datos y hace **una sola** transferencia; retorno = resultado de `ioctl` (`-12` si falla `malloc`). **No relee ni compara nada**; no hay ACK/readback a nivel de protocolo más allá del resultado del `ioctl`.
- `iicRead` ejecuta una transferencia de lectura y devuelve el resultado del `ioctl` (`-22` si parámetros inválidos).
- No hay espera de "busy/ready" ni reintento por trama en la primitiva (el reintento está a nivel de `BaseCommIIC::start`, ver §1.3).

### 1.3 Secuencia de `BaseCommIIC::start`@`0x0e5a88` (tamaño `0xb18`) [HECHO, decompilado]

1. **Espera de BASE conectado:** en bucle `iicRead(0x2000, 1, &estado, 1)`; si `ioctl<0` loguea `[ERR] Timeout, retry after 1s`; si lee `0` loguea `[INFO] Baseband is not ready, retry after 1s`. Cada 20 iteraciones emite `sig_basebandStateChanged(0)`. Log `[INFO] Wait baseband connection...`.
2. **Info de baseband:** `iicRead(0x0000, 1, BaseCommIIC::mBasebandInfo, 0x81 = 129 B)` (`[ERR] Read baseband info failed` si falla).
3. **Calibración:** `iicRead(0x0100, 1, this+8, 0x1fb = 507 B)` (`[OK] Read calibrate data` / `[ERR] Read calibrate data failed`).
4. **Bucle principal (10 ms):** consume una cola `QVector<basecomiic_param_t>` (`this+0x204`, con mutex):
   - `host_cmd` → `iicWrite(0xfffe, 1, &cmd_u16, 2)` + log `[DEBUG] Write host cmd %04X`.
   - `update_calibdata` → `iicWrite(0xf000, 1, this+8, 0x1fb)`.
   - `wbrm` → estado del receptor de banda ancha (`[DEBUG] Wide Band Receiver State =`).
   - resto → `BaseCommIIC::parseParamItem(...)` sobre el **bloque de workparams** `BaseCommIIC+0x20d` y programa una escritura/lectura (`this+0x318`=dirección, `this+0x31c`=puntero, `+0x320`=longitud, `this+0x324`=estado 0=read/1=write/2=idle).
5. **Refresco periódico:** en estado 2, cada 100 iteraciones de 10 ms (**≈1 s**) y bajo mutex: `iicWrite(0x0000, 1, BaseCommIIC+0x20d, 0xd8 = 216)` — **reescribe el bloque completo de workparams**. Si el `ioctl` devuelve error se cuenta; al llegar a 10 errores vuelve a estado 0/reinit.
6. **Salida:** si el flag de stop `this+0x20a` (`BaseCommIIC::stop()`@`0x0ded58` lo pone a 1) ⇒ `close(fd)` y `exited`. **No escribe ningún valor de restauración** (no pone bits a 0, no reescribe un bloque "seguro").

---

## 2. Bloque de workparams y registro de MUX (`reg 12`) [HECHO en gran parte]

- El bloque `basecommiic_pack` tiene **216 B (`0xd8`)**, ubicado en `BaseCommIIC+0x20d`, y se escribe a la **dirección I²C `0x0000`** (modo 16-bit) tal cual (no hay framing adicional en el bloque: `iicWrite` antepone sólo la dirección).
- **Word 12 = bytes `0x30..0x34`** del bloque es la palabra de flags que nuestra RE llama `reg12` (`0x0030`). Evidencia: `HALLAZGOS.md` #77 documenta `reg12` en `0x0030` con `iptt=0x40000` y `voice_rec=0x8`; la APP 1.0.8 escribe exactamente `pack+0x30`.
- **Bits que la APP 1.0.8 escribe/borra en `pack+0x30`** (`BaseCommIIC::parseParamItem`@`0x0ded64`, tamaño `0x39dc`; instrucciones `orr/bic` sobre `[pack+0x30]`): `0x2`, `0x8`, `0x10`, `0x20`, `0x1000`, `0x2000`, `0x4000`, `0x8000`, `0x10000`, **`0x40000`** (dos ramas distintas), `0x100000`, `0x200000`, `0x400000`, `0x800000`. Además escribe `pack+0x34` bit `0x10000` y bytes de `pack+0x34`.
- **Mapeos param→bit resueltos [HECHO]:** `updateWorkParams`@`0x0e2b88` llama `setWFlags(&valor, &(BaseCommIIC+0x23d), mask)` (donde `+0x23d = pack+0x30`) con:
  - param `wparam/sple` → bit `0x2`
  - param `wparam/atue` (ATU enable) → bit `0x1000`
- **Mapeo param→bit NO resuelto [UNKNOWN]:** el resto de los bits, incluido **qué parámetro del `parseParamItem` activa `0x8`** y cuál `0x40000`. Nombres de parámetros manejados por `parseParamItem` (de las comparaciones de `QString`): `vfoa:{freq,agc,att,mode,pre}`, `vfob:{…}`, `sple`, `atue`, `tune`, `rece`, `mtx`, `swrt`, `cal`, `caltx`, `iptt`, `civptt`, `pof`, `tune_flag`, `bias_final_off`, `bias_drive_off`, `iqswap`, `vi`, `vm`, `rxvol`, `rfg`, `txpwr`, `atuparam`, etc. (`pof`, `bias_*`, `auxiqen`, `160m_start` también aparecen como cadenas en el área `basecommiic.cpp`). **No se inventa el mapeo**: se reporta `PARTIAL`.
- **Cruce con la RE previa:** nuestra librería enlazada usa `reg12` (palabra 12) con `voice_rec=0x8` y `iptt/PTT=0x40000` en read-modify-write sobre una sombra. La APP vendor 1.0.8 **escribe la misma palabra y contiene ambos bits en su repertorio de escrituras** ⇒ hay señal de contrato MUX, pero **no** se acreditó el mapeo completo ni el efecto físico. `mux_contract_signal = FOUND_UNVERIFIED`.

---

## 3. ¿Hay ACK/readback/restore/supervisor? [HECHO]

| Mecanismo exigido por #362 | ¿Existe en la APP 1.0.8? | Evidencia |
|---|---|---|
| **U1 verdad de estado (readback físico de reg12)** | **No.** No se relee el bloque de workparams tras escribirlo. Sólo se leen `0x2000` (estado), `0x0000/129 B` (info) y `0x0100/507 B` (calibración) al iniciar. | `BaseCommIIC::start` §1.3; no hay `iicRead` posterior a cada `iicWrite` del bloque |
| **U2 arbitraje/interlocks** | **No** en la APP (no hay dueño serializado del bit ni interlocks PTT/ATU/voice_rec; la APP asume que es la única dueña del bloque). | `parseParamItem`/`updateWorkParams` sin mutex del bit más allá del mutex de la cola |
| **U3 restore independiente ≤250 ms tras crash/cierre** | **No.** Al salir sólo `close(fd)`. No hay reescritura de estado seguro, ni hilo supervisor, ni watchdog/heartbeat. El refresco ~1 s del bloque completo sólo actúa **mientras la APP está viva** y re-afirma la sombra (no es readback ni restore tras muerte). | `BaseCommIIC::start` punto 5/6; `BaseCommIIC::stop`=`this[0x20a]=1`; sin cadenas `watchdog/heartbeat` |
| **U4 prueba física** | **No aplica / no ejecutada** (prohibido en esta fase). | — |

- El único "supervisor" es el **refresco periódico del bloque cada ≈1 s**; **no** garantiza ≤250 ms ni verifica estado físico. `MUX_SAFETY_STILL_BLOCKED`.
- La APP tampoco escribe "registro 12" como operación atómica: escribe el bloque de 216 B completo (read-modify-write del lado de la APP sobre su copia). Si otra entidad (nuestra ROM) escribe el mismo bloque, se pisan mutuamente — riesgo de MUX ya conocido en #362.

---

## 4. Matriz 1.0.6 vs 1.0.8 — camino de control BASE

| Campo | 1.0.6 (auténtica) | 1.0.8 (payload no autenticado) | Veredicto |
|---|---|---|---|
| Dispositivo | `/dev/i2c-0`, `open(O_RDWR)` | ídem | igual [HECHO] |
| Primitivas | `iicRead`/`iicWrite` vía `ioctl I2C_RDWR` | ídem | igual [HECHO] |
| Tamaños/instrucciones `BaseCommIIC::{iicRead(0x100), iicWrite(0x178), start(0xb18), parseParamItem(0x39dc), updateWorkParams(0x2a08), updateRxFilters(0x78), setCalibrateData(0x1b4), cwDecodeWPM(0x2c)}` | — | — | **misma longitud y secuencia de instrucciones normalizada**; sólo cambian literales de relocación (GOT/rodata) [HECHO comparando ambos disassemblies] |
| Bloque de workparams | 216 B en `BaseCommIIC+0x20d`, flags en `+0x30` | ídem | igual [HECHO] |
| Bits de flags escritos | mismos `orr/bic` en `parseParamItem` | ídem | igual [HECHO] |
| ACK/readback del bloque | no | no | igual (ausente) [HECHO] |
| Restore tras crash/cierre | no | no | igual (ausente) [HECHO] |
| Refresco periódico bloque | sí, ≈1 s | sí, ≈1 s | igual [HECHO] |
| Dependencia externa | MCU/BASE (firmware `.xgf`, opaco) | MCU/BASE (`.xgf` 1.0.8, opaco) | **`MCU_OPAQUE`** (no decodificado) |
| Uplink serie | sin SLIP, trama 2088 B, magic sync | **SLIP**, trama 552 B, sin magic check | **único cambio de transporte** |
| Clases de audio | `XAudio`, `xaudio_pcm_t`, `xaudio_t`, `XAudioPlayer`, `XAudioRecorder`, `DialogVoiceRec`, `DialogVoiceCall`, `AudioWorker` | mismas clases presentes (tamaños de funciones de audio comparables sin cambios) + **nuevo `PanelCall`** (voz + Morse) y **`SpectrogramWidget`** (bandscope OpenGL 3D); se retiran `XComm`/`XScopeGL` | sin cambios en la ruta de control; sólo UI/paneles |
| MUX/RX readback, restore, supervisor | ausentes | ausentes | **sin mejora** |

**Conclusión:** el cambio 1.0.8 relevante para compatibilidad es **exclusivamente el uplink SLIP + payload de 552 B**; el control del BASE (I²C/workparams/reg12) es **el mismo código** que 1.0.6 y **no aporta** readback de estado físico, ACK ni restore. El MCU sigue opaco ⇒ no se puede afirmar nada de su comportamiento interno.

---

## 5. `.xgf` (firmware MCU/BASE): `MCU_OPAQUE` [HECHO]

| Archivo | Tamaño | SHA256 | Entropía | Símbolos distintos | Header |
|---|---:|---|---:|---:|---|
| `x6200_mcu_20260820141208.xgf` (1.0.8) | 524288 B | `9a19f97a5ad9e141a3663d222f984073bf0b18b38d4662604372a0036f9daa23` | 7.470 bit/B | 256 | `2cb6ce74a1837bae…` (sin firma conocida) |
| `x6200_mcu_20250111134652.xgf` (1.0.6) | 524288 B | `dca90387b70d9550448006310a69ea65625bec87f402c10df789deeb13ff12f3` | 7.748 bit/B | 256 | `087d23fa0a47006d…` |

Entropía alta + 256 símbolos + ausencia de cabecera/strings legibles ⇒ consistente con imagen cifrada/comprimida. **No se intentó romper el cifrado ni extraer el firmware**; se clasifica **`MCU_OPAQUE`** y no se usa para inferir el protocolo.

---

## 6. UNKNOWNs de esta parte

1. Mapeo exacto **param→bit** de `parseParamItem` para todos los bits de `pack+0x30` (sólo `atue→0x1000`, `sple→0x2` resueltos). **[UNKNOWN]**
2. Qué param enciende el bit `0x8` (candidato funcional: `voice_rec` según RE previa) y cuál `0x40000` (candidatos: `iptt`/`civptt`). **[UNKNOWN]**
3. Semántica del byte/word `pack+0x34` y del bit `0x10000` que la APP escribe. **[UNKNOWN]**
4. Comportamiento interno del MCU 1.0.8 (imagen opaca). **[UNKNOWN]**
5. Si el refresco ~1 s bastaría como mitigación parcial y con qué latencia real. **[HIPÓTESIS] no acreditada; no cumple el criterio U3 de #362.**
6. No se probó nada físicamente: no hay readback, ni restore, ni medición de audio. **[NO EJECUTADO por autorización]**

**Estado:** `mux_contract_signal = FOUND_UNVERIFIED` (escrituras vendor del mismo registro, sin readback/ACK/restore); **`MUX_SAFETY_STILL_BLOCKED`**; #362 no se desbloquea por estos datos.
