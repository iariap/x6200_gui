# QA39 — Pipeline I2S3 RX/TX: estado y límites de la evidencia

Fecha: 2026-11-04. Offline; sin radio, sin flash, sin TX. Rama aislada desde `main`.
QA37/QA38 se conservan; este documento no repite su escaneo, lo sigue.

## 1. DMAMUX: confirmado

El firmware configura dos handles DMA1 en `0x08021AAC`:

| Stream | Base | Request | Uso DMAMUX |
|---|---:|---:|---|
| DMA1 Stream0 | `0x40020010` | `0x3D` (61) | **SPI3_RX** |
| DMA1 Stream5 | `0x40020088` | `0x3E` (62) | **SPI3_TX** |

Referencias: `0x08021BBC` (`0x3D`), `0x08021B74` (`0x3E`).
La dirección del **periférico** es, por tanto, RX para Stream0 y TX para Stream5.

**No se sigue de esto qué buffer de memoria es RX o TX**: la dirección del request es
periférico→memoria o memoria→periférico, pero la asignación de buffers debe seguirse por
los punteros reales.

## 2. Buffers y estado

Hechos:

- Estado `0x24005A38` (`r6` en `0x08035B40`).
- `[r6+0x2C] = 0x30000C00`
- `[r6+0x34] = 0x30000000`
- `[r6+0x30] = 0x600`, `[r6+0x38] = 0x300`
- Dos regiones de **0xC00 B** en **SRAM1 interna** (`0x30000000..0x30003FFF`).
- Handles DMA en `0x24005D4C` y `0x24005DC4`, enlazados desde `0x08021AAC`.

`0x08035B40`:

- limpia las dos regiones;
- escribe `[r6+0x2C]`/`[r6+0x34]`;
- configura los handles `[r6+0x44]`/`[r6+0x48]`;
- usa `0x08028FE8` para transferencias.

`0x0803D978` llama a `0x08035B40`.

## 3. ¿Cuál es RX y cuál TX?

**No demostrado.** El código usa ambos punteros, pero no se recuperó de forma inequívoca la
asignación de `r6+0x2C`/`r6+0x34` a los handles `r6+0x44`/`r6+0x48`. Por tanto:

- `I2S3_RX_DMA_BUFFER = UNKNOWN` (candidatos: `0x30000C00`, `0x30000000`)
- `I2S3_TX_DMA_BUFFER = UNKNOWN`

No se asignan por proximidad de direcciones. La estructura `0x24005A38` y las llamadas
`0x08035C0C`/`0x08035C32` son el punto exacto a resolver; no hay base para atribuir una mitad
a RX y otra a TX sólo por el nombre del stream.

## 4. Formato y tasa

- `0x0802B0E0` escribe **SPI3+0x50 = I2SCFGR**; `0x0802B1D4` pone `I2SMOD=1`.
- `0x0802B1D8` escribe `I2SCFGR`; hay además escrituras a `CR1`, `CFG2`, `IFCR`.
- No se recuperaron los valores de `r4` que definen `I2SCFG`, `I2SSTD`, `DATLEN`, `CHLEN`,
  `CKPOL`, `PCMSYNC`, ni la relación de prescaler.
- **No se infiere sample rate por tamaño de buffer**.

Gates:

- `I2S3_FORMAT = PARTIAL` (I2S3 confirmado; formato completo **UNKNOWN**)
- `I2S3_SAMPLE_RATE = UNKNOWN`

## 5. Origen/destino lógico

La evidencia disponible muestra I2S3 + DMA + buffers SRAM, pero **no demuestra** que las
muestras sean audio de RF demodulado. Posibilidades abiertas:

1. codec de audio (ADC/DAC) conectado a I2S3;
2. FPGA como extremo del enlace I2S3;
3. otro componente de audio/control.

No se encontraron GPIO alternate functions ni rutinas de codec que permitan cerrar la
atribución. **No se asume RF RX por el nombre `SPI3_RX`**; ese nombre sólo indica la dirección
STM32 respecto del periférico.

- `I2S3_RF_RX_AUDIO = INCONCLUSIVE`
- `RX_TO_SPEAKER_CHAIN = PARTIAL`

## 6. Callbacks y underruns

- IRQ DMA1 Stream0/5: `0x08023D04`/`0x08023D18`; llaman a `0x08029614`.
- `0x08029614` es un callback/dispatcher DMA; no se identificó el punto que repone buffers
  ni el que emite los underruns.
- Strings `0x08067E2E`/`0x08067E4A` sin xref directa; la tabla/relativos no se resolvieron.
- `CODEC_UNDERRUN_SOURCE = UNKNOWN`
- `FPGA_UNDERRUN_SOURCE = UNKNOWN`

El obstáculo es concreto: las cadenas están en una tabla empaquetada sin puntero directo;
hace falta un análisis de tabla/relativos, no otra búsqueda de literales.

## 7. Viabilidad DSP

- No hay un buffer RX identificado de forma inequívoca.
- No hay formato/tasa verificados.
- No hay punto de inserción seguro.
- **No** se propone prototipo ni firmware.

Gates:

- `MCU_DSP_INSERTION_POINT = NOT_FOUND`
- `DSP_PROTOTYPE_READY = NO`
- `MCU_RX_PCM_FOUND = INCONCLUSIVE`

## 8. Siguiente paso offline

1. Resolver `0x08035AD4`/`0x08035B40` y la asignación de `r6+0x2C/0x34` a los handles.
2. Recuperar los campos de configuración de `0x0802B0E0` (formato y prescaler).
3. Analizar tabla/relativos para los strings de underrun.
4. Sólo si RX y formato quedan demostrados, diseñar passthrough/filtro offline.

Sin flasheo, sin registros, sin TX/PTT/ATU/Bluetooth; firmware oficial 1.0.8 intacto.
