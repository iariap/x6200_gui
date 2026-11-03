# QA5 — intento de evitar ciclos por backpressure; RECHAZADO

Tras confirmarse cortes auditivos en QA4, se probó QA5 con el mismo buffer QA de 3000 ms pero
elevando **sólo en QA** el timeout de backpressure de 200 ms a 3000 ms, para no ciclar a BASE.

Resultado (30 s):

```
writes=135 drops=161 underruns=0 backpressure=6
RESULT PASS_BASE_RESTORED
```

El router dejó de ciclar pero el anillo se desbordó (161 drops): el stream de salida no consume
10 bloques/s y el worker, sin degradar, no puede drenar. QA5 es **peor** que QA4; se rechazó y se
revirtió el timeout a 200 ms. El binario QA5 no es la versión final.

## Lectura

- El problema residual no es sólo el tamaño del buffer: el stream PulseAudio de QA no sostiene el
  ritmo de 100 ms/bloque. QA4 (buffer 3000 ms, timeout 200 ms) es la mejor variante medida:
  `writes=284`, `backpressure=16`, pero con cortes audibles confirmados por el operador.
- Producción no se toca: 200 ms, fail-closed.
- Rollback final a `e32a81f3…`, un solo GUI, RX, `tx=false`, volumen 1.

## Clasificación

- `QA_DEPLOY = PASS`; `QA_ROLLBACK = PASS`.
- `SHORT_RESTORE = 2/2 PASS`.
- `SOAK_RESTORE = PASS`.
- `SOAK_AUDIO_CONTINUITY = FAIL_CUTS` (QA4, confirmado por el operador; QA5 peor).
- `EXPERIMENTAL_DEPLOY_GO = NO`.
- `CRASH_RECOVERY = NOT_VERIFIED`; `PRODUCTION_READY = NO`.
- Siguiente diagnóstico: consumo real del sink / latencia PulseAudio y captura; no seguir subiendo
  buffers a ciegas.
