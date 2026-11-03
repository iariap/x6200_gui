# QA10b — Repetición fase por fase (evaluación audible)

Binario QA10b: 14.026.976 B, SHA256
`d2510489dbf7cdbda8867da1377dea8c747c35381b603cb07fe146e88f8f1596`,
md5 `2d1cb89355752287e47a8346ad212407`; gate OK. Mismo stream/MUX, frecuencia 7.3555 MHz LSB,
volumen 10, preset SSB voice; cada comando ejecuta **una sola fase de 5 s** y restaura BASE.

## Resultados medidos

| Fase | writes | drops | underruns | backpressure | MUX_ON | resultado router |
|---|---:|---:|---:|---:|---:|---|
| ZERO | 52 | 0 | 0 | 0 | 1 | PASS_BASE_RESTORED |
| RAW | 52 | 0 | 0 | 0 | 1 | PASS_BASE_RESTORED |
| PROCESSED | 52 | 0 | 0 | 0 | 1 | PASS_BASE_RESTORED |

`read_index` avanza ~1,0 s/s, `playing=1`, un solo stream `X6200 RX Live`; `X6200 GUI Play`
presente sin datos; sin módulos loopback/remap/echo.

## Observación del operador

- **DAC_ZERO:** respondió **B** —seguía oyéndose la RX— y **un momento de silencio**.
- **DAC_RAW:** «empezó bien y luego D» (mucho eco/no se entendía).
- **DAC_PROCESSED:** «arrancó bien y se desvaneció el sonido en fade-out».

## Lectura

Con PCM de QA en cero y RX aún audible, existe una **ruta adicional fuera del PCM del router**
(no explicada por el stream QA ni por los sink-inputs de PulseAudio). El eco de RAW/PROCESSED
podría ser la suma de esa ruta (RX nativa/BASE o mezcla del MUX) con la copia DAC retardada.
No se atribuye al DSP todavía; PROCESSED es la comparación que falta.

- `QA10B_PHASES = PASS` (router/stream/restore).
- `EXTRA_RX_PATH_DURING_ZERO = CONFIRMED_BY_OPERATOR`.
- `ECHO_CAUSE = EXTRA_PATH_SUSPECTED`; `PROCESSED_FADE = OBSERVED`; falta distinguir si el fade ocurrió dentro de los 5 s o coincidió con el cierre de fase.
- `PRODUCTION_READY = NO`; no merge.
