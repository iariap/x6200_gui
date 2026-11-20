# QA44 — Análisis de flujo de datos del MCU con Ghidra

Fecha: 2026-11-04. Offline. Sin radio, sin flash, sin TX.

## 1. Ghidra operativo

- **Ghidra 12.2 PUBLIC** en `/tmp/ghidra-install/ghidra_12.2_PUBLIC`.
- Java incluido por Ghidra; `analyzeHeadless` y decompilador funcionan.
- Firmware importado: `x6200-108.dec.bin`
  SHA256 `c3f7d028bc63e43fbb478a09e1032d46059b71efe217a60b576231866fbc2c0a`.
- Lenguaje: **ARM:LE:32:Cortex**, base `0x08020000`.
- Regiones añadidas con `tools/ghidra/AddMemory.java`:
  FLASH del loader, AXI SRAM `0x24000000` (0x80000), SRAM1 `0x30000000`
  (0x20000), SRAM2 `0x30020000`, PERIPH `0x40000000`.
- Tabla de vectores verificada en Ghidra antes de analizar; se crearon funciones en
  los handlers de FLASH.
- Scripts reproducibles:
  - `tools/ghidra/AddMemory.java`
  - `tools/ghidra/QA44Decomp.java`
  - `tools/ghidra/QA44CallGraph.java`
- Descompilación clave: `qa44-key-decomp.txt`.

## 2. DMA y buffers

De `FUN_08035B40` (Ghidra):

```c
FUN_0803995c(0x30000c00,0,0xc00);
FUN_0803995c(0x30000000,0,0xc00);
...
iVar5 = FUN_08028fe8(rx_handle, SPI3+0x30, 0x30000000, 0x300);
iVar5 = FUN_08028fe8(tx_handle, 0x30000c00, SPI3+0x20, 0x600);
```

- RX: `PAR=SPI3+0x30`, `M0AR=0x30000000`, `NDTR=0x300` unidades.
- TX: `PAR=SPI3+0x20`, `M0AR=0x30000C00`, `NDTR=0x600` unidades.
- PSIZE/MSIZE: RX 32/32-bit; TX 16/16-bit (QA40/QA41).
- Callbacks registrados: RX `+0x40=0x08035A55`, `+0x3C=0x08035A41`,
  `+0x4C=0x08035A11`; TX `+0x40=0x08035AD5`, `+0x3C=0x08035A69`,
  `+0x4C=0x08035A11`.
- Los callbacks sólo actualizan estado/flags. **No copian muestras.**

## 3. Consumidor RX real

De `FUN_080319E0` (y `FUN_080339F0`), Ghidra resuelve:

```c
uVar2 = FUN_08039e88();                 // 0x300 - NDTR
FUN_08039250(0x30000000, 0x2400A848, uVar2 & 0xfffffffe, 0x300);
...
VectorSignedToFloat(*(int *)(0x2400A848 + i*8)) / 132.0;
```

- `0x30000000` es el buffer RX.
- `FUN_08039250` copia una ventana circular desde el buffer RX a **0x2400A848**
  (buffer interno de análisis).
- Luego convierte enteros de 32 bits a float, normaliza y alimenta rutinas de
  `FUN_08030B74`/FFT.
- `FUN_080339F0` hace lo mismo hacia `0x2400A248`.

Esto demuestra un **consumidor real de los datos RX**: un camino de análisis
(FFT/espectro), no una copia al buffer TX ni al codec.

## 4. Productor TX

Ghidra no encontró ninguna escritura de CPU al buffer TX `0x30000C00`; sólo:

- limpieza inicial en `FUN_08035B40`;
- arranque DMA `memoria→SPI3`.

No se localizó un productor de muestras TX en el MCU en este análisis estático.
`TX_BUFFER_PRODUCER = UNRESOLVED`.

## 5. Otras funciones

- `FUN_0803D1E8`: opera sobre `param_3` y `FUN_0803586C`; **no lee/escribe** los buffers
  `0x30000000`/`0x30000C00`.
- `FUN_0803D978`: sólo `FUN_08035B40` → `FUN_0803D1E8` → `FUN_0803C540`; no hay copia
  RX→TX.
- `FUN_0802B0E0`: escribe `I2SCFGR` (`SPI3+0x50`), `I2SMOD=1`; no se recuperan
  frame/sample rate completos.
- `FUN_08021AAC`: DMA1 Stream0/5 y GPIO AF; ya descrito en QA41/QA42.

## 6. Grafo de flujo

```text
I2S3 RXDR ──DMA1 Stream0──► 0x30000000 (32-bit, 0xC00 B)
                                  │
                                  ├─ FUN_08039250 ──► 0x2400A848
                                  │       │
                                  │       └─ int32→float / FFT / análisis
                                  └─ (sin copia al buffer TX)

0x30000C00 (16-bit, 0xC00 B) ──DMA1 Stream5──► I2S3 TXDR
      └─ productor CPU: NO ENCONTRADO (UNRESOLVED)

I2S3 ──► endpoint externo: UNKNOWN (FCC photos sin netlist; QA43)
```

Etiquetas:

| Enlace | Etiqueta |
|---|---|
| I2S3 RX → `0x30000000` | **VERIFIED** |
| `0x30000000` → buffer interno FFT | **VERIFIED** |
| Buffer interno → parlante | **UNKNOWN** |
| `0x30000C00` → I2S3 TX | **VERIFIED (DMA)** |
| Productor de `0x30000C00` | **UNKNOWN** |
| I2S3 ↔ A33/FPGA/codec | **UNKNOWN** |

## 7. Decisión

- `MCU_AUDIO_DATAFLOW = PARTIAL`: el RX se consume en análisis; el TX no tiene productor
  demostrado y no hay camino al parlante.
- `I2S3_RX_CONTENT = IDENTIFIED` como **datos de análisis de 32 bits** (FFT/espectro);
  **no** se demuestra PCM de audio RX audible.
- `I2S3_TX_CONTENT = UNKNOWN`.
- `RF_RX_PCM_IN_MCU = INCONCLUSIVE`.
- `NATIVE_SPEAKER_DSP_PATH = UNPROVEN`.
- `DSP_INSERTION_READY = NO`.

No hay punto de procesamiento que garantice audio por el parlante. El análisis estático MCU
se cierra acá. La siguiente dependencia es **documentación eléctrica auténtica o una fuente
verificable del hardware** (esquema/boardview/netlist), no otra búsqueda genérica.

## 8. Estado

Firmware oficial 1.0.8; GUI/ALSA/MUX/ajustes intactos; sin TX/PTT/ATU/Bluetooth; sin flash.
