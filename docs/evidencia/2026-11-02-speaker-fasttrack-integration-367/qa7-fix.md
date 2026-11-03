# QA7/QA8 — Corrección mínima fundamentada

## Cambio 1 (genérico): pacing del worker

`src/rx_audio_live.c`: cuando el backend devuelve `WRITE_NO_SPACE`, el worker hace un respiro de
5 ms (`RX_AUDIO_LIVE_NO_SPACE_WAIT_MS`) antes de reintentar. Evita el busy-spin medido por QA6
(28.731 llamadas/s) que monopolizaba el mainloop de PulseAudio. El worker no cambia estados ni
interlocks; sólo deja correr al sink. Test host nuevo: `write_calls < 500` en
`test_backpressure_degrades` (125/0 router).

## Cambio 2 (sólo QA): `prebuf=0`

`src/audio.c`, bloque `X6200_RX_SPEAKER_QA`: al abrir el stream QA se fija `attr.prebuf=0`.
QA6 midió que el servidor negociaba `prebuf=132302 B` frente a `tlength=134064 B`; el stream
quedaba en prebuffer (`read_index=0`, `playing=0`) y sólo se destrababa al degradar/reabrir.
`prebuf=0` inicia la reproducción con el primer bloque. Producción conserva sus atributos
(200 ms, backend fail-closed).

## Resultado QA8

Dos `rx_speaker_test` PASS y soak de 30 s con `writes=302`, `drops=0`, `underruns=0`,
`backpressure=0`, un único `MUX_ON`, retorno BASE. El operador reportó **eco** y bajó el volumen
durante la prueba; no se declara producción por ese hallazgo.

## No cambios

- Backend de producción: sigue fail-closed, default OFF, target 200 ms.
- Sin cambios de MUX/I²C/interlocks ni de configuración global ALSA/PulseAudio.
- El buffer QA de 3000 ms y `prebuf=0` **no** se promueven a producción.
