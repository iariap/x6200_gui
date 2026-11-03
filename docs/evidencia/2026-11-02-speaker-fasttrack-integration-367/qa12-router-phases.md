# QA12 — Router real, cuatro fases en una sola sesión RX_DAC

Binario QA12: 14.036.252 B, SHA256
`c35c4d1e41e9555a00e3f53fb441c3cd894c14160f11f098afe4f71c3a272796`,
md5 `0bd6f54310fa83fa4cb532c5340f0d97`; gate OK.

## Protocolo

Una única sesión `RX_DAC`, un único stream PulseAudio, preset OFF, volumen10, RX sin TX/PTT/ATU.
Fases de 5 s por el **router real** `rx_audio_live`; captura/recorder/decoders normales:

| Fase | PCM al router | Router state | reg12/voice |
|---|---|---|---|
| ZERO 1 | buffer QA cero | RX_DAC | 0x1008 / 1 |
| TONE | 1 kHz -36 dBFS, 500 on/off, buffer precomputado | RX_DAC | 0x1008 / 1 |
| ZERO 2 | cero | RX_DAC | 0x1008 / 1 |
| RAW | captura pre-DSP | RX_DAC | 0x1008 / 1 |

Final: BASE 0x1000, stream cerrado, preset restaurado.

## Instrumentación (201 muestras / 100 ms)

`qa12/ticks-100ms.csv`: estado router, reg12 completo, voice_bit, radio RX/TX, PA state/writable/
latencia/read-write index, submitted/writes/drops/underruns/backpressure/degraded/MUX.
`qa12/rx_qa.log`: eventos y fases; `qa12/result.txt`: salida completa.

- Router: `RX_DAC` durante las cuatro fases; sin drops, underruns ni backpressure.
- `voice_rec`: 0x1008/1 estable durante las fases; 0x1000/0 al final, sin cambios inesperados.
- Un solo MUX_ON, un solo stream; stream abierto una vez.
- Monitor `parec` (fase tras descartar 2 s): ZERO1 RMS0; TONE RMS258 patrón; ZERO2 RMS0;
  RAW RMS464 con audio de captura.

## Falta la escucha del operador para discernir

- ZERO1: ¿silencio o RX?
- TONE: ¿sólo tono o RX+tono?
- ZERO2: ¿silencio o RX?
- RAW: ¿audio normal, eco, o ambos?

Si ZERO1/2 silencio y TONE sólo tono, el router selecciona DAC correctamente y el eco de QA10
no se reproduce; si RAW tiene eco, investigar captura/reproducción RAW, no el selector.

`QA12_OBJECTIVE = PCM/MUX/STREAM CORRECTOS`; `AUDIBLE_CLASSIFICATION = PENDING_OPERATOR`.
Radio restaurada `e32a81f3…`, un GUI, RX, tx=false, volumen1. `PRODUCTION_READY=NO`; sin merge.
