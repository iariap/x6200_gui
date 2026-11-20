# REA + Ghidra — comandos, versiones, evidencia y errores (#QA-NATIVE-DSP)

Offline; sin radio/flash/escrituras. 2026-11-04.

## 1. Herramientas reales

| Herramienta | Versión/ruta | Estado |
|---|---|---|
| REA | `rea@6.7.0` · `/home/pablo/desa/x6200/tools/rea.sh` (wrapper: node 22.23.4, Ghidra 12.1.4, JDK 21) | ejecutado |
| Ghidra provider | **12.1.4** (`provider.version=12.1.4`) | disponible |
| Ghidra headless | `tools/ghidra/ghidra_12.1.4_PUBLIC/support/analyzeHeadless` + JDK 21.0.13 | ejecutado |
| Proyecto Ghidra | `/home/pablo/desa/x6200/qa390-ghidra` (X6200, MCU 1.0.8 de QA390) | reutilizado |

**Desviación registrada:** el issue indicaba Ghidra **12.2** en `/tmp/ghidra-install/ghidra_12.2_PUBLIC`
y proyecto `/tmp/ghidra-proj/X6200`. En este host/sandbox **esas rutas no existen** (`/tmp` vacío;
verificado). `tools/rea.sh` fija Ghidra **12.1.4**; se usó ése (real) y se registra la diferencia.
No se inventaron resultados de 12.2.

## 2. REA — comandos y evidencia

```
tools/rea.sh --version                        -> 6.7.0
tools/rea.sh capabilities                     -> exit 0; logs/qa19-rea-capabilities.txt
tools/rea.sh analyze fw108-scratch/x6200-base-rea/x6200-108.dec.bin \
  --target-format raw-binary --architecture arm --entry-point 0x08020000 --provider ghidra --format json
  -> exit 0; evidence_id ev_86dc1b46449e4e1e5e656a57a767e4bfa1f108e3d535afd5ada323a64ec649f8
     (MCU 1.0.8, SHA256 c3f7d028…), logs/qa19-rea-analyze-108.json
tools/rea.sh analyze fw108-scratch/ext108/fs108/x6200_ui_v100 --provider ghidra --format json
  -> exit 0; evidence_id ev_659f9cf1fa0e984347972a6d7347fff5bc0317d92f65352da1c83736f029e600
     (GUI vendor 1.0.8), logs/qa19-rea-analyze-vgui.json
tools/rea.sh decompile fw108-scratch/ext108/fs108/x6200_ui_v100 0xe5590 --provider ghidra --format json
  -> exit 0; evidence_id ev_168cf99fab1bc8d0f00b1dc5b9172f666c332d2aab587752d60061a3768c9737
     (BaseCommIIC::updateRxFilters(short,short,bool) @0xE5590), logs/qa19-rea-decomp-updateRxFilters.json
```

El decompile del **raw del MCU** por REA ya se documentó como no aplicable en QA390 (bloque `ram` sin
procedimientos; error exacto `File offset 0x… has no loaded source mapping / no procedure starts…`).
Para pseudocódigo del MCU se usó Ghidra headless con scripts QA19 (apoyo permitido), no escaneos
literales como sustituto: REA analizó ambos binarios y decompiló la GUI vendor.

## 3. Ghidra — scripts y resultados

Entrada MCU: `x6200-108.dec.bin` SHA256 `c3f7d028…`; ARM:LE:32:Cortex, base `0x08020000`; proyecto QA390.

```
analyzeHeadless qa390-ghidra X6200 -process x6200-108.dec.bin -noanalysis \
  -scriptPath qa19-scripts -postScript QA19Scan.java      -> RC=0; logs/qa19-scan.log
... QA19Fw.java      -> RC=0; callers de wrappers FPGA (FUN_080444B0/D4/70)
... QA19H2.java      -> RC=0; 18 handlers candidatos, 6 pseudocódigos; logs/qa19-h2.log
... QA19Handlers.java-> RC=0; heurística de prologo falló (0 starts); logs/qa19-handlers.log
... QA19Region.java  -> RC=1 (constructor DisassembleCommand corregido; segunda corrida RC=0 sin starts por
                        bloque no ejecutable en la importación raw); logs/qa19-region.log
```

Hallazgos del lado MCU:
- Strings de handler con dirección (`logs/qa19-scan-extract.txt`):
  `update rx filter, hpf/lpf` `0x08050DFB`; `update notch filter` `0x080505CB`; `update dnr threshold`
  `0x080508B4`; `update dnr type` `0x080508D0`; `update nb enable` `0x080508E8`; `update nb threshold`
  `0x08050900`; `update rxeq_en/p1..p5` `0x08050C60/0x08050BE4..0x08050C44`; `rxeqwfm_*`
  `0x08050D04/0x08050C78..`; `miceq_*` `0x08050D98/0x08050D20..`; AGC `0x08050E5C`/`0x08050814`.
- Los literales de log están en pools `0x0804213C`, `0x08042418–24`, `0x08042E80`, `0x08043200`; el
  código cercano (región 0x08042000–0x08042F00) es el de actualización de estado de audio (p. ej.
  `FUN_08042CF6` actualiza campos de un struct, setea dirty flag `unaff_r7+0x1F` y loguea; no escribe
  directamente el registro de hardware). El **apply** posterior al FPGA/DSP no se resolvió
  estáticamente (limitación declarada).
- Identidad índice→comando FPGA: QA390 (`voice_rec FUN_08045320`) escribe reg12→cmd `0x0C` y
  reg24→cmd `0x18`, dando base para mapear los índices de filtros a registros FPGA del mismo id
  (INFERRED para el resto de índices; VERIFIED para 12/24).
- No se repitió el escaneo genérico de QA390: se partió de su cadena y de la API real del SDK.

## 4. Límites

1. Sin bitstream FPGA en el set (QA390 `FPGA_ARTIFACT_FOUND=NO`) ⇒ algoritmo exacto de los filtros no
   reconstruible; no se presenta aproximación como bit-exact.
2. Aplicación estado→FPGA/DSP no identificada con Ghidra 12.1.4 ⇒ implementaciones PARTIAL/UNKNOWN.
3. Efecto audible y respuesta: requieren radio/owner (PENDING).
4. Diferencia Ghidra 12.2 (esperado) vs 12.1.4 (usado): registrada.
