# QA37 — Mapa del camino RX en el MCU BASE 1.0.8

Fecha: 2026-11-04. Estudio **offline**, sin radio, sin flash, sin TX.
Rama aislada `research/x6200-mcu-rx-path` desde `84777dff`; `main` no se tocó.

## 1. Artefactos y método

- MCU 1.0.8 descifrado: `x6200-108.dec.bin`
  SHA256 `c3f7d028bc63e43fbb478a09e1032d46059b71efe217a60b576231866fbc2c0a`
- Imagen de 524288 B, base de carga **0x08020000**.
- **Ghidra no está instalado** en el entorno. Se usó REA reproducible con
  **Capstone/objdump + CMSIS STM32H725** (`stm32h725xx.h`) y el escaneo
  `tools/mcu/mcu_rx_path_scan.py`; el resultado está en `scan.json`.
- No se usó la radio ni se escribió ningún registro.

## 2. Hechos verificados

### 2.1 Vector table

- SP inicial `0x24050000`; reset `0x08024881`.
- 104 entradas no nulas en la tabla.
- IRQ relevantes:

| IRQ | Periférico | Handler | Estado |
|---:|---|---|---|
| 11 | DMA1 Stream0 | `0x08023d05` (real `0x08023d04`) | activo |
| 16 | DMA1 Stream5 | `0x08023d19` (real `0x08023d18`) | activo |
| 47 | DMA1 Stream7 | `0x08023701` (real `0x08023700`) | activo |
| 51 | SPI3 | `0x080248d1` | **default; no IRQ propio** |
| 68 | DMA2 Stream5 | `0x08023c99` (real `0x08023c98`) | activo |
| 87 | SAI1 | `0x080248d1` | **default** |
| 122 | MDMA | `0x080248d1` | default |

### 2.2 Periféricos referenciados

Conteo de literales de 32 bits en la imagen (base+offset, no flujo completo):

| Periférico | Referencias |
|---|---:|
| SAI1, SAI2, SAI3, SAI4 | **0** |
| SPI1, SPI2, SPI4, SPI5, SPI6 | **0** |
| SPI3 | 5 (`0x40003c00`) |
| DMA1 / DMA2 | 34 / 12 |
| DMAMUX1 | 3 |
| MDMA | 1 |
| DAC | 3 |
| ADC1 / ADC3 | 25 / 10 |

No hay ninguna referencia a SAI ni IRQ de SAI. **No se encontró un periférico I²S/SAI
de audio**.

### 2.3 SPI3 y DMA

- `0x08021aac` configura DMA1 para el objeto cuyo primer campo es `0x40003c00` (SPI3):
  - usa `0x40020010` (DMA1 Stream0) y `0x40020088` (DMA1 Stream5);
  - buffers en RAM `0x24005d4c` y `0x24005dc4`;
  - no configura SAI; el IRQ de SPI3 queda en default y el movimiento lo hace DMA.
- `0x08023d04` (DMA1 Stream0) usa `0x24005dc4` y salta a `0x08029614`.
- `0x08023d18` (DMA1 Stream5) usa `0x24005d4c`.
- `0x08023c98` (DMA2 Stream5) usa `0x24005860`.
- `0x08035b40` inicializa/usa memoria externa `0x30000000`/`0x30000c00` y escribe
  `0xE000EF68`; es compatible con un bus de FPGA/externo, **no con un codec I²S**.

### 2.4 Strings de inicialización

- `[OK] Init audio codec` — `0x0804fc99`
- `[OK] Init audio processor` — `0x0804fcd5`
- `[OK] Init FPGA` — `0x0804fd2d`
- `[ERR] CODEC buffer underrun` — `0x08067e2e`
- `[ERR] FPGA buffer underrun` — `0x08067e4a`
- `RX Volume` — `0x080682a1`

Las cadenas no prueban por sí solas qué periférico transporta PCM. La palabra “codec” no
implica que el H725 reciba muestras PCM.

## 3. Hipótesis de camino físico (no confirmada)

**Hecho:** no hay SAI/I²S en el MCU; SPI3/DMA mueven datos hacia buffers pequeños y hacia
memoria externa/FPGA. La ruta de audio nativa que el operador escucha por el parlante es
**analógica** en el tramo final.

**Hipótesis mejor sostenida:**

