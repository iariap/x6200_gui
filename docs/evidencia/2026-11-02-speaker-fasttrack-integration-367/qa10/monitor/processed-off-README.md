# DSP-off vs SSB voice — comparación objetiva

Mismo `parec` del monitor del sink, misma fase PROCESSED de 5 s, mismo stream/MUX/volumen:

| Ruta | RMS mediana (1,5–6 s) | bloques < 50% mediana | mín/máx RMS |
|---|---:|---:|---:|
| RAW (pre-DSP) | 7.421 | 11,1% | 2.240 / 11.929 |
| PROCESSED con preset OFF (DSP off) | 5.643 | 13,3% | 1.278 / 11.723 |
| PROCESSED con SSB voice (NR Speex) | **255** | **46,7%** | 18 / 7.264 |

El fade aparece **sólo con el preset SSB voice/NR**; con DSP off el PCM queda continuo como RAW.
Conclusión: el fade es un efecto de `audio_enh_process` (NR Speex con level 40), no del router ni
del stream PulseAudio. El eco de RAW/ZERO queda explicado por la ruta extra nativa (monitor ZERO
en silencio).

- `PROCESSED_FADE = NR/SPEEX_CONFIRMED_OBJECTIVELY`.
- No se modifica producción ni el preset del usuario; el siguiente paso es decidir si el NR del
  preset SSB debe ajustarse o si el eco/ruta extra se resuelve primero.
