# REA + Ghidra — comandos, versiones y resultados (#390)

Investigación **offline**. Sin radio, sin flash, sin escrituras. Fechas en UTC (2026-11-04).

## 1. Herramientas y versiones reales

| Herramienta | Versión / ruta real | Estado |
|---|---|---|
| REA | `rea@6.7.0` · `/home/pablo/desa/x6200/refs/rea/scripts/rea.mjs` (`node 22.23.1`) | ejecutado |
| Proveedor Ghidra de REA | **Ghidra 12.1.4** (`provider.version = 12.1.4`) | disponible |
| Ghidra headless | `/home/pablo/desa/x6200/tools/ghidra/ghidra_12.1.4_PUBLIC/support/analyzeHeadless` · JDK `21.0.13+9` | ejecutado |
| Proyecto Ghidra | `/home/pablo/desa/x6200/qa390-ghidra` (X6200) | creado local (no versionado) |

**Desviación registrada:** el issue indicaba Ghidra **12.2** en `/tmp/ghidra-install/ghidra_12.2_PUBLIC`
y proyecto `/tmp/ghidra-proj/X6200`. En la sesión de ejecución **esas rutas no existen** (`/tmp` vacío;
verificado con `ls`). Se usó la instalación **real y persistente** de Ghidra 12.1.4 + JDK 21 del host.
REA analiza con `provider ghidra 12.1.4`. No se inventó ninguna ruta ni resultado de 12.2.

## 2. REA — comandos ejecutados y evidencia

```
node refs/rea/scripts/rea.mjs capabilities
  -> exit 0; provider ghidra available. log: logs/rea-capabilities.txt
node refs/rea/scripts/rea.mjs doctor
  -> exit 0; node 22.23.1 (aviso de versión), ghidra ok (12.1.4)
node refs/rea/scripts/rea.mjs analyze fw108-scratch/x6200-base-rea/x6200-108.dec.bin \
  --target-format raw-binary --architecture arm --entry-point 0x08020000 --provider ghidra --format json
  -> exit 0; evidence_id ev_0c40ee444f84ccc79b334ee3d9196fe7ee875bac32f86209d4370580fcdc9c0d
     segment "ram" 0x8020000..0x80a0000 (524288 B), 322 procedimientos, 351 strings
     log: logs/rea-analyze-108.json
node refs/rea/scripts/rea.mjs inspect-native-load-image fw108-scratch/x6200-base-rea/x6200-108.dec.bin \
  --target-format raw-binary --architecture arm --entry-point 0x08020000 --provider ghidra --json
  -> exit 0; evidence_id ev_7d4f61b04d4d1f8e4f0bfe4cd70162920b2cca25db23d85ef292052649b9ad6f
node refs/rea/scripts/rea.mjs analyze fw108-scratch/ext108/fs108/x6200_ui_v100 --provider ghidra --format json
  -> exit 0; evidence_id ev_ed994bd9f810f530498b762c075cb8bb409ad6f2f5c8b84ff61d086af0c7eff5
     GUI vendor 1.0.8 (ELF ARM), log: logs/rea-analyze-vgui.json
node refs/rea/scripts/rea.mjs decompile fw108-scratch/ext108/fs108/x6200_ui_v100 0xd9d40 \
  --provider ghidra --format json
  -> exit 0; evidence_id ev_6257747421299d679b8a7336e91ef269c581ef1150019c418a084efb5534e896
     (AudioWorker::udpDataReceived), log: logs/rea-vgui-decomp1.json
```

**Limitación de REA sobre el raw del MCU (documentada, no sustituida):**

```
node refs/rea/scripts/rea.mjs decompile fw108-scratch/x6200-base-rea/x6200-108.dec.bin \
  --file-offset 0x4881 --target-format raw-binary --architecture arm \
  --entry-point 0x08020000 --provider ghidra --format json
  -> exit 1; code=invalid_request: "File offset 0x4881 maps to analysis address 0x8024881,
     but no procedure starts at or contains that address in the current Ghidra analysis."
```

Causa: el loader raw de REA marca el bloque como `ram` (segmento `ram` 0x8020000) y no crea
procedimientos del Cortex‑M en la sesión efímera; por eso `decompile` por dirección/offset no aplica.
`analyze` sí funciona. Para el pseudocódigo del MCU se usó **Ghidra headless con los scripts QA390**
(apoyo permitido por el issue; los scripts QA44 se reutilizaron como base). REA sí decompiló la GUI
vendor (ELF) y analizó ambos binarios, por lo que la herramienta no fue reemplazada por escaneos
literales.

## 3. Ghidra headless — importación y análisis del MCU 1.0.8

Entrada: `x6200-108.dec.bin` SHA256 `c3f7d028bc63e43fbb478a09e1032d46059b71efe217a60b576231866fbc2c0a`.
Lenguaje `ARM:LE:32:Cortex`, `BinaryLoader`, base `0x08020000`. Scripts `tools/rea-native-rx/`.

