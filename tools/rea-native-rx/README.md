# REA/Ghidra — #390 (offline)

Scripts reproducibles del análisis del MCU 1.0.8 descifrado y de la GUI vendor 1.0.8. **No** se
publican binarios vendor; los insumos están fuera del repo (ver `docs/evidencia/2026-11-04-rea-native-rx/firmware-inventory.csv`).

- `run.sh`: ejecuta REA (capabilities/analyze/decompile) y Ghidra headless (import+análisis+decompilación dirigida).
- `AddMemory.java`: agrega AXI SRAM/SRAM1/SRAM2/PERIPH (reuso QA44).
- `QA390Dump.java`: xrefs a direcciones clave + callers/callees/pseudocódigo de objetivos.
- `QA390Scan.java`: barrido de constantes de periféricos, callers y decompilación dirigida.
- `QA390Lit.java`: quién referencia literales (DAC 0x40007400 / buffers 0x30000C00 / 0x30000000).
- `QA390Dac.java`: funciones DAC MSP init/deinit, wrappers FPGA (FUN_080444b0/d4) y callers.
- `QA390Handle.java`: referencias al handle DAC en RAM (0x2400584c) para clasificar su rol.

Limitación registrada: el issue esperaba Ghidra 12.2 en `/tmp/ghidra-install` y proyecto
`/tmp/ghidra-proj/X6200`; **esas rutas no existen en este host/sandbox**. Se usó Ghidra **12.1.4**
(tools/ghidra) y proyecto propio `qa390-ghidra`. REA reporta `provider ghidra 12.1.4`. En el raw del
MCU REA decompile no expone procedimientos (bloque `ram`); se documenta el error y se usa Ghidra
headless como apoyo permitido (REA sí decompiló la GUI vendor y analizó ambos binarios).
