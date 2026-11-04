# Recomendación, Q1–Q10 y listas de continuar/posponer/descartar

> El encargo pide responder **Q1–Q10**; no adjuntó el listado literal. Se define y responde acá de forma explícita.

| # | pregunta | respuesta | clase |
|---|---|---|---|
| Q1 | ¿Qué mejora el RX audible por el parlante con mínima complejidad? | **El DSP nativo ya existente** y sus controles (DFL/NR/NB/DNF/AGC/RX EQ). No hay evidencia de que añadir un motor nuevo mejore; sí de que los ajustes nativos son el camino de producción. | `CODE` + `AUDIBLE` nativo |
| Q2 | ¿El PCM procesado por Linux puede salir por el parlante? | **Sí, experimentalmente**: prueba física 2026-11-02, `SPEAKER_AUDIO_HEARD=YES` y diferencia Off vs SSB voice oída, con router experimental y MUX. | `AUDIBLE` (previo) |
| Q3 | ¿Es una ruta segura/permanente? | **No**: router no está en la base, QA363 `BLOCKED_SAFE_MUX` (sin readback ni restore independiente), sin interlocks. QA390/QA-NATIVE-DSP **no** niegan la prueba audible: niegan la vía segura/permanente. | `CODE`+`MEASURED-HARDWARE` |
| Q4 | ¿Qué ofrece el DSP nativo? | Filtros DFL por modo, NR/NB, DNF manual/auto, AGC (modo/knee/slope/hang/tiempo), RX EQ 5 bandas + WFM; control VERIFIED, algoritmo UNKNOWN (sin bitstream). | #392 gates |
| Q5 | ¿Se puede controlar sin cambio de firmware? | **Sí**: API `x6200_control_*` ya usada por la GUI; no requiere MCU/firmware nuevo. | `CODE` |
| Q6 | ¿Qué procesamiento Linux existe y dónde aplica? | SpeexDSP NR + notch + AF leveler sobre la **copia capturada** (recorder/decoders), 44.1k mono S16, 20 ms de latencia; **no** al parlante en la base. | `CODE` |
| Q7 | ¿Hay híbrido audible? | No demostrado sin B (no hay retorno de PCM procesado al parlante); el híbrido real es A audible + procesamiento Linux no audible para grabación/análisis. | `UNKNOWN`/`NO_GO` |
| Q8 | ¿Qué se puede medir offline sin engañar? | Mapeo/constantes, consistencia de buffers/latencia, y magnitudes de **modelos** de técnicas. La inteligibilidad/calidad audible: **PENDING_PHYSICAL_VALIDATION** (no hay voz licenciada ni radio). | `ARITH`/`MODEL`/`PENDING` |
| Q9 | ¿Qué continuar/posponer/descartar? | Continuar: DSP nativo + captura existente. Posponer: ruta B (audible Linux) hasta cerrar MUX seguro y estabilidad + owner. Descartar: motor DSP nuevo live, RNNoise, escrituras MUX/reg12/24/0x34, cambios TX/MIC/WFM, presets por defecto nuevos sin escucha. | `CODE`+`AUDIBLE` |
| Q10 | ¿Recomendación mínima y próximo paso? | **A (nativo) como audible de producción; B experimental/postergada; sin ganador acústico.** Próximo paso: A/B físico autorizado (ver `ab-test-plan.md`) empezando por BASE vs nativo optimizado (A0–A5); B sólo si se cierra seguridad y el owner autoriza. | `PENDING` |

## Recomendación mínima

- **No cambiar arquitectura ni firmware** en esta etapa: mantener **A** (DSP nativo) como ruta audible y **B** como experimental documentada, sin integrar.
- **No declarar** que "Linux DSP es imposible": está **demostrado audible**; lo que falta es **seguridad, estabilidad y calidad**.
- **No elegir ganador acústico** hasta A/B físico.

## Candidatos concretos de parámetros/presets (para A/B, NO defaults)

- **Filtro SSB:** base 50–2950 Hz; candidato angosto 100–2400 Hz para señal débil; nunca cambiar a la mitad de una comparación.
- **AGC:** slow/auto, knee −60 dB, slope 6, hang off; fijarlo antes de comparar.
- **NR:** on, nivel moderado (≈20; el estado de la radio del 2026-11-02 mostraba nr_level 40, que no se propone como default sin escucha).
- **NB:** on con nivel/ancho 10/10 (defaults actuales) para impulsos.
- **DNF:** off; si hay heterodino, manual centro 1000 Hz ancho 50 Hz; evitar auto en señales múltiples.
- **RX EQ:** off/flat; probar bandas sólo si timbre es el problema y con A/B.
- **Linux capture (offline):** preset Off vs SSB voice (hay diferencia audible previa en ruta experimental; en captura sólo se puede medir, no escuchar).

## Degradaciones a evitar / descartar

- NR/NB agresivos sin escucha; DNF auto sobre voz; AGC cambiado en medio de una comparación.
- RNNoise/otros motores nuevos live sin ruta audible segura ni métricas de radio.
- Cualquier escritura directa de MUX/reg12/reg24/0x34 o cambio de `audio_play_en`.
- Cambios de TX/MIC/WFM, menús, settings o defaults sin validación.

## Postura sobre trabajos previos

- **Continuar:** DSP nativo (#392, A), captura existente para recorder/decoders, evidencia de A/B previa como prueba de posibilidad.
- **Posponer:** ruta audible Linux (#364/#365/#363), integración segura del router, RNNoise, A/B acústico (requiere owner).
- **Descartar:** parches de MUX experimentales como solución permanente, motores nuevos sin evidencia, y cualquier declaración de "mejor sonido" sin escucha.
