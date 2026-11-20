# QA33 — Reporte del propietario: audio audible durante las pruebas

Fecha: 2026-11-03. El propietario informa: “el audio siempre fue audible, estaba escuchando
un QSO”. El MCU tiene H1 flasheado; no se hizo otro flash.

## Interpretación correcta

- La medición A/B/A de QA29/QA32 es **captura del ADC/Mic1**, no el parlante.
- Un RMS bajo en DAC+ZERO significa que **la captura pierde RX**, no necesariamente que el
  parlante quede en silencio.
- QA10 ya había observado una **ruta nativa extra fuera de PulseAudio**: el RX puede seguir
  audible aunque el sink PCM esté silencioso.
- Por tanto, el QSO audible es evidencia de **audio presente en el parlante**, pero **no
  identifica la fuente**: puede ser RX nativo del BASE, no DSP de Linux.
- `LINUX_DSP_SPEAKER` sigue **NOT_VERIFIED** hasta disponer de un tono/marca generada por
  Linux y una confirmación inequívoca.

## Gates actualizados

- `SPEAKER_AUDIO_HEARD = YES (reporte del propietario)`
- `AUDIO_SOURCE = UNIDENTIFIED / probablemente BASE nativo`
- `BASE_RX_PRESERVED_WITH_DAC = NO` (captura ADC; no contradice el audio del parlante)
- `DAC_FEEDBACK_IN_CAPTURE = INCONCLUSIVE`
- `LINUX_DSP_RX_CAPTURE = FAIL`
- `LINUX_DSP_SPEAKER = NOT_VERIFIED`
- `BASE_RETURN = PASS`
- `H1_FLASHED = YES`
- `H1_RESULT = FAIL_FUNCTIONAL` (no recuperó captura RX)

## Pregunta pendiente

Para distinguir fuente nativa vs DSP Linux hace falta saber si, durante las pruebas, el
propietario escuchó:

1. sólo el QSO de RX nativo;
2. también un tono de 1 kHz generado por la prueba PCM;
3. o una mezcla de ambos.

No se hará ninguna prueba adicional ni otro flash hasta aclararlo.
