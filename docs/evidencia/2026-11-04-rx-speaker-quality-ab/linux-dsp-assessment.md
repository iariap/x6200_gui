# Evaluación del DSP Linux existente y del camino audible experimental

## 1. Cadena actual en la base 681074e0 (captura, NO audible)

`CODE` verificado:
- `src/audio.c`: captura `alsa_input...stereo-fallback` a **44.1 kHz mono S16NE** (`src/audio.h:15`), scratch de 4410 muestras (100 ms, `src/audio.c:61`), `capture_deliver()` aplica `audio_enh_process()` y entrega el **mismo** buffer a `screen_rec_put_audio_samples()` y `dsp_put_audio_samples()` (`src/audio.c:74-83`). No hay escritura al stream de reproducción en esa ruta.
- `audio_enh_process` (según `docs/AUDIO_ENHANCEMENT_GUIDE.md`): **notch → AF leveler → SpeexDSP NR**, longitud preservada, trama Speex 441 muestras (10 ms), latencia medida **882 muestras = 20 ms** (441 del frame + 441 de libspeexdsp), priming único de 10 ms. Aplica **solo** a grabadora/decoders/spectrum; nunca al parlante.
- `audio_play_en()` (usado por voz/playback de apps) enciende `voice_rec` pero además pone HMic/IMic a 0 (`src/audio.c:458-478` según QA363); no es una vía RX neutral.

## 2. Camino audible experimental (fuera de la base)

`AUDIBLE` (prueba física 2026-11-02, operador):
- Ruta: captura → `audio_enh_process` → `rx_audio_live` (ring 4×4410=400 ms, worker, stream propio, staleness 200 ms) → DAC SoC → MUX (`voice_rec`) → parlante.
- Resultado: `SPEAKER_AUDIO_HEARD=YES`; en A/B Off vs SSB voice: `PRESET_DIFFERENCE_HEARD=YES`; `writes=18 drops=0 underruns=0`; retorno a BASE y preset restaurados; sin TX/PTT/ATU; HMic/IMic sin tocar.
- Límites declarados por la misma evidencia: **PRODUCTION_NOT_READY**, sin restore automático ante crash ni interlocks; tests previos tuvieron underruns 319/326 (el 3º los corrigió a 0).
- El router **no está en `681074e0`** (verificado: `src/rx_audio_live.c` ausente). Es una rama de prueba (`test/rx-speaker-fasttrack`) con autorización temporal.

## 3. Seguridad (por qué C no es ruta permanente)

QA363 (`GATE_A=BLOCKED_SAFE_MUX`): sin readback real del reg12, sin dueño serializado fuera de la GUI, restore sólo reiniciando la GUI (≳4 s) y reescribiendo defaults. QA365/364 dependen de eso. El router evita `audio_play_en` y serializa `radio_audio_route_set()`, pero sigue dependiendo de una escritura de MUX sin verdad física ni guardián independiente.

## 4. Reutilización / simplificación (antes de un motor nuevo)

- Reutilizar el **mismo** PCM post-`audio_enh_process` y la infraestructura del router ya probada; no duplicar SpeexDSP ni crear un motor nuevo.
- Del banco #345 (MEASURED-HOST, base previa): Speex SSB_VOICE atenúa RMS ~12.7–13.8 dB en ruido/tonos (ojo: reduce nivel percibido), ~0.17–0.23 % del tiempo real en host, 0 clipping; RNNoise host ~1.95 % del frame y 21.38 ms de latencia (2 frames + resampling) — mayor costo y latencia, sin validación audible.
- Defectos medidos en la auditoría previa (base anterior a la guía actual): el `level` podía estar invertido y el tail por callback quedaba sin NR; la guía actual documenta longitud-preservada y 882 muestras de latencia. **No se re-miden en esta tarea**; si se retoma B, primero hay que verificar que el comportamiento actual coincide con la guía.
- Conclusión: si se retoma la vía audible Linux, **reusar** esta cadena y corregir/pacing/estabilidad; **no** añadir RNNoise ni un motor nuevo sin A/B.
