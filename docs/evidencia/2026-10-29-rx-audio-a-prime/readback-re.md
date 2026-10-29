# Fase R — readback real del registro 12: `R_BLOCKED_NO_HARDWARE_TRUTH`

**Regla de aceptación (issue #367 §2):** sólo se acepta una lectura del MCU documentada con
`dev, addr, request bytes, response bytes, endianness, index, bit mask`, fundamentada por REA y
contrastada con evidencia no invasiva (lecturas históricas o instrumentación read-only dentro del
dueño de I²C). La sombra `x6200_control_get()` **no** es readback.

## 1. Artefactos y herramientas

| artefacto | sha256 | uso |
|---|---|---|
| `refs/vendor-fw/x6200_ui_v100` (GUI de fábrica) | `b76331c5aa859ae98dc1819b1079a3e40489e9986c9656e0dc2c553aeb28871b` | REA/Ghidra 12.1.4 (REA 6.7.0) |
| `.xgf` MCU (2025-01-11) | `dca90387b70d9550448006310a69ea65625bec87f402c10df789deeb13ff12f3` | **encriptado**, no analizable |
| lib desplegada | `80c460e403bfe77684c773a50a054bfe6c4a858753695a01167493e35e126df6` | lado Linux que escribe el reg 12 |
| `flow.h` del SDK (protocolo serial BASE→GUI) | repo `refs/X6200Control` | fuente de verdad física de TX/ATU |

Comandos REA (host; evidence_ids en `logs/rea-evidence-ids.txt`):

```sh
node refs/rea/scripts/rea.mjs decompile refs/vendor-fw/x6200_ui_v100 0xe2f70 --snapshot .qa367/vendor.snapshot --provider ghidra
node refs/rea/scripts/rea.mjs decompile refs/vendor-fw/x6200_ui_v100 0xe62b8  # BaseCommIIC::start()
node refs/rea/scripts/rea.mjs decompile refs/vendor-fw/x6200_ui_v100 0xe5dc0  # updateRxFilters
node refs/rea/scripts/rea.mjs decompile refs/vendor-fw/x6200_ui_v100 0x5c1f4  # slot_slaveReportMessage
```

## 2. Framing I2C de lectura del vendor (`BaseCommIIC::iicRead`, 0xe2f70) — **documentado**

```
dev            = /dev/i2c-0   (open64(O_RDWR) en BaseCommIIC::start, fd global)
slave          = BaseCommIIC::addr = 0x72 (86 decimal; el constructor recibe 114=0x72)
request bytes  = 2 bytes BE (AddressMode==1): (addr & 0xff) << 8 | (addr >> 8)
                 [AddressMode==0 escribe 1 byte; el vendor usa 1 = 2 bytes para todas las lecturas]
response       = len bytes crudos (ioctl I2C_RDWR con 2 mensajes: write addr + read len)
endianness     = el payload se interpreta según el consumidor: float LE de 32 bits para 0x3040,
                 bytes/strings para 0x0000, enteros LE de 32 bits para parámetros de 4 B
```

## 3. Qué lee el vendor (6 call sites, todos identificados)

| addr | len | función | evidencia |
|---|---|---|---|
| `0x2000` | 1 | `BaseCommIIC::start()` — baseband ready | `0xe6444` |
| `0x0000` | `0x81` (129) | `BaseCommIIC::start()` — baseband info (string con versión/fecha) | `0xe6530` |
| `0x0100` | `0x1fb` (507) | `BaseCommIIC::start()` — calibration data | `0xe65b0` |
| `0x3000` | `0x40` (64) | `decodeTextMessage()` — texto del BASE | `0xe5e7c` |
| `0x3040` | 4 | `cwDecodeWPM()` — float de WPM | `0xe5edc` |
| `queue addr` | `queue len` | `iicRead(addr,buf,len)` en la cola de work params (`mode==0`) | `0xe6a50` |

**No hay ninguna lectura de `0x0030` (registro 12) ni del bloque de 54 registros.**

## 4. El bloque de registros (216 B @ addr 0) se **escribe**, no se relee

- `BaseCommIIC::start()`: write periódico del bloque completo `iicWrite(0, 1, this+0x20d, 216)`
  (`0xe6a98`) — 54 registros × 4 B, el mismo layout que la lib Aether (reg 12 = offset 48 = `0x30`).
- `updateWorkParams()` termina igual: `iicWrite(0,1,this+0x20d,216)` (`0xe55a8`) cuando hay que
  reenviar todo.
- Cola de parámetros: cada entrada fija `addr = parseParamItem(...)` (offset en bytes),
  `buf = this+0x20d+offset`, `len = 4`, `mode = 1 (write)`. Tras escribir pone `mode = 2`.
- **El camino `mode == 0` (read de la cola, `0xe6a3c`) es código muerto**: en todo el binario sólo se
  almacenan `mode=1` y `mode=2` (barrido de stores a `this+0x324`); ninguna entrada de la cola pide
  lectura. Es decir: el vendor **escribe** los work params/registros y **no los relee**.

## 5. Canal serial BASE→GUI: estado físico real, pero no el reg 12

`MainWidget::slot_slaveReportMessage(basecomm_pack const&)` (0x5c1f4) decodifica el paquete que el
BASE manda por el puerto serial (mismo canal que en el port usa `x6200_flow_read()`): muestras de
audio, flags (TX/ATU/SQL/power), medidores (dBm, potencia, ROE, ALC), batería/fuente, teclas.
**No contiene el bloque de registros ni `voice_rec`.**

Sin embargo es **verdad física del MCU** para interlocks: `x6200_flow_t.flag.tx` (bit 1) refleja el
estado de TX real reportado por el BASE (el port ya lo usa para su máquina de estados,
`src/radio.c::radio_tick`). Contrato de lectura para los interlocks de B:
`dev=/dev/ttyS1, request=ninguno (push ~35 ms), response=paquete x6200_flow_t (548 B), endianness=LE
del host, index=flag bit 1, mask=0x2`. **Esto no es readback del reg 12** y no reemplaza A2.

## 6. Evidencia no invasiva histórica (contraste)

`docs/HALLAZGOS.md` §69 (medido en radio): el BASE responde lecturas I2C de `0x2000` (1 B),
`0x0000` (info 129 B), `0x0100` (calibración) y `0x1000` (8 B de work params); las direcciones
`0x0200…0x0C00`, `0x1800`, `0x3000…0x5000` devolvieron 0. **No hay ninguna lectura registrada de
`0x0030`** ni del bloque de registros. El único intento del proyecto de recuperar estado del BASE fue
con replay de escrituras, no con readback (§70).

## 7. Conclusión

**`R_BLOCKED_NO_HARDWARE_TRUTH`**: no existe una lectura real, documentada y contrastada del estado
de `voice_rec`/reg 12. Las opciones que quedan **no** cumplen la regla de aceptación:

- leer `0x0030` con framing del vendor: sería una **dirección candidata no probada** (el vendor no la
  lee; el issue prohíbe sondear direcciones candidatas) y requeriría I2C crudo en un segundo proceso;
- leer el bloque completo 216 B: mismo problema (el MCU nunca muestra ese bloque en las lecturas
  documentadas);
- usar el canal serial: no lleva el registro;
- firmware del MCU: encriptado (`.xgf`), sin fuente ni símbolos.

**Qué haría falta para desbloquear R**: documentación del MCU/FPGA que especifique la ventana de
lectura del bloque de registros, o evidencia de lectura read-only dentro del dueño de I²C con
respuesta consistente (patrón de dos lecturas + valor esperado de un registro conocido), autorizada
por el owner. Sin eso, el coordinador y el guardián **no escriben** (fail-closed) y `A_PASS` sigue
bloqueado.