```text
RF/FPGA ── audio RX analógico ──► conmutador/MUX (FPGA + MCU) ──► codec/amplificador
                                                                  └─► parlante interno
A33 Linux ── codec sun8i ── ADC (captura Mic1) ──► DSP Linux
             codec sun8i ── DAC ──► (ruta PCM alternativa, sujeta a MUX)
```

No hay evidencia en el MCU de un buffer PCM de RX ni de un consumidor de audio hacia el
parlante. El MCU aparece como **controlador** (SPI3/FPGA, MUX, volumen) y no como DSP de
audio RX.

## 4. MUX y control

Hechos de QA30/QA36 (sin nuevas escrituras):

- Handler `0x08045320`.
- BASE: `reg12=0x08`, `reg24=0x28`, `0x080450C8` (rutas 0..5).
- DAC: `reg12=0x0A`, `reg24=0x00/0x28` según parche, más `0x08043840`,
  `0x080439B4`, `0x080432A8`, `0x080439B4`, `0x080432A8`.
- `0x08044470` es el helper de escritura SPI/FPGA.
- Regs FPGA `0x34/0x36/0x37/0x39`; BASE caso 0: `4/0x40/4/0x40`; DAC incorpora
  `bit0x40` en `reg0x34` (candidato ≈0x44) y escribe `reg0x37/0x39` desde campos.
- Filtros `reg0x3B/0x3C` limitados a `0x5F`.
- H1 quitó `bit0x40` de `reg0x34`: no recuperó RX (QA32 B=143,0 vs A=2160,7) y
  probablemente deshabilitó DAC. **No confirmado**.
- No hay evidencia de un modo combinado RX+DAC. `COMBINED_RX_DAC_ROUTE=UNPROVEN`.

Sin verificar: semántica exacta de `reg12`, `reg24` y de los campos FPGA; existencia de un
modo combinado; si el SPI3 transporta audio o sólo control/espectro/FPGA.

## 5. Respuesta a la pregunta

**¿Dónde están las muestras RX inmediatamente antes de convertirse en el audio del
parlante?** Con la evidencia disponible: en el tramo **analógico final** entre el
conmutador/FPGA y el codec/amplificador; no en un buffer PCM accesible del H725.

**¿Se pueden procesar allí sin romper la radio?** No con el firmware BASE actual: el MCU no
expone un punto PCM de RX y un parche en SPI3/FPGA no garantiza audio ni bypass seguro.
La única ruta digital demostrada históricamente es la de **A33/Linux** (QA11b), no el MCU.

## 6. Clasificación

| Clasificación | Resultado | Confianza | Evidencia |
|---|---|---|---|
| `MCU_RX_PCM_FOUND` | **NO** | media-alta | sin SAI/IRQ SAI; sin buffer PCM; sólo SPI3/DMA de control/FPGA |
| `NATIVE_DSP_FEASIBLE` | **NO** (con el BASE actual) | media | no hay punto de datos PCM; no hay consumidor de audio en MCU |
| `INTERNAL_PCM_TRANSPORT_FEASIBLE` | **UNPROVEN** | baja-media | no hay enlace digital FPGA–A33 identificado; DTS sólo describe codec sun8i |
| `NEXT_EXPERIMENT_READY` | **YES** (offline) | alta | próximo paso estático, sin radio |

## 7. Prototipo y próximo experimento

No se especifica un prototipo de DSP en el MCU porque **no hay punto PCM identificado**.
Un prototipo passthrough/filtro en MCU sería especulativo y podría afectar MUX/FPGA.

Próximo experimento **offline**:

1. Confirmar el destino de SPI3/DMA1 con el mapa de FPGA y el esquema (sin radio).
2. Verificar si el codec es sólo control I²C/analógico o si existe un enlace PCM oculto
   hacia el H725 (no hay SAI; revisar SPI3/I²S y pines).
3. Comparar la ruta A33 (DTS/ALSA) para decidir si el DSP debe quedarse en Linux.
4. No generar firmware, no flashear, no probar tonos.

## 8. Estado

Radio con firmware oficial 1.0.8, GUI original, RX, `tx=false`; sin TX/PTT/ATU/Bluetooth.
Sin cambios de ALSA, MUX ni ajustes. `main` no modificado. PR #385/#386 sin merge.
