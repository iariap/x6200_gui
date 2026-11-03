# QA6 — Diagnóstico de cortes (mediciones)

Fecha: 2026-11-03 03:06–03:18 UTC. Variante `X6200_RX_SPEAKER_QA=ON`, buffer QA 3000 ms.
Instrumentación: muestra/s, eventos MUX/stream, `pa_stream_get_buffer_attr`,
`pa_stream_get_latency/time/timing`, `PA_STREAM_AUTO_TIMING_UPDATE`.

## Captura

- ~44.100 muestras/s, ~10 bloques/s, `cap_max_interval_ms` 11–13 ms, `cap_gaps=0`,
  `cap_holes=0`. **La captura no entrega de más ni de menos.**

## Router/stream antes de QA7 (QA6)

- Escrituras ~8–9 bloques/s; **ciclo `DEGRADED` → `MUX_OFF` → `close` → `open` → `MUX_ON`
  cada ~1,8 s**; 16 ciclos/30 s; `backpressure=16`.
- `pa_write_calls` llegó a **28.750 en un segundo**, con **28.731 `WRITE_NO_SPACE`**: el worker
  hacía busy-spin contra `pa_stream_writable_size()`, martillando el mainloop de PulseAudio.
- Atributos: solicitado `tlength=3.000.000 us`, `prebuf=-1`; **negociado `tlength=134064 B`
  (~1,52 s), `prebuf=132302 B` (~1,50 s)**, `maxlength=4194304`, `minreq=1764`, `fragsize=8820`.
- `pa_timing_info`: `read_index=0` en todas las muestras, `playing=0`, `pa_stream_get_time=0`;
  el stream quedaba en prebuffer y no avanzaba la reproducción. El prebuf negociado era casi igual
  al tlength.

Diagnóstico: **Caso D (scheduling/pacing del worker) + Caso C (atributos negociados incompatibles,
prebuf≈tlength)**. No es captura excesiva ni falta de muestras.

## QA7 — corrección de pacing

Se agregó un respiro de 5 ms cuando `WRITE_NO_SPACE` (sin martillar el mainloop). Efecto:
`pa_write_calls` bajó de ~28.700/s a ~40–160/s; **pero los ciclos de ~1,8 s persistieron**
(backpressure=16). Conclusión: el busy-spin era real y había que corregirlo, pero no era la causa
suficiente de los cortes.

## QA8 — corrección de prebuffer (sólo QA)

Con la evidencia de `prebuf≈tlength` y `read_index=0`, se fijó **`attr.prebuf=0` sólo en QA**.
Resultado medido (30 s):

- **302 writes** (30,2 s de PCM), `drops=0`, `underruns=0`, **`backpressure=0`**.
- **Un solo `MUX_ON`/`RX_DAC`**, sin ciclos `MUX_OFF`; retorno a BASE al final.
- `pa_read_index` avanza **+88.200 B/s = 1,0 s/s**; `playing=1`; `tlength` real 134064 B,
  `prebuf=0`; sink ~352–380 ms.
- Cortes por ciclo: desaparecen en las métricas. **QA8 es la primera pasada continua.**
- El operador bajó el volumen durante el soak porque escuchó **eco**, no cortes; esa acción externa
  hizo fallar el check de volumen del harness (el test no escribe volumen). Se clasifica aparte.

## Estado

- `CUTS_QA4 = CONFIRMED`.
- `QA6_ROOT_CAUSE = WORKER_BUSY_SPIN + PREBUF_NEGOTIATED_NEAR_TLENGTH`.
- `QA8_CONTINUITY_METRICS = PASS` (302/300 bloques, 0 drops, 0 backpressure, 1 MUX_ON).
- `ECHO_OBSERVED = YES` (operador; pendiente de causa; no se toca producción).
- `PRODUCTION_READY = NO`; el fix `prebuf=0` y el buffer QA 3000 ms no se trasladan a producción.
