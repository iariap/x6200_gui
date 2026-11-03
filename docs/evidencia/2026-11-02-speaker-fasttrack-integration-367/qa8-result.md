# QA8 — Resultado de continuidad (prebuf=0)

Binario QA8: 14.026.552 B, SHA256
`5e7bac7d5ebaab1bcb217ac229e74962b50345b37dc3b9e0814f3dbe9da0092c`,
md5 `648eaac9461064f063b30886ac6bfa8a`; gate OK. QA9 (mismo audio, clasificación de volumen)
compila ARM: SHA256 `8e06b2845091c2428d2ea42d3a84aff6cd31cefd7e01eca703e0fdcc853b68d1`.

Dos short consecutivos: **PASS_BASE_RESTORED** (`writes` 16/31, `backpressure=0`).

Soak de 30 s (segunda corrida, volumen 30 inicial):

```
writes=302 (30,2 s de PCM)
drops=0
underruns=0
backpressure=0
mux_on_ok=1
mux_off_ok=1
```

- `cap_samples`: 44.100/s; `cap_gaps=0`; `holes=0`; intervalo máximo 13 ms.
- Atributos reales: `tlength=134064 B` (~1,52 s), `prebuf=0`, `fragsize=8820`,
  `minreq=1764`, `maxlength=4194304`.
- `pa_read_index`: +88.200 B/s = **1,0 s/s**; `playing=1`; `pa_latency` ~0,34–0,40 s.
- Un solo `MUX_ON`/`RX_DAC`, sin ciclos `DEGRADED`; `MUX_OFF` y `BASE` al terminar.
- El test marcó `FAIL_OR_DEGRADED` **sólo** porque el operador bajó el volumen 30→1 durante el
  soak (acción externa); el router no escribe volumen. El código QA9 ya clasifica ese cambio como
  `VOLUME_CHANGED_EXTERNAL` y no lo mezcla con el resultado del router.

## Eco

El operador reportó **eco** durante el audio y bajó el volumen por eso. No es un corte de stream:
las métricas de QA8 no muestran drops, underruns ni ciclos. El eco queda **no diagnosticado**;
la hipótesis a verificar en QA siguiente es la suma de la ruta nativa BASE y la ruta DAC (o un
delay del sink), no el buffer del router. No se toca producción.

## Conclusión

- `CUTS_QA4 = CONFIRMED`.
- `QA8_STREAM_CONTINUITY = PASS` (302/300 bloques, 0 drops, 0 backpressure, 1 MUX_ON).
- `QA8_ROUTER_RESTORE = PASS` (MUX/BASE/preset/HMic/IMic; volumen externo excluido).
- `ECHO = OBSERVED_UNRESOLVED`.
- `PRODUCTION_READY = NO`; no se promueve `prebuf=0` ni el buffer QA.
