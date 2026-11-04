# QA41 — DMA I2S3: configuración, callbacks y límites del flujo RX/TX

Fecha: 2026-11-04. Offline; sin radio, sin flash, sin TX. Continúa QA40.

## 1. Configuración DMA efectiva

Handles: RX `0x24005DC4` (DMA1 Stream0), TX `0x24005D4C` (DMA1 Stream5).

| Campo | RX | TX |
|---|---|---|
| Instance | `0x40020010` | `0x40020088` |
| Request | `0x3D` (RX) | `0x3E` (TX) |
| PAR | `SPI3+0x30` (RXDR) | `SPI3+0x20` (TXDR) |
| M0AR | `0x30000000` | `0x30000C00` |
| NDTR | `0x300` unidades | `0x600` unidades |
| PSIZE | **32-bit** (`0x1000`) | **16-bit** (`0x800`) |
| MSIZE | **32-bit** (`0x4000`) | **16-bit** (`0x2000`) |
| MINC | 1 (`0x400`) | 1 (`0x400`) |
| PINC | 0 | 0 |
| CIRC | 1 (`0x100`) | 1 (`0x100`) |
| DBM | 0 | 0 |
| DIR | 0 (periférico→memoria) | 1 (`0x40`, memoria→periférico) |
| Bytes efectivos | `0x300×4=0xC00` | `0x600×2=0xC00` |

`DMA_SxCR` base resultante: RX `0x35500`; TX `0x32D40` (sin `EN` ni bits de interrupción;
el valor final completo no se recuperó). `DMA_CONFIG = PARTIAL`;
`DMA_TRANSFER_SIZES = VERIFIED` (unidades + PSIZE/MSIZE + bytes).

`0x08028FE8` es el arranque de transferencia: recibe handle, PAR/dirección, M0AR/buffer y
NDTR/unidades. `0x080287DC` es la inicialización de stream.

## 2. Callbacks registrados

`0x08035B40` escribe los callbacks en los handles:

- RX `[0x24005DC4]`:
  - `+0x3C` = `0x08035A41` → entrada real **0x08035A40**
  - `+0x40` = `0x08035A55` → **0x08035A54**
  - `+0x4C` = `0x08035A11` → **0x08035A10**
- TX `[0x24005D4C]`:
  - `+0x3C` = `0x08035A69` → **0x08035A68**
  - `+0x40` = `0x08035AD5` → **0x08035AD4**
  - `+0x4C` = `0x08035A11` → **0x08035A10**

`0x08029614` es el dispatcher de IRQ DMA. Invoca `[r7+0x3C]`, `[r7+0x40]`, `[r7+0x4C]`
según half/complete/error; la asignación exacta de esos tres campos es **inferida** del flujo.
`DMA_CALLBACK_GRAPH = COMPLETE` para los callbacks registrados, con esa salvedad.

Las funciones callback **no acceden a los buffers de audio**: leen/escriben estado de SPI3,
flags y llaman a `0x0802B260/64/68`. No se encontró una copia/transformación RX→TX en ellas.

## 3. Accesos a los buffers

- `0x08035B40` escribe los punteros `0x30000C00`/`0x30000000` en `0x24005A38`.
- `0x08039E88`: lee handle RX `[+0x48]`, NDTR y calcula `0x300 - NDTR` (progreso RX).
- `0x08039EAC`: lee handle TX `[+0x44]`, NDTR y calcula `0x600 - NDTR` (progreso TX).
- `0x0803D978` / `0x0803D1E8` / `0x0803C540`: no se demostró que lean
  `0x30000000` ni escriban `0x30000C00`; los literales `0x30000000` de `0x8031C38`/
  `0x8033D08` son tablas/pools, no copias.
- No se encontró `memcpy` ni conversión 32→16 entre buffers.

`RX_BUFFER_CONSUMER = UNRESOLVED`; `TX_BUFFER_PRODUCER = UNRESOLVED`;
`RX_TO_TX_PROCESSING = UNRESOLVED`.

## 4. Formato I2S3

- `0x0802B0E0` configura `I2SCFGR` (`SPI3+0x50`) con `I2SMOD=1`.
- RX 32-bit y TX 16-bit son hechos de **DMA**, no del frame I2S.
- Master/slave, `I2SCFG`, `I2SSTD`, `DATLEN`, `CHLEN`, `CKPOL`, prescaler/PLL y sample rate
  siguen sin recuperarse.

`I2S3_FORMAT = PARTIAL`; `I2S3_SAMPLE_RATE = UNKNOWN`.

## 5. Extremo físico

GPIO AF/RCC se inicializan en `0x08021AEE..0x08021B42`, pero no se identificó el codec/FPGA
conectado. `I2S3_RF_RX_AUDIO = INCONCLUSIVE`: no se demostró que el RXDR reciba audio de RF
audible y no audio de un codec/auxiliar.

## 6. Decisión

- `DMA_CONFIG = PARTIAL`
- `DMA_CALLBACK_GRAPH = COMPLETE` (registrados; campos half/complete/error inferidos)
- `RX_BUFFER_CONSUMER = UNRESOLVED`
- `TX_BUFFER_PRODUCER = UNRESOLVED`
- `RX_TO_TX_PROCESSING = UNRESOLVED`
- `I2S3_RF_RX_AUDIO = INCONCLUSIVE`
- `I2S3_FORMAT = PARTIAL`
- `MCU_DSP_INSERTION = BLOCKED`

No hay evidencia suficiente para un passthrough/filtro. No se genera harness con muestras
falsas.

## 7. Próximo paso único

**Investigar el extremo externo de I2S3**: reconstruir GPIO alternate functions de SPI3 y la
inicialización del codec/procesador de audio/FPGA para decidir quién escribe RXDR y quién lee
TXDR. Offline, sin radio.

## 8. Estado

Firmware oficial 1.0.8 intacto; GUI/ALSA/MUX/ajustes sin cambios; sin TX/PTT/ATU/Bluetooth.
