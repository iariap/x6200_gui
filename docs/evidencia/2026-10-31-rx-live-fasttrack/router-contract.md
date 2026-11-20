# Contrato del router RX live (#364)

Arquitectura: `BASE -> PulseAudio capture -> audio_enh_process() -> rx_audio_live -> worker -> propio stream PulseAudio -> DAC SoC -> MUX -> speaker`.

## Ring y callback

- 4 bloques fijos de 4410 muestras (100 ms a 44100 Hz), máximo 400 ms.
- `rx_audio_live_submit()` corre en el hilo de captura: copia al ring bajo mutex corto; nunca llama
  PulseAudio/I2C, nunca reserva memoria y no espera al worker.
- Overflow: descarta el bloque más viejo y contabiliza `dropped_blocks`.
- El worker consume bloques; con menos de 2 bloques no se pide DAC. Un fallo de escritura pasa a
  `DEGRADED_BASE`.

## Máquina de estados

`BASE -> PREPARE_DAC -> RX_DAC`; vuelve a `BASE` por TX, audio generado, error/ stall de captura,
datos viejos (>200 ms), fallo de stream o fallo/denegación de MUX.

`PREPARE_DAC` sólo se alcanza si:

1. `enabled` (la activación física queda fuera de esta entrega);
2. el backend `mux_ready()` devuelve true (gate A/B/C acreditado);
3. el stream de RX está READY;
4. hay ≥2 bloques procesados y frescos.

En el target actual `mux_ready` devuelve **false** y `mux_request` devuelve error: el router no puede
llegar a `RX_DAC`.

## Prioridades y convivencia

- TX/PTT/ATU: `radio.c` publica `rx_audio_live_set_tx()`; el router suspende, descarta buffers y
  pide BASE. No escribe I2C.
- Audio generado/TTS/mensajes: `audio_play()`/`audio_play_en()` marcan `generated_audio`; el router se
  suspende; `audio_play_wait()`/`audio_play_en(false)` lo liberan.
- `play_stm` sigue exclusivo de TTS/audio generado; el router usa otro stream.
- No hay loopback: la captura sigue siendo la fuente del codec, no el DAC.
- El mismo PCM post-Speex va a recorder/decodificadores y al router: no se procesa dos veces.

## Limitaciones actuales

- El backend PulseAudio real del router se compila en el target pero no se abre mientras
  `enabled=false` y `mux_ready=false`.
- No hay UI/setting nuevo ni activación automática.
- `audio_play_en()` no se reutiliza para RX permanente; se conserva intacto su comportamiento de
  audio generado (HMic/IMic/record).
