#!/usr/bin/env bash
# QA390 (#390) — reproducción del análisis REA/Ghidra del MCU 1.0.8 (offline)
# Uso: ./run.sh   (desde el worktree x6200-rea-native-rx)
# Requisitos: node (rea en refs/rea), JDK 21, Ghidra 12.1.4 en tools/ghidra.
# NOTA: el issue esperaba Ghidra 12.2 en /tmp; en este host/sandbox esa ruta no existe.
#       Los resultados publicados se generaron con Ghidra 12.1.4 (REA lo reporta como provider 12.1.4).
set -u
ROOT=$(cd "$(dirname "$0")/../../.." && pwd)   # /home/pablo/desa/x6200
FW="$ROOT/fw108-scratch/x6200-base-rea/x6200-108.dec.bin"
VGUI="$ROOT/fw108-scratch/ext108/fs108/x6200_ui_v100"
OUT="$ROOT/.qa390/logs"; PROJ="$ROOT/qa390-ghidra"; SCRIPTS="$ROOT/x6200-rea-native-rx/tools/rea-native-rx"
mkdir -p "$OUT"
export HOME="$ROOT/.qa390/home"
export JAVA_HOME="$ROOT/tools/jdk/jdk-21.0.13+9"
export PATH="$JAVA_HOME/bin:$PATH"
export JAVA_TOOL_OPTIONS="-Duser.home=$HOME"
export GHIDRA_INSTALL_DIR="$ROOT/tools/ghidra/ghidra_12.1.4_PUBLIC"
REA="node $ROOT/refs/rea/scripts/rea.mjs"

echo "== REA capabilities/doctor =="
$REA capabilities > "$OUT/rea-capabilities.txt" 2>&1; echo "capabilities rc=$?"
echo "== REA analyze MCU 1.0.8 (raw-binary ARM, entry 0x08020000) =="
$REA analyze "$FW" --target-format raw-binary --architecture arm --entry-point 0x08020000 --provider ghidra --format json > "$OUT/rea-analyze-108.json" 2>"$OUT/rea-analyze-108.err"; echo "analyze MCU rc=$?"
echo "== REA analyze GUI vendor 1.0.8 (ELF ARM) =="
$REA analyze "$VGUI" --provider ghidra --format json > "$OUT/rea-analyze-vgui.json" 2>"$OUT/rea-analyze-vgui.err"; echo "analyze vgui rc=$?"
echo "== REA decompile GUI vendor (AudioWorker::udpDataReceived @0xd9d40) =="
$REA decompile "$VGUI" 0xd9d40 --provider ghidra --format json > "$OUT/rea-vgui-decomp1.json" 2>"$OUT/rea-vgui-decomp1.err"; echo "decompile vgui rc=$?"
echo "== REA decompile del raw del MCU (limitación conocida: bloque 'ram' sin procedimientos) =="
$REA decompile "$FW" --file-offset 0x4881 --target-format raw-binary --architecture arm --entry-point 0x08020000 --provider ghidra --format json > "$OUT/rea-decomp-test.json" 2>&1; echo "decompile MCU rc=$? (esperado 1; ver rea-ghidra-log.md)"
echo "== Ghidra headless: import+análisis+memoria+decompilación dirigida =="
"$GHIDRA_INSTALL_DIR/support/analyzeHeadless" "$PROJ" X6200 \
  -import "$FW" -processor 'ARM:LE:32:Cortex' -loader BinaryLoader -loader-baseAddr 0x08020000 \
  -scriptPath "$SCRIPTS" -preScript AddMemory.java -postScript QA390Dump.java \
  -analysisTimeoutPerFile 2400 > "$OUT/ghidra-analyze-108b.log" 2>&1; echo "ghidra import rc=$?"
echo "== Ghidra barridos dirigidos sobre el proyecto existente =="
"$GHIDRA_INSTALL_DIR/support/analyzeHeadless" "$PROJ" X6200 -process x6200-108.dec.bin -noanalysis \
  -scriptPath "$SCRIPTS" -postScript QA390Scan.java > "$OUT/ghidra-scan.log" 2>&1; echo "scan rc=$?"
"$GHIDRA_INSTALL_DIR/support/analyzeHeadless" "$PROJ" X6200 -process x6200-108.dec.bin -noanalysis \
  -scriptPath "$SCRIPTS" -postScript QA390Lit.java > "$OUT/ghidra-lit.log" 2>&1; echo "lit rc=$?"
"$GHIDRA_INSTALL_DIR/support/analyzeHeadless" "$PROJ" X6200 -process x6200-108.dec.bin -noanalysis \
  -scriptPath "$SCRIPTS" -postScript QA390Dac.java > "$OUT/ghidra-dac.log" 2>&1; echo "dac rc=$?"
"$GHIDRA_INSTALL_DIR/support/analyzeHeadless" "$PROJ" X6200 -process x6200-108.dec.bin -noanalysis \
  -scriptPath "$SCRIPTS" -postScript QA390Handle.java > "$OUT/ghidra-handle.log" 2>&1; echo "handle rc=$?"
echo "Logs en $OUT ; project en $PROJ"
