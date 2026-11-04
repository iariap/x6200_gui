# QA43 — Procesador de audio nativo y extremo de I2S3

Fecha: 2026-11-04. Offline. Sin radio, sin flash, sin TX, sin abrir el equipo.

## 1. Evidencia pública de hardware

- **FCC ID 2BGMW-X6200**, Internal Photos doc **7396922**:
  [FCC ID](https://fccid.io/2BGMW-X6200) ·
  [Internal Photos](https://fccid.io/2BGMW-X6200/Internal-Photos/Internal-Photos-7396922)
- PDF descargado: `qa43/internal-photos-7396922.pdf`
  SHA256 `df2fc0befb1df7109d76b89f2a98dc271e8828a52a5646d214523757923da61f`, 6 páginas.
- El expediente FCC marca **Schematics — Metadata only**: no hay esquema descargable.
- El desmontaje de **R1CAU** en xiegu.ru devolvió **403/DDoS-Guard** desde este entorno;
  no se usó como evidencia.

### Componentes legibles en las fotos

| Componente | Evidencia | Estado |
|---|---|---|
| **STM32H725** | U8, página 5 inferior | **VERIFIED** |
| **SoC Allwinner** (familia A33 por el ecosistema Linux) | página 5 superior | **SUPPORTED** (marcado parcial) |
| **eMMC Samsung KLMBG2JETD** | página 5 | **VERIFIED** |
| **DDR3 Micron BD4H7 D9T0** | página 5 | **VERIFIED** |
| **AXP223** | página 5 | **VERIFIED** |
| **QN8035** y otro receptor FM (BK1080?) | página 6 superior | **VERIFIED** (FM broadcast) |
| **U22 QFN** con cristal 4AK | página 6 inferior | **UNKNOWN** (posible FPGA/ADC; no se lee) |
| **FPGA** | no hay marking legible en la placa digital | **UNKNOWN** |

No se identificó un **codec de audio independiente** en las fotos. La web pública de EA5JTT
menciona una versión **FPGA 1.0.0**, pero no asigna pines ni componente al I2S3:
[EA5JTT X6200](https://ea5jtt.blogspot.com/2025/10/xiegu-x6200.html).

No se dedujo ninguna conexión eléctrica por cercanía. Las fotos son de la revisión FCC
2024; no prueban el routing.

## 2. Pines I2S3

| Señal | Pin | AF |
|---|---|---|
| WS | **PA15** | AF6 |
| CK | **PC10** | AF6 |
| SDI (RX) | **PC11** | AF6 |
| SDO (TX) | **PC12** | AF6 |

Inicialización `0x08021AEE..0x08021B6A`; GPIOA/GPIOC; helper `0x08029A38`. No MCK.
`I2S3_PINS = VERIFIED`.

## 3. Formato/reloj

- `0x0802B0E0` escribe `I2SCFGR` (`SPI3+0x50`) con `I2SMOD=1`.
- No se recuperaron `I2SCFG`, `I2SSTD`, `DATLEN`, `CHLEN`, `CKPOL`, prescaler ni PLLI2S.
- Master/slave y sample rate: **UNKNOWN**.
- `I2S3_FORMAT = PARTIAL`; `I2S3_SAMPLE_RATE = UNKNOWN`.

## 4. Consumidores/productores

Callers verificados:

- `0x08039E88` (progreso RX) ← `0x080319E0`, `0x080339F0`.
- `0x08039EA8` (progreso TX) ← `0x0803198A`.
- `0x0803D960` ← `0x08022C16` (inicialización).

Los bloques `0x080319E0`/`0x080339F0` usan `0x180`/`0x300` y llaman a
`0x08039250`/`0x08039A5C`. Su contrato **no está documentado**; son **compatibles con
FFT/espectro**, pero no se clasifican como FFT ni como PCM sin demostrar entradas/salidas.

No se encontró una instrucción que lea `0x30000000` y escriba `0x30000C00`.
Los callbacks DMA no copian muestras.

- `RX_BUFFER_CONSUMER = UNRESOLVED`
- `TX_BUFFER_PRODUCER = UNRESOLVED`

## 5. Arquitectura: hechos vs hipótesis

```text
[RF/FPGA?] ──?──► [I2S3 H725] ──?──► [A33/codec?] ──?──► parlante
     │                │
     └─ UNKNOWN       ├─ DMA RX 0x30000000 (32-bit)
                      ├─ DMA TX 0x30000C00 (16-bit)
                      └─ consumidores de progreso (FFT-compatible, no probado)

[STM32H725] ── I2S3 PA15/PC10/PC11/PC12 AF6 ──► endpoint externo UNKNOWN
```

| Enlace | Etiqueta |
|---|---|
| RF → audio demodulado | **UNKNOWN** |
| FPGA → I2S3 | **UNKNOWN** |
| I2S3 ↔ A33/codec | **UNKNOWN** |
| A33 codec → parlante | **SUPPORTED_HYPOTHESIS** (Linux/ALSA, no netlist) |
| MCU procesa PCM RX | **UNKNOWN** |

## 6. Clasificación final

**UNRESOLVED (Caso D).** No se demostró que I2S3 transporte el RX audible ni que el MCU
procese muestras del receptor. Tampoco se demostró la ruta FPGA; no se elige FPGA_DSP ni
MCU_DSP por proximidad.

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

No hay diseño de passthrough: falta el extremo y el contrato del consumidor.

## 7. Requisito concreto para continuar

Se necesita **uno** de estos artefactos:

1. esquema/boardview auténtico del X6200 que conecte PA15/PC10/PC11/PC12; o
2. continuidad medida (fuera de este trabajo offline) entre esos pines y A33/FPGA/codec.

Sin eso, cualquier clasificación positiva sería inventada. La etapa offline se cierra acá;
no se inicia QA44 automáticamente.

## 8. Estado

Firmware oficial 1.0.8; GUI/ALSA/MUX/ajustes intactos; sin TX/PTT/ATU/Bluetooth; sin flash.