```
analyzeHeadless qa390-ghidra X6200 -import .../x6200-108.dec.bin \
  -processor ARM:LE:32:Cortex -loader BinaryLoader -loader-baseAddr 0x08020000 \
  -preScript AddMemory.java -postScript QA390Dump.java -analysisTimeoutPerFile 2400
  -> GHIDRA_RC=0; log: logs/ghidra-analyze-108b.log
analyzeHeadless qa390-ghidra X6200 -process x6200-108.dec.bin -noanalysis -postScript QA390Scan.java
  -> RC=0; log: logs/ghidra-scan.log (pseudocódigo completo en logs/qa390-scan.txt)
... QA390Lit.java  -> logs/ghidra-lit.log  / qa390-lit.txt
... QA390Dac.java  -> logs/ghidra-dac.log  / qa390-dac.txt
... QA390Handle.java -> logs/ghidra-handle.log
```

Regiones añadidas (`AddMemory.java`): AXI_SRAM `0x24000000` (0x80000), SRAM1 `0x30000000` (0x20000),
SRAM2 `0x30020000` (0x20000), PERIPH `0x40000000` (0x20000000).

## 4. Hechos decodificados (con dirección)

### 4.1 DAC interno STM32H725 (`0x40007400`)

- El literal `0x40007400` aparece **sólo 3 veces** en la imagen descifrada: `0x08021090`, `0x0802110C`,
  `0x08021138` (todas en literal pools).
- Funciones que los consumen:
  - `FUN_08021020`: init estilo HAL; instala callback MSP `*puRam08021094 = _FUN_08021090` y llama
    `FUN_080284B0` (HAL init) dos veces. Handle DAC en RAM `0x2400584C`.
  - `FUN_08021098(n)`: si `*n == 0x40007400` ⇒ enciende bit de reloj en RCC `0x58024400+0xE8`
    (`|= 0x20000000`) y configura GPIO `0x58020000`; luego `HAL_GPIO_Init`.
  - `FUN_08021118(n)`: si `*n == 0x40007400` ⇒ **apaga** ese bit de RCC y `FUN_08029E24(...,0x30)`.
    Lo llama `FUN_08022588` (apagado global del sistema) → es el **deinit**.
- No hay literales de los registros de datos/trigger (`0x40007400+0x08/0x0C/0x20/…`) ni
  configuración DMA/trigger para DAC. El handle `0x2400584C` aparece en init/deinit y en el apagado;
  no en el pipeline DSP de RX ni en `voice_rec`.
- **Conclusión:** `MCU_DAC_ROLE = UNUSED`: DAC inicializado y desinicializado (RCC+GPIO) pero **sin
  camino de datos**; no es la fuente de audio RX.

### 4.2 I2S3 / productor de TX (`0x30000C00`)

- `0x30000C00` aparece como literal en `0x08031954`, `0x080321F8`, `0x08032518`, `0x080354B8`,
  `0x08035C88`, `0x08035D10`.
- `FUN_08031734(char half)` (literal `0x08031954`): calcula muestras desde estado DSP
  (`DAT_08031934 = 0x24024870`, `DAT_08031948 = 0x2400AE50`), aplica `FUN_080398EC`/`FUN_08025F84`/
  `FUN_08039954` y escribe 192 muestras por mitad en `0x30000C00` (offsets `+0x600` para la segunda
  mitad; palabras duplicadas L/R). Después limpia D-cache por dirección:
  `*(0xE000EF68) = addr` en pasos de 0x20 → **DCCMVAC** del Cortex‑M7.
- Lo llama el pipeline de RX `FUN_0803195c` en `0x080320E0` cuando `mode==4 || mode==5`; para el resto
  de modos el mismo pipeline genera PCM con el mismo patrón hacia `0x30000C00`
  (`DAT_08032518`/`0x080321F8`). Esto **corrige QA44**, que no había resuelto el literal.
- `FUN_08035B40` (init DMA): `FUN_0803995C(0x30000C00,0,0xC00)` y `FUN_0803995C(0x30000000,0,0xC00)`,
  y arranca DMA1 Stream5 (TX) con `PAR=SPI3+0x20`, `M0AR=0x30000C00`, `NDTR=0x600`
  (16-bit → 0xC00 B) y Stream0 (RX) con `PAR=SPI3+0x30`, `M0AR=0x30000000`, `NDTR=0x300`
  (32-bit → 0xC00 B).
- **Conclusión:** `I2S3_TX_PRODUCER = FOUND` (CPU `FUN_08031734`/pipeline `FUN_0803195c`). El
  **endpoint externo** de I2S3 sigue `UNKNOWN`.

### 4.3 Pipeline de RX y PCM

