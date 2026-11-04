# QA-NATIVE-DSP — scripts reproducibles (#390)

- `QA19Scan.java`: strings de handlers DSP del MCU, máscaras SDK y decompilación dirigida.
- `QA19Fw.java`: callers de wrappers FPGA (`FUN_080444B0/D4/70`).
- `QA19H2.java`: decompilación de handlers candidatos (starts inferidos).
- `QA19Handlers.java`: heurística de prólogo (no resolvió; se conserva por trazabilidad).
- `QA19Region.java`: intento de desensamblado/decompilación de la región de handlers (limitación registrada).
- `tests/test_register_mapping.py`: 9 tests offline del mapeo GUI→X6200Control→registro (VERIFIED).
- `run.sh`: reproduce REA + Ghidra (usa `tools/rea.sh` y el proyecto `qa390-ghidra`).

Limitaciones: el pseudocódigo completo del apply MCU→FPGA/DSP y el bitstream FPGA no están disponibles;
los resultados publicados se limitan a lo citado en `docs/evidencia/2026-11-04-native-dsp-filter-path/`.
Ghidra 12.2 esperado en `/tmp` no existe en este sandbox; se usa 12.1.4 (tools/rea.sh) y se registra.
