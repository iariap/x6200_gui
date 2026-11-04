# Plan mínimo de implementación (arquitectura A)

1. **GUI/UX (sin firmware):** completar los controles que el SDK ya expone para filtros nativos:
   - DFL edges ya presentes (`cfg_cur.filter.low/high`, `dfl_*`);
   - NR: enable + nivel; NB: enable + nivel + ancho; DNF: modo + centro + ancho;
   - AGC: modo por VFO + knee/slope/hang/tiempo (hoy knee/slope/hang ya cableados; tiempo no expuesto);
   - RX EQ: enable + 5 bandas; RX EQ WFM: enable + 5 bandas; MIC EQ fuera del alcance RX.
   Todos bajo `WITH_RADIO_LOCK` y con los setters existentes (`x6200_control_*`), sin tocar `reg12/24/0x34`.
2. **Tests offline:** `tools/native-dsp-rea/tests/test_register_mapping.py` (ya verde) + casos nuevos al
   agregar UI (rangos/step/labels verificados contra `src/cfg`).
3. **Documentación:** este mapeo y los límites; aclarar que "Capture filters" (Speex/notch/AF leveler)
   procesa la copia de captura y **no** la salida audible.
4. **Validación física autorizada (PENDING):** A/B auditivo y captura externa con el owner; registrar
   resultados y restaurar defaults. Sin USB/BT/audio externo/cambios físicos.
5. **No hacer:** parche de firmware MCU, DSP Linux→parlante, MUX/reg12/reg24/0x34, USB/BT.

Criterio de éxito de A: los filtros nativos se controlan desde el GUI y el owner confirma el efecto
audible sin perder RX ni parlante. Si A no alcanzara, el siguiente paso requeriría bitstream FPGA y
netlist (hoy ausentes), no B/C a ciegas.
