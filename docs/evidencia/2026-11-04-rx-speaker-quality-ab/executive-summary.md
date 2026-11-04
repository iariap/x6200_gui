# QA-SPEAKER-QUALITY — resumen ejecutivo

**Issue/épica:** #390 · #362 · **PRs preservados:** #392 `697cea97`, #391 `de35affa`, #389 `c0299e0b`, #388 `7cb97cf6`, #386 `84777dff`, #385 `68c501a8`.
**Base real:** `681074e0adac5a79f0392b4e51284167abf5e342` (`fork/main`); `origin/main`=`e1476ece` distinto, no usado.
**Alcance:** investigación **offline**; sin radio/firmware/GPIO/MUX/ALSA/GUI/ajustes/TX. Sin implementación de producción.
**Clases de evidencia usadas:** `CODE` (árbol 681074e0), `MEASURED-HOST` (bancos previos #345 y esta corrida offline), `MEASURED-HARDWARE` (registros previos de la radio, citados), `AUDIBLE` (prueba física 2026-11-02 confirmada por el operador), `MODEL` (modelo propio offline), `PENDING_PHYSICAL_VALIDATION` (no ejecutado en esta tarea).

## Reconciliación central: ruta audible ≠ ruta segura ≠ calidad acústica ≠ madurez

| Pregunta | Respuesta | Clase |
|---|---|---|
| ¿Se demostró **audiblemente** que PCM Linux procesado puede salir por el parlante interno? | **SÍ**, una prueba física corta: `SPEAKER_AUDIO_HEARD=YES` y `PRESET_DIFFERENCE_HEARD=YES` (operador, 2026-11-02), con `writes=18 drops=0 underruns=0` y retorno a BASE. | `AUDIBLE` (evidencia previa, no repetida acá) |
| ¿Esa es una **ruta permanente/segura**? | **NO**. Requirió un router experimental **no presente en la base 681074e0** (`src/rx_audio_live.*` ausente) y escritura del MUX/`voice_rec`; QA363 la dejó `BLOCKED_SAFE_MUX` (sin readback, sin dueño serializado fuera de la GUI, restore sólo reiniciando la GUI). | `CODE` + `MEASURED-HARDWARE` (QA363) |
| ¿QA390/QA-NATIVE-DSP contradicen el resultado audible? | **No**: esos trabajos concluyeron NO-GO para una ruta **segura/permanente y sin firmware** y para reutilizar/extensionar DSP sin evidencia de endpoint. La prueba de 2026-11-02 demuestra **posibilidad audible experimental**, no producción. | Reconciliación explícita |
| ¿Calidad acústica comparada? | **PENDING_PHYSICAL_VALIDATION**: no hay A/B ciego, ni grabación de voz con licencia, ni instrumentación. `writes/drops/underruns` y el preset oído no son una métrica de calidad. | `PENDING` |
| ¿Madurez de implementación? | Router Linux: **EXPERIMENTAL** (no en base, sin restore ante crash, sin interlocks). DSP nativo: **producción del fabricante** (control por API existente). | `CODE` |

**Corolario:** la evidencia audible previa **no** habilita a declarar "Linux DSP imposible"; lo que está
bloqueado es la **vía segura/permanente**. El DSP nativo es la vía audible de producción hoy.

## Recomendación mínima (sin cambios de radio/firmware)

1. **Audible RX actual: mantener DSP nativo (A)**, controlable por la API existente (`x6200_control_*`); no requiere firmware ni código nuevo.
2. **No implementar** DSP Linux→parlante en esta etapa: ruta B demostrada audiblemente pero **no segura ni madura**; queda **POSTPONED** hasta cerrar MUX seguro (#363/#364/#365), stream estable y autorización física.
3. **No hay ganador acústico**: comparación A vs B vs "native optimized" queda **PENDING_PHYSICAL_VALIDATION**.
4. **Descartar** por ahora: motores DSP nuevos (RNNoise live), escrituras directas de MUX/reg12/reg24/0x34, cambios de TX/MIC/WFM y presets por defecto nuevos sin escucha.

## Respuestas cortas

- **Q1** ¿Qué mejora el RX audible con mínima complejidad? **DSP nativo ya existente**; no hay evidencia de que un motor nuevo mejore.
- **Q2** ¿Linux puede sonar por el parlante? **Sí, experimentalmente** (prueba física 2026-11-02); **no** de forma segura/permanente.
- **Q3** ¿Conviene hoy? **No**: riesgo MUX/restore, router no integrado, calidad sin A/B.
- **Q4** ¿Qué falta para decidir? A/B físico ciego con el owner + cerrar seguridad del MUX y estabilidad del router.
- **Q5–Q10:** ver `recommendation.md` (definidas y respondidas una por una).

## Entregables

`audio-paths.md` · `native-dsp-assessment.md` · `linux-dsp-assessment.md` · `benchmark.md` ·
`ab-test-plan.md` · `measurements.md` · `recommendation.md` · `implementation-plan.md` ·
`commands.log` · `logs/` · `tools/rx-speaker-quality/` (herramienta + tests reproducibles).
