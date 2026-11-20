# Paquete ciego C/D1/D2/bypass

- Generado por `make_blind.py`: 72 muestras (4 speakers × 2 frases × 3 condiciones: white_noise,
  adjacent_qrm, level_steps) × 4 segmentos **A/B/C/D en orden aleatorio** (seed 20261104) = 288 WAV.
- **Nivel perceptual igualado por muestra**: objetivo = mínimo RMS de los candidatos; factor aplicado
  a todos; medición post: max ΔRMS = **0.0002 dB**, mediana 0.0 (no clipping por nivel).
- `answer-key.csv` separada; `listening-results-template.csv` con comprensión/claridad/naturalidad/
  artefactos/esfuerzo/global/no-diferencia y notas. Sin ganador forzado.
- **No ejecutado con operador** en esta tarea ⇒ `AUDIBLE_PENDING`. Instrucciones: escuchar cada set
  A–D en el mismo parlante/volumen, marcar sin ver la key; no se entrega winner automático.

- **Corrección 2026-11-04:** las salidas C/D1/D2 tienen 882 muestras (20 ms) de latencia; el paquete se regeneró alineando C/D1/D2 antes de nivelar. Ver `stoi-and-rerun-corrections.md`.
