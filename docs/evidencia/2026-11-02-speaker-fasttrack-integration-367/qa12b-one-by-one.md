# QA12b — router real, fases una por una

Binario QA12b: 14.036.364 B, SHA256
`58feaca5bed6f7ba12e65017c559cfa51f5e9a8a26169934ffca020308779882`,
md5 `f90d7f16669dd4d0b1ca58b812d76ab2`; gate OK.

Cada comando: una sola fase de 5 s por el **router real**, una sesión RX_DAC, un stream, preset OFF,
volumen10, RX sin TX/PTT/ATU. Sombra reg12 `0x1008` con voice=1 durante la fase; retorno `0x1000`.

| Fase | PCM | Router | Escucha |
|---|---|---|---|
| ZERO 1 | cero | RX_DAC | **Z1: silencio** |
| TONE | 1 kHz, 500 on/off | RX_DAC | **T1: sólo tono** |
| ZERO 2 | cero | RX_DAC | **Z2a: silencio** |
| RAW | captura pre-DSP | RX_DAC | pendiente |

Con ZERO1/2 silencio y TONE sólo tono, el **router real selecciona DAC correctamente** en esta
condición. Queda RAW para ver si el eco de QA10 se reproduce en la captura/reproducción RAW.

`QA12B_ZERO = PASS`, `QA12B_TONE = PASS`, `QA12B_RAW = PENDING`. Radio restaurada original
`e32a81f3…`, un GUI, RX, tx=false, volumen1. Sin merge; `PRODUCTION_READY=NO`.
