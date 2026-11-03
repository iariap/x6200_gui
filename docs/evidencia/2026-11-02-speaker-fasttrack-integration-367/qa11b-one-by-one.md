# QA11b — Ruteo físico evaluado fase por fase

Binario QA11b: 14.031.712 B, SHA256
`ff14ec6817f1f81b203150b791e4d7fcdc68b590c373ae0f83082413bcdb76ab`,
md5 `75301ee64aea94a6559b33968d29a42d`; gate OK.

Cada comando: un único stream QA (abierto/cerrado en la fase), preset OFF, volumen 10, RX,
misma frecuencia; MUX sólo por `radio_audio_route_set()`; 5 s por fase.

| Fase | MUX | PCM | Sombra reg12 | Escucha del operador |
|---|---|---|---|---|
| A | BASE | ZERO | 0x1000 | **A2: RX normal** (esperado en BASE) |
| B | DAC | ZERO | 0x1008 | **B1: silencio** |
| C | DAC | TONE 1 kHz 500 ms on/off | 0x1008 | **C1: sólo tono, sin RX** |
| D | BASE | TONE | 0x1000 | **D1: sólo RX, sin tono** |
| E | BASE | ZERO | 0x1000 | **E2: RX normal** (esperado en BASE) |

## Interpretación

- `voice_rec` **sí selecciona DAC**: B silencio con PCM cero.
- **DAC llega al parlante** y sin RX: C sólo tono.
- **BASE filtra el DAC**: D sólo RX, sin tono.
- A/E en BASE con RX son el comportamiento correcto de la ruta nativa.
- **No se reprodujo mezcla de rutas** en la prueba directa de hardware.

## Relación con QA10

QA10 había reportado RX durante ZERO y eco en RAW/PROCESSED. QA11b no reproduce eso con la misma
MUX y un stream directo: B/C/D son exclusivos. Por lo tanto, **el eco de QA10 no se explica por
`ANALOG_PATH_MIXING` hardware**. Queda como diferencia del camino del router/QA (secuencia de
fases, timings de stream/MUX o percepción durante transiciones), no como mezcla física.

`QA11B_MUX = WORKS`; `ANALOG_PATH_MIXING = NOT_REPRODUCED`; `QA10_ECHO = NOT_REPRODUCED_BY_HW_ROUTE`.
Radio restaurada original `e32a81f3…`, un GUI, RX, tx=false, volumen1. Sin merge; `PRODUCTION_READY=NO`.
