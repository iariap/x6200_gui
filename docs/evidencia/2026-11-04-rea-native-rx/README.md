# #390 — Ruta RX nativa, DAC STM32 y audio FPGA (REA/Ghidra sobre firmware de fábrica)

**Issue:** [#390](https://github.com/iariap/x6200_gui/issues/390) · **Épica:** [#362](https://github.com/iariap/x6200_gui/issues/362) · **Antecedentes:** [#385](https://github.com/iariap/x6200_gui/pull/385), [#386](https://github.com/iariap/x6200_gui/pull/386), [#388](https://github.com/iariap/x6200_gui/pull/388), [#389](https://github.com/iariap/x6200_gui/pull/389).
**Tipo:** investigación **offline** (sin radio, sin flash, sin escrituras). No incluye firmware ni binarios vendor.
**Worktree:** `x6200-rea-native-rx` · **Rama:** `research/x6200-rea-native-rx`.
**Base real verificada:** `681074e0adac5a79f0392b4e51284167abf5e342` (`fork/main` = HEAD del remoto de PR; `git ls-remote fork` lo confirma). `origin/main` (espejo `gdyuldin`) = `e1476ece…`, distinto; **no se usó ni modificó**. `main` local estaba añejo (`6a29687d`); no se tocó.

## Veredicto: **NO-GO** — Gates A y B **FAIL** (sin harness)

| Gate | Valor | Evidencia |
|---|---|---|
| `FACTORY_IMAGES_INVENTORIED` | **YES** | `firmware-inventory.md`: imagen de fábrica eMMC `parts/1.img` (SHA256 `3c475b16…`), imagen 1.0.8 `sdcard.img`/`rootfs.tar`/`boot.vfat`, MCU `.xgf` 1.0.6/1.0.8 + descifrados, GUI vendor 1.0.6/1.0.8. Rutas reales, hashes calculados. |
| `FPGA_ARTIFACT_FOUND` | **NO** | Búsqueda por nombre (`.rbf/.sof/.pof/bitstream/fpga`) y por contenido en las imágenes de fábrica/rootfs/eMMC: **no hay bitstream separado**. El MCU 1.0.8 contiene cadenas de control/versión de FPGA (`Init FPGA`, `FPGA FW Version`, `FPGA Flash verification failed`) pero **no** un archivo de configuración para la 10CL025. El artefacto no está en el set provisto. |
| `MCU_DAC_ROLE` | **UNUSED** | El literal de `DAC` `0x40007400` aparece **sólo** en 3 palabras de literal pool (0x08021090/0x0802110C/0x08021138), consumidas por init/deinit estilo HAL MSP (RCC+GPIO, base de instancia) y por `FUN_08021118` (deinit) llamado desde el apagado global `FUN_08022588`. No hay escrituras a registros de datos/trigger/DMA del DAC ni callers de audio. **No es la fuente de audio RX.** |
| `I2S3_TX_PRODUCER` | **FOUND** (corrige QA44) | `FUN_08031734(char half)` escribe **PCM de CPU** en `0x30000C00` (literal del buffer en 0x08031954), dos mitades (`+0x600`), y limpia D-cache por dirección con `DCCMVAC` (`0xE000EF68`). Lo llama **`FUN_0803195c`** (pipeline DSP de RX) en 0x080320E0. QA44 lo daba “UNRESOLVED” porque el literal no estaba resuelto. |
| `RX_PCM_PRODUCER` | **FOUND (PARTIAL en formato/tasa)** | En el mismo pipeline: `FUN_0803195c` copia la ventana RX `0x30000000` con `FUN_08039250`, convierte pares **I/Q int32→float** (`real=+i*8`, `imag=+(2i+1)*4`), aplica el DSP por modo y **genera muestras PCM** (float→int, `FUN_08025f84`/`FUN_08039954`) hacia el buffer TX: rama `mode==4/5` vía `FUN_08031734`, resto con el mismo patrón en `DAT_08032518`/`0x080321F8`. Formato/tasa exactos del I2S3 aún **PARTIAL** (I2SCFGR en `FUN_0802b0e0`; sin decodificar frame/rate contra relojes). |
| `RX_AUDIO_OUTPUT_CONSUMER` | **UNKNOWN** | El consumidor externo del I2S3 TX (PA15/PC10/PC11/PC12 AF6) no está evidenciado por firmware ni por documentación; sin netlist/boardview (QA43/QA45). La ruta hasta el **parlante interno** no se puede cerrar: “endpoint → codec/FPGA → parlante” sigue **UNKNOWN**. |
| `BASE_DAC_SWITCH_SEMANTICS` | **PARTIAL (secuencia de registros VERIFIED; semántica de bits UNKNOWN)** | `voice_rec` `FUN_08045320`: BASE → `reg12=0x08`, `reg24=0x28`, `FUN_080450C8(switch)`; DAC → `reg12=0x0A`, `reg24=0x00`, y secuencia `FUN_08043840`/`FUN_080439B4`/`FUN_080432A8`. `FUN_080450C8` es un **switch de 0..5** que escribe vía FPGA (`FUN_080444D4`) `reg0x34/0x36/0x37/0x39` y filtros `0x3B/0x3C`. **Corrección a QA30:** en el código real el caso 0 escribe `0x34=0x10,0x36=0x50` y los casos 2/3/4 escriben `0x34=4,0x36=0x40,0x37=4,0x39=0x40` (QA30 los tenía invertidos). El efecto físico de cada bit no se puede fundar sin extremo/FPGA. |
| `AUDIO_PCM_AND_SPEAKER_LINK_VERIFIED` | **NO** | No hay un productor PCM **capturable por A33 Linux** ni un consumidor hacia el parlante verificado. El PCM RX existe **solo dentro del MCU** → I2S3 TX, cuyo extremo externo es UNKNOWN. Gate A **FAIL**. |
| `SAFE_INTERCEPTION` | **NO** | No hay punto de inserción accesible, fail-safe bit-perfect, ni presupuestos DMA/cache/clock/CPU definidos. El único punto de intervención sería parchear firmware del MCU (flash, sin recuperación verificada) o interceptar I2S3 físicamente (cambio físico). Gate B **FAIL**. |
| `NEXT_EXECUTABLE_STEP` | **1 paso concreto** | Obtener evidencia del **extremo de I2S3 TX**: esquema/boardview auténtico o traza pasiva (analizador lógico) de PA15/PC10/PC11/PC12 y su conexión al parlante, con el switch BASE/DAC conmutado; en paralelo, decodificar `I2SCFGR` (formato/frame/rate) de `FUN_0802b0e0`. Sin eso Gate A no puede pasar y no hay harness. |

**Gate A = FAIL** (falta consumidor externo/parlante y formato exacto). **Gate B = FAIL** (no hay
interception segura ni presupuestos). Por lo tanto **NO se implementa harness offline** (ni
passthrough bit-exacto ni filtro); ver `report` de gates en este README y `route-matrix.csv`.

## Hallazgos nuevos (correcciones a QA37–QA45)

1. **Productor de TX de I2S3 encontrado (QA44 lo daba UNRESOLVED):** `FUN_08031734`, llamado por el
   pipeline de RX `FUN_0803195c`; escribe PCM en `0x30000C00` y limpia D-cache (`DCCMVAC`).
2. **`0xE000EF68` no es FPGA/bus externo (corrección a QA37/QA44):** es `DCCMVAC`, registro de
   *clean data cache by address* del Cortex-M7; los bucles que escriben esa dirección son
   mantenimiento de caché sobre los buffers DMA, no un bus a FPGA.
3. **Secuencia BASE/DAC en `FUN_080450C8` corregida:** casos 0..5 reales, con `reg0x34/0x36/0x37/0x39`
   y filtros `0x3B/0x3C`; QA30 tenía invertidos los casos 0 y 2.
4. **DAC del STM32H725 (0x40007400) no es de audio:** sólo init/deinit HAL MSP (RCC+GPIO) y el deinit
   del apagado; sin datos/trigger/DMA ⇒ `MCU_DAC_ROLE = UNUSED`.
5. **Ruta RX→PCM en el MCU:** RX I/Q 32-bit en `0x30000000` → `FUN_08039250` → float I/Q → DSP por
   modo → **PCM** en `0x30000C00` → DMA I2S3 TX. El eslabón que falta es **externo** (endpoint/parlante).

## Cómo se generó (REA + Ghidra, real)

Ver `rea-ghidra-log.md` y `logs/`. Resumen:

- **REA** (obligatorio) ejecutado de verdad: `rea@6.7.0` → `capabilities`, `doctor`, `analyze` del
  MCU 1.0.8 descifrado (raw-binary ARM, entry 0x08020000) y de la GUI vendor 1.0.8 (ELF ARM),
  `inspect-native-load-image`, y `decompile --provider ghidra` (funciona en ELF; en el raw del MCU el
  bridge marca el bloque como `ram` y no expone procedimientos → se documenta el error exacto y se usa
  Ghidra headless como **apoyo permitido**, no como sustituto).
- **Ghidra 12.2 no disponible en este sandbox**: `/tmp/ghidra-install/ghidra_12.2_PUBLIC` y
  `/tmp/ghidra-proj/X6200` **no existen** en la sesión de ejecución (se verificó; `/tmp` vacío). Se usó
  **Ghidra 12.1.4 PUBLIC real** (`tools/ghidra/ghidra_12.1.4_PUBLIC`, JDK 21.0.13) con proyecto propio;
  REA reporta `provider ghidra 12.1.4`. Diferencia de versión registrada como limitación, no silenciada.
- Scripts reproducibles en `tools/rea-native-rx/` (`run.sh`, `AddMemory.java`, `QA390Dump.java`,
  `QA390Scan.java`, `QA390Lit.java`, `QA390Dac.java`, `QA390Handle.java`), logs crudos en `logs/`.

## Firmas / hashes verificados

| Artefacto | Ruta real | SHA256 |
|---|---|---|
| Imagen de fábrica eMMC (ext4, 400 MB) | `fw108-scratch/ext108/parts/1.img` | `3c475b16d48acb23f727cbc3d6d3b62f103bbc26bd3ce7f2e8f1b3be27ff4764` |
| Imagen SD 1.0.8 (compat) | `fw108-scratch/fw108-image-377/sdcard.img` | `7ec5d8cc4cb6cbf98fc92af9cf6489ad1a94f5dce8585d43c93156afde68b5d9` |
| Rootfs 1.0.8 | `fw108-scratch/fw108-image-377/rootfs.tar` | `a2eb1d6582e766a38df0ea7db9b49e958063713d6dd10d7cb96d0621215d171e` |
| Boot 1.0.8 | `fw108-scratch/fw108-image-377/boot.vfat` | `1960b896e510dfe8d619a14c207ad86209a2e31bbe310ed9a26c4ba136d5b99e` |
| GUI vendor 1.0.8 | `fw108-scratch/ext108/fs108/x6200_ui_v100` | `26623266bfcd3ed4d2e08b03b9f023cef809579f5ed497899670fc01f66e9b2b` |
| MCU 1.0.8 `.xgf` | `fw108-scratch/ext108/fs108/x6200_mcu_20260820141208.xgf` | `9a19f97a5ad9e141a3663d222f984073bf0b18b38d4662604372a0036f9daa23` |
| MCU 1.0.8 descifrado | `fw108-scratch/x6200-base-rea/x6200-108.dec.bin` | `c3f7d028bc63e43fbb478a09e1032d46059b71efe217a60b576231866fbc2c0a` |
| MCU 1.0.6 `.xgf` | `fw108-scratch/ref106/fs106/x6200_mcu_20250111134652.xgf` | `dca90387b70d9550448006310a69ea65625bec87f402c10df789deeb13ff12f3` |
| MCU 1.0.6 descifrado | `fw108-scratch/x6200-base-rea/x6200-106.dec.bin` | `1b7606b657a89d011682ea6f41a84a910d76ab5fe5526f82f5dc54d86cc18685` |
| GUI vendor 1.0.6 | `refs/vendor-fw/x6200_ui_v100` | `b76331c5aa859ae98dc1819b1079a3e40489e9986c9656e0dc2c553aeb28871b` |

## Por qué no se puede insertar DSP todavía (exacto)

El PCM de RX **existe en el MCU** pero su consumidor es el **I2S3 TX**, y el extremo físico de ese bus
(PA15/PC10/PC11/PC12 AF6) **no está identificado** (sin netlist; QA43/QA45). A33 Linux sólo ve RX
analógico por Mic1 (`src/audio.c::capture_deliver` no entrega al speaker; QA11b/14/15/29–36). Por lo
tanto:

- no hay **productor PCM accesible desde Linux** → no se puede procesar en A33;
- no hay **consumidor/parlante verificado** del PCM del MCU → no hay garantía de que un DSP en el MCU
  conserve parlante;
- parchear el MCU exige flash (sin recuperación SWD/ISP verificada; QA28–32) y un bypass fail-safe que
  no existe;
- interceptar I2S3 sería un cambio físico, prohibido por el issue.

**Faltan exactamente:** (1) identificación del endpoint I2S3/parlante (esquema o traza), (2) formato y
tasa exactos de I2S3, (3) artefacto bitstream FPGA si se pretende intervenir el FPGA, (4) contrato de
aceptación/recuperación si se parchea MCU. Con eso se reevalúa Gate A; recién entonces Gate B.

## Estado

Radio **intacta**: no se conectó ni modificó (no flash/IAP/XMODEM/GPIO/registros/MUX/ALSA/GUI/ajustes).
Firmware oficial 1.0.8 y configuración del operador sin cambios. Sin merge; sin issues nuevas.
