# QA38 — SPI3/I2S3, DMA y buffers de audio

Fecha: 2026-11-04. Investigación **offline**. Sin radio, sin flash, sin TX.
Corrige y amplía QA37. `main` no se tocó.

## 1. Correcciones a QA37

| Afirmación QA37 | Corrección QA38 |
|---|---|
| `0x30000000` era memoria externa/FPGA | Es **SRAM1 interna** del H725; contiene **buffers de datos** de 0xC00 B |
| H725 tiene cuatro SAI | El H725 dispone de **SAI1 y SAI4**; SAI2/SAI3 no existen en este modelo |
| No hay SAI ⇒ no hay audio | La ausencia de literales SAI no demuestra ausencia de audio. **SPI3 está configurado como I2S3** |
| `0x24005DC4/0x24005D4C` son buffers | Son **estructuras de control/DMA handle**, no los datos |
| `MCU_RX_PCM_FOUND=NO` | Reclasificado **INCONCLUSIVE**: hay I2S3 y buffers candidatos, falta demostrar dirección/formatos |
| SPI3 era sólo control | **SPI3=I2S3** con dos transferencias DMA; el contenido exacto sigue sin probarse |

Los literales alineados no son prueba de ausencia de referencias calculadas. Este informe usa
**flujo de ejecución y registros**, no sólo el escaneo de palabras.

## 2. Hechos nuevos

### 2.1 SPI3 está en modo I2S

La función `0x0802B0E0` (actualización de I2S3) escribe **SPI3+0x50 = I2SCFGR**:

- `0x0802B1D0`: `I2SMOD` se fuerza con `orr r3,#1` antes de:
- `0x0802B1D8`: `str r3,[r2,#0x50]`.

En el header CMSIS H725, `SPI_I2SCFGR` está en **offset 0x50** y `I2SMOD` es el **bit 0**.
Por tanto:

- `SPI3_MODE = I2S` (no SPI convencional).
- No hay referencias literales a SAI1/SAI4 en el código; la ruta de audio digital del H725 es
  **I2S3**, no SAI.

### 2.2 Dos transferencias DMA

`0x08021AAC` inicializa dos handles DMA1 para el objeto cuyo primer campo es `0x40003C00` (SPI3):

- DMA1 Stream5: `0x40020088`, DMAMUX request `0x3E`.
- DMA1 Stream0: `0x40020010`, DMAMUX request `0x3D`.
- Handles en RAM: `0x24005D4C` y `0x24005DC4`; **no son buffers de audio**.
- El IRQ de SPI3 queda en el handler default (`0x080248D1`); el movimiento lo hace DMA.

### 2.3 Buffers de audio en SRAM1

La función `0x08035B40` (llamada desde `0x0803D978`):

1. Limpia **0xC00 bytes en `0x30000C00`** y **0xC00 bytes en `0x30000000`**.
2. Guarda en el estado `0x24005A38`:
   - `[+0x2C] = 0x30000C00`
   - `[+0x34] = 0x30000000`
   - `[+0x30] = 0x600`, `[+0x38] = 0x300` (mitades/tamaños)
3. Configura los handles DMA `[+0x44]`/`[+0x48]` y arranca la transferencia I2S3.

`0x30000000..0x30003FFF` es **SRAM1 interna**, no FPGA ni memoria externa. Los dos buffers
de 0xC00 B son candidatos claros a **PCM I2S3**: uno RX y otro TX.

`0x24005860` (DMA2 Stream5) no es el mismo camino; se mantiene como estructura/otro DMA, sin
atribuirle audio.

## 3. Cadena de audio propuesta

```text
codec/FPGA ── I2S3 ──► DMA1 Stream0/5 ──► 0x30000000 / 0x30000C00
                                            │
                                            ├─ consumidor: 0x0803D978 / 0x08035B40
                                            └─ relleno: DMA + IRQ
```

- `0x0803D978` llama a `0x08035B40` y luego a `0x0803D1E8`.
- `0x08035AD4` y `0x08035B40` gestionan el estado de los buffers/DMA.
- `0x08029614` es el callback común de DMA (DMA1 Stream0/5/7, DMA2 Stream5); no se identificó
  todavía el punto exacto que emite los strings de underrun.

**No está demostrado** cuál de los dos buffers es RX y cuál TX, ni el formato exacto
(profundidad de bits, canales, sample rate). No se infiere una tasa por la longitud.

## 4. Underruns

Strings localizados:

- `CODEC buffer underrun` — `0x08067E2E`
- `FPGA buffer underrun` — `0x08067E4A`

No se encontró una referencia directa (literal ni `ADR`) a esas direcciones en el código
lineal. El escaneo de palabras no es suficiente para cadenas empaquetadas/tablas; por eso:

- `CODEC_UNDERRUN_SOURCE = UNKNOWN`
- `FPGA_UNDERRUN_SOURCE = UNKNOWN`

La hipótesis más fuerte es que el primero corresponde a **I2S3 TX/codec** y el segundo a un
buffer de **FPGA/otro DMA**, pero **no está demostrado**. No se atribuye un buffer a cada
mensaje sin la referencia de código.

## 5. Comparación cruzada

- 1.0.6 y 1.0.8 contienen los mismos strings de audio y la misma secuencia de escritura
  `43 f0 01 03` (`orr #1`) que puede corresponder a `I2SCFGR`; la estructura de audio es
  estable entre versiones.
- La GUI vendor no se usó en esta etapa para atribuir PCM; no hay evidencia de que el MCU
  entregue muestras a la GUI.

## 6. Decisión técnica

**B. Transferencia digital de audio identificada, pero buffers/dirección incompletos.**

- `SPI3_MODE = I2S`
- `DMA_AUDIO_PAYLOAD = UNPROVEN` (I2S3 es audio digital, pero falta probar que el buffer
  concreto transporta PCM RX y no sólo una mitad TX/control)
- `CODEC_UNDERRUN_SOURCE = UNKNOWN`
- `FPGA_UNDERRUN_SOURCE = UNKNOWN`
- `MCU_RX_PCM_FOUND = INCONCLUSIVE`
- `NEXT_IMPLEMENTATION_PATH = MCU_DSP` (condicional a confirmar cuál buffer es RX; si no,
  `ROUTING_REA`)

No se propone parche ni se genera firmware. Tampoco se declara FPGA como procesador de RX.

## 7. Próximo experimento offline

1. Seguir el estado `0x24005A38` en `0x08035AD4`/`0x08035B40` para asignar RX/TX a
   `0x30000000` y `0x30000C00`.
2. Reconstruir `0x0802B0E0`/`0x08021AAC` para obtener la tasa I2S3 si está codificada en
   `I2SCFGR`/`I2SPR`; no inferirla de la longitud.
3. Buscar referencias calculadas a los strings de underrun (tablas/relativos), no literales.
4. Sólo después, prototipo passthrough **offline**; sin flashear.
