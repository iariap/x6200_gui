# QA42 — Extremo de I2S3, pines y consumidores reales

Fecha: 2026-11-04. Offline; sin radio, sin flash, sin TX. Continúa QA41.

## 1. Pines I2S3: verificados

En `0x08021AEE..0x08021B6A` se inicializan GPIOA/GPIOC con AF6:

| Señal I2S3 | Pin STM32 | GPIO base | Config |
|---|---|---|---|
| WS | **PA15** | `0x58020000` | Pin `0x8000`, AF6 |
| CK/SCK | **PC10** | `0x58020800` | Pin `0x1C00` (PC10/11/12), AF6 |
| SDI (RX) | **PC11** | `0x58020800` | AF6 |
| SDO (TX) | **PC12** | `0x58020800` | AF6 |

No se configuró **MCK**. Los GPIO se inicializan con `0x8029A38` (helper HAL).
No se encontró reset/control de un codec ni selección de FPGA en este bloque.

`I2S3_PINS = VERIFIED` (PA15/PC10/PC11/PC12, AF6).

## 2. Extremo externo: UNKNOWN

Los pines no identifican por sí solos el dispositivo. En el repositorio no hay esquema,
boardview ni pinout verificable del X6200 que conecte estas cuatro señales. La inicialización
no contiene un reset de codec, selección de FPGA ni identificador de componente.

Posibilidades no decidibles: codec de audio, FPGA o procesador de audio.
`I2S3_EXTERNAL_ENDPOINT = UNKNOWN`; no se inventa el componente.

## 3. Formato y reloj

- `0x0802B0E0` escribe `I2SCFGR` (`SPI3+0x50`) con `I2SMOD=1`.
- No se recuperaron `I2SCFG`, `I2SSTD`, `DATLEN`, `CHLEN`, `CKPOL`, prescaler ni PLLI2S.
- Master/slave y origen del clock: **UNKNOWN**.
- Sample rate: **UNKNOWN**; no se infiere de los tamaños.

`I2S3_FORMAT = PARTIAL`; `I2S3_SAMPLE_RATE = UNKNOWN`.

## 4. Consumidores/productores reales

Se siguieron los callers de los helpers de progreso:

- `0x08039E88` (RX: `0x300 − NDTR`) es llamado por:
  - **0x080319E0**
  - **0x080339F0**
- `0x08039EA8` (TX: `0x600 − NDTR`) es llamado por:
  - **0x0803198A**
- `0x0803D960` es llamado por **0x08022C16** (inicialización principal).

Contexto de los consumidores:

- `0x080319E0`/`0x080339F0` leen el progreso y llaman a `0x08039250`/`0x08039A5C`;
  trabajan con `0x180`/`0x300` y lógica de bloques. Son compatibles con **FFT/espectro**,
  no con una copia de audio hacia el parlante.
- **No se encontró** una instrucción que lea `0x30000000` y escriba `0x30000C00`; los
  callbacks DMA sólo actualizan estado/flags.
- No se identificó el productor del buffer TX ni un consumidor de audio del buffer RX.

`RX_BUFFER_CONSUMER = UNRESOLVED`; `TX_BUFFER_PRODUCER = UNRESOLVED`.

## 5. Naturaleza de los datos

- I2S3 + DMA + buffers está demostrado.
- El extremo externo y el formato no; por tanto **no** se demuestra que `0x30000000`
  contenga el RX de RF audible.
- Los consumidores de progreso se usan en rutinas de espectro/FFT, lo que sugiere que los
  buffers pueden alimentar análisis, no el camino de audio del parlante.
- `RF_RX_PCM_IN_MCU = INCONCLUSIVE`; `NATIVE_SPEAKER_PATH = PARTIAL`.

## 6. Decisión

**Caso D — información insuficiente.** Falta el eslabón externo: no hay evidencia del
componente conectado a PA15/PC10/PC11/PC12 ni del contrato del consumidor de audio.

Gates:

- `I2S3_PINS = VERIFIED`
- `I2S3_EXTERNAL_ENDPOINT = UNKNOWN`
- `I2S3_FORMAT = PARTIAL`
- `I2S3_SAMPLE_RATE = UNKNOWN`
- `RX_BUFFER_CONSUMER = UNRESOLVED`
- `TX_BUFFER_PRODUCER = UNRESOLVED`
- `RF_RX_PCM_IN_MCU = INCONCLUSIVE`
- `NATIVE_SPEAKER_PATH = PARTIAL`
- `DSP_INSERTION_READY = NO`

No hay un único camino de implementación implementable todavía. No se genera firmware ni
harness con muestras falsas.

## 7. Próximo paso único

**Obtener/revisar el esquema o boardview del X6200** que conecta PA15/PC10/PC11/PC12 y el
componente externo de I2S3. Sin ese artefacto, cualquier atribución a codec/FPGA es inventada.
No repetir búsquedas genéricas en el MCU.

## 8. Estado

Firmware oficial 1.0.8 intacto; GUI/ALSA/MUX/ajustes sin cambios; sin TX/PTT/ATU/Bluetooth.
