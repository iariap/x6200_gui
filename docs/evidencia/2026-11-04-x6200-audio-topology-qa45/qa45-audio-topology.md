# QA45 — Topología del audio nativo del X6200 y viabilidad de DSP

Fecha: 2026-11-04. Investigación offline. Sin flash, sin radio, sin TX, sin abrir el equipo.

## 1. Base y alcance

- Base de trabajo: `681074e0` (main usado por el PR; `origin` upstream difiere, no se modificó).
- Rama aislada `research/x6200-audio-topology-qa45`.
- QA11–QA44 preservados en sus PR/ramas; este PR sólo agrega QA45.
- No se implementó ni flasheó nada.

## 2. Fuentes de hardware

### 2.1 FCC 2BGMW-X6200

- [FCC ID](https://fccid.io/2BGMW-X6200)
- [Internal Photos 7396922](https://fccid.io/2BGMW-X6200/Internal-Photos/Internal-Photos-7396922)
- PDF: 6 páginas, SHA256 `df2fc0befb1df7109d76b89f2a98dc271e8828a52a5646d214523757923da61f`.
- **Schematics: Metadata only**; no se obtuvo esquema.
- Fotos: STM32H725 (U8), SoC Allwinner, eMMC, DDR3, AXP223, QN8035/BK1080 (FM),
  U22 QFN no identificado. Sin netlist.

### 2.2 Desmontaje R1CAU (archivado)

El artículo original está bloqueado por DDoS-Guard, pero hay copia Wayback:

- https://web.archive.org/web/20250426012236/https://www.xiegu.ru/clauses/xiegu---chto-tam-kitaytsy-navayali-ch2-kh6200-alek/
- Lista de componentes de la placa digital:
  - **STM32H725** (DSP)
  - **Allwinner PB0880AB (A33)**
  - **10CL025YU256I7** (FPGA Cyclone 10 LP)
  - **ADS4145** (ADC 125 MHz)
  - **MXT2144** (DAC 14 bit)
  - **CM108B** (audio codec USB)
  - **CH342F**, **AXP223**, USB HUB.
- Es una descripción de placa; **no publica netlist ni conexiones**.

### 2.3 Otras fuentes

- [EA5JTT X6200](https://ea5jtt.blogspot.com/2025/10/xiegu-x6200.html): menciona FPGA 1.0.0;
  no da topología.
- No se encontró boardview, service manual ni netlist auténtico.

## 3. Inventario de componentes

| Componente | Evidencia | Estado |
|---|---|---|
| STM32H725 | FCC U8 + R1CAU | **VERIFIED** |
| Allwinner A33 | R1CAU PB0880AB + FCC | **VERIFIED** |
| FPGA 10CL025 | R1CAU | **VERIFIED** (componente) |
| ADC ADS4145 | R1CAU | **VERIFIED** |
| DAC MXT2144 | R1CAU | **VERIFIED** |
| CM108B audio codec | R1CAU | **VERIFIED** (componente) |
| I2S3 H725: PA15/PC10/PC11/PC12 AF6 | QA42/firmware | **VERIFIED** |
| Conexión I2S3 → CM108B/A33/FPGA | — | **UNKNOWN** |

## 4. Topología sustentada (con etiquetas)

```text
ANT ──► RF front-end ──► ADS4145 ADC ──► 10CL025 FPGA (DDC/DUC)
                                              │
                                              ├──?──► STM32H725 (DSP/control)
                                              │         │
                                              │         ├─ I2S3 PA15/PC10/PC11/PC12
                                              │         │     └──?──► CM108B / A33 / FPGA
                                              │         └─ SPI/control FPGA, MUX
                                              │
                                              └──?──► Allwinner A33 (Linux)
                                                        │
                                                        ├─ codec sun8i / CM108B ──?──► speaker
                                                        └─ ADC Mic1 (RX capture)
```

| Enlace | Tipo | Etiqueta |
|---|---|---|
| RF → ADS4145 | analógico | **SUPPORTED_HYPOTHESIS** |
| ADS4145 → FPGA 10CL025 | digital | **SUPPORTED_HYPOTHESIS** |
| FPGA → H725 / A33 | digital | **UNKNOWN** |
| H725 I2S3 ↔ externo | digital | **UNKNOWN**; candidato CM108B/A33/FPGA |
| H725 I2S3 RX → 0x30000000 | digital | **VERIFIED** (QA44) |
| H725 RX buffer → FFT/análisis | digital | **VERIFIED** (QA44) |
| A33 ADC Mic1 → Linux capture | analógico/digital | **VERIFIED** (QA14/QA15) |
| A33 DAC / codec → parlante | analógico | **VERIFIED funcionalmente** (QA11b) |
| Selector BASE/DAC | control | **VERIFIED comportamiento**, mecanismo **UNKNOWN** |
| Ruta digital RX independiente del selector | digital | **UNKNOWN** |

No se deduce conexión por cercanía. La presencia de CM108B y A33 no demuestra que
I2S3 termine en uno de ellos.

## 5. Evidencia funcional de QA11–QA44

- QA14/QA15: el ADC del Allwinner captura RX útil en modo BASE.
- QA11b: PCM del Allwinner es audible por el parlante interno; DAC funciona.
- QA11b/QA12: al seleccionar DAC (`voice_rec`), la captura RX útil desaparece.
- QA28–QA36: parches de `reg24`/`reg0x34` no lograron simultaneidad; no son solución.
- QA37–QA44: I2S3 RX `0x30000000` (32 bits) es consumido por FFT/análisis;
  no hay productor de TX ni camino al parlante demostrado en el MCU.
- QA44: Ghidra confirma que `FUN_08039250` copia RX a memoria de análisis; no RX→TX.

## 6. Las tres arquitecturas

### A — DSP en STM32H725

Requisitos: muestras RX audibles accesibles + recorrido hasta parlante + punto de
procesamiento seguro.

Evidencia a favor: H725 tiene I2S3, DMA, buffer RX y DSP/FPU; QA44 ve RX en `0x30000000`.
Evidencia faltante: no se demostró que esas muestras sean el audio RX audible ni que exista
una salida al parlante; el buffer TX no tiene productor; el endpoint I2S3 es desconocido.

**MCU_DSP_FEASIBILITY = INCONCLUSIVE.** No se elige.

### B — DSP en FPGA o procesador externo

Requisitos: identificar el procesador de audio RX y una interfaz modificable.

Evidencia a favor: FPGA 10CL025 y ADC ADS4145 están identificados; el artículo lo describe
como DDC/DUC; EA5JTT reporta firmware FPGA 1.0.0.
Evidencia faltante: no hay bitstream, mapa de registros ni prueba de que la demodulación de
audio ocurra en la FPGA; no hay interfaz de modificación documentada.

**FPGA_DSP_FEASIBILITY = INCONCLUSIVE.** No se elige.

### C — DSP Linux con salida por parlante interno

Requisitos: ruta de captura RX y salida analógica independientes del selector BASE/DAC.

Evidencia a favor: A33 captura RX (QA14) y reproduce PCM (QA11b); el parlante interno funciona.
Evidencia faltante: al pasar a DAC se pierde la captura RX; no se encontró una ruta
independiente; el mecanismo del selector no está documentado. Los parches fallidos no cuentan.

**LINUX_DSP_FEASIBILITY = INCONCLUSIVE.** No se elige.

## 7. Decisión de ingeniería

**No hay una arquitectura positiva demostrada.** El bloqueo es documental/eléctrico: falta
saber qué componente termina I2S3 y cómo el selector BASE/DAC conmuta la captura y el parlante.

Clasificación:

- `NATIVE_AUDIO_TOPOLOGY = PARTIAL`
- `I2S3_EXTERNAL_ENDPOINT = UNKNOWN` (candidato no verificado: CM108B/A33/FPGA)
- `I2S3_RF_RX_AUDIO = INCONCLUSIVE`
- `RX_AUDIO_PROCESSOR = UNKNOWN`
- `INTERNAL_SPEAKER_DSP_PATH = UNPROVEN`
- `MCU_DSP_FEASIBILITY = INCONCLUSIVE`
- `FPGA_DSP_FEASIBILITY = INCONCLUSIVE`
- `LINUX_DSP_FEASIBILITY = INCONCLUSIVE`

Ninguna opción se elige por descarte. No se propone parche ni firmware.

## 8. Borrador de correo a Xiegu (no enviado)

**To:** Xiegu technical support — address to be confirmed (not invented)
**Subject:** X6200 audio topology / I2S3 interface information request

Dear Xiegu Support,

I am documenting an X6200 for an independent, receive-only audio research project. I am not
requesting firmware, keys, calibration data or proprietary source code. Could you please
provide or confirm:

1. A block diagram of the RX audio path from the ADC/FPGA to the internal speaker.
2. Which device(s) connect to STM32H725 pins PA15, PC10, PC11 and PC12 (I2S3).
3. The source and destination of the I2S3 signals.
4. Which codec or audio processor receives the demodulated RX audio.
5. Where the BASE/DAC selector is located and what it switches.
6. Whether an independent digital RX-audio route exists that does not depend on that selector.
7. Whether RX can remain active while the Allwinner DAC is used, and under what constraints.

A block diagram, connector/pin table or service documentation would be sufficient. No
firmware image or calibration data is requested.

Thank you,
[Name / project contact]

**No se envía automáticamente; no se inventa destinatario.**

## 9. Siguiente acción única

Obtener el **esquema/boardview/netlist auténtico** del X6200 o la respuesta de Xiegu que
identifique el extremo de I2S3 y el selector BASE/DAC. Sin esa evidencia, el análisis se
detiene; no se inicia otra búsqueda genérica ni QA46.

## 10. Estado

Firmware oficial 1.0.8; GUI/ALSA/MUX/ajustes intactos; sin TX/PTT/ATU/Bluetooth; sin flash.
