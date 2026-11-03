# QA11 — Ruteo físico BASE/DAC con tono sintético

Binario QA11: 14.031.600 B, SHA256
`4d8c04061285d47533fcd61cc893b223325c4338de3b73c60c378f7e21f4f656`,
md5 `b4e8d3fd360e1f79b5fd152700e5ec21`; gate OK.

## Secuencia (mismo stream abierto, sin recrearlo)

| Fase | MUX solicitado | PCM DAC | Sombra reg12 | Monitor digital |
|---|---|---|---|---|
| A | BASE | ZERO | `0x00001000` | silencio |
| B | DAC (única subida) | ZERO | `0x00001008` | silencio |
| C | DAC | TONE 1 kHz / -36 dBFS, 500 ms on/off | `0x00001008` | tono |
| D | BASE (única bajada) | TONE | `0x00001000` | tono (stream sigue) |
| E | BASE | ZERO | `0x00001000` | silencio |

- Volumen 10, RX, sin TX/PTT/ATU, preset OFF, un solo stream QA; `radio_audio_route_set()`
  serializado desde el dueño GUI; sin I2C externo.
- Cada fase 5 s; análisis del monitor descartando los primeros 2 s: A/B/E RMS 0; C/D tono
  (peak 519, patrón 500 ms); E residual de los primeros 2 s es cola del buffer.
- Sombra del registro 12: `0x1000` BASE / `0x1008` DAC; **no es readback físico**.
- Sin drops/backpressure; retorno final a BASE y stream cerrado.

## Falta confirmación audible

El monitor sólo confirma el PCM digital. Para cerrar:

- A: ¿silencio o RX?
- B: ¿silencio (MUX selecciona DAC) o RX (no selecciona)?
- C: ¿sólo tono, o RX + tono?
- D: ¿sólo RX sin tono (DAC filtrado), o tono persiste (mezcla/filtración)?
- E: ¿silencio tras la cola inicial?

Interpretación: si B silencio y C sólo tono y D sin tono → MUX funciona. Si B RX y C sin tono
físico → la orden no se aplica. Si B RX y C RX+tono o D tono → `ANALOG_PATH_MIXING`.

## Estado

- `QA11_EXECUTED = YES`; `AUDIBLE_CLASSIFICATION = PENDING_OPERATOR`.
- Radio restaurada original `e32a81f3…`, un GUI, RX, tx=false, volumen1.
- `PRODUCTION_READY = NO`; sin merge.
