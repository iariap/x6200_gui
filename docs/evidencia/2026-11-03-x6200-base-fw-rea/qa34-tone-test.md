# QA34 — Prueba de tono identificable por DAC

Fecha: 2026-11-03. H1 instalado. Sin nuevo flash.

## Procedimiento ejecutado

1. Snapshot de estado/params/ALSA.
2. GUI, daemon y PulseAudio detenidos.
3. DAC seleccionado con helper `libaether_x6200_control` (`record_set(1)`).
4. Patrón de audio por `hw:0,0`, s16le/48 kHz/2ch:
   - 0,6 s 1 kHz
   - 0,4 s silencio
   - 0,6 s 1 kHz
   - 0,4 s silencio
   - 0,6 s 1 kHz
   - 1,0 s silencio
5. `frames_total=172800`, `drain=0 Success`.
6. Retorno explícito a BASE; GUI/PulseAudio reiniciados; estado/ALSA restaurados.

## Resultado técnico

- PCM aceptado y transferencia completa.
- DAC/MUX seleccionado por `record_set(1)` (sin readback).
- Patrón de tono reproducido por ALSA.
- **Audibilidad del tono: pendiente de reporte del propietario.**
- No se puede declarar `LINUX_DSP_SPEAKER=VERIFIED` sin ese reporte.

## Interpretación

- Si el propietario oyó los tres beeps: evidencia parcial/fuerte de ruta Linux→DAC→parlante.
- Si sólo oyó el QSO nativo: el audio audible era la ruta nativa del BASE, no Linux.
- Si no oyó nada: la ruta DAC/MUX o el volumen no entrega el PCM al parlante.

Estado: ALSA diff vacío; GUI original; RX; sin TX/PTT/ATU/Bluetooth; sin nuevo flash.

## Observación del propietario posterior

“Hay mucho QRM, 9 de ruido.”

- El patrón se reprodujo y ALSA confirmó `frames_total=172800`, `drain=0`.
- La **audibilidad del patrón queda INCONCLUSA** por QRM S9; no se puede atribuir la
  falta de identificación al DAC ni al DSP.
- No se repitió la prueba ni se cambió volumen/frecuencia del operador.
- Gates sin cambios: `LINUX_DSP_SPEAKER=NOT_VERIFIED`; `TONES_DELIVERED=YES`;
  `AUDIBILITY_INCONCLUSIVE_QRM`.
