# QA39 — Pipeline I2S3 RX/TX: buffers identificados, formato pendiente

Fecha: 2026-11-04. Offline; sin radio, sin flash, sin TX. Rama aislada desde `main`.
QA37/QA38 se conservan; este documento los sigue y corrige su punto abierto.

## 1. DMAMUX: confirmado

En `0x08021AAC`:

| Stream | Base | Request | Uso |
|---|---:|---:|---|
| DMA1 Stream0 | `0x40020010` | `0x3D` (61) | **SPI3_RX** |
| DMA1 Stream5 | `0x40020088` | `0x3E` (62) | **SPI3_TX** |

Referencias: `0x08021BBC` (`0x3D`), `0x08021B74` (`0x3E`).
La dirección del periférico es RX para Stream0 y TX para Stream5.

## 2. Buffers RX/TX: identificados por los argumentos de arranque

Estado `0x24005A38` (`r6` en `0x08035B40`), que guarda:

- `[+0x2C] = 0x30000C00`
- `[+0x34] = 0x30000000`
- `[+0x30] = 0x600`, `[+0x38] = 0x300`
- handles DMA en `[+0x44]` y `[+0x48]`.

En `0x08035B40`:

### RX

```text
0x08035BD8: r0 = [r6+0x48]          ; handle RX
0x08035BF6: r1 = [r6] + 0x30       ; SPI3 RXDR
0x08035C08: r2 = 0x30000000        ; buffer
0x08035C0A: r3 = [r6+0x38] = 0x300 ; longitud
0x08035C0C: bl 0x08028FE8          ; start: periférico -> memoria
```

→ `I2S3_RX_DMA_BUFFER = 0x30000000`, longitud inicial `0x300`, handle `[+0x48]`
(Stream0, request 0x3D).

### TX

```text
0x08035C26: r3 = [r6+0x30] = 0x600 ; longitud
0x08035C2C: r1 = [r6+0x2C] = 0x30000C00 ; buffer
0x08035C2E: r2 = [r6] + 0x20        ; SPI3 TXDR
0x08035C30: r0 = [r6+0x44]          ; handle TX
0x08035C32: bl 0x08028FE8           ; start: memoria -> periférico
```

→ `I2S3_TX_DMA_BUFFER = 0x30000C00`, longitud inicial `0x600`, handle `[+0x44]`
(Stream5, request 0x3E).

No se asignó por proximidad: la asignación sale de **PAR/M0AR/dirección** en los argumentos
de `0x08028FE8` y de los offsets de dato `SPI3+0x30` (RXDR) y `SPI3+0x20` (TXDR).

## 3. Buffers, mitades y coherencia

- Dos regiones de **0xC00 B** en **SRAM1** (`0x30000000..0x30003FFF`).
- RX usa **0x30000000**, longitud `0x300` B por transferencia.
- TX usa **0x30000C00**, longitud `0x600` B por transferencia.
- Las longitudes `0x300`/`0x600` aparecen en `[r6+0x38]`/`[r6+0x30]`;
  no demuestran ping-pong ni sample rate.
- **No se identificó** double-buffer/circular explícito ni una segunda llamada a RX/TX.
  La repetición de `0x08035B40` no se pudo fechar; no se declara ping-pong.
- Coherencia de caché Cortex-M7: no se encontró un `SCB_CleanDCache`/`Invalidate` asociado a
  estos buffers en el flujo; el punto queda **UNKNOWN**.

## 4. Formato y tasa

- `0x0802B0E0` escribe `SPI3+0x50 = I2SCFGR`; `0x0802B1D4` pone `I2SMOD=1`.
- `0x0802B1D8` escribe `I2SCFGR`; también toca `CR1`, `CFG2`, `IFCR`.
- No se recuperaron los valores de `r4` que definen `I2SCFG`, `I2SSTD`, `DATLEN`, `CHLEN`,
  `CKPOL`, `PCMSYNC`, ni el prescaler.
- **No se infiere sample rate por tamaños**.

Gates:

- `I2S3_FORMAT = PARTIAL` (I2S3 confirmado; profundidad/canales/dirección completos: UNKNOWN)
- `I2S3_SAMPLE_RATE = UNKNOWN`

## 5. Origen y destino lógico

I2S3 + DMA + buffers está demostrado. Lo que **no** está demostrado es que el extremo sea el
RX de RF audible:

1. codec de audio (ADC/DAC);
2. FPGA como extremo I2S3;
3. otro componente.

No se encontraron GPIO alternate functions/codec init que cierren la atribución.
**No se asume RF RX por el nombre `SPI3_RX`.**

- `I2S3_RF_RX_AUDIO = INCONCLUSIVE`
- `RX_TO_SPEAKER_CHAIN = PARTIAL`

## 6. Callbacks y underruns

- IRQ DMA1 Stream0/5: `0x08023D04`/`0x08023D18`; llaman a `0x08029614`.
- `0x08029614` es el dispatcher DMA; no se identificó el punto que repone buffers ni el que
  emite underruns.
- Strings `0x08067E2E`/`0x08067E4A` sin xref directa; hace falta análisis de tabla/relativos.
- `CODEC_UNDERRUN_SOURCE = UNKNOWN`
- `FPGA_UNDERRUN_SOURCE = UNKNOWN`

## 7. Viabilidad DSP

- Buffer RX identificado: `0x30000000`.
- Formato/tasa sin verificar; no hay aún un punto de procesamiento seguro.
- **No** se propone firmware ni prototipo ejecutable.

Gates:

- `MCU_DSP_INSERTION_POINT = 0x30000000` (buffer RX candidato; **no** es aún un punto de
  procesamiento validado)
- `DSP_PROTOTYPE_READY = NO`
- `MCU_RX_PCM_FOUND = INCONCLUSIVE`

## 8. Próximo paso offline

1. Resolver `0x08028FE8` (PAR/M0AR/NDTR/dirección) y la repetición de `0x08035B40`.
2. Recuperar campos de `0x0802B0E0` para formato/tasa.
3. Resolver strings de underrun por tabla/relativos.
4. Sólo si el contenido RX y el formato quedan demostrados, diseñar passthrough/filtro
   offline.

Sin flasheo, sin registros, sin TX/PTT/ATU/Bluetooth; firmware oficial 1.0.8 intacto.
