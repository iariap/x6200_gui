# QA40 — DMA sizes, procesamiento RX/TX y límite del punto de inserción

Fecha: 2026-11-04. Offline; sin radio, sin flash, sin TX. Continúa QA39.

## 1. Corrección de longitudes

QA39 llamó “bytes” a `0x300`/`0x600`. Eso **no está justificado**: son argumentos de
`0x08028FE8` (arranque DMA) y se expresan en **unidades de dato**, no en bytes.

Configuración de los handles (campos usados por `0x080287DC`):

| Handle | Campo `[+0x14]` | Campo `[+0x18]` | Interpretación |
|---|---:|---:|---|
| RX (Stream0) | `0x1000` | `0x4000` | PSIZE/MSIZE = **32 bits** |
| TX (Stream5) | `0x800` | `0x2000` | PSIZE/MSIZE = **16 bits** |

En el `DMA_SxCR` del H7, `0x1000`/`0x800` ocupan los bits de **PSIZE** y `0x4000`/`0x2000`
los de **MSIZE**. Por tanto:

- RX: `0x300` unidades × 4 B = **0xC00 B**.
- TX: `0x600` unidades × 2 B = **0xC00 B**.

Ambas transferencias ocupan exactamente **0xC00 B**, coherente con las dos regiones SRAM de
0xC00 B. `DMA_TRANSFER_SIZES = INCONCLUSIVE` a nivel de registro completo (no se decodificó
todavía `0x080287DC` entero), pero **la corrección bytes/unidades y 32/16 bits está fundada**
en los campos anteriores y en los argumentos de `0x08028FE8`.

### Argumentos efectivos

- RX (`0x08035C0C`): `r0=[r6+0x48]`, `r1=SPI3+0x30` (RXDR), `r2=0x30000000`,
  `r3=0x300` → PAR=`SPI3+0x30`, M0AR=`0x30000000`, NDTR=`0x300`.
- TX (`0x08035C32`): `r0=[r6+0x44]`, `r1=0x30000C00`, `r2=SPI3+0x20` (TXDR),
  `r3=0x600` → PAR=`SPI3+0x20`, M0AR=`0x30000C00`, NDTR=`0x600`.
- **DIR**: RX periférico→memoria; TX memoria→periférico.
- **MINC/PINC/CIRC/DBM**: no resueltos con evidencia suficiente; no se declaran.

## 2. ¿Quién procesa entre RX y TX?

Se revisaron:

- `0x08035AD4`, `0x08035B40`: estado/DMA.
- `0x0803D978`: llama a `0x08035B40`, luego a `0x0803D1E8` y `0x0803C540`.
- `0x0803D1E8`: rutina grande con FPU/CORDIC y estado; **no se demostró** que lea
  `0x30000000` y escriba `0x30000C00`.
- referencias literales a `0x30000000` (`0x8031C38`, `0x8033D08`, `0x8035C8C`):
  son tablas/pools/estado, no una copia RX→TX.
- `0x24005A38`: estado de audio/DMA; usada por `0x08035B40` y `0x08039E??`.

**Resultado:** no se encontró una función que lea RX y escriba TX de forma verificable.

- `RX_BUFFER_CONSUMER = UNKNOWN`
- `TX_BUFFER_PRODUCER = UNKNOWN`
- `RX_TO_TX_PROCESSING = NOT_FOUND`

No se descarta que `0x0803D1E8` procese muestras; falta demostrar sus accesos efectivos.
No se infiere conversión 32→16 por la diferencia de PSIZE.

## 3. Formato y reloj

- `0x0802B0E0` escribe `I2SCFGR` (`SPI3+0x50`) y `I2SMOD=1`.
- No se recuperaron los valores efectivos de `I2SCFG/I2SSTD/DATLEN/CHLEN/CKPOL` ni el
  prescaler; **sample rate desconocido**.
- La diferencia RX 32-bit / TX 16-bit es un hecho de DMA, no una prueba de formato I2S ni de
  frecuencia.

`I2S3_FORMAT = PARTIAL`; `I2S3_SAMPLE_RATE = UNKNOWN`.

## 4. Origen del audio

- I2S3 + DMA + buffers está demostrado.
- El extremo lógico (codec, FPGA u otro) **no** está demostrado.
- `I2S3_RF_RX_AUDIO = INCONCLUSIVE`.
- No se puede afirmar que el buffer RX sea el RX audible.

## 5. Underruns

Sin referencias verificables a `0x08067E2E`/`0x08067E4A`; el grafo DMA→buffer no incluye
todavía el emisor. `CODEC_UNDERRUN_SOURCE = UNKNOWN`; `FPGA_UNDERRUN_SOURCE = UNKNOWN`.
No bloquea la conclusión principal.

## 6. Punto de inserción

No hay un punto seguro demostrado. Un parche en `0x30000000` o `0x0803D1E8` sería especulativo:
no se conoce el contrato de la rutina ni si el buffer es RX de RF, ni el formato.

- `MCU_DSP_INSERTION = BLOCKED`
- `DSP_PROTOTYPE_READY = NO`

No se genera firmware.

## 7. Próximo paso (único)

**Decodificar `0x08028FE8` + `0x080287DC` y seguir el contrato de `0x0803D1E8`** para
identificar el consumidor real de `0x30000000` y el productor de `0x30000C00`.
Sin flashear; sin radio.

## 8. Gates

| Gate | Resultado |
|---|---|
| `DMA_TRANSFER_SIZES` | **INCONCLUSIVE** (unidades/32/16 fundadas; CR completo pendiente) |
| `I2S3_FORMAT` | **PARTIAL** |
| `I2S3_SAMPLE_RATE` | **UNKNOWN** |
| `RX_BUFFER_CONSUMER` | **UNKNOWN** |
| `TX_BUFFER_PRODUCER` | **UNKNOWN** |
| `RX_TO_TX_PROCESSING` | **NOT_FOUND** |
| `I2S3_RF_RX_AUDIO` | **INCONCLUSIVE** |
| `MCU_DSP_INSERTION` | **BLOCKED** |

Evidencia reproducible: `qa39-i2s3-pipeline.md`, `qa38-flow.json`, `scan.json`,
`tools/mcu/qa38_flow.py`.