- `FUN_0803195c`: `FUN_08039250(DAT_08031c38, DAT_08031c34, n, 0x300, 0x180)` copia la ventana
  circular de RX; luego recorre 192 pares:
  `real = VectorSignedToFloat(*(int*)(DAT_08031c34 + i*8)) / 132.0`,
  `imag = VectorSignedToFloat(*(int*)(DAT_08031c34 + (2i+1)*4)) / 132.0` → **RX = I/Q 32-bit**;
  aplica DSP/FFT/filtros por modo (`FUN_08030B74`, `FUN_08032FA0`, …) y genera PCM hacia I2S3 TX.
- **Conclusión:** `RX_PCM_PRODUCER = FOUND` (el MCU demodula y produce PCM); `RX_PCM_FORMAT/RATE` queda
  `PARTIAL` (ver `FUN_0802B0E0`: escribe `SPI3+0x50` I2SCFGR y prescalers; frame/rate exactos no
  cerrados sin hoja de relojes).

### 4.4 `voice_rec` y switch BASE/DAC

`FUN_08045320(param_1)` (decompilado):

```c
if (param_1 == 0) {                       // BASE
  FUN_08044470(PTR, 1, 0x0c, 0x08);       // reg12 = 0x08
  FUN_08044470(PTR, 1, 0x18, 0x28);       // reg24 = 0x28
  FUN_080450c8(*PTR_DAT_080453b0);        // switch de ruteo BASE
  *PTR_DAT_080453b4 = 0; *PTR_DAT_080453b8 = 0;
} else {                                  // DAC
  FUN_08044470(PTR, 1, 0x0c, 0x0a);       // reg12 = 0x0A
  FUN_08044470(PTR, 1, 0x18, 0x00);       // reg24 = 0x00
  FUN_08043840(PTR, 1, 0);                // limpia bits 0x10 en reg0x34/0x36
  FUN_080439b4(PTR, 1);                   // enciende bits 0x40 en reg0x34/0x36
  FUN_080432a8(PTR, 1, byte[0xCC]);       // filtro reg0x3B
  FUN_080439b4(PTR, 2, 4);                // reg0x37/0x39
  FUN_080432a8(PTR, 2, byte[0xD0]);       // filtro reg0x3C
  *PTR_DAT_080453b4 = 1; *PTR_DAT_080453b8 = 1;
}
```

`FUN_080450c8(v)` (switch BASE, decompilado): `v` 0..5; escribe vía `FUN_080444D4`
(FPGA, `FUN_0802A634`) pares `reg:valor`:

| caso | reg0x34 | reg0x36 | reg0x37 | reg0x39 | filtros 0x3B/0x3C |
|---:|---:|---:|---:|---:|---|
| 0 | 0x10 | 0x50 | — | — | 0x3B=byte[0xCE] |
| 1 | 0x40 | 0x40 | — | — | 0x3B=byte[0xCC] |
| 2,3,4 | 0x04 | 0x40 | 0x04 | 0x40 | 0x3B=0; 0x3C=byte[0xD0] |
| 5 | — | — | — | — | `FUN_08047C30(...)` |

(**Corrección a QA30**, que tenía invertidos los casos 0 y 2.) La secuencia de registros queda
**VERIFIED**; la semántica de cada bit/efecto físico sigue **UNKNOWN** (sin netlist/readback).

### 4.5 FPGA

- Escrituras FPGA: `FUN_08044470` → `FUN_0802A634` (handler de transacción serie del objeto);
  `FUN_080444B0`/`FUN_080444D4` son wrappers de 1/2 bytes. `FUN_08043840`/`FUN_080439B4`/
  `FUN_080432A8` usan ese camino con cabecera `0x100` + `reg:valor`.
- **Bitstream FPGA:** no aparece en las imágenes de fábrica inventariadas. El MCU contiene cadenas de
  control/versión (`Init FPGA` 0x0804FD2D, `FPGA FW Version` 0x080502E7, `FPGA Flash verification
  failed` 0x080503B2, `FPGA BUS Error` 0x080502C2) pero **no** el archivo de configuración de la
  10CL025. `FPGA_ARTIFACT_FOUND = NO` (no se declara inexistencia absoluta; no está en el set).

## 5. Límites del análisis

1. Sin netlist/boardview auténtico (FCC sin esquemas; QA43/QA45): el extremo de I2S3 y el camino al
   parlante no se pueden deducir de firmware.
2. Ghidra 12.1.4 (no 12.2) y proyecto propio (no `/tmp/ghidra-proj/X6200`): diferencia registrada.
3. El análisis de Ghidra es estático; algunas zonas no se decodificaron como funciones completas
   (se fuerza creación de funciones en las direcciones de interés). Los hechos publicados citan
   literales/pseudocódigo concretos y son reproducibles con `run.sh`.
4. El formato/tasa exactos de I2S3 y la semántica de bits FPGA quedan **PARTIAL/UNKNOWN**.
