#!/usr/bin/env bash
# Reproduce QA-NATIVE-DSP (#390). Offline; no toca radio.
set -u
ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
FW="$ROOT/fw108-scratch/x6200-base-rea/x6200-108.dec.bin"
VGUI="$ROOT/fw108-scratch/ext108/fs108/x6200_ui_v100"
OUT="$ROOT/.qa390/logs"; PROJ="$ROOT/qa390-ghidra"; SCRIPTS="$ROOT/x6200-native-dsp-filter-path/tools/native-dsp-rea"
mkdir -p "$OUT"
export HOME="$ROOT/.qa390/home"
export JAVA_HOME="$ROOT/tools/jdk/jdk-21.0.13+9"
export PATH="$JAVA_HOME/bin:$PATH"
export JAVA_TOOL_OPTIONS="-Duser.home=$HOME"
export GHIDRA_INSTALL_DIR="$ROOT/tools/ghidra/ghidra_12.1.4_PUBLIC"

echo "== REA =="
"$ROOT/tools/rea.sh" capabilities > "$OUT/qa19-rea-capabilities.txt" 2>&1
"$ROOT/tools/rea.sh" analyze "$FW" --target-format raw-binary --architecture arm \
  --entry-point 0x08020000 --provider ghidra --format json > "$OUT/qa19-rea-analyze-108.json" 2>&1
"$ROOT/tools/rea.sh" analyze "$VGUI" --provider ghidra --format json > "$OUT/qa19-rea-analyze-vgui.json" 2>&1
"$ROOT/tools/rea.sh" decompile "$VGUI" 0xe5590 --provider ghidra --format json > "$OUT/qa19-rea-decomp-updateRxFilters.json" 2>&1

echo "== Ghidra headless (12.1.4; /tmp 12.2 no existe en este sandbox) =="
H="$GHIDRA_INSTALL_DIR/support/analyzeHeadless"
"$H" "$PROJ" X6200 -process x6200-108.dec.bin -noanalysis -scriptPath "$SCRIPTS" -postScript QA19Scan.java > "$OUT/qa19-scan.log" 2>&1
"$H" "$PROJ" X6200 -process x6200-108.dec.bin -noanalysis -scriptPath "$SCRIPTS" -postScript QA19Fw.java > "$OUT/qa19-fw.log" 2>&1
"$H" "$PROJ" X6200 -process x6200-108.dec.bin -noanalysis -scriptPath "$SCRIPTS" -postScript QA19H2.java > "$OUT/qa19-h2.log" 2>&1
"$H" "$PROJ" X6200 -process x6200-108.dec.bin -noanalysis -scriptPath "$SCRIPTS" -postScript QA19Handlers.java > "$OUT/qa19-handlers.log" 2>&1
"$H" "$PROJ" X6200 -process x6200-108.dec.bin -noanalysis -scriptPath "$SCRIPTS" -postScript QA19Region.java > "$OUT/qa19-region.log" 2>&1

echo "== tests =="
python3 "$SCRIPTS/tests/test_register_mapping.py" | tee "$OUT/qa19-tests.txt"
