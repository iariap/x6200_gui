# Resultados offline (QEMU ARM, sintéticos) y decisión DSP

Promedios sobre 9 fixtures (detalle por fixture en `bench-results.json`):
| modo | SNR desc. | env corr | brillo | pumping | CPU RTF QEMU | clip máx |
|---|---|---|---|---|---|---|
| A BASE | 85.83 | 0.778 | 0.496 | 0.512 | ~0.00001 | 100% (fixture max-level) |
| B Linux bypass | 85.83 | 0.778 | 0.496 | 0.512 | ~0 | 100% (max-level) |
| C actual SSB | -20.81 | 0.383 | 0.380 | 0.863 | 0.0468 | 0% |
| D1 conservador | -20.79 | 0.390 | 0.338 | 0.955 | 0.0470 | 0% |
| D2 NR55 | -20.79 | 0.383 | 0.369 | 0.887 | 0.0459 | 0% |

Integridad: misma longitud (52920), partición invariante (`partition_mismatch_samples=0`) en SSB Voice.
`max-level` es un fixture de saturación intencional; A/B clip 100% es esperado de la entrada, C/D no
clipan (NR/atenuación). CPU RTF QEMU ~4.6% por modo; no es medida de la radio.

## Lectura honesta
- Los objetivos SNR/env no son STOI/ESTOI y **no prueban mejor inteligibilidad**. En sintéticos, C/D
  difieren poco entre sí; D1 baja brillo/pumping distinto pero no hay validación perceptual ni de voz
  real ⇒ **no hay candidato justificado para cambiar el DSP por defecto**.
- **Recomendación:** mantener DSP actual (SSB Voice) y no tocar defaults/presets; D1/D2 quedan como
  candidatos **sólo para escucha autorizada**, no implementados.
- Sin grabación de voz/escucha no se declara mejora; `AUDIBLE_PENDING`, `PRODUCTION_BLOCKED`.
